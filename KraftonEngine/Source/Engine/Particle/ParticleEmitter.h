#pragma once

#include "Core/EngineTypes.h"
#include "Object/Object.h"
#include "ParticleEmitter.generated.h"

class UParticleLODLevel;
class UParticleModule;
class UMaterialInterface;

UENUM()
enum EParticleBurstMethod : int
{
	EPBM_Instant,
	EPBM_Interpolated,
	EPBM_MAX
};

UENUM()
enum EParticleScreenAlignment : int
{
	PSA_FacingCameraPosition,
	PSA_Square,
	PSA_Velocity,
	PSA_TypeSpecific,
	PSA_MAX
};

USTRUCT()
struct FParticleBurst
{
	GENERATED_BODY(FParticleBurst)

	UPROPERTY(Edit, Category="ParticleBurst", DisplayName="Count")
	int32 Count = 0;

	UPROPERTY(Edit, Category="ParticleBurst", DisplayName="Count Low")
	int32 CountLow = -1;

	UPROPERTY(Edit, Category="ParticleBurst", DisplayName="Time", Min=0.0, Max=1.0, Speed=0.01)
	float Time = 0.0f;
};

UCLASS()
class UParticleEmitter : public UObject
{
public:
	GENERATED_BODY(UParticleEmitter)

	FName EmitterName;

	uint8 bUseLegacySpawningBehavior : 1 = false;
	uint8 bRequiresLoopNotification : 1 = false;
	uint8 bAxisLockEnabled : 1 = false;
	uint8 bMeshRotationActive : 1 = false;
	uint8 ConvertedModules : 1 = false;
	uint8 bIsSoloing : 1 = false;
	uint8 bCookedOut : 1 = false;
	uint8 bDisabledLODsKeepEmitterAlive : 1 = false;

	TArray<UParticleLODLevel*> LODLevels;

	int32 PeakActiveParticles = 0;
	int32 InitialAllocationCount = 0;

	TMap<UParticleModule*, uint32> ModuleOffsetMap;
	TMap<UParticleModule*, uint32> ModuleInstanceOffsetMap;

	int32 ParticleSize = 0;
	int32 ReqInstanceBytes = 0;
	int32 TypeDataOffset = 0;
	int32 TypeDataInstanceOffset = 0;

	TArray<UParticleModule*> ModulesNeedingInstanceData;

	virtual void UpdateModuleLists();
	virtual void SetEmitterName(FName Name) { EmitterName = Name; }
	virtual FName& GetEmitterName() { return EmitterName; }
	virtual void SetLODCount(int32 LODCount);
	virtual UParticleLODLevel* GetLODLevel(int32 LODLevel);
	virtual bool CalculateMaxActiveParticleCount();
	virtual void Build() {}
	virtual void CacheEmitterModuleInfo() {}
	virtual bool HasAnyEnabledLODs() const;
};
