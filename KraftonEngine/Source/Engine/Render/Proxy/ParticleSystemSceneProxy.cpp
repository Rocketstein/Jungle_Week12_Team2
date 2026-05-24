#include "ParticleSystemSceneProxy.h"
#include "Particle/ParticleHelper.h"
#include "Render/Types/FrameContext.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Particle/ParticleDynamicData.h"
#include "Component/ParticleSystemComponent.h"
#include "Materials/Material.h"
#include "Mesh/StaticMesh.h"

#include <algorithm>

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
		bIsEmitterOrderDirty = true;
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
		const FDynamicEmitterReplayDataBase& Source = DynamicData[i]->GetSource();
		const FDynamicRenderableEmitterReplayDataBase* RenderableSource =
			dynamic_cast<const FDynamicRenderableEmitterReplayDataBase*>(&Source);

		EmitterDraws[i].Material = RenderableSource && RenderableSource->MaterialInterface
			? RenderableSource->MaterialInterface->GetMaterial()
			: nullptr;
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
			const auto& Source = static_cast<const FDynamicSpriteEmitterReplayData&>(
				DynamicData[i]->GetSource());
			const uint32 ParticleCount = static_cast<uint32>(Source.ActiveParticleCount);

			// 6 indices per particle into the proxy's shared sprite index buffer.
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
	SpritePacker.ResetFrame();
	MeshPacker.ResetFrame(EmitterDraws);

	if (DynamicData.empty())
	{
		bVisible = false;
		return;
	}

	// CPU pack: sort, quad expansion, and mesh instance payloads.
	PackParticles(Frame);

	bVisible = bInstancePacked;
	bInstancePacked = false;
	if (!bVisible) return;

	SpritePacker.MarkGpuBuffersDirty();
}

// PrepareDrawBuffer: GPU upload + bind hand-off
bool FParticleSystemSceneProxy::PrepareDrawBuffer(
	ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, FDrawCommandBuffer& Out) const
{
	if (SpritePacker.HasPackedSprites())
	{
		return SpritePacker.PrepareDrawBuffer(InDevice, InDeviceContext, Out);
	}

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

	return Out.VB != nullptr && Out.IB != nullptr;
}

void FParticleSystemSceneProxy::SortEmitters()
{
	const uint16 Count = static_cast<uint16>(EmitterDraws.size());
	EmitterDrawOrder.resize(Count);
	for (uint16 i = 0; i < Count; ++i)
	{
		EmitterDrawOrder[i] = i;
	}

	// Ascending: lower SortingPriority draws first, higher draws on top.
	std::stable_sort(EmitterDrawOrder.begin(), EmitterDrawOrder.end(),
		[this](uint16 A, uint16 B)
		{
			return EmitterDraws[A].SortingPriority < EmitterDraws[B].SortingPriority;
		});

	bIsEmitterOrderDirty = false;
}

void FParticleSystemSceneProxy::PackParticles(const FFrameContext& Frame)
{
	// Rebuild Emitter order before packing vertices
	if (bIsEmitterOrderDirty) {
		SortEmitters();
	}

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
			SpritePacker.PackEmitter(Frame,
				static_cast<FDynamicSpriteEmitterData&>(*DynamicData[i]), IndexCursor);

			// Refresh section range so DrawCommandBuilder sees the right slice
			// even if ActiveParticleCount shrank since UpdateMesh.
			Draw.FirstIndex = IndexBefore;
			Draw.IndexCount = IndexCursor - IndexBefore;
			break;
		}
		case DET_Mesh:
		{
			MeshPacker.PackEmitter(Frame,
				static_cast<FDynamicMeshEmitterData&>(*DynamicData[i]),
				Draw);
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

		if (!Draw.PackedInstances.empty() || SpritePacker.HasPackedSprites()) { bInstancePacked = true; }
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

bool FParticleSystemSceneProxy::FSpriteParticlePacker::PrepareDrawBuffer(
	ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, FDrawCommandBuffer& Out) const
{
	if (!HasPackedSprites())
	{
		return false;
	}

	const uint32 VertCount  = static_cast<uint32>(PackedVertices.size());
	const uint32 IndexCount = static_cast<uint32>(IndexPattern.size());

	if (VertexBuffer.GetMaxCount() == 0)
	{
		VertexBuffer.Create(InDevice, VertCount, sizeof(FParticleSpriteVertex));
	}
	else
	{
		VertexBuffer.EnsureCapacity(InDevice, VertCount);
	}

	// NOTE: FDynamicVertexBuffer::Update takes ELEMENT count, not byte count.
	if (bGpuBuffersDirty)
	{
		VertexBuffer.Update(InDeviceContext, PackedVertices.data(), VertCount);
		bGpuBuffersDirty = false;
	}

	if (bIndexBufferDirty)
	{
		if (IndexBuffer.GetMaxCount() == 0)
		{
			IndexBuffer.Create(InDevice, IndexCount);
		}
		else
		{
			IndexBuffer.EnsureCapacity(InDevice, IndexCount);
		}

		IndexBuffer.Update(InDeviceContext, IndexPattern.data(), IndexCount);
		bIndexBufferDirty = false;
	}

	Out.VB         = VertexBuffer.GetBuffer();
	Out.VBStride   = sizeof(FParticleSpriteVertex);
	Out.IB         = IndexBuffer.GetBuffer();
	Out.BaseVertex = 0;
	return Out.VB != nullptr && Out.IB != nullptr;
}

void FParticleSystemSceneProxy::FSpriteParticlePacker::PackEmitter(const FFrameContext& Frame, FDynamicSpriteEmitterData& Emitter, uint32& IndexCursor)
{
	const FDynamicSpriteEmitterReplayData& Source = Emitter.Source;
	const int32 Count = Source.ActiveParticleCount;
	if (Count <= 0 ||
		!Source.DataContainer.ParticleData ||
		!Source.DataContainer.ParticleIndices ||
		Source.ParticleStride < static_cast<int32>(sizeof(FBaseParticle)))
	{
		return;
	}

	TArray<uint16> SortedParticleIndices(Source.DataContainer.ParticleIndices, Source.DataContainer.ParticleIndices + Count);
	Emitter.SortParticles(Source.SortMode, Frame.CameraPosition, Frame.CameraForward, FMatrix::Identity,
		SortedParticleIndices.data(), Count, Source.DataContainer.ParticleData, Source.ParticleStride);

	const uint32 ParticleCount = static_cast<uint32>(Count);
	const uint32 FirstParticle = IndexCursor / 6;
	EnsureIndexPattern(FirstParticle + ParticleCount);

	PackedVertices.reserve(PackedVertices.size() + Count * 4);
	for (int32 i = 0; i < Count; ++i)
	{
		const uint16 Idx = SortedParticleIndices[i];
		const uint8* Bytes = Source.DataContainer.ParticleData + Idx * Source.ParticleStride;
		const FBaseParticle& P = *reinterpret_cast<const FBaseParticle*>(Bytes);

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
			PackedVertices.push_back(V);
		}
	}

	IndexCursor += ParticleCount * 6;
}

