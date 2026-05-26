#include "Particle/ParticleModule.h"

#include "Component/ParticleSystemComponent.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Particle/ParticleEmitterInstances.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/TypeData/ParticleModuleTypeDataRibbon.h"

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

UParticleModuleInitialRotation::UParticleModuleInitialRotation()
{
	bSpawnModule = true;
}

void UParticleModuleInitialRotation::Spawn(const FSpawnContext& Context)
{
	if (!Context.ParticleBase)
	{
		return;
	}

	const FVector SpawnRotationDegrees = RandomRange(StartRotationDegreesMin, StartRotationDegreesMax);
	Context.ParticleBase->Rotation = SpawnRotationDegrees * FMath::DegToRad;
}

UParticleModule* UParticleModuleInitialRotation::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleInitialRotation* Copy = GUObjectArray.CreateObject<UParticleModuleInitialRotation>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->StartRotationDegrees = StartRotationDegrees;
	Copy->StartRotationDegreesMin = StartRotationDegreesMin;
	Copy->StartRotationDegreesMax = StartRotationDegreesMax;
	return Copy;
}

UParticleModuleInitialRotationRate::UParticleModuleInitialRotationRate()
{
	bSpawnModule = true;
}

void UParticleModuleInitialRotationRate::Spawn(const FSpawnContext& Context)
{
	if (!Context.ParticleBase)
	{
		return;
	}

	const FVector SpawnRotationRateDegrees = RandomRange(StartRotationRateDegreesMin, StartRotationRateDegreesMax);
	const FVector SpawnRotationRate = SpawnRotationRateDegrees * FMath::DegToRad;
	Context.ParticleBase->BaseRotationRate = SpawnRotationRate;
	Context.ParticleBase->RotationRate = SpawnRotationRate;
}

UParticleModule* UParticleModuleInitialRotationRate::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleInitialRotationRate* Copy = GUObjectArray.CreateObject<UParticleModuleInitialRotationRate>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->StartRotationRateDegrees = StartRotationRateDegrees;
	Copy->StartRotationRateDegreesMin = StartRotationRateDegreesMin;
	Copy->StartRotationRateDegreesMax = StartRotationRateDegreesMax;
	return Copy;
}

UParticleModuleAcceleration::UParticleModuleAcceleration()
{
	bUpdateModule = true;
}

void UParticleModuleAcceleration::Update(const FUpdateContext& Context)
{
	FParticleEmitterInstance& Owner = Context.Owner;
	if (!Owner.ParticleData || !Owner.ParticleIndices)
	{
		return;
	}

	const FVector VelocityDelta = Acceleration * Context.DeltaTime;
	for (int32 ParticleIndex = 0; ParticleIndex < Owner.ActiveParticles; ++ParticleIndex)
	{
		FBaseParticle* Particle = reinterpret_cast<FBaseParticle*>(
			Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[ParticleIndex]);
		if (!Particle)
		{
			continue;
		}

		Particle->BaseVelocity = Particle->BaseVelocity + VelocityDelta;
		Particle->Velocity = Particle->BaseVelocity;
	}
}

UParticleModule* UParticleModuleAcceleration::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleAcceleration* Copy = GUObjectArray.CreateObject<UParticleModuleAcceleration>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->Acceleration = Acceleration;
	return Copy;
}

UParticleModuleCollision::UParticleModuleCollision()
{
	bFinalUpdateModule = true;
}

uint32 UParticleModuleCollision::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	(void)TypeData;
	return sizeof(FParticleCollisionPayload);
}

