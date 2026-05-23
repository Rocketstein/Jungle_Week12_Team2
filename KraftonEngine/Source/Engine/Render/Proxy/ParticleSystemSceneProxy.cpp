#include "ParticleSystemSceneProxy.h"
#include "Particle/ParticleHelper.h"
#include "Render/Types/FrameContext.h"
#include "Render/Command/DrawCommand.h"
#include "Component/ParticleSystemComponent.h"

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
	EmitterDraws.resize(DynamicData.size());
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
	EmitterDraws.resize(DynamicData.size());
	for (uint32 i = 0; i < DynamicData.size(); i++)
	{
		const FDynamicSpriteEmitterReplayDataBase& Source =
			static_cast<const FDynamicSpriteEmitterReplayDataBase&>(DynamicData[i]->GetSource());
		EmitterDraws[i].Material = Source.MaterialInterface;
		EmitterDraws[i].Type	 = Source.eEmitterType;
	}
}

void FParticleSystemSceneProxy::UpdateVisibility()
{
	FPrimitiveSceneProxy::UpdateVisibility();
	bCastShadow = false;
}

void FParticleSystemSceneProxy::UpdateMesh()
{
	SectionDraws.clear();
	uint32 IndexCursor = 0;

	for (size_t i = 0; i < DynamicData.size(); ++i)
	{
		FEmitterDraw& Draw = EmitterDraws[i];
		const auto& Source = static_cast<const FDynamicSpriteEmitterReplayDataBase&>(
			DynamicData[i]->GetSource());
		const uint32 ParticleCount = static_cast<uint32>(Source.ActiveParticleCount);

		if (Draw.Type == DET_Sprite)
		{
			// 6 indices per particle into the proxy's shared SpriteIB = Quad
			const uint32 IdxCount = ParticleCount * 6;
			Draw.FirstIndex = IndexCursor;
			Draw.IndexCount = IdxCount;
			IndexCursor += IdxCount;
		}
		else if (Draw.Type == DET_Mesh)
		{
			// Mesh path: section's index range is the static mesh's own IB.
			// FirstIndex/IndexCount come from MeshGeom, and InstanceCount = ParticleCount.
			Draw.FirstIndex = 0;
			Draw.IndexCount = Draw.MeshGeom ? Draw.MeshGeom->GetIndexBuffer().GetIndexCount() : 0;
			Draw.InstanceCount = ParticleCount;
		}
		SectionDraws.push_back({ Draw.Material, Draw.FirstIndex, Draw.IndexCount });
	}

	UpdateMaterial();
}

// UpdatePerViewport: per-frame CPU work
// Runs in RenderCollector::Collect, gated by EPrimitiveProxyFlags::PerViewportUpdate.
// Actual GPU upload is deferred to PrepareDrawBuffer, signalled via bGpuBuffersDirty.
void FParticleSystemSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	if (DynamicData.empty())
	{
		bVisible = false;
		PackedVertices.clear();
		PackedIndices.clear();
		return;
	}

	// Reset scratch arrays
	PackedVertices.clear();
	PackedIndices.clear();

	// CPU pack: sort + quad expansion into the scratch arrays.
	PackSprites(Frame);

	// Visibility = at least one particle ended up in the scratch.
	bVisible = !PackedVertices.empty();
	if (!bVisible) return;

	// Signal PrepareDrawBuffer that the dynamic GPU buffers must be re-uploaded.
	bGpuBuffersDirty = true;
}

// PrepareDrawBuffer: GPU upload + bind hand-off
// Called once per proxy by DrawCommandBuilder.
// Lazy-uploads the scratch arrays when bGpuBuffersDirty is set.
bool FParticleSystemSceneProxy::PrepareDrawBuffer(
	ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, FDrawCommandBuffer& Out) const
{
	if (PackedVertices.empty() || PackedIndices.empty())
	{
		return false;
	}

	if (bGpuBuffersDirty)
	{
		const uint32 VertCount  = static_cast<uint32>(PackedVertices.size());
		const uint32 IndexCount = static_cast<uint32>(PackedIndices.size());

		if (SpriteVB.GetMaxCount() == 0)
		{
			SpriteVB.Create(InDevice, VertCount, sizeof(FParticleSpriteVertex));
		}
		if (SpriteIB.GetMaxCount() == 0)
		{
			SpriteIB.Create(InDevice, IndexCount);
		}

		// Grow if needed (doubles capacity inside).
		SpriteVB.EnsureCapacity(InDevice, VertCount);
		SpriteIB.EnsureCapacity(InDevice, IndexCount);

		// NOTE: FDynamicVertexBuffer::Update takes ELEMENT count, not byte count.
		SpriteVB.Update(InDeviceContext, PackedVertices.data(),  VertCount);
		SpriteIB.Update(InDeviceContext, PackedIndices.data(), IndexCount);

		bGpuBuffersDirty = false;
	}

	Out.VB         = SpriteVB.GetBuffer();
	Out.VBStride   = sizeof(FParticleSpriteVertex);
	Out.IB         = SpriteIB.GetBuffer();
	Out.BaseVertex = 0;
	// FirstIndex/IndexCount are written per-section by DrawCommandBuilder.

	return Out.VB != nullptr && Out.IB != nullptr;
}

