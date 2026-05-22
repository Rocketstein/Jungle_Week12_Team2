#include "Particle/ParticleEmitter.h"

#include <algorithm>

void UParticleEmitter::AddLODLevel(UParticleLODLevel* LODLevel)
{
	if (!LODLevel)
	{
		return;
	}

	if (std::find(LODLevels.begin(), LODLevels.end(), LODLevel) == LODLevels.end())
	{
		LODLevels.push_back(LODLevel);
	}
}

void UParticleEmitter::RemoveLODLevel(UParticleLODLevel* LODLevel)
{
	LODLevels.erase(std::remove(LODLevels.begin(), LODLevels.end(), LODLevel), LODLevels.end());
}

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 Index) const
{
	if (Index < 0 || Index >= static_cast<int32>(LODLevels.size()))
	{
		return nullptr;
	}
	return LODLevels[Index];
}
