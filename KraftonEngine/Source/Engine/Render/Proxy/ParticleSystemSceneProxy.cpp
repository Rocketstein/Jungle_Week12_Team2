#include "ParticleSystemSceneProxy.h"
#include "Particle/ParticleHelper.h"
#include "Render/Types/FrameContext.h"
#include "Render/Command/DrawCommand.h"
#include "Component/ParticleSystemComponent.h"
#include "Materials/Material.h"
#include "Mesh/StaticMesh.h"

FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	// Per-frame CPU pack (sort + quad expansion) needs the FrameContext for
	// camera-dependent sort order. Required for RenderCollector to invoke
	// UpdatePerViewport on this proxy each frame.
	ProxyFlags |= EPrimitiveProxyFlags::PerViewportUpdate;

	// Bounds for particle systems are owned by UParticleSystem and FParticleEmitterInstance (dynamic per-frame bounds).
	// Until that's wired up on the CPU side, opt out of frustum + occlusion culling
	ProxyFlags |= EPrimitiveProxyFlags::NeverCull;
	ProxyFlags &= ~EPrimitiveProxyFlags::ShowAABB;
}

FParticleSystemSceneProxy::~FParticleSystemSceneProxy()
{
	for (FDynamicEmitterDataBase* P : DynamicData)
	{
		delete P;
	}
	DynamicData.clear();
}

void FParticleSystemSceneProxy::UpdateDynamicData(TArray<FDynamicEmitterDataBase*>&& NewData)
{
	for (FDynamicEmitterDataBase* Old : DynamicData)
	{
		delete Old;
	}
	DynamicData = std::move(NewData);

	if (EmitterDraws.size() != DynamicData.size())
	{
		EmitterDraws.resize(DynamicData.size());
	}
}

void FParticleSystemSceneProxy::UpdateTransform()
{
	FPrimitiveSceneProxy::UpdateTransform();
	ComponentToWorld = GetOwner()->GetWorldMatrix();
	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(FMatrix::Identity);
	MarkPerObjectCBDirty();
}

void FParticleSystemSceneProxy::UpdateMaterial()
{
	for (uint32 i = 0; i < DynamicData.size(); i++)
	{
		const FDynamicSpriteEmitterReplayDataBase& Source =
			static_cast<const FDynamicSpriteEmitterReplayDataBase&>(DynamicData[i]->GetSource());
		EmitterDraws[i].Material = Source.MaterialInterface ? Source.MaterialInterface->GetMaterial() : nullptr;
		EmitterDraws[i].Type	 = Source.eEmitterType;
		EmitterDraws[i].EmitterIndex = DynamicData[i]->EmitterIndex;

		UpdateCB(EmitterDraws[i], Source);
	}
}

void FParticleSystemSceneProxy::UpdateVisibility()
{
	FPrimitiveSceneProxy::UpdateVisibility();
	bCastShadow = false;
}

void FParticleSystemSceneProxy::UpdateMesh()
{
	UpdateMaterial();

	SectionDraws.clear();
	uint32 IndexCursor = 0;

	for (size_t i = 0; i < DynamicData.size(); ++i)
	{
		FEmitterDraw& Draw = EmitterDraws[i];

		if (Draw.Type == DET_Sprite)
		{
			const auto& Source = static_cast<const FDynamicSpriteEmitterReplayDataBase&>(
				DynamicData[i]->GetSource());
			const uint32 ParticleCount = static_cast<uint32>(Source.ActiveParticleCount);

			// 6 indices per particle into the proxy's shared SpriteIB = Quad
			const uint32 IdxCount = ParticleCount * 6;
			Draw.FirstIndex = IndexCursor;
			Draw.IndexCount = IdxCount;
			IndexCursor += IdxCount;
		}
		else if (Draw.Type == DET_Mesh)
		{
			const auto& Source = static_cast<const FDynamicMeshEmitterReplayData&>(
				DynamicData[i]->GetSource());
			const uint32 ParticleCount = static_cast<uint32>(Source.ActiveParticleCount);

			// Mesh path: section's index range is the static mesh's own IB.
			// FirstIndex/IndexCount come from MeshGeom, and InstanceCount = ParticleCount.
			Draw.FirstIndex = 0;
			Draw.InstanceCount = ParticleCount;
			Draw.MeshGeom = Source.StaticMesh ? Source.StaticMesh->GetLODMeshBuffer(Source.LODLevel) : nullptr;
			Draw.IndexCount = Draw.MeshGeom ? Draw.MeshGeom->GetIndexBuffer().GetIndexCount() : 0;

		}
		SectionDraws.push_back({ Draw.Material, Draw.FirstIndex, Draw.IndexCount });
	}
}

