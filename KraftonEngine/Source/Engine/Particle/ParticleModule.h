#pragma once

#include "Core/EngineTypes.h"
#include "Object/Object.h"
#include "ParticleModule.generated.h"

class FParticleEmitterInstance;
struct FBaseParticle;

UENUM()
enum class EParticleModuleType
{
	General,
	Required,
	Spawn,
	Lifetime,
	Location,
	Velocity,
	Color,
	Size,
	TypeData
};

UCLASS()
class UParticleModule : public UObject
{
public:
	GENERATED_BODY(UParticleModule)

	virtual EParticleModuleType GetModuleType() const { return EParticleModuleType::General; }
	virtual bool IsSpawnModule() const { return false; }
	virtual bool IsUpdateModule() const { return true; }
};

UCLASS()
class UParticleModuleRequired : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleRequired)

	EParticleModuleType GetModuleType() const override { return EParticleModuleType::Required; }
};

UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleSpawn)

	EParticleModuleType GetModuleType() const override { return EParticleModuleType::Spawn; }
	bool IsSpawnModule() const override { return true; }

	UPROPERTY(Edit, Category="Spawn", DisplayName="Rate", Min=0.0, Max=10000.0, Speed=1.0)
	float Rate = 10.0f;
};

UCLASS()
class UParticleModuleLifetime : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleLifetime)

	EParticleModuleType GetModuleType() const override { return EParticleModuleType::Lifetime; }

	UPROPERTY(Edit, Category="Lifetime", DisplayName="Lifetime", Min=0.0, Max=1000.0, Speed=0.1)
	float Lifetime = 1.0f;
};

UCLASS()
class UParticleModuleLocation : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleLocation)

	EParticleModuleType GetModuleType() const override { return EParticleModuleType::Location; }

	UPROPERTY(Edit, Category="Location", DisplayName="Start Location")
	FVector StartLocation = FVector::ZeroVector;
};

UCLASS()
class UParticleModuleVelocity : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleVelocity)

	EParticleModuleType GetModuleType() const override { return EParticleModuleType::Velocity; }

	UPROPERTY(Edit, Category="Velocity", DisplayName="Start Velocity")
	FVector StartVelocity = FVector::UpVector;
};

UCLASS()
class UParticleModuleColor : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleColor)

	EParticleModuleType GetModuleType() const override { return EParticleModuleType::Color; }
};

UCLASS()
class UParticleModuleSize : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleSize)

	EParticleModuleType GetModuleType() const override { return EParticleModuleType::Size; }

	UPROPERTY(Edit, Category="Size", DisplayName="Start Size")
	FVector StartSize = FVector::OneVector;
};

UCLASS()
class UParticleModuleTypeDataBase : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleTypeDataBase)

	EParticleModuleType GetModuleType() const override { return EParticleModuleType::TypeData; }
};

UCLASS()
class UParticleModuleTypeDataMesh : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY(UParticleModuleTypeDataMesh)
};
