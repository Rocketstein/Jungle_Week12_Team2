#pragma once

#include "Component/PrimitiveComponent.h"
#include "Core/UObject/TSoftObjectPtr.h"
#include "Particle/ParticleSystem.h"
#include "ParticleSystemComponent.generated.h"

class UFXSystemAsset;
class FParticleSystemSceneProxy;
struct FParticleEmitterInstance;

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
	~UParticleSystemComponent() override;

	void PostEditProperty(const char* PropertyName) override;
	void EndPlay() override;
	UFXSystemAsset* GetFXSystemAsset() const override;
	void SetTemplate(UParticleSystem* NewTemplate);
	FPrimitiveSceneProxy* CreateSceneProxy() override;
	FParticleSystemSceneProxy* GetSceneProxy() const;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	virtual void InitParticles();
	void ResetParticles(bool bEmptyInstances = false);
	void InitializeSystem();

	UPROPERTY(Edit, Category="Particles", DisplayName="Template", Type=SoftObject, Class=UParticleSystem)
	TSoftObjectPtr<UParticleSystem> Template;

	UPROPERTY(Edit, Category="Particles", DisplayName="Particle System Priority", Min=0, Max=65535, Speed=1.0f)
	int32 SortPriority = 0;

	TArray<FParticleEmitterInstance*> EmitterInstances;
	int32 LODLevel = 0;
};
