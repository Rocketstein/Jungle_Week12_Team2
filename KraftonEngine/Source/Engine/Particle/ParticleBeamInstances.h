#pragma once
#include "ParticleEmitterInstances.h"

struct FBeam2EmitterInstance : public FParticleEmitterInstance
{
	using FParticleEmitterInstance::FParticleEmitterInstance;

	void Tick(float DeltaTime, bool bSuppressSpawning) override;
	FDynamicEmitterReplayDataBase* GetReplayData() override;
};