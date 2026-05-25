#include "Particle/ParticleEmitterInstances.h"

#include "Component/ParticleSystemComponent.h"
#include "Materials/Material.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"

#include <algorithm>
#include <cstring>
#include <cmath>
#include <malloc.h>
#include <utility>

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
	SetCurrentLODLevel(0);
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

void FParticleEmitterInstance::SetCurrentLODLevel(int32 LODLevel)
{
	CurrentLODLevelIndex = std::max(0, LODLevel);
	CurrentLODLevel = SpriteTemplate ? SpriteTemplate->GetBestLODLevel(CurrentLODLevelIndex) : nullptr;
}

void FParticleEmitterInstance::Tick(float DeltaTime, int32 LODLevel, bool bSuppressSpawning)
{
	LastDeltaTime = DeltaTime;
	SecondsSinceCreation += DeltaTime;
	EmitterTime += DeltaTime;
	OldLocation = Location;
	Location = Component ? Component->GetWorldLocation() : FVector::ZeroVector;

	SetCurrentLODLevel(LODLevel);
	if (!CurrentLODLevel)
	{
		return;
	}

	SpawnFraction = Tick_SpawnParticles(DeltaTime, CurrentLODLevel, bSuppressSpawning, false);

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

		const bool bJustSpawned = (Particle->Flags & STATE_Particle_JustSpawned) != 0;
		Particle->Flags &= ~STATE_Particle_JustSpawned;

		Particle->Velocity = Particle->BaseVelocity;
		Particle->RotationRate = Particle->BaseRotationRate;
		if (!bJustSpawned)
		{
			Particle->Location = Particle->Location + Particle->Velocity * DeltaTime;
			Particle->Rotation += Particle->RotationRate * DeltaTime;
		}

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

float FParticleEmitterInstance::Tick_SpawnParticles(float DeltaTime, UParticleLODLevel* InCurrentLODLevel,
	bool bSuppressSpawning, bool bFirstTime)
{
	(void)InCurrentLODLevel;
	(void)bFirstTime;

	if (bSuppressSpawning)
	{
		return SpawnFraction;
	}

	return Spawn(DeltaTime);
}

float FParticleEmitterInstance::Spawn(float DeltaTime)
{
	if (!CurrentLODLevel || !CurrentLODLevel->SpawnModule)
	{
		return SpawnFraction;
	}

	const float SpawnRate = std::max(0.0f, CurrentLODLevel->SpawnModule->Rate);
	if (SpawnRate <= 0.0f)
	{
		return SpawnFraction;
	}

	const float OldLeftover = SpawnFraction;
	float NewLeftover = OldLeftover + std::max(0.0f, DeltaTime) * SpawnRate;
	const int32 Number = static_cast<int32>(std::floor(NewLeftover));
	const float Increment = SpawnRate > 0.0f ? 1.0f / SpawnRate : 0.0f;
	const float StartTime = DeltaTime + OldLeftover * Increment - Increment;
	NewLeftover = NewLeftover - static_cast<float>(Number);

	if (Number > 0)
	{
		SpawnParticles(Number, StartTime, Increment, Location, FVector::ZeroVector, nullptr);
	}

	return NewLeftover;
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
	if (!ParticleData || !ParticleIndices)
	{
		return;
	}

	float SpawnTime = StartTime;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 DirectIndex = ParticleIndices ? ParticleIndices[ActiveParticles] : ActiveParticles;
		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * DirectIndex);
		std::memset(&Particle, 0, ParticleSize);

		PreSpawn(&Particle, InitialLocation, InitialVelocity);

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

		PostSpawn(&Particle, 0.0f, SpawnTime);

		++ActiveParticles;
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
	const uint16 RemovedDirectIndex = ParticleIndices[Index];
	if (Index != LastActiveIndex)
	{
		ParticleIndices[Index] = ParticleIndices[LastActiveIndex];
	}
	ParticleIndices[LastActiveIndex] = RemovedDirectIndex;

	--ActiveParticles;
}

FDynamicEmitterReplayDataBase* FParticleEmitterInstance::GetReplayData()
{
	if (ActiveParticles <= 0)
	{
		return nullptr;
	}

	FDynamicEmitterReplayDataBase* NewEmitterReplayData = nullptr;
	if (CurrentLODLevel && CurrentLODLevel->TypeDataModule && CurrentLODLevel->TypeDataModule->IsAMeshEmitter())
	{
		NewEmitterReplayData = new FDynamicMeshEmitterReplayData();
	}
	else
	{
		NewEmitterReplayData = new FDynamicSpriteEmitterReplayData();
	}

	if (!FillReplayData(*NewEmitterReplayData))
	{
		delete NewEmitterReplayData;
		return nullptr;
	}

	return NewEmitterReplayData;
}