void UParticleModuleCollision::FinalUpdate(const FUpdateContext& Context)
{
	FParticleEmitterInstance& Owner = Context.Owner;
	if (!Owner.Component || !Owner.ParticleData || !Owner.ParticleIndices || MaxCollisions <= 0)
	{
		return;
	}

	UWorld* World = Owner.Component->GetWorld();
	if (!World)
	{
		return;
	}

	const float ClampedDamping = std::clamp(DampingFactor, 0.0f, 1.0f);
	const float ClampedOffset = std::max(CollisionOffset, 0.0f);

	for (int32 ParticleIndex = Owner.ActiveParticles - 1; ParticleIndex >= 0; --ParticleIndex)
	{
		FBaseParticle* Particle = Owner.GetParticleDirect(Owner.ParticleIndices[ParticleIndex]);
		if (!Particle || (Particle->Flags & STATE_Particle_CollisionIgnoreCheck) != 0)
		{
			continue;
		}

		FParticleCollisionPayload* Payload = reinterpret_cast<FParticleCollisionPayload*>(
			reinterpret_cast<uint8*>(Particle) + Context.Offset);
		if (!Payload || Payload->CollisionCount >= MaxCollisions)
		{
			continue;
		}

		const FVector Segment = Particle->Location - Particle->OldLocation;
		const float SegmentLength = Segment.Length();
		if (SegmentLength <= 0.0001f)
		{
			continue;
		}

		FVector Direction = Segment / SegmentLength;
		FHitResult Hit;
		if (!World->PhysicsRaycast(Particle->OldLocation, Direction, SegmentLength, Hit, TraceChannel, Owner.Component->GetOwner()))
		{
			continue;
		}

		FVector Normal = Hit.ImpactNormal.IsNearlyZero() ? Hit.WorldNormal : Hit.ImpactNormal;
		if (Normal.IsNearlyZero())
		{
			Normal = Direction * -1.0f;
		}
		Normal.Normalize();

		FParticleEventCollideData EventData;
		EventData.EmitterIndex = Owner.EmitterIndex;
		EventData.ParticleIndex = ParticleIndex;
		EventData.Location = Hit.WorldHitLocation;
		EventData.OldLocation = Particle->OldLocation;
		EventData.Velocity = Particle->BaseVelocity;
		EventData.Normal = Normal;
		EventData.HitActor = Hit.HitActor;
		EventData.HitComponent = Hit.HitComponent;
		Owner.Component->QueueParticleCollisionEvent(EventData);

		++Payload->CollisionCount;
		Particle->Flags |= STATE_Particle_CollisionHasOccurred;
		Particle->Location = Hit.WorldHitLocation + Normal * ClampedOffset;

		if (ResponseMode == EParticleCollisionResponseMode::Kill)
		{
			Owner.KillParticle(ParticleIndex);
			continue;
		}

		if (ResponseMode == EParticleCollisionResponseMode::Stop)
		{
			Particle->BaseVelocity = FVector::ZeroVector;
			Particle->Velocity = FVector::ZeroVector;
			Particle->Flags |= STATE_Particle_FreezeTranslation;
		}
		else
		{
			const FVector IncomingVelocity = Particle->BaseVelocity;
			const float NormalVelocity = IncomingVelocity.Dot(Normal);
			FVector ReflectedVelocity = IncomingVelocity;
			if (NormalVelocity < 0.0f)
			{
				ReflectedVelocity = IncomingVelocity - Normal * (2.0f * NormalVelocity);
			}
			Particle->BaseVelocity = ReflectedVelocity * ClampedDamping;
			Particle->Velocity = Particle->BaseVelocity;
		}

		if (Payload->CollisionCount >= MaxCollisions)
		{
			Particle->Flags |= STATE_Particle_IgnoreCollisions;
		}
	}
}

UParticleModule* UParticleModuleCollision::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleCollision* Copy = GUObjectArray.CreateObject<UParticleModuleCollision>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->TraceChannel = TraceChannel;
	Copy->ResponseMode = ResponseMode;
	Copy->DampingFactor = DampingFactor;
	Copy->CollisionOffset = CollisionOffset;
	Copy->MaxCollisions = MaxCollisions;
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
	const FVector SpawnColor = RandomRange(StartColorMin, StartColorMax);
	Context.ParticleBase->BaseColor = FLinearColor(SpawnColor.X, SpawnColor.Y, SpawnColor.Z, Alpha);
	Context.ParticleBase->Color = Context.ParticleBase->BaseColor;
}

UParticleModule* UParticleModuleColor::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleColor* Copy = GUObjectArray.CreateObject<UParticleModuleColor>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->StartColor = StartColor;
	Copy->StartColorMin = StartColorMin;
	Copy->StartColorMax = StartColorMax;
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

UParticleModuleColorScaleOverLife::UParticleModuleColorScaleOverLife()
{
	bUpdateModule = true;
}