void FParticleSystemSceneProxy::FMeshParticlePacker::ResetFrame(TArray<FEmitterDraw>& EmitterDraws)
{
	for (FEmitterDraw& Draw : EmitterDraws)
	{
		if (Draw.Type == DET_Mesh)
		{
			Draw.PackedInstances.clear();
		}
	}
}

bool FParticleSystemSceneProxy::FMeshParticlePacker::HasPackedInstances(const TArray<FEmitterDraw>& EmitterDraws) const
{
	for (const FEmitterDraw& Draw : EmitterDraws)
	{
		if (Draw.Type == DET_Mesh && !Draw.PackedInstances.empty())
		{
			return true;
		}
	}
	return false;
}

void FParticleSystemSceneProxy::FMeshParticlePacker::PackEmitter(const FFrameContext& Frame,
	FDynamicMeshEmitterData& Emitter, FEmitterDraw& Draw)
{
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

	TArray<uint16> SortedParticleIndices(Source.DataContainer.ParticleIndices, Source.DataContainer.ParticleIndices + Count);
	Emitter.SortParticles(Source.SortMode, Frame.CameraPosition, Frame.CameraForward, FMatrix::Identity,
		SortedParticleIndices.data(), Count, Source.DataContainer.ParticleData, Source.ParticleStride);

	for (int32 i = 0; i < Count; ++i)
	{
		const uint16 Idx = SortedParticleIndices[i];
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

void FParticleSystemSceneProxy::UpdateCB(FEmitterDraw& EmitterDraw, const FDynamicEmitterReplayDataBase& Source)
{
	const FDynamicSpriteEmitterReplayData* SpriteSource = dynamic_cast<const FDynamicSpriteEmitterReplayData*>(&Source);
	const uint32 SubUVCols = SpriteSource ? static_cast<uint32>(SpriteSource->SubImages_Horizontal) : 1;
	const uint32 SubUVRows = SpriteSource ? static_cast<uint32>(SpriteSource->SubImages_Vertical) : 1;
	const uint32 ScreenAlignment = SpriteSource ? static_cast<uint32>(SpriteSource->ScreenAlignment) : 0;

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

// Lazily grows the persistent sprite quad index buffer to cover RequiredParticleCount quads; no-op if already large enough.
void FParticleSystemSceneProxy::FSpriteParticlePacker::EnsureIndexPattern(uint32 RequiredParticleCount)
{
	if (RequiredParticleCount <= IndexPatternParticleCapacity)
	{
		return;
	}

	/* Quad = 6 Indices */
	IndexPattern.reserve(RequiredParticleCount * 6);
	for (uint32 ParticleIndex = IndexPatternParticleCapacity; ParticleIndex < RequiredParticleCount; ++ParticleIndex)
	{
		const uint32 V0 = ParticleIndex * 4;
		IndexPattern.push_back(V0 + 0);
		IndexPattern.push_back(V0 + 2);
		IndexPattern.push_back(V0 + 1);
		IndexPattern.push_back(V0 + 2);
		IndexPattern.push_back(V0 + 3);
		IndexPattern.push_back(V0 + 1);
	}

	IndexPatternParticleCapacity = RequiredParticleCount;
	bIndexBufferDirty = true;
}
