#pragma once

#include "FX/FXSystemComponent.h"
#include "ParticleSystemComponent.generated.h"

class UParticleSystem;

UCLASS()
class UParticleSystemComponent : public UFXSystemComponent
{
public:
	GENERATED_BODY(UParticleSystemComponent)

	UFXSystemAsset* GetFXSystemAsset() const override;

	void SetTemplate(UParticleSystem* InTemplate);
	UParticleSystem* GetTemplate() const { return Template; }

private:
	UParticleSystem* Template = nullptr;
};
