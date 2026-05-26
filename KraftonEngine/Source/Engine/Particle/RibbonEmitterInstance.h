#pragma once
#include "ParticleEmitterInstances.h"

struct FRibbonEmitterInstance : public FParticleEmitterInstance 
{
	explicit FRibbonEmitterInstance(UParticleSystemComponent* InComponent);
	
	void PostSpawn(FBaseParticle* Particle, float Interp, float SpawnTime) override;
	FDynamicEmitterReplayDataBase* GetReplayData() override;
	
private:
	uint32 RibbonSpawnSequence=0;
};
