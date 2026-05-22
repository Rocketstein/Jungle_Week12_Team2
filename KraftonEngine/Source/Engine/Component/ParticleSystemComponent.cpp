#include "Component/ParticleSystemComponent.h"

#include "Particle/ParticleSystem.h"

UFXSystemAsset* UParticleSystemComponent::GetFXSystemAsset() const
{
	return Template;
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* NewTemplate)
{
	Template = NewTemplate;
}
