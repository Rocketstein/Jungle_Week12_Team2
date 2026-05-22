#include "Particle/ParticleEmitterInstances.h"

#include "Component/ParticleSystemComponent.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"

#include <algorithm>
#include <cstring>
#include <malloc.h>
#include <utility>

FParticleDataContainer::~FParticleDataContainer()
{
	Free();
}

FParticleDataContainer::FParticleDataContainer(FParticleDataContainer&& Other) noexcept
{
	*this = std::move(Other);
}

FParticleDataContainer& FParticleDataContainer::operator=(FParticleDataContainer&& Other) noexcept
{
	if (this != &Other)
	{
		Free();

		MemBlockSize = Other.MemBlockSize;
		ParticleDataNumBytes = Other.ParticleDataNumBytes;
		ParticleIndicesNumShorts = Other.ParticleIndicesNumShorts;
		ParticleData = Other.ParticleData;
		ParticleIndices = Other.ParticleIndices;

		Other.MemBlockSize = 0;
		Other.ParticleDataNumBytes = 0;
		Other.ParticleIndicesNumShorts = 0;
		Other.ParticleData = nullptr;
		Other.ParticleIndices = nullptr;
	}
	return *this;
}

void FParticleDataContainer::Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts)
{
	Free();

	ParticleDataNumBytes = AlignParticleDataSize(std::max(0, InParticleDataNumBytes), 16);
	ParticleIndicesNumShorts = std::max(0, InParticleIndicesNumShorts);
	MemBlockSize = ParticleDataNumBytes + ParticleIndicesNumShorts * static_cast<int32>(sizeof(uint16));

	if (MemBlockSize > 0)
	{
		ParticleData = static_cast<uint8*>(_aligned_malloc(MemBlockSize, 16));
		std::memset(ParticleData, 0, MemBlockSize);
		ParticleIndices = reinterpret_cast<uint16*>(ParticleData + ParticleDataNumBytes);
	}
}

void FParticleDataContainer::Free()
{
	if (ParticleData)
	{
		_aligned_free(ParticleData);
	}

	MemBlockSize = 0;
	ParticleDataNumBytes = 0;
	ParticleIndicesNumShorts = 0;
	ParticleData = nullptr;
	ParticleIndices = nullptr;
}

FParticleEmitterInstance::FParticleEmitterInstance(UParticleSystemComponent* InComponent)
	: Component(InComponent)
{
}

FParticleEmitterInstance::~FParticleEmitterInstance()
{
	DataContainer.Free();
	ParticleData = nullptr;
	ParticleIndices = nullptr;
	InstanceData = nullptr;
}

void FParticleEmitterInstance::InitParameters(UParticleEmitter* InTemplate)
{
	SpriteTemplate = InTemplate;
	CurrentLODLevelIndex = 0;
	CurrentLODLevel = SpriteTemplate ? SpriteTemplate->GetLODLevel(CurrentLODLevelIndex) : nullptr;
	PayloadOffset = sizeof(FBaseParticle);
	ParticleSize = AlignParticleDataSize(sizeof(FBaseParticle), 16);
	ParticleStride = ParticleSize;
	ActiveParticles = 0;
	ParticleCounter = 0;
	SpawnFraction = 0.0f;
	SecondsSinceCreation = 0.0f;
	EmitterTime = 0.0f;
	LastDeltaTime = 0.0f;

	const int32 InitialCount = SpriteTemplate ? std::max(SpriteTemplate->InitialAllocationCount, 0) : 0;
	Resize(InitialCount);
}

void FParticleEmitterInstance::Init()
{
	if (SpriteTemplate)
	{
		SpriteTemplate->UpdateModuleLists();
	}
}

bool FParticleEmitterInstance::Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount)
{
	(void)bSetMaxActiveCount;

	NewMaxActiveParticles = std::max(0, NewMaxActiveParticles);
	if (NewMaxActiveParticles == MaxActiveParticles)
	{
		return true;
	}

	FParticleDataContainer OldData = std::move(DataContainer);
	const int32 NewActiveParticles = std::min(ActiveParticles, NewMaxActiveParticles);

	MaxActiveParticles = NewMaxActiveParticles;
	ActiveParticles = NewActiveParticles;

	DataContainer.Alloc(ParticleStride * MaxActiveParticles, MaxActiveParticles);
	ParticleData = DataContainer.ParticleData;
	ParticleIndices = DataContainer.ParticleIndices;

	if (MaxActiveParticles > 0 && (!ParticleData || !ParticleIndices))
	{
		MaxActiveParticles = 0;
		ActiveParticles = 0;
		return false;
	}

	for (int32 Index = 0; Index < NewActiveParticles; ++Index)
	{
		const uint16 OldDirectIndex = OldData.ParticleIndices ? OldData.ParticleIndices[Index] : static_cast<uint16>(Index);
		std::memcpy(ParticleData + ParticleStride * Index,
			OldData.ParticleData + ParticleStride * OldDirectIndex,
			ParticleSize);
		ParticleIndices[Index] = static_cast<uint16>(Index);
	}

	for (int32 Index = NewActiveParticles; Index < MaxActiveParticles; ++Index)
	{
		ParticleIndices[Index] = static_cast<uint16>(Index);
	}

	return true;
}

