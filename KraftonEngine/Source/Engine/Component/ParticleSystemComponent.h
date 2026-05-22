#pragma once

#include "Component/PrimitiveComponent.h"
#include "ParticleSystemComponent.generated.h"

class UParticleSystem;

UCLASS(HiddenInComponentList)
class UFXSystemComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY(UFXSystemComponent)
};

UCLASS()
class UParticleSystemComponent : public UFXSystemComponent
{
public:
	GENERATED_BODY(UParticleSystemComponent)

	UParticleSystem* Template = nullptr;
};