bool FParticleEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData)
{
	if (ActiveParticles <= 0 || !ParticleData || !ParticleIndices || ParticleStride <= 0)
	{
		return false;
	}

	OutData.ActiveParticleCount = ActiveParticles;
	OutData.ParticleStride = ParticleStride;
	OutData.Scale = FVector::OneVector;
	OutData.SortMode = EParticleSortMode::PSORTMODE_None;

	if (CurrentLODLevel && CurrentLODLevel->RequiredModule)
	{
		OutData.SortMode = CurrentLODLevel->RequiredModule->SortMode;
		if (FDynamicRenderableEmitterReplayDataBase* RenderableData = dynamic_cast<FDynamicRenderableEmitterReplayDataBase*>(&OutData))
		{
			RenderableData->MaterialInterface = CurrentLODLevel->RequiredModule->Material;
			if (UMaterial* Material = CurrentLODLevel->RequiredModule->Material
				? CurrentLODLevel->RequiredModule->Material->GetMaterial()
				: nullptr)
			{
				RenderableData->BlendMode = Material->GetBlendState();
			}
		}
		if (FDynamicSpriteEmitterReplayData* SpriteData = dynamic_cast<FDynamicSpriteEmitterReplayData*>(&OutData))
		{
			SpriteData->ScreenAlignment = static_cast<uint8>(CurrentLODLevel->RequiredModule->ScreenAlignment);
			SpriteData->EmitterOrigin = Location + CurrentLODLevel->RequiredModule->EmitterOrigin;
			SpriteData->SubImages_Horizontal = std::max(1, CurrentLODLevel->RequiredModule->SubImages_Horizontal);
			SpriteData->SubImages_Vertical = std::max(1, CurrentLODLevel->RequiredModule->SubImages_Vertical);
			SpriteData->AlphaSource = static_cast<uint32>(std::clamp(CurrentLODLevel->RequiredModule->AlphaSource, 0, 1));
			SpriteData->AlphaThreshold = std::clamp(CurrentLODLevel->RequiredModule->AlphaThreshold, 0.0f, 1.0f);
			SpriteData->AlphaPower = std::max(0.001f, CurrentLODLevel->RequiredModule->AlphaPower);
			SpriteData->ColorIntensity = std::max(0.0f, CurrentLODLevel->RequiredModule->ColorIntensity);
		}
	}

	if (CurrentLODLevel && CurrentLODLevel->TypeDataModule && CurrentLODLevel->TypeDataModule->IsAMeshEmitter())
	{
		OutData.eEmitterType = DET_Mesh;
		if (FDynamicMeshEmitterReplayData* MeshData = dynamic_cast<FDynamicMeshEmitterReplayData*>(&OutData))
		{
			if (UParticleModuleTypeDataMesh* MeshTypeData = Cast<UParticleModuleTypeDataMesh>(CurrentLODLevel->TypeDataModule))
			{
				MeshData->StaticMesh = MeshTypeData->Mesh;
			}
		}
	}
	else
	{
		OutData.eEmitterType = DET_Sprite;
	}

	OutData.DataContainer.Alloc(ParticleStride * ActiveParticles, ActiveParticles);
	if (!OutData.DataContainer.ParticleData || !OutData.DataContainer.ParticleIndices)
	{
		return false;
	}

	for (int32 Index = 0; Index < ActiveParticles; ++Index)
	{
		const uint16 DirectIndex = ParticleIndices[Index];
		std::memcpy(OutData.DataContainer.ParticleData + ParticleStride * Index,
			ParticleData + ParticleStride * DirectIndex,
			ParticleSize);
		OutData.DataContainer.ParticleIndices[Index] = static_cast<uint16>(Index);
	}

	return true;
}

FBaseParticle* FParticleEmitterInstance::GetParticleDirect(int32 DirectIndex) const
{
	if (!ParticleData || DirectIndex < 0 || DirectIndex >= MaxActiveParticles)
	{
		return nullptr;
	}
	return reinterpret_cast<FBaseParticle*>(ParticleData + ParticleStride * DirectIndex);
}

void FParticleEmitterInstance::PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	if (!Particle)
	{
		return;
	}

	Particle->OldLocation = InitialLocation;
	Particle->Location = InitialLocation;
	Particle->BaseVelocity = InitialVelocity;
	Particle->Velocity = InitialVelocity;
	Particle->BaseSize = FVector::OneVector;
	Particle->Size = FVector::OneVector;
	Particle->Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	Particle->BaseColor = Particle->Color;
	Particle->RelativeTime = 0.0f;
	Particle->OneOverMaxLifetime = 1.0f;
	Particle->Flags = 0;
}

void FParticleEmitterInstance::PostSpawn(FBaseParticle* Particle, float Interp, float SpawnTime)
{
	(void)Interp;
	(void)SpawnTime;
	if (!Particle)
	{
		return;
	}

	Particle->Flags |= ((ParticleCounter++) & STATE_CounterMask);
	Particle->Flags |= STATE_Particle_JustSpawned;
}
