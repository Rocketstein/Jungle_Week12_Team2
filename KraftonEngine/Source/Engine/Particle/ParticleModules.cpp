#include "Particle/ParticleModule.h"

#include "Particle/ParticleEmitterInstances.h"
#include "Particle/ParticleLODLevel.h"

#include <algorithm>
#include <random>

namespace
{
	float RandomRange(float MinValue, float MaxValue)
	{
		if (MaxValue < MinValue)
		{
			std::swap(MinValue, MaxValue);
		}

		static thread_local std::mt19937 Generator{ std::random_device{}() };
		std::uniform_real_distribution<float> Distribution(MinValue, MaxValue);
		return Distribution(Generator);
	}

	FVector RandomRange(const FVector& MinValue, const FVector& MaxValue)
	{
		return FVector(
			RandomRange(MinValue.X, MaxValue.X),
			RandomRange(MinValue.Y, MaxValue.Y),
			RandomRange(MinValue.Z, MaxValue.Z));
	}
}

void UParticleModule::CopyModuleBaseTo(UParticleModule* Copy) const
{
	if (!Copy)
	{
		return;
	}

	Copy->bSpawnModule = bSpawnModule;
	Copy->bUpdateModule = bUpdateModule;
	Copy->bFinalUpdateModule = bFinalUpdateModule;
	Copy->bEnabled = bEnabled;
	Copy->bEditable = bEditable;
	Copy->LODValidity = LODValidity;
}

UParticleModule* UParticleModule::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModule* Copy = Cast<UParticleModule>(Duplicate(NewOuter));
	CopyModuleBaseTo(Copy);
	return Copy;
}

UParticleModule* UParticleModuleRequired::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleRequired* Copy = GUObjectArray.CreateObject<UParticleModuleRequired>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->Material = Material;
	Copy->EmitterOrigin = EmitterOrigin;
	Copy->ScreenAlignment = ScreenAlignment;
	Copy->SubImages_Horizontal = SubImages_Horizontal;
	Copy->SubImages_Vertical = SubImages_Vertical;
	Copy->AlphaSource = AlphaSource;
	Copy->AlphaThreshold = AlphaThreshold;
	Copy->AlphaPower = AlphaPower;
	Copy->ColorIntensity = ColorIntensity;
	Copy->bUseLocalSpace = bUseLocalSpace;
	Copy->bKillOnDeactivate = bKillOnDeactivate;
	Copy->bKillOnCompleted = bKillOnCompleted;
	Copy->SortMode = SortMode;
	Copy->EmitterDuration = EmitterDuration;
	Copy->EmitterDelay = EmitterDelay;
	Copy->EmitterLoops = EmitterLoops;
	Copy->MaxDrawCount = MaxDrawCount;
	return Copy;
}

UParticleModuleSpawn::UParticleModuleSpawn()
{
	bEnabled = true;
	bProcessSpawnRate = true;
	bProcessBurstList = true;
}

bool UParticleModuleSpawn::GetSpawnAmount(const FContext& Context, int32 Offset, float OldLeftover, float DeltaTime,
	int32& Number, float& OutRate)
{
	(void)Context;
	(void)Offset;
	(void)OldLeftover;
	(void)DeltaTime;
	Number = 0;
	OutRate = std::max(0.0f, Rate);
	return true;
}

int32 UParticleModuleSpawn::GetMaximumBurstCount()
{
	int32 MaxBurst = 0;
	for (const FParticleBurst& Burst : BurstList)
	{
		MaxBurst += std::max(Burst.Count, Burst.CountLow);
	}
	return std::max(0, MaxBurst);
}

UParticleModule* UParticleModuleSpawn::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleSpawn* Copy = GUObjectArray.CreateObject<UParticleModuleSpawn>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->Rate = Rate;
	Copy->BurstList = BurstList;
	Copy->ParticleBurstMethod = ParticleBurstMethod;
	return Copy;
}

UParticleModuleLifetime::UParticleModuleLifetime()
{
	bSpawnModule = true;
}

void UParticleModuleLifetime::Spawn(const FSpawnContext& Context)
{
	if (!Context.ParticleBase)
	{
		return;
	}

	const float SpawnLifetime = std::max(RandomRange(LifetimeMin, LifetimeMax), 0.0001f);
	Context.ParticleBase->OneOverMaxLifetime = 1.0f / SpawnLifetime;
}

UParticleModule* UParticleModuleLifetime::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleLifetime* Copy = GUObjectArray.CreateObject<UParticleModuleLifetime>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->Lifetime = Lifetime;
	Copy->LifetimeMin = LifetimeMin;
	Copy->LifetimeMax = LifetimeMax;
	return Copy;
}

UParticleModuleLocation::UParticleModuleLocation()
{
	bSpawnModule = true;
}

void UParticleModuleLocation::Spawn(const FSpawnContext& Context)
{
	if (!Context.ParticleBase)
	{
		return;
	}

	Context.ParticleBase->Location = Context.ParticleBase->Location + RandomRange(StartLocationMin, StartLocationMax);
	Context.ParticleBase->OldLocation = Context.ParticleBase->Location;
}