// PackSprites: Dispatches each emitter to the right per-emitter packer.
// Sprite emitters write into the proxy's shared scratch arrays.
// Also refreshes per-section (FirstIndex, IndexCount) so SectionDraws stays
// consistent when emitters' active counts change frame-to-frame.
void FParticleSystemSceneProxy::PackSprites(const FFrameContext& Frame)
{
	uint32 IndexCursor = 0;
	for (size_t i = 0; i < DynamicData.size(); ++i)
	{
		if (i >= EmitterDraws.size()) break;   // defensive — UpdateMaterial sizes this
		FEmitterDraw& Draw = EmitterDraws[i];

		switch (Draw.Type)
		{
		case DET_Sprite:
		{
			const uint32 IndexBefore = IndexCursor;
			PackSpriteEmitter(Frame,
				static_cast<FDynamicSpriteEmitterData&>(*DynamicData[i]),
				PackedVertices, PackedIndices, IndexCursor);

			// Refresh section range so DrawCommandBuilder sees the right slice
			// even if ActiveParticleCount shrank since UpdateMesh.
			Draw.FirstIndex = IndexBefore;
			Draw.IndexCount = IndexCursor - IndexBefore;
			break;
		}
		case DET_Mesh:
		{
			PackMeshEmitter(Frame,
				static_cast<FDynamicMeshEmitterData&>(*DynamicData[i]));
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

bool FParticleSystemSceneProxy::PrepareDrawCommandBindings(ID3D11Device*, ID3D11DeviceContext*,
	const FPrimitiveDrawOptions&, FDrawCommand& Cmd) const
{
	// Identify which emitter this command belongs to by FirstIndex (unique per section).
	const FEmitterDraw* Hit = nullptr;
	for (const FEmitterDraw& E : EmitterDraws) {
		if (E.FirstIndex == Cmd.Buffer.FirstIndex && E.IndexCount == Cmd.Buffer.IndexCount)
		{
			Hit = &E; break;
		}
	}
	if (!Hit) return true;   // sprite path. Leave Cmd as is

	if (Hit->Type == DET_Mesh && Hit->MeshGeom)
	{
		Cmd.Buffer.VB = Hit->MeshGeom->GetVertexBuffer().GetBuffer();
		Cmd.Buffer.VBStride = Hit->MeshGeom->GetVertexBuffer().GetStride();
		Cmd.Buffer.IB = Hit->MeshGeom->GetIndexBuffer().GetBuffer();
		Cmd.Buffer.FirstIndex = 0;
		Cmd.Buffer.IndexCount = Hit->IndexCount;
		Cmd.Buffer.BaseVertex = 0;

		Cmd.Buffer.InstanceVB = Hit->InstanceVB.GetBuffer();
		Cmd.Buffer.InstanceVBStride = sizeof(FMeshParticleInstanceVertex);
		Cmd.Buffer.InstancedCount = Hit->InstanceCount;
		Cmd.Buffer.InstanceStart = 0;
	}
	return true;
}

void FParticleSystemSceneProxy::PackSpriteEmitter(const FFrameContext& Frame, FDynamicSpriteEmitterData& Emitter,
	TArray<FParticleSpriteVertex>& OutVerts,
	TArray<uint32>& OutIndices, uint32& IndexCursor)
{
	const FDynamicSpriteEmitterReplayDataBase& Source = Emitter.Source;
	const int32 Count = Source.ActiveParticleCount;
	if (Count <= 0) return;

	// Sort
	Emitter.SortSpriteParticles(Source.SortMode, Frame.CameraPosition, Frame.CameraForward, FMatrix::Identity, Source.DataContainer.ParticleIndices,
								Count, Source.DataContainer.ParticleData, Source.ParticleStride);

	for (int32 i = 0; i < Count; ++i)
	{
		const uint16 Idx = Source.DataContainer.ParticleIndices[i];
		const uint8* Bytes = Source.DataContainer.ParticleData + Idx * Source.ParticleStride;
		const FBaseParticle& P = *reinterpret_cast<const FBaseParticle*>(Bytes);

		const uint32 V0 = static_cast<uint32>(OutVerts.size());
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
			OutVerts.push_back(V);
		}

		// CW quad
		OutIndices.push_back(V0 + 0); OutIndices.push_back(V0 + 2); OutIndices.push_back(V0 + 1);
		OutIndices.push_back(V0 + 2); OutIndices.push_back(V0 + 3); OutIndices.push_back(V0 + 1);
		IndexCursor += 6;
	}
}

void FParticleSystemSceneProxy::PackMeshEmitter(const FFrameContext& Frame, FDynamicMeshEmitterData& Emitter)
{
	
}
