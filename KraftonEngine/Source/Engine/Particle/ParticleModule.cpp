#include "Particle/ParticleModule.h"

#include "Particle/ParticleEmitterInstances.h"

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

bool UParticleModuleSpawn::GetSpawnAmount(const FContext& Context, int32 Offset, float OldLeftover, float DeltaTime,
	int32& Number, float& OutRate)
{
	(void)Context;
	(void)Offset;
	(void)OldLeftover;
	(void)DeltaTime;
	Number = 0;
	OutRate = 0.0f;
	return false;
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

UParticleModuleColor::UParticleModuleColor()
{
	bSpawnModule = true;
	bUpdateModule = true;
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

void UParticleModuleColor::Update(const FUpdateContext& Context)
{
	FParticleEmitterInstance& Owner = Context.Owner;
	if (!Owner.ParticleData || !Owner.ParticleIndices)
	{
		return;
	}

	const float ClampedEndAlpha = std::clamp(EndAlpha, 0.0f, 1.0f);

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
			Base.R + (EndColor.X - Base.R) * T,
			Base.G + (EndColor.Y - Base.G) * T,
			Base.B + (EndColor.Z - Base.B) * T,
			Base.A + (ClampedEndAlpha - Base.A) * T);
	}
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
