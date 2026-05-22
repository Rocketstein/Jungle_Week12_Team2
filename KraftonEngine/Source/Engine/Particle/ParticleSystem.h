#pragma once

#include "FX/FXSystemAsset.h"
#include "ParticleSystem.generated.h"

class UParticleEmitter;

UCLASS()
class UParticleSystem : public UFXSystemAsset
{
public:
	GENERATED_BODY(UParticleSystem)

	const TArray<UParticleEmitter*>& GetEmitters() const { return Emitters; }
	void AddEmitter(UParticleEmitter* Emitter);
	void RemoveEmitter(UParticleEmitter* Emitter);
	UParticleEmitter* GetEmitter(int32 Index) const;

private:
	TArray<UParticleEmitter*> Emitters;
};
