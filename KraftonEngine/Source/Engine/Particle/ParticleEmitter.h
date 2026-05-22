#pragma once

#include "Object/Object.h"
#include "ParticleEmitter.generated.h"

class UParticleLODLevel;

UCLASS()
class UParticleEmitter : public UObject
{
public:
	GENERATED_BODY(UParticleEmitter)

	TArray<UParticleLODLevel*> LODLevels;
};

UCLASS()
class UParticleSpriteEmitter : public UParticleEmitter
{
public:
	GENERATED_BODY(UParticleSpriteEmitter)
};
