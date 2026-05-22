#include "Particle/ParticleEmitter.h"

#include "Particle/ParticleLODLevel.h"

#include <algorithm>

void UParticleEmitter::UpdateModuleLists()
{
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (LODLevel)
		{
			LODLevel->UpdateModuleLists();
		}
	}
}

void UParticleEmitter::SetLODCount(int32 LODCount)
{
	if (LODCount < 0)
	{
		LODCount = 0;
	}
	LODLevels.resize(static_cast<size_t>(LODCount));
}

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 LODLevel)
{
	if (LODLevel < 0 || LODLevel >= static_cast<int32>(LODLevels.size()))
	{
		return nullptr;
	}
	return LODLevels[LODLevel];
}

bool UParticleEmitter::CalculateMaxActiveParticleCount()
{
	PeakActiveParticles = 0;
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (LODLevel)
		{
			PeakActiveParticles = std::max(PeakActiveParticles, LODLevel->CalculateMaxActiveParticleCount());
		}
	}
	return true;
}

bool UParticleEmitter::HasAnyEnabledLODs() const
{
	for (const UParticleLODLevel* LODLevel : LODLevels)
	{
		if (LODLevel && LODLevel->bEnabled)
		{
			return true;
		}
	}
	return false;
}
