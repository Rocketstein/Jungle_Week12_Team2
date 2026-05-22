#include "ParticleSystemSceneProxy.h"

FParticleSystemSceneProxy::~FParticleSystemSceneProxy()
{

}

void FParticleSystemSceneProxy::UpdateDynamicData(TArray<FDynamicEmitterDataBase*>&& NewData)
{

}

void FParticleSystemSceneProxy::UpdateTransform()
{

}

void FParticleSystemSceneProxy::UpdateMaterial()
{

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