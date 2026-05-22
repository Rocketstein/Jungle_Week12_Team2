#include "ParticleSystemSceneProxy.h"
#include "Component/SubUVComponent.h"
#include "Render/Types/FrameContext.h"

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
}

void FParticleSystemSceneProxy::UpdateTransform()
{

}

void FParticleSystemSceneProxy::UpdateMaterial()
{
	EmitterDraws.resize(DynamicData.size());
	for (uint32 i = 0; i < DynamicData.size(); i++)
	{
		const FDynamicSpriteEmitterReplayDataBase& Source = static_cast<FDynamicSpriteEmitterReplayDataBase>(DynamicData[i]->GetSource());
		EmitterDraws[i].Material = Source.MaterialInterface;
		EmitterDraws[i].Type	 = Source.eEmitterType;
	}
}

void FParticleSystemSceneProxy::UpdateVisibility()
{

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

		if (Draw.Type == EDynamicEmitterType::Sprite)
		{
			// 6 indices per particle into the proxy's shared SpriteIB = Quad
			const uint32 IdxCount = ParticleCount * 6;
			Draw.FirstIndex = IndexCursor;
			Draw.IndexCount = IdxCount;
			IndexCursor += IdxCount;
		}
		else if (Draw.Type == EDynamicEmitterType::Mesh)
		{
			// Mesh path: section's index range is the static mesh's own IB.
			// FirstIndex/IndexCount come from MeshGeom, and InstanceCount = ParticleCount.
			Draw.FirstIndex = 0;
			Draw.IndexCount = Draw.MeshGeom ? Draw.MeshGeom->GetIndexBuffer().GetIndexCount() : 0;
			Draw.InstanceCount = ParticleCount;
		}
		SectionDraws.push_back({ Draw.Material, Draw.FirstIndex, Draw.IndexCount });
	}
}

void FParticleSystemSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{

}

bool FParticleSystemSceneProxy::PrepareDrawBuffer(ID3D11Device*, ID3D11DeviceContext*, FDrawCommandBuffer&) const
{
	return false;
}

bool FParticleSystemSceneProxy::PrepareDrawCommandBindings(ID3D11Device*, ID3D11DeviceContext*,
	const FPrimitiveDrawOptions&, FDrawCommand&) const
{
	return false;
}

void FParticleSystemSceneProxy::PackSpriteEmitter(const FFrameContext& Frame, FDynamicSpriteEmitterData& Emitter,
	TArray<FParticleSpriteVertex>& OutVerts,
	TArray<uint32>& OutIndices, uint32& IndexCursor)
{
	const FDynamicSpriteEmitterReplayDataBase& Source = Emitter.Source;
	const int32 Count = Source.ActiveParticleCount;
	if (Count <= 0) return;

	// Sort
	for (uint32 i = 0; i < Count; i++)
	{
		Emitter.SortSpriteParticles(Source.SortMode, Frame.CameraPosition, Frame.CameraForward, FMatrix::Identity, Source.DataContainer.ParticleIndices,
									Count, Source.DataContainer.ParticleData, Source.ParticleStride);
	}

	// Expand each particle into a 4-vert quad
	const float SubUInv = (Source.SubImages_Horizontal > 0) ? 1.0f / Source.SubImages_Horizontal : 1.0f;
	const float SubVInv = (Source.SubImages_Vertical > 0) ? 1.0f / Source.SubImages_Vertical : 1.0f;
}

void FParticleSystemSceneProxy::PackMeshEmitter(const FFrameContext& Frame, FDynamicMeshEmitterData& Emitter)
{
	
}