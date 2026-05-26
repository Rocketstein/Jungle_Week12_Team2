#pragma once

#include "Core/EngineTypes.h"
#include "Core/CollisionTypes.h"
#include "Object/Object.h"
#include "Particle/ParticleEmitter.h"
#include "ParticleModule.generated.h"

struct FParticleEmitterInstance;
class UMaterialInterface;
class UParticleModuleTypeDataBase;
class UStaticMesh;
class UParticleLODLevel;
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

UENUM()
enum EBeamTangentMethod : int
{
	PEBTANM_Direct,
	PEBTANM_UserSet,
	PEBTANM_MAX
};

UENUM()
enum class EParticleCollisionResponseMode : uint8
{
	Bounce = 0,
	Stop = 1,
	Kill = 2,
};

UCLASS()
class UParticleModule : public UObject
{
public:
	GENERATED_BODY(UParticleModule)

	uint8 bSpawnModule : 1 = false;
	uint8 bUpdateModule : 1 = false;
	uint8 bFinalUpdateModule : 1 = false;
	uint8 bEnabled : 1 = true;
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
	virtual UParticleModule* CloneForLOD(UParticleLODLevel* NewOuter) const;

protected:
	void CopyModuleBaseTo(UParticleModule* Copy) const;
};

UCLASS()
class UParticleModuleRequired : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleRequired)

	EModuleType GetModuleType() const override { return EPMT_Required; }
	UParticleModule* CloneForLOD(UParticleLODLevel* NewOuter) const override;

	UMaterialInterface* Material = nullptr;
	FVector EmitterOrigin = FVector::ZeroVector;
	EParticleScreenAlignment ScreenAlignment = PSA_FacingCameraPosition;
	int32 SubImages_Horizontal = 1;
	int32 SubImages_Vertical = 1;
	int32 AlphaSource = 0; // 0: texture alpha, 1: texture luminance
	float AlphaThreshold = 0.0f;
	float AlphaPower = 1.0f;
	float ColorIntensity = 1.0f;
	uint8 bUseLocalSpace : 1 = false;
	uint8 bKillOnDeactivate : 1 = false;
	uint8 bKillOnCompleted : 1 = false;
	EParticleSortMode SortMode = PSORTMODE_None;
	float EmitterDuration = 1.0f;
	float EmitterDelay = 0.0f;
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

	UParticleModuleSpawn();

	UPROPERTY(Edit, Category="Spawn", DisplayName="Rate", Min=0.0f, Max=10000.0f, Speed=1.0f)
	float Rate = 10.0f;

	TArray<FParticleBurst> BurstList;
	EParticleBurstMethod ParticleBurstMethod = EPBM_Instant;

	bool GetSpawnAmount(const FContext& Context, int32 Offset, float OldLeftover, float DeltaTime, int32& Number, float& OutRate) override;
	float GetMaximumSpawnRate() override { return Rate; }
	float GetEstimatedSpawnRate() override { return Rate; }
	int32 GetMaximumBurstCount() override;
	UParticleModule* CloneForLOD(UParticleLODLevel* NewOuter) const override;
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
	float LifetimeMin = 1.0f;
	float LifetimeMax = 1.0f;

	void Spawn(const FSpawnContext& Context) override;
	float GetMaxLifetime() override { return LifetimeMax; }
	UParticleModule* CloneForLOD(UParticleLODLevel* NewOuter) const override;
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
	FVector StartLocationMin = FVector::ZeroVector;
	FVector StartLocationMax = FVector::ZeroVector;

	void Spawn(const FSpawnContext& Context) override;
	UParticleModule* CloneForLOD(UParticleLODLevel* NewOuter) const override;
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
	FVector StartVelocityMin = FVector::UpVector;
	FVector StartVelocityMax = FVector::UpVector;

	void Spawn(const FSpawnContext& Context) override;
	UParticleModule* CloneForLOD(UParticleLODLevel* NewOuter) const override;
};

struct FParticleCollisionPayload
{
	int32 CollisionCount = 0;
};

UCLASS()
class UParticleModuleCollision : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleCollision)

	UParticleModuleCollision();

	uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;
	void FinalUpdate(const FUpdateContext& Context) override;
	UParticleModule* CloneForLOD(UParticleLODLevel* NewOuter) const override;

	UPROPERTY(Edit, Category="Collision", DisplayName="Trace Channel", Type=Enum, Enum=StaticEnum_ECollisionChannel())
	ECollisionChannel TraceChannel = ECollisionChannel::WorldStatic;

	UPROPERTY(Edit, Category="Collision", DisplayName="Response Mode", Type=Enum, Enum=StaticEnum_EParticleCollisionResponseMode())
	EParticleCollisionResponseMode ResponseMode = EParticleCollisionResponseMode::Bounce;

	UPROPERTY(Edit, Category="Collision", DisplayName="Damping Factor", Min=0.0f, Max=1.0f, Speed=0.01f)
	float DampingFactor = 0.5f;

	UPROPERTY(Edit, Category="Collision", DisplayName="Collision Offset", Min=0.0f, Max=100.0f, Speed=0.1f)
	float CollisionOffset = 0.1f;

	UPROPERTY(Edit, Category="Collision", DisplayName="Max Collisions", Min=0, Max=128, Speed=1.0f)
	int32 MaxCollisions = 1;
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
	float StartAlphaMin = 1.0f;
	float StartAlphaMax = 1.0f;

	void Spawn(const FSpawnContext& Context) override;
	UParticleModule* CloneForLOD(UParticleLODLevel* NewOuter) const override;
};