void FParticleEmitterInstance::Tick(float DeltaTime, bool bSuppressSpawning)
{
	(void)bSuppressSpawning;

	LastDeltaTime = DeltaTime;
	SecondsSinceCreation += DeltaTime;
	EmitterTime += DeltaTime;

	if (!CurrentLODLevel)
	{
		return;
	}

	for (UParticleModule* Module : CurrentLODLevel->UpdateModules)
	{
		if (!Module)
		{
			continue;
		}

		UParticleModule::FUpdateContext Context(*this, 0, DeltaTime);
		Module->Update(Context);
	}

	for (int32 ActiveIndex = ActiveParticles - 1; ActiveIndex >= 0; --ActiveIndex)
	{
		FBaseParticle* Particle = GetParticleDirect(ParticleIndices[ActiveIndex]);
		if (!Particle)
		{
			continue;
		}

		Particle->Velocity = Particle->BaseVelocity;
		Particle->Location = Particle->Location + Particle->Velocity * DeltaTime;
		Particle->RotationRate = Particle->BaseRotationRate;
		Particle->Rotation += Particle->RotationRate * DeltaTime;

		if (Particle->OneOverMaxLifetime > 0.0f)
		{
			Particle->RelativeTime += DeltaTime * Particle->OneOverMaxLifetime;
			if (Particle->RelativeTime >= 1.0f)
			{
				KillParticle(ActiveIndex);
			}
		}
	}
}

void FParticleEmitterInstance::SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation,
	const FVector& InitialVelocity, FParticleEventInstancePayload* EventPayload)
{
	(void)EventPayload;

	if (Count <= 0)
	{
		return;
	}

	if (ActiveParticles + Count > MaxActiveParticles)
	{
		Resize(std::max(ActiveParticles + Count, std::max(1, MaxActiveParticles * 2)));
	}

	float SpawnTime = StartTime;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 DirectIndex = ActiveParticles;
		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * DirectIndex);
		std::memset(&Particle, 0, ParticleSize);

		PreSpawn(Particle, InitialLocation, InitialVelocity);

		if (CurrentLODLevel)
		{
			for (UParticleModule* Module : CurrentLODLevel->SpawnModules)
			{
				if (!Module)
				{
					continue;
				}

				UParticleModule::FSpawnContext Context(*this, 0, SpawnTime, &Particle);
				Module->Spawn(Context);
			}
		}

		PostSpawn(Particle, 0.0f, SpawnTime);

		ParticleIndices[ActiveParticles] = static_cast<uint16>(DirectIndex);
		++ActiveParticles;
		++ParticleCounter;
		SpawnTime += Increment;
	}
}

void FParticleEmitterInstance::KillParticle(int32 Index)
{
	if (Index < 0 || Index >= ActiveParticles)
	{
		return;
	}

	const int32 LastActiveIndex = ActiveParticles - 1;
	if (Index != LastActiveIndex)
	{
		ParticleIndices[Index] = ParticleIndices[LastActiveIndex];
	}

	--ActiveParticles;
}

FBaseParticle* FParticleEmitterInstance::GetParticleDirect(int32 DirectIndex) const
{
	if (!ParticleData || DirectIndex < 0 || DirectIndex >= MaxActiveParticles)
	{
		return nullptr;
	}
	return reinterpret_cast<FBaseParticle*>(ParticleData + ParticleStride * DirectIndex);
}

void FParticleEmitterInstance::PreSpawn(FBaseParticle& Particle, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	Particle.OldLocation = InitialLocation;
	Particle.Location = InitialLocation;
	Particle.BaseVelocity = InitialVelocity;
	Particle.Velocity = InitialVelocity;
	Particle.BaseSize = FVector::OneVector;
	Particle.Size = FVector::OneVector;
	Particle.Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	Particle.BaseColor = Particle.Color;
	Particle.RelativeTime = 0.0f;
	Particle.OneOverMaxLifetime = 1.0f;
	Particle.Flags = STATE_Particle_JustSpawned;
}

void FParticleEmitterInstance::PostSpawn(FBaseParticle& Particle, float Interp, float SpawnTime)
{
	(void)Interp;
	(void)SpawnTime;
	Particle.Flags &= ~STATE_Particle_JustSpawned;
}