void UParticleModuleColorScaleOverLife::Update(const FUpdateContext& Context)
{
	FParticleEmitterInstance& Owner = Context.Owner;
	if (!Owner.ParticleData || !Owner.ParticleIndices)
	{
		return;
	}

	const FVector ClampedColorScale(
		(std::max)(0.0f, ColorScaleOverLife.X),
		(std::max)(0.0f, ColorScaleOverLife.Y),
		(std::max)(0.0f, ColorScaleOverLife.Z));
	const float ClampedAlphaScale = (std::max)(0.0f, AlphaScaleOverLife);

	for (int32 ParticleIndex = 0; ParticleIndex < Owner.ActiveParticles; ++ParticleIndex)
	{
		FBaseParticle* Particle = reinterpret_cast<FBaseParticle*>(
			Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[ParticleIndex]);
		if (!Particle)
		{
			continue;
		}

		const float T = std::clamp(Particle->RelativeTime, 0.0f, 1.0f);
		const FVector Scale(
			1.0f + (ClampedColorScale.X - 1.0f) * T,
			1.0f + (ClampedColorScale.Y - 1.0f) * T,
			1.0f + (ClampedColorScale.Z - 1.0f) * T);
		const float AlphaScale = 1.0f + (ClampedAlphaScale - 1.0f) * T;

		Particle->Color = FLinearColor(
			Particle->Color.R * Scale.X,
			Particle->Color.G * Scale.Y,
			Particle->Color.B * Scale.Z,
			Particle->Color.A * AlphaScale);
	}
}

UParticleModule* UParticleModuleColorScaleOverLife::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleColorScaleOverLife* Copy = GUObjectArray.CreateObject<UParticleModuleColorScaleOverLife>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->ColorScaleOverLife = ColorScaleOverLife;
	Copy->AlphaScaleOverLife = AlphaScaleOverLife;
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

UParticleModule* UParticleModuleBeamSource::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleBeamSource* Copy = GUObjectArray.CreateObject<UParticleModuleBeamSource>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->SourcePoint = SourcePoint;
	Copy->SourceTangentMethod = SourceTangentMethod;
	Copy->SourceTangent = SourceTangent;
	return Copy;
}

UParticleModule* UParticleModuleBeamTarget::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleBeamTarget* Copy = GUObjectArray.CreateObject<UParticleModuleBeamTarget>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->TargetPoint = TargetPoint;
	Copy->TargetTangentMethod = TargetTangentMethod;
	Copy->TargetTangent = TargetTangent;
	return Copy;
}

UParticleModule* UParticleModuleBeamNoise::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleBeamNoise* Copy = GUObjectArray.CreateObject<UParticleModuleBeamNoise>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->NoiseAmplitude = NoiseAmplitude;
	Copy->NoiseFrequency = NoiseFrequency;
	Copy->NoiseSpeed = NoiseSpeed;
	Copy->NoiseSeed = NoiseSeed;
	Copy->bLowFreqEnabled = bLowFreqEnabled;
	Copy->NoiseRangeMin = NoiseRangeMin;
	Copy->NoiseRangeMax = NoiseRangeMax;
	return Copy;
}

UParticleModule* UParticleModuleTypeDataMesh::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleTypeDataMesh* Copy = GUObjectArray.CreateObject<UParticleModuleTypeDataMesh>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->MeshPath = MeshPath;
	Copy->Mesh = Mesh;
	return Copy;
}

uint32 UParticleModuleTypeDataRibbon::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	(void)TypeData;
	return sizeof(FRibbonParticlePayload);
}

UParticleModule* UParticleModuleTypeDataRibbon::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleTypeDataRibbon* Copy = GUObjectArray.CreateObject<UParticleModuleTypeDataRibbon>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->MaxTessellationBetweenParticles = MaxTessellationBetweenParticles;
	Copy->SheetsPerTrail = SheetsPerTrail;
	Copy->MaxTrailCount = MaxTrailCount;
	Copy->MaxParticleInTrailCount = MaxParticleInTrailCount;
	Copy->bDeadTrailsOnDeactivate = bDeadTrailsOnDeactivate;
	Copy->bDeadTrailsOnSourceLoss = bDeadTrailsOnSourceLoss;
	Copy->bClipSourceSegment = bClipSourceSegment;
	Copy->bEnablePreviousTangentRecalculation = bEnablePreviousTangentRecalculation;
	Copy->bTangentRecalculationEveryFrame = bTangentRecalculationEveryFrame;
	Copy->bSpawnInitialParticle = bSpawnInitialParticle;
	Copy->RenderAxis = RenderAxis;
	Copy->TangentSpawningScalar = TangentSpawningScalar;
	Copy->bRenderGeometry = bRenderGeometry;
	Copy->bRenderSpawnPoints = bRenderSpawnPoints;
	Copy->bRenderTangents = bRenderTangents;
	Copy->bRenderTessellation = bRenderTessellation;
	Copy->TilingDistance = TilingDistance;
	Copy->DistanceTessellationStepSize = DistanceTessellationStepSize;
	Copy->bEnableTangentDiffInterpScale = bEnableTangentDiffInterpScale;
	Copy->TangentTessellationScalar = TangentTessellationScalar;
	Copy->Width = Width;
	Copy->Color = Color;
	Copy->Alpha = Alpha;
	return Copy;
}
