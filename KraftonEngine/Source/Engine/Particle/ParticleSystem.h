#pragma once

#include "Object/Object.h"
#include "ParticleSystem.generated.h"

class UParticleEmitter;

UCLASS()
class UFXSystemAsset : public UObject
{
public:
	GENERATED_BODY(UFXSystemAsset)
};

UCLASS()
class UParticleSystem : public UFXSystemAsset
{
public:
	GENERATED_BODY(UParticleSystem)

	TArray<UParticleEmitter*> Emitters;
};
