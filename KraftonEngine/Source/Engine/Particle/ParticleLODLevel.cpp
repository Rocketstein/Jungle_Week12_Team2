#include "Particle/ParticleLODLevel.h"

#include <algorithm>

void UParticleLODLevel::AddModule(UParticleModule* Module)
{
	if (!Module)
	{
		return;
	}

	if (std::find(Modules.begin(), Modules.end(), Module) == Modules.end())
	{
		Modules.push_back(Module);
	}
}

void UParticleLODLevel::RemoveModule(UParticleModule* Module)
{
	Modules.erase(std::remove(Modules.begin(), Modules.end(), Module), Modules.end());
}
