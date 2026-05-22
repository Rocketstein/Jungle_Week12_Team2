#pragma once

#include "Component/PrimitiveComponent.h"
#include "Particle/ParticleSystem.h"
#include "ParticleSystemComponent.generated.h"

class UFXSystemAsset;
class FParticleEmitterInstance;

UCLASS(HiddenInComponentList)
class UFXSystemComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY(UFXSystemComponent)

	virtual UFXSystemAsset* GetFXSystemAsset() const { return nullptr; }
};

UCLASS()
class UParticleSystemComponent : public UFXSystemComponent
{
public:
	GENERATED_BODY(UParticleSystemComponent)

	UFXSystemAsset* GetFXSystemAsset() const override;
	void SetTemplate(UParticleSystem* NewTemplate);

	UPROPERTY(Edit, Category="Particles", DisplayName="Template", Type=SoftObject, Class=UParticleSystem)
	UParticleSystem* Template = nullptr;
	TArray<FParticleEmitterInstance*> EmitterInstances;
	int32 LODLevel = 0;
};
