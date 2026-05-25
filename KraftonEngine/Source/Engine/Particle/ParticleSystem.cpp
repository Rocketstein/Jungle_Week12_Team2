#include "Particle/ParticleSystem.h"

#include "Particle/ParticleEmitter.h"

#include <algorithm>

namespace
{
	constexpr float DefaultLODDistanceStep = 1250.0f;
}

int32 UParticleSystem::GetLODCount() const
{
	return std::max(1, static_cast<int32>(LODDistances.size()));
}

int32 UParticleSystem::CreateLOD(float Distance)
{
	if (LODDistances.empty())
	{
		LODDistances.push_back(0.0f);
	}

	const int32 NewLODIndex = static_cast<int32>(LODDistances.size());
	const float DefaultDistance = LODDistances.back() + DefaultLODDistanceStep;
	LODDistances.push_back(Distance >= 0.0f ? Distance : DefaultDistance);
	NormalizeLODData();

	return NewLODIndex;
}

bool UParticleSystem::RemoveLOD(int32 LODIndex)
{
	if (LODIndex <= 0 || LODIndex >= GetLODCount())
	{
		return false;
	}

	LODDistances.erase(LODDistances.begin() + LODIndex);
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (Emitter && LODIndex < static_cast<int32>(Emitter->LODLevels.size()))
		{
			Emitter->LODLevels.erase(Emitter->LODLevels.begin() + LODIndex);
		}
	}

	NormalizeLODData();
	return true;
}

float UParticleSystem::GetLODDistance(int32 LODIndex) const
{
	if (LODIndex < 0 || LODIndex >= static_cast<int32>(LODDistances.size()))
	{
		return 0.0f;
	}
	return LODDistances[LODIndex];
}

bool UParticleSystem::SetLODDistance(int32 LODIndex, float Distance)
{
	if (LODDistances.empty())
	{
		LODDistances.push_back(0.0f);
	}

	if (LODIndex < 0 || LODIndex >= static_cast<int32>(LODDistances.size()))
	{
		return false;
	}

	LODDistances[LODIndex] = std::max(0.0f, Distance);
	NormalizeLODData();
	return true;
}

void UParticleSystem::NormalizeLODData()
{
	if (LODDistances.empty())
	{
		LODDistances.push_back(0.0f);
	}

	const int32 LODCount = static_cast<int32>(LODDistances.size());
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (Emitter)
		{
			Emitter->SyncLODLevelsToSystemCount(LODCount);
		}
	}
}
