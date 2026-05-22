#pragma once

#include "Object/Object.h"
#include "ParticleLODLevel.generated.h"

class UParticleModule;
class UParticleModuleEventGenerator;
class UParticleModuleEventReceiverBase;
class UParticleModuleRequired;
class UParticleModuleSpawn;
class UParticleModuleSpawnBase;
class UParticleModuleTypeDataBase;

UCLASS()
class UParticleLODLevel : public UObject
{
public:
	GENERATED_BODY(UParticleLODLevel)

	int32 Level = 0;
	uint32 bEnabled : 1;

	UParticleModuleRequired* RequiredModule = nullptr;
	TArray<UParticleModule*> Modules;

	UParticleModuleTypeDataBase* TypeDataModule = nullptr;
	UParticleModuleSpawn* SpawnModule = nullptr;
	UParticleModuleEventGenerator* EventGenerator = nullptr;

	TArray<UParticleModuleSpawnBase*> SpawningModules;
	TArray<UParticleModule*> SpawnModules;
	TArray<UParticleModule*> UpdateModules;
	TArray<UParticleModuleEventReceiverBase*> EventReceiverModules;

	uint32 ConvertedModules : 1;
	int32 PeakActiveParticles = 0;

	virtual void UpdateModuleLists();
	virtual int32 CalculateMaxActiveParticleCount();
	int32 GetModuleIndex(UParticleModule* InModule);
	UParticleModule* GetModuleAtIndex(int32 InIndex);
	virtual void SetLevelIndex(int32 InLevelIndex);
};