// UpdatePerViewport: per-frame CPU work
// Runs in RenderCollector::Collect, gated by EPrimitiveProxyFlags::PerViewportUpdate.
void FParticleSystemSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	// Reset scratch arrays. Sprite scratch is shared, mesh scratch is per-emitter.
	PackedSpriteVertices.clear();
	PackedSpriteIndices.clear();
	for (FEmitterDraw& Draw : EmitterDraws)
	{
		if (Draw.Type == DET_Mesh)
		{
			Draw.PackedInstances.clear();
		}
	}

	if (DynamicData.empty())
	{
		bVisible = false;
		return;
	}

	// CPU pack: sort + quad expansion (sprites)
	PackParticles(Frame);

	// Visibility = at least one emitter produced data.
	bool bAnyMeshInstances = false;
	for (const FEmitterDraw& Draw : EmitterDraws)
	{
		if (Draw.Type == DET_Mesh && !Draw.PackedInstances.empty())
		{
			bAnyMeshInstances = true;
			break;
		}
	}
	bVisible = !PackedSpriteVertices.empty() || bAnyMeshInstances;
	if (!bVisible) return;

	bGpuBuffersDirty = true;
}

// PrepareDrawBuffer: GPU upload + bind hand-off
bool FParticleSystemSceneProxy::PrepareDrawBuffer(
	ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, FDrawCommandBuffer& Out) const
{
	const bool bHasSprites = !PackedSpriteVertices.empty() && !PackedSpriteIndices.empty();

	if (bHasSprites && bGpuBuffersDirty)
	{
		const uint32 VertCount  = static_cast<uint32>(PackedSpriteVertices.size());
		const uint32 IndexCount = static_cast<uint32>(PackedSpriteIndices.size());

		if (SpriteVB.GetMaxCount() == 0)
		{
			SpriteVB.Create(InDevice, VertCount, sizeof(FParticleSpriteVertex));
		}
		if (SpriteIB.GetMaxCount() == 0)
		{
			SpriteIB.Create(InDevice, IndexCount);
		}

		// Grow if needed
		SpriteVB.EnsureCapacity(InDevice, VertCount);
		SpriteIB.EnsureCapacity(InDevice, IndexCount);

		// NOTE: FDynamicVertexBuffer::Update takes ELEMENT count, not byte count.
		SpriteVB.Update(InDeviceContext, PackedSpriteVertices.data(),  VertCount);
		SpriteIB.Update(InDeviceContext, PackedSpriteIndices.data(), IndexCount);

		bGpuBuffersDirty = false;
	}

	if (bHasSprites)
	{
		Out.VB         = SpriteVB.GetBuffer();
		Out.VBStride   = sizeof(FParticleSpriteVertex);
		Out.IB         = SpriteIB.GetBuffer();
		Out.BaseVertex = 0;
	}
	else
	{
		for (const FEmitterDraw& Draw : EmitterDraws)
		{
			if (Draw.Type == DET_Mesh && Draw.MeshGeom && Draw.InstanceCount > 0)
			{
				Out.VB         = Draw.MeshGeom->GetVertexBuffer().GetBuffer();
				Out.VBStride   = Draw.MeshGeom->GetVertexBuffer().GetStride();
				Out.IB         = Draw.MeshGeom->GetIndexBuffer().GetBuffer();
				Out.BaseVertex = 0;
				break;
			}
		}
	}

	return Out.VB != nullptr && Out.IB != nullptr;
}

void FParticleSystemSceneProxy::PackParticles(const FFrameContext& Frame)
{
	uint32 IndexCursor = 0;
	for (size_t i = 0; i < DynamicData.size(); ++i)
	{
		if (i >= EmitterDraws.size()) break;
		FEmitterDraw& Draw = EmitterDraws[i];

		switch (Draw.Type)
		{
		case DET_Sprite:
		{
			const uint32 IndexBefore = IndexCursor;
			PackSpriteEmitter(Frame,
				static_cast<FDynamicSpriteEmitterData&>(*DynamicData[i]), IndexCursor);

			// Refresh section range so DrawCommandBuilder sees the right slice
			// even if ActiveParticleCount shrank since UpdateMesh.
			Draw.FirstIndex = IndexBefore;
			Draw.IndexCount = IndexCursor - IndexBefore;
			break;
		}
		case DET_Mesh:
		{
			PackMeshEmitter(Frame,
				static_cast<FDynamicMeshEmitterData&>(*DynamicData[i]),
				static_cast<uint32>(i));
			// Mesh section range stays as UpdateMesh set it (static-mesh IB range
			// is fixed; InstanceCount is the per-frame variable).
			break;
		}
		default:
			break;
		}

		// Mirror the refreshed range into SectionDraws so the builder picks it up.
		if (i < SectionDraws.size())
		{
			SectionDraws[i].FirstIndex = Draw.FirstIndex;
			SectionDraws[i].IndexCount = Draw.IndexCount;
		}
	}
}

