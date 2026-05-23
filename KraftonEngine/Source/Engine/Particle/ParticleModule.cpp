#include "Particle/ParticleModule.h"

#include "Particle/ParticleEmitterInstances.h"

#include <algorithm>

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

	const float SpawnLifetime = std::max(Lifetime, 0.0001f);
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

	Context.ParticleBase->Location = Context.ParticleBase->Location + StartLocation;
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

	Context.ParticleBase->BaseVelocity = StartVelocity;
	Context.ParticleBase->Velocity = StartVelocity;
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

	const float Alpha = std::max(0.0f, std::min(StartAlpha, 1.0f));
	Context.ParticleBase->BaseColor = FLinearColor(StartColor.X, StartColor.Y, StartColor.Z, Alpha);
	Context.ParticleBase->Color = Context.ParticleBase->BaseColor;
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

	Context.ParticleBase->BaseSize = StartSize;
	Context.ParticleBase->Size = StartSize;
}
