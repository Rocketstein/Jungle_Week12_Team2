#include "ParticleSystemSceneProxy.h"

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

}

void FParticleSystemSceneProxy::PackMeshEmitter(const FFrameContext& Frame, FDynamicMeshEmitterData& Emitter)
{
	
}