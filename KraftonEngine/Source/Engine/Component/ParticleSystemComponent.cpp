#include "Component/ParticleSystemComponent.h"

#include "Particle/ParticleSystem.h"
#include "Render/Proxy/ParticleSystemSceneProxy.h"

UFXSystemAsset* UParticleSystemComponent::GetFXSystemAsset() const
{
	return Template;
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* NewTemplate)
{
	Template = NewTemplate;
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	return new FParticleSystemSceneProxy(this);
}
