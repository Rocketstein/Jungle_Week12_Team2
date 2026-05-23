#pragma once

#include "Core/EngineTypes.h"
#include "Object/Object.h"
#include "Particle/ParticleEmitter.h"
#include "ParticleModule.generated.h"

struct FParticleEmitterInstance;
class UMaterialInterface;
class UParticleModuleTypeDataBase;
class UStaticMesh;
struct FBaseParticle;

/** ModuleType
 *	Indicates the kind of emitter the module can be applied to.
 *	ie, EPMT_Beam - only applies to beam emitters.
 *
 *	The TypeData field is present to speed up finding the TypeData module.
 */
UENUM()
enum EModuleType : int
{
	EPMT_General,
	EPMT_TypeData,
	EPMT_Beam,
	EPMT_Trail,
	EPMT_Spawn,
	EPMT_Required,
	EPMT_Event,
	EPMT_Light,
	EPMT_SubUV,
	EPMT_MAX
};

UENUM()
enum EParticleSortMode : int
{
	PSORTMODE_None,
	PSORTMODE_ViewProjDepth,
	PSORTMODE_DistanceToView,
	PSORTMODE_Age_OldestFirst,
	PSORTMODE_Age_NewestFirst,
	PSORTMODE_MAX
};

UCLASS()
class UParticleModule : public UObject
{
public:
	GENERATED_BODY(UParticleModule)

	uint8 bSpawnModule : 1 = false;
	uint8 bUpdateModule : 1 = false;
	uint8 bFinalUpdateModule : 1 = false;
	uint8 bEnabled : 1 = false;
	uint8 bEditable : 1 = false;
	uint8 LODValidity = 0xff;

	struct FContext
	{
		FParticleEmitterInstance& Owner;
		FContext(FParticleEmitterInstance& InOwner) : Owner(InOwner) {}
	};

	struct FSpawnContext : FContext
	{
		int32 Offset;
		float SpawnTime;
		FBaseParticle* ParticleBase;

		FSpawnContext(FParticleEmitterInstance& InOwner, int32 InOffset, float InSpawnTime, FBaseParticle* InParticleBase)
			: FContext(InOwner), Offset(InOffset), SpawnTime(InSpawnTime), ParticleBase(InParticleBase)
		{
		}
	};

	struct FUpdateContext : FContext
	{
		int32 Offset;
		float DeltaTime;

		FUpdateContext(FParticleEmitterInstance& InOwner, int32 InOffset, float InDeltaTime)
			: FContext(InOwner), Offset(InOffset), DeltaTime(InDeltaTime)
		{
		}
	};

	virtual void Spawn(const FSpawnContext& Context) { (void)Context; }
	virtual void Update(const FUpdateContext& Context) { (void)Context; }
	virtual void FinalUpdate(const FUpdateContext& Context) { (void)Context; }
	virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) { (void)TypeData; return 0; }
	virtual uint32 RequiredBytesPerInstance() { return 0; }
	virtual uint32 PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData) { (void)Owner; (void)InstData; return 0; }
	virtual void SetToSensibleDefaults(UParticleEmitter* Owner) { (void)Owner; }
	virtual EModuleType GetModuleType() const { return EPMT_General; }
	virtual bool IsSpawnModule() const { return bSpawnModule != 0; }
	virtual bool IsUpdateModule() const { return bUpdateModule != 0; }
	virtual bool IsSizeMultiplyLife() { return false; }
	virtual bool TouchesMeshRotation() const { return false; }
};

UCLASS()
class UParticleModuleRequired : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleRequired)

	EModuleType GetModuleType() const override { return EPMT_Required; }

	UMaterialInterface* Material = nullptr;
	FVector EmitterOrigin = FVector::ZeroVector;
	EParticleScreenAlignment ScreenAlignment = PSA_Square;
	uint8 bUseLocalSpace : 1 = false;
	uint8 bKillOnDeactivate : 1 = false;
	uint8 bKillOnCompleted : 1 = false;
	EParticleSortMode SortMode = PSORTMODE_None;
	float EmitterDuration = 1.0f;
	TArray<FParticleBurst> BurstList;
	float EmitterDelay = 0.0f;
	EParticleBurstMethod ParticleBurstMethod = EPBM_Instant;
	int32 EmitterLoops = 0;
	int32 MaxDrawCount = 0;
};

