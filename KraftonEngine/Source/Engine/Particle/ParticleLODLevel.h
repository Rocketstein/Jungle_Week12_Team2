#pragma once

#include "Object/Object.h"
#include "ParticleLODLevel.generated.h"

class UParticleModule;
class UParticleModuleRequired;
class UParticleModuleTypeDataBase;

UCLASS()
class UParticleLODLevel : public UObject
{
public:
	GENERATED_BODY(UParticleLODLevel)

	int32 Level = 0;
	bool bEnabled = true;

	UParticleModuleRequired* RequiredModule = nullptr;
	TArray<UParticleModule*> Modules;
	UParticleModuleTypeDataBase* TypeDataModule = nullptr;

	const TArray<UParticleModule*>& GetModules() const { return Modules; }
	void AddModule(UParticleModule* Module);
	void RemoveModule(UParticleModule* Module);
};