bool FParticleSystemSceneProxy::PrepareDrawCommandBindings(ID3D11Device* InDevice,
	ID3D11DeviceContext* InDeviceContext,
	const FPrimitiveDrawOptions&, FDrawCommand& Cmd, int32 SectionIndex) const
{
	if (SectionIndex < 0 || SectionIndex >= static_cast<int32>(EmitterDraws.size()))
	{
		return false;
	}

	const FEmitterDraw& Hit = EmitterDraws[SectionIndex];

	// Upload CB if dirty
	if (Hit.bParticleParamCBDirty)
	{
		if (!Hit.ParticleParamCB.GetBuffer())
		{
			Hit.ParticleParamCB.Create(
				InDevice,
				sizeof(FParticleParamConstants),
				"ParticleParamCB");
		}

		Hit.ParticleParamCB.Update(
			InDeviceContext,
			&Hit.ParticleParams,
			sizeof(FParticleParamConstants));

		Hit.bParticleParamCBDirty = false;
	}
	Cmd.Bindings.PerShaderCB[0] = &Hit.ParticleParamCB;

	if (Hit.Type == DET_Mesh && Hit.MeshGeom && Hit.InstanceCount > 0)
	{
		if (Hit.bInstanceVBDirty && !Hit.PackedInstances.empty())
		{
			const uint32 Count = static_cast<uint32>(Hit.PackedInstances.size());
			if (Hit.InstanceVB.GetMaxCount() == 0)
			{
				Hit.InstanceVB.Create(InDevice, Count, sizeof(FMeshParticleInstanceVertex));
			}
			Hit.InstanceVB.EnsureCapacity(InDevice, Count);
			Hit.InstanceVB.Update(InDeviceContext, Hit.PackedInstances.data(), Count);
			Hit.bInstanceVBDirty = false;
		}

		Cmd.Buffer.VB                = Hit.MeshGeom->GetVertexBuffer().GetBuffer();
		Cmd.Buffer.VBStride          = Hit.MeshGeom->GetVertexBuffer().GetStride();
		Cmd.Buffer.IB                = Hit.MeshGeom->GetIndexBuffer().GetBuffer();
		Cmd.Buffer.FirstIndex        = 0;
		Cmd.Buffer.IndexCount        = Hit.IndexCount;
		Cmd.Buffer.BaseVertex        = 0;

		Cmd.Buffer.InstanceVB        = Hit.InstanceVB.GetBuffer();
		Cmd.Buffer.InstanceVBStride  = sizeof(FMeshParticleInstanceVertex);
		Cmd.Buffer.InstancedCount    = Hit.InstanceCount;
		Cmd.Buffer.InstanceStart     = 0;
	}
	return true;
}

