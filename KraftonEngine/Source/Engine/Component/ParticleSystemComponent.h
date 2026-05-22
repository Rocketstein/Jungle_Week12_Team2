#pragma once

#include "Component/PrimitiveComponent.h"
#include "Render/Proxy/ParticleSystemSceneProxy.h"
#include "ParticleSystemComponent.generated.h"

class UParticleSystem;
class UFXSystemAsset;
class FParticleEmitterInstance;

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

	UFXSystemAsset* GetFXSystemAsset() const override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;
	FParticleSystemSceneProxy* GetSceneProxy() const { return static_cast<FParticleSystemSceneProxy*>(SceneProxy); }

	UParticleSystem* Template = nullptr;
	TArray<FParticleEmitterInstance*> EmitterInstances;
	int32 LODLevel = 0;
};
