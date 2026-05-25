#include "ParticleBeamInstances.h"

void FBeam2EmitterInstance::Tick(float DeltaTime, bool bSuppressSpawning)
{
	
}

FDynamicEmitterReplayDataBase* FBeam2EmitterInstance::GetReplayData()
{
	if (ActiveParticles <= 0)
	{
		return nullptr;
	}

	FDynamicEmitterReplayDataBase* NewEmitterReplayData = new FDynamicBeamEmitterReplayData();

	if (!FillReplayData(*NewEmitterReplayData))
	{
		delete NewEmitterReplayData;
		return nullptr;
	}

	return NewEmitterReplayData;
}