UCLASS()
class UParticleModuleSpawnBase : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleSpawnBase)

	uint32 bProcessSpawnRate : 1 = false;
	uint32 bProcessBurstList : 1 = false;

	EModuleType GetModuleType() const override { return EPMT_Spawn; }
	bool IsSpawnModule() const override { return true; }

	virtual bool GetSpawnAmount(const FContext& Context, int32 Offset, float OldLeftover, float DeltaTime, int32& Number, float& Rate)
	{
		(void)Context; (void)Offset; (void)OldLeftover; (void)DeltaTime;
		Number = 0;
		Rate = 0.0f;
		return bProcessSpawnRate != 0;
	}

	virtual bool GetBurstCount(FParticleEmitterInstance* Owner, int32 Offset, float OldLeftover, float DeltaTime, int32& Number)
	{
		(void)Owner; (void)Offset; (void)OldLeftover; (void)DeltaTime;
		Number = 0;
		return bProcessBurstList != 0;
	}

	virtual float GetMaximumSpawnRate() { return 0.0f; }
	virtual float GetEstimatedSpawnRate() { return 0.0f; }
	virtual int32 GetMaximumBurstCount() { return 0; }
};

UCLASS()
class UParticleModuleSpawn : public UParticleModuleSpawnBase
{
public:
	GENERATED_BODY(UParticleModuleSpawn)

	UPROPERTY(Edit, Category="Spawn", DisplayName="Rate", Min=0.0f, Max=10000.0f, Speed=1.0f)
	float Rate = 10.0f;

	bool GetSpawnAmount(const FContext& Context, int32 Offset, float OldLeftover, float DeltaTime, int32& Number, float& OutRate) override;
	float GetMaximumSpawnRate() override { return Rate; }
	float GetEstimatedSpawnRate() override { return Rate; }
};

UCLASS()
class UParticleModuleLifetimeBase : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleLifetimeBase)

	virtual float GetMaxLifetime() { return 0.0f; }
	virtual float GetLifetimeValue(const FContext& Context, float InTime, UObject* Data = nullptr)
	{
		(void)Context; (void)Data;
		return InTime;
	}
};

UCLASS()
class UParticleModuleLifetime : public UParticleModuleLifetimeBase
{
public:
	GENERATED_BODY(UParticleModuleLifetime)

	UParticleModuleLifetime();

	UPROPERTY(Edit, Category="Lifetime", DisplayName="Lifetime", Min=0.0f, Max=1000.0f, Speed=0.1f)
	float Lifetime = 1.0f;

	void Spawn(const FSpawnContext& Context) override;
	float GetMaxLifetime() override { return Lifetime; }
};

UCLASS()
class UParticleModuleLocationBase : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleLocationBase)
};

UCLASS()
class UParticleModuleLocation : public UParticleModuleLocationBase
{
public:
	GENERATED_BODY(UParticleModuleLocation)

	UParticleModuleLocation();

	UPROPERTY(Edit, Category="Location", DisplayName="Start Location")
	FVector StartLocation = FVector::ZeroVector;

	void Spawn(const FSpawnContext& Context) override;
};

UCLASS()
class UParticleModuleVelocityBase : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleVelocityBase)

	uint32 bInWorldSpace : 1 = false;
	uint32 bApplyOwnerScale : 1 = false;
};

UCLASS()
class UParticleModuleVelocity : public UParticleModuleVelocityBase
{
public:
	GENERATED_BODY(UParticleModuleVelocity)

	UParticleModuleVelocity();

	UPROPERTY(Edit, Category="Velocity", DisplayName="Start Velocity")
	FVector StartVelocity = FVector::UpVector;

	void Spawn(const FSpawnContext& Context) override;
};

UCLASS()
class UParticleModuleColorBase : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleColorBase)
};

UCLASS()
class UParticleModuleColor : public UParticleModuleColorBase
{
public:
	GENERATED_BODY(UParticleModuleColor)

	UParticleModuleColor();

	UPROPERTY(Edit, Category="Color", DisplayName="Start Color")
	FVector StartColor = FVector::OneVector;

	UPROPERTY(Edit, Category="Color", DisplayName="Start Alpha", Min=0.0f, Max=1.0f, Speed=0.01f)
	float StartAlpha = 1.0f;

	void Spawn(const FSpawnContext& Context) override;
};

UCLASS()
class UParticleModuleSizeBase : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleSizeBase)
};

UCLASS()
class UParticleModuleSize : public UParticleModuleSizeBase
{
public:
	GENERATED_BODY(UParticleModuleSize)

	UParticleModuleSize();

	UPROPERTY(Edit, Category="Size", DisplayName="Start Size")
	FVector StartSize = FVector::OneVector;

	void Spawn(const FSpawnContext& Context) override;
};

UCLASS()
class UParticleModuleTypeDataBase : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleTypeDataBase)

	EModuleType GetModuleType() const override { return EPMT_TypeData; }

	virtual bool RequiresBuild() const { return false; }
	virtual bool SupportsSpecificScreenAlignmentFlags() const { return false; }
	virtual bool IsAMeshEmitter() const { return false; }
};

UCLASS()
class UParticleModuleTypeDataMesh : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY(UParticleModuleTypeDataMesh)

	UStaticMesh* Mesh = nullptr;

	bool IsAMeshEmitter() const override { return true; }
};