UParticleModule* UParticleModuleLocation::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleLocation* Copy = GUObjectArray.CreateObject<UParticleModuleLocation>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->StartLocation = StartLocation;
	Copy->StartLocationMin = StartLocationMin;
	Copy->StartLocationMax = StartLocationMax;
	return Copy;
}

UParticleModuleVelocity::UParticleModuleVelocity()
{
	bSpawnModule = true;
}

void UParticleModuleVelocity::Spawn(const FSpawnContext& Context)
{
	if (!Context.ParticleBase)
	{
		return;
	}

	const FVector SpawnVelocity = RandomRange(StartVelocityMin, StartVelocityMax);
	Context.ParticleBase->BaseVelocity = SpawnVelocity;
	Context.ParticleBase->Velocity = SpawnVelocity;
}

UParticleModule* UParticleModuleVelocity::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleVelocity* Copy = GUObjectArray.CreateObject<UParticleModuleVelocity>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->StartVelocity = StartVelocity;
	Copy->StartVelocityMin = StartVelocityMin;
	Copy->StartVelocityMax = StartVelocityMax;
	Copy->bInWorldSpace = bInWorldSpace;
	Copy->bApplyOwnerScale = bApplyOwnerScale;
	return Copy;
}

UParticleModuleColor::UParticleModuleColor()
{
	bSpawnModule = true;
}

void UParticleModuleColor::Spawn(const FSpawnContext& Context)
{
	if (!Context.ParticleBase)
	{
		return;
	}

	const float Alpha = std::max(0.0f, std::min(RandomRange(StartAlphaMin, StartAlphaMax), 1.0f));
	Context.ParticleBase->BaseColor = FLinearColor(StartColor.X, StartColor.Y, StartColor.Z, Alpha);
	Context.ParticleBase->Color = Context.ParticleBase->BaseColor;
}

UParticleModule* UParticleModuleColor::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleColor* Copy = GUObjectArray.CreateObject<UParticleModuleColor>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->StartColor = StartColor;
	Copy->StartAlpha = StartAlpha;
	Copy->StartAlphaMin = StartAlphaMin;
	Copy->StartAlphaMax = StartAlphaMax;
	return Copy;
}

UParticleModuleColorOverLife::UParticleModuleColorOverLife()
{
	bSpawnModule = true;
	bUpdateModule = true;
}

void UParticleModuleColorOverLife::Spawn(const FSpawnContext& Context)
{
	if (!Context.ParticleBase)
	{
		return;
	}

	Context.ParticleBase->Color = Context.ParticleBase->BaseColor;
}

void UParticleModuleColorOverLife::Update(const FUpdateContext& Context)
{
	FParticleEmitterInstance& Owner = Context.Owner;
	if (!Owner.ParticleData || !Owner.ParticleIndices)
	{
		return;
	}

	const float ClampedAlphaOverLife = std::clamp(AlphaOverLife, 0.0f, 1.0f);

	for (int32 ParticleIndex = 0; ParticleIndex < Owner.ActiveParticles; ++ParticleIndex)
	{
		FBaseParticle* Particle = reinterpret_cast<FBaseParticle*>(
			Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[ParticleIndex]);
		if (!Particle)
		{
			continue;
		}

		const float T = std::clamp(Particle->RelativeTime, 0.0f, 1.0f);
		const FLinearColor& Base = Particle->BaseColor;
		Particle->Color = FLinearColor(
			Base.R + (ColorOverLife.X - Base.R) * T,
			Base.G + (ColorOverLife.Y - Base.G) * T,
			Base.B + (ColorOverLife.Z - Base.B) * T,
			Base.A + (ClampedAlphaOverLife - Base.A) * T);
	}
}

UParticleModule* UParticleModuleColorOverLife::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleColorOverLife* Copy = GUObjectArray.CreateObject<UParticleModuleColorOverLife>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->ColorOverLife = ColorOverLife;
	Copy->AlphaOverLife = AlphaOverLife;
	return Copy;
}

UParticleModuleSize::UParticleModuleSize()
{
	bSpawnModule = true;
}

void UParticleModuleSize::Spawn(const FSpawnContext& Context)
{
	if (!Context.ParticleBase)
	{
		return;
	}

	const FVector SpawnSize = RandomRange(StartSizeMin, StartSizeMax);
	Context.ParticleBase->BaseSize = SpawnSize;
	Context.ParticleBase->Size = SpawnSize;
}

UParticleModule* UParticleModuleSize::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleSize* Copy = GUObjectArray.CreateObject<UParticleModuleSize>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->StartSize = StartSize;
	Copy->StartSizeMin = StartSizeMin;
	Copy->StartSizeMax = StartSizeMax;
	return Copy;
}

UParticleModule* UParticleModuleTypeDataMesh::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleTypeDataMesh* Copy = GUObjectArray.CreateObject<UParticleModuleTypeDataMesh>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->Mesh = Mesh;
	return Copy;
}
