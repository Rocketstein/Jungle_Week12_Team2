#include "Component/ParticleSystemComponent.h"

#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleEmitterInstances.h"
#include "Particle/ParticleSystem.h"
#include "Render/Proxy/ParticleSystemSceneProxy.h"

UParticleSystemComponent::~UParticleSystemComponent()
{
	ResetParticles(true);
}

UFXSystemAsset* UParticleSystemComponent::GetFXSystemAsset() const
{
	return Template;
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* NewTemplate)
{
	if (Template == NewTemplate)
	{
		return;
	}

	ResetParticles(true);
	Template = NewTemplate;
	InitializeSystem();
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	return new FParticleSystemSceneProxy(this);
}

void UParticleSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Template)
	{
		return;
	}

	if (EmitterInstances.empty())
	{
		InitializeSystem();
	}

	for (FParticleEmitterInstance* EmitterInstance : EmitterInstances)
	{
		if (EmitterInstance)
		{
			EmitterInstance->Tick(DeltaTime, false);
		}
	}
}

void UParticleSystemComponent::InitParticles()
{
	ResetParticles(true);

	if (!Template)
	{
		return;
	}

	EmitterInstances.reserve(Template->Emitters.size());
	for (UParticleEmitter* Emitter : Template->Emitters)
	{
		if (!Emitter)
		{
			EmitterInstances.push_back(nullptr);
			continue;
		}

		FParticleEmitterInstance* Instance = new FParticleEmitterInstance(this);
		Instance->InitParameters(Emitter);
		Instance->Init();
		EmitterInstances.push_back(Instance);
	}
}

void UParticleSystemComponent::ResetParticles(bool bEmptyInstances)
{
	for (FParticleEmitterInstance* EmitterInstance : EmitterInstances)
	{
		delete EmitterInstance;
	}

	if (bEmptyInstances)
	{
		EmitterInstances.clear();
	}
	else
	{
		for (FParticleEmitterInstance*& EmitterInstance : EmitterInstances)
		{
			EmitterInstance = nullptr;
		}
	}
}

void UParticleSystemComponent::InitializeSystem()
{
	InitParticles();
}
