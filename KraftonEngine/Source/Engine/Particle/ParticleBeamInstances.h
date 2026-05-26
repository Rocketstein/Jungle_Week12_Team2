#pragma once
#include "ParticleEmitterInstances.h"

struct FBeam2EmitterInstance : public FParticleEmitterInstance
{
	using FParticleEmitterInstance::FParticleEmitterInstance;

	void Tick(float DeltaTime, int32 LODLevel, bool bSuppressSpawning) override;
	FDynamicEmitterReplayDataBase* GetReplayData() override;

	void ResetBeamTravelTime() { BeamTravelTime = 0.f; }

private:
	float BeamTravelTime = 0.f;
};