UCLASS()
class UParticleModuleColorOverLife : public UParticleModuleColorBase
{
public:
	GENERATED_BODY(UParticleModuleColorOverLife)

	UParticleModuleColorOverLife();

	UPROPERTY(Edit, Category="Color", DisplayName="Color Over Life")
	FVector ColorOverLife = FVector::OneVector;

	UPROPERTY(Edit, Category="Color", DisplayName="Alpha Over Life", Min=0.0f, Max=1.0f, Speed=0.01f)
	float AlphaOverLife = 0.0f;

	void Spawn(const FSpawnContext& Context) override;
	void Update(const FUpdateContext& Context) override;
	UParticleModule* CloneForLOD(UParticleLODLevel* NewOuter) const override;
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
	FVector StartSizeMin = FVector::OneVector;
	FVector StartSizeMax = FVector::OneVector;

	void Spawn(const FSpawnContext& Context) override;
	UParticleModule* CloneForLOD(UParticleLODLevel* NewOuter) const override;
};

UCLASS()
class UParticleModuleBeamBase : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleBeamBase)

	EModuleType GetModuleType() const override { return EPMT_Beam; }
};

UCLASS()
class UParticleModuleBeamSource : public UParticleModuleBeamBase
{
public:
	GENERATED_BODY(UParticleModuleBeamSource)

	UPROPERTY(Edit, Category="Beam Source", DisplayName="Source Point")
	FVector SourcePoint = FVector::ZeroVector;

	UPROPERTY(Edit, Category="Beam Source", DisplayName="Source Tangent Method")
	EBeamTangentMethod SourceTangentMethod = PEBTANM_Direct;

	UPROPERTY(Edit, Category="Beam Source", DisplayName="Source Tangent")
	FVector SourceTangent = FVector(0.0f, 0.0f, 40.0f);

	UParticleModule* CloneForLOD(UParticleLODLevel* NewOuter) const override;
};

UCLASS()
class UParticleModuleBeamTarget : public UParticleModuleBeamBase
{
public:
	GENERATED_BODY(UParticleModuleBeamTarget)

	UPROPERTY(Edit, Category="Beam Target", DisplayName="Target Point")
	FVector TargetPoint = FVector(100.0f, 0.0f, 0.0f);

	UPROPERTY(Edit, Category="Beam Target", DisplayName="Target Tangent Method")
	EBeamTangentMethod TargetTangentMethod = PEBTANM_Direct;

	UPROPERTY(Edit, Category="Beam Target", DisplayName="Target Tangent")
	FVector TargetTangent = FVector(0.0f, 0.0f, -40.0f);

	UParticleModule* CloneForLOD(UParticleLODLevel* NewOuter) const override;
};

UCLASS()
class UParticleModuleBeamNoise : public UParticleModuleBeamBase
{
public:
	GENERATED_BODY(UParticleModuleBeamNoise)

	UPROPERTY(Edit, Category="Beam Noise", Min=0.0f, Max=1000.0f, Speed=0.25f)
	float NoiseAmplitude = 0.0f;

	UPROPERTY(Edit, Category="Beam Noise", Min=0.0f, Max=128.0f, Speed=0.1f)
	float NoiseFrequency = 3.0f;

	UPROPERTY(Edit, Category="Beam Noise", Min=0.0f, Max=100.0f, Speed=0.05f)
	float NoiseSpeed = 0.0f;

	UPROPERTY(Edit, Category="Beam Noise", Min=0.0f, Max=10000.0f, Speed=1.0f)
	float NoiseSeed = 0.0f;

	UPROPERTY(Edit, Category="Beam Noise", DisplayName="Low Freq Enabled")
	bool bLowFreqEnabled = false;

	UPROPERTY(Edit, Category="Beam Noise", DisplayName="Noise Range Min")
	FVector NoiseRangeMin = FVector(0.0f, -30.0f, -30.0f);

	UPROPERTY(Edit, Category="Beam Noise", DisplayName="Noise Range Max")
	FVector NoiseRangeMax = FVector(0.0f, 30.0f, 30.0f);

	UParticleModule* CloneForLOD(UParticleLODLevel* NewOuter) const override;
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
	virtual bool IsABeamEmitter() const { return false; }
	virtual bool IsARibbonEmitter() const { return false; }
};

UCLASS()
class UParticleModuleTypeDataMesh : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY(UParticleModuleTypeDataMesh)

	UStaticMesh* Mesh = nullptr;

	bool IsAMeshEmitter() const override { return true; }
	UParticleModule* CloneForLOD(UParticleLODLevel* NewOuter) const override;
};
