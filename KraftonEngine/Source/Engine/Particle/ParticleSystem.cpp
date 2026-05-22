#include "Particle/ParticleSystem.h"

#include <algorithm>

void UParticleSystem::AddEmitter(UParticleEmitter* Emitter)
{
	if (!Emitter)
	{
		return;
	}

	if (std::find(Emitters.begin(), Emitters.end(), Emitter) == Emitters.end())
	{
		Emitters.push_back(Emitter);
	}
}

void UParticleSystem::RemoveEmitter(UParticleEmitter* Emitter)
{
	Emitters.erase(std::remove(Emitters.begin(), Emitters.end(), Emitter), Emitters.end());
}

UParticleEmitter* UParticleSystem::GetEmitter(int32 Index) const
{
	if (Index < 0 || Index >= static_cast<int32>(Emitters.size()))
	{
		return nullptr;
	}
	return Emitters[Index];
}
