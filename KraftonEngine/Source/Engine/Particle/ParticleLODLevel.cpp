#include "Particle/ParticleLODLevel.h"

#include "Particle/ParticleModule.h"

void UParticleLODLevel::UpdateModuleLists()
{
	SpawningModules.clear();
	SpawnModules.clear();
	UpdateModules.clear();
	TypeDataModule = nullptr;

	if (SpawnModule && SpawnModule->bEnabled)
	{
		SpawningModules.push_back(SpawnModule);
	}

	for (UParticleModule* Module : Modules)
	{
		if (!Module || !Module->bEnabled)
		{
			continue;
		}

		if (UParticleModuleSpawnBase* SpawnBase = Cast<UParticleModuleSpawnBase>(Module))
		{
			SpawningModules.push_back(SpawnBase);
		}

		if (UParticleModuleTypeDataBase* TypeData = Cast<UParticleModuleTypeDataBase>(Module))
		{
			TypeDataModule = TypeData;
		}

		if (Module->IsSpawnModule())
		{
			SpawnModules.push_back(Module);
		}

		if (Module->IsUpdateModule())
		{
			UpdateModules.push_back(Module);
		}
	}
}

int32 UParticleLODLevel::CalculateMaxActiveParticleCount()
{
	PeakActiveParticles = 0;
	return PeakActiveParticles;
}

int32 UParticleLODLevel::GetModuleIndex(UParticleModule* InModule)
{
	for (int32 Index = 0; Index < static_cast<int32>(Modules.size()); ++Index)
	{
		if (Modules[Index] == InModule)
		{
			return Index;
		}
	}
	return -1;
}

UParticleModule* UParticleLODLevel::GetModuleAtIndex(int32 InIndex)
{
	if (InIndex < 0 || InIndex >= static_cast<int32>(Modules.size()))
	{
		return nullptr;
	}
	return Modules[InIndex];
}

void UParticleLODLevel::SetLevelIndex(int32 InLevelIndex)
{
	Level = InLevelIndex;
}
