#pragma once

#include "Component/PrimitiveComponent.h"
#include "Core/UObject/TSoftObjectPtr.h"
#include "Particle/ParticleLODContext.h"
#include "Particle/ParticleSystem.h"
#include "ParticleSystemComponent.generated.h"

class UFXSystemAsset;
class FParticleSystemSceneProxy;
struct FParticleEmitterInstance;

struct FParticleEventCollideData
{
	int32 EmitterIndex = -1;
	uint16 ParticleDirectIndex = 0;
	uint32 ParticleId = 0;
	FVector Location = FVector::ZeroVector;
	FVector OldLocation = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FVector Normal = FVector::ZeroVector;
	float EmitterTime = 0.0f;
	float ParticleRelativeTime = 0.0f;
	float HitTime = 0.0f;
	bool bParticleWasKilled = false;
};

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
	int32 DecideLODLevel(const FParticleLODContext& Context) const;
	void SetForcedLODLevel(int32 InLODLevel);
	void ClearForcedLODLevel();
	virtual void InitParticles();
	void ResetParticles(bool bEmptyInstances = false);
	void InitializeSystem();
	void ClearParticleEvents();
	void AddCollisionEvent(const FParticleEventCollideData& EventData);
	const TArray<FParticleEventCollideData>& GetCollisionEvents() const { return CollisionEvents; }

	UPROPERTY(Edit, Category="Particles", DisplayName="Template", Type=SoftObject, Class=UParticleSystem)
	TSoftObjectPtr<UParticleSystem> Template;

	UPROPERTY(Edit, Category="Particles", DisplayName="Particle System Priority", Min=0, Max=65535, Speed=1.0f)
	int32 SortPriority = 0;

	TArray<FParticleEmitterInstance*> EmitterInstances;
	TArray<FParticleEventCollideData> CollisionEvents;
	int32 LODLevel = 0;
	int32 ForcedLODLevel = -1;
	TArray<float> LODDistances;
};
