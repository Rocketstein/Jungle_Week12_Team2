#pragma once

#include "Object/Object.h"
#include "ParticleEmitter.generated.h"

class UParticleLODLevel;

UENUM()
enum class EParticleEmitterKind
{
	Sprite
};

UCLASS()
class UParticleEmitter : public UObject
{
public:
	GENERATED_BODY(UParticleEmitter)

	virtual EParticleEmitterKind GetEmitterKind() const { return EParticleEmitterKind::Sprite; }

	const TArray<UParticleLODLevel*>& GetLODLevels() const { return LODLevels; }
	void AddLODLevel(UParticleLODLevel* LODLevel);
	void RemoveLODLevel(UParticleLODLevel* LODLevel);
	UParticleLODLevel* GetLODLevel(int32 Index) const;

protected:
	TArray<UParticleLODLevel*> LODLevels;
};

UCLASS()
class UParticleSpriteEmitter : public UParticleEmitter
{
public:
	GENERATED_BODY(UParticleSpriteEmitter)
};