void FParticleSystemSceneProxy::PackSpriteEmitter(const FFrameContext& Frame, FDynamicSpriteEmitterData& Emitter, uint32& IndexCursor)
{
	const FDynamicSpriteEmitterReplayDataBase& Source = Emitter.Source;
	const int32 Count = Source.ActiveParticleCount;
	if (Count <= 0 ||
		!Source.DataContainer.ParticleData ||
		!Source.DataContainer.ParticleIndices ||
		Source.ParticleStride < static_cast<int32>(sizeof(FBaseParticle)))
	{
		return;
	}

	// Sort
	Emitter.SortSpriteParticles(Source.SortMode, Frame.CameraPosition, Frame.CameraForward, FMatrix::Identity, Source.DataContainer.ParticleIndices,
								Count, Source.DataContainer.ParticleData, Source.ParticleStride);

	const bool bShouldUpdateIB = EnsureSpriteIndexUpdate();

	PackedSpriteVertices.reserve(PackedSpriteVertices.size() + Count * 4);
	if (bShouldUpdateIB) PackedSpriteIndices.reserve(PackedSpriteIndices.size() + Count * 6);
	for (int32 i = 0; i < Count; ++i)
	{
		const uint16 Idx = Source.DataContainer.ParticleIndices[i];
		const uint8* Bytes = Source.DataContainer.ParticleData + Idx * Source.ParticleStride;
		const FBaseParticle& P = *reinterpret_cast<const FBaseParticle*>(Bytes);

		const uint32 V0 = static_cast<uint32>(PackedSpriteVertices.size());
		for (int corner = 0; corner < 4; ++corner)
		{
			FParticleSpriteVertex V;
			V.Position = P.Location;
			V.Size = FVector(P.Size.X, P.Size.Y, /*subImageLerp*/ 0.0f);
			V.UV = FVector2{ float(corner & 1), float((corner >> 1) & 1) };  // 0,0..1,1
			V.Color = FVector4(P.Color.R, P.Color.G, P.Color.B, P.Color.A);
			V.Rotation = P.Rotation;
			V.SubImageIndex = 0.0f;
			V.Velocity = P.Velocity;
			PackedSpriteVertices.push_back(V);
		}

		// CW quad
		if (bShouldUpdateIB) {
			PackedSpriteIndices.push_back(V0 + 0); PackedSpriteIndices.push_back(V0 + 2); PackedSpriteIndices.push_back(V0 + 1);
			PackedSpriteIndices.push_back(V0 + 2); PackedSpriteIndices.push_back(V0 + 3); PackedSpriteIndices.push_back(V0 + 1);
			IndexCursor += 6;
		}
	}
}

void FParticleSystemSceneProxy::PackMeshEmitter(const FFrameContext& Frame,
	FDynamicMeshEmitterData& Emitter, uint32 SectionIndex)
{
	if (SectionIndex >= EmitterDraws.size()) return;
	FEmitterDraw& Draw = EmitterDraws[SectionIndex];

	const FDynamicMeshEmitterReplayData& Source = Emitter.MeshSource;
	const int32 Count = Source.ActiveParticleCount;
	Draw.InstanceCount = static_cast<uint32>(Count > 0 ? Count : 0);

	if (Count <= 0 ||
		!Source.DataContainer.ParticleData ||
		!Source.DataContainer.ParticleIndices ||
		Source.ParticleStride < static_cast<int32>(sizeof(FBaseParticle)))
	{
		return;
	}

	Draw.PackedInstances.clear();
	Draw.PackedInstances.reserve(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		const uint16 Idx = Source.DataContainer.ParticleIndices[i];
		const uint8* Bytes = Source.DataContainer.ParticleData + Idx * Source.ParticleStride;
		const FBaseParticle& P = *reinterpret_cast<const FBaseParticle*>(Bytes);

		const FMatrix Model = FMatrix::MakeScaleMatrix(P.Size)
		                    * FMatrix::MakeRotationZ(P.Rotation)
		                    * FMatrix::MakeTranslationMatrix(P.Location);

		FMeshParticleInstanceVertex V;
		V.Transform    = Model;
		V.Color        = FVector4(P.Color.R, P.Color.G, P.Color.B, P.Color.A);
		V.DynamicParam = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
		Draw.PackedInstances.push_back(V);
	}

	Draw.bInstanceVBDirty = true;
}

void FParticleSystemSceneProxy::UpdateCB(FEmitterDraw& EmitterDraw, const FDynamicSpriteEmitterReplayDataBase& Source)
{
	const uint32 SubUVCols = static_cast<uint32>(Source.SubImages_Horizontal);
	const uint32 SubUVRows = static_cast<uint32>(Source.SubImages_Vertical);
	const uint32 ScreenAlignment = static_cast<uint32>(Source.ScreenAlignment);

	if (EmitterDraw.ParticleParams.SubUVCols == SubUVCols &&
		EmitterDraw.ParticleParams.SubUVRows == SubUVRows &&
		EmitterDraw.ParticleParams.ScreenAlignment == ScreenAlignment)
	{
		return;
	}

	EmitterDraw.ParticleParams.SubUVCols = SubUVCols;
	EmitterDraw.ParticleParams.SubUVRows = SubUVRows;
	EmitterDraw.ParticleParams.ScreenAlignment = ScreenAlignment;
	EmitterDraw.bParticleParamCBDirty = true;
}

bool FParticleSystemSceneProxy::EnsureSpriteIndexUpdate() const 
{
	if (PackedSpriteIndices.empty() || DynamicData.size() > PackedSpriteIndices.size() * 6 /* Quad = 4 vertices, 6 indices */) return true;

	return false;
}