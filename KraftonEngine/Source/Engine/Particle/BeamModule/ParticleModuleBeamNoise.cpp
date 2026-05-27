#include "Particle/BeamModule/ParticleModuleBeamNoise.h"

#include "Particle/ParticleBeamInstances.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleHelper.h"
#include "Particle/TypeData/ParticleModuleTypeDataBeam2.h"

#include <algorithm>
#include <limits>
#include <random>

namespace
{
float RandomUnitSigned()
{
	static thread_local std::mt19937 Generator{ std::random_device{}() };
	std::uniform_real_distribution<float> Distribution(-1.0f, 1.0f);
	return Distribution(Generator);
}

int32 ParticleSlot(const FParticleBeam2EmitterInstance& Beam, const FBaseParticle* Particle)
{
	if (!Particle || !Beam.ParticleData || Beam.ParticleStride <= 0)
	{
		return -1;
	}
	const ptrdiff_t Byte = reinterpret_cast<const uint8*>(Particle) - Beam.ParticleData;
	const int32 Slot = static_cast<int32>(Byte / Beam.ParticleStride);
	return (Slot < 0 || Slot >= Beam.MaxActiveParticles) ? -1 : Slot;
}

bool EnsureArenaSize(FParticleBeam2EmitterInstance& Beam, int32 Frequency)
{
	if (Frequency <= 0 || Beam.MaxActiveParticles <= 0)
	{
		return false;
	}
	const int32 Required = Beam.MaxActiveParticles * Frequency;
	if (static_cast<int32>(Beam.NoisePointArena.size()) < Required)
	{
		Beam.NoisePointArena.resize(Required);
	}
	if (static_cast<int32>(Beam.NoiseOffsetArena.size()) < Required)
	{
		Beam.NoiseOffsetArena.resize(Required);
	}
	if (static_cast<int32>(Beam.NoiseTimeArena.size()) < Required)
	{
		Beam.NoiseTimeArena.resize(Required);
	}
	return true;
}

float NoisePointAlpha(int32 PointIndex, int32 NumPoints, float BeamLength, float FrequencyDistance)
{
	if (FrequencyDistance > 1e-6f && BeamLength > 1e-6f)
	{
		return std::clamp(
			(FrequencyDistance * static_cast<float>(PointIndex + 1)) / BeamLength,
			0.0f,
			1.0f);
	}
	return static_cast<float>(PointIndex + 1) / static_cast<float>(NumPoints + 1);
}

float NextRollTime(float CurrentTime, float LockTime, float NoiseSpeed)
{
	if (LockTime > 0.0f)
	{
		return CurrentTime + LockTime;
	}
	return NoiseSpeed > 1e-6f
		? CurrentTime + (1.0f / NoiseSpeed)
		: std::numeric_limits<float>::max();
}
}

UParticleModule* UParticleModuleBeamNoise::CloneForLOD(UParticleLODLevel* NewOuter) const
{
	UParticleModuleBeamNoise* Copy = GUObjectArray.CreateObject<UParticleModuleBeamNoise>(NewOuter);
	CopyModuleBaseTo(Copy);
	Copy->Frequency         = Frequency;
	Copy->FrequencyDistance = FrequencyDistance;
	Copy->NoiseRange        = NoiseRange;
	Copy->NoiseSpeed        = NoiseSpeed;
	Copy->NoiseLockTime     = NoiseLockTime;
	Copy->bTargetNoise      = bTargetNoise;
	return Copy;
}

void UParticleModuleBeamNoise::BuildNoiseOffsets(FVector* OutOffsets, float* OutTimes,
	int32 NumPoints, float CurrentTime) const
{
	if (!OutOffsets || NumPoints <= 0)
	{
		return;
	}

	const float NextTime = NextRollTime(CurrentTime, NoiseLockTime, NoiseSpeed);
	for (int32 i = 0; i < NumPoints; ++i)
	{
		OutOffsets[i] = FVector(
			RandomUnitSigned() * NoiseRange.X,
			RandomUnitSigned() * NoiseRange.Y,
			RandomUnitSigned() * NoiseRange.Z);
		if (OutTimes)
		{
			OutTimes[i] = NextTime;
		}
	}
}

void UParticleModuleBeamNoise::ApplyNoiseOffsets(FVector* OutPoints, const FVector* Offsets,
	int32 NumPoints, const FBeam2TypeDataPayload& BeamPayload) const
{
	if (!OutPoints || !Offsets || NumPoints <= 0)
	{
		return;
	}

	// Hermite-form Bezier: matches the curve evaluation in
	// FParticleSystemSceneProxy::EvaluateBeamCurve so noise + tangent curve
	// compose cleanly. The renderer ignores tangents when NoisePoints is
	// non-empty, so we have to bake them into the centerline here.
	const FVector SourceLocal = BeamPayload.SourcePoint;
	const FVector TargetLocal = BeamPayload.TargetPoint;
	const FVector SourceControl = SourceLocal + BeamPayload.SourceTangent * (std::max(0.0f, BeamPayload.SourceStrength) / 3.0f);
	const FVector TargetControl = TargetLocal - BeamPayload.TargetTangent * (std::max(0.0f, BeamPayload.TargetStrength) / 3.0f);
	const float BeamLength = (TargetLocal - SourceLocal).Length();

	for (int32 i = 0; i < NumPoints; ++i)
	{
		const float T = NoisePointAlpha(i, NumPoints, BeamLength, FrequencyDistance);
		const float InvT = 1.0f - T;
		const FVector Center = SourceLocal * (InvT * InvT * InvT)
			+ SourceControl * (3.0f * InvT * InvT * T)
			+ TargetControl * (3.0f * InvT * T * T)
			+ TargetLocal * (T * T * T);
		const float TargetScale = bTargetNoise ? 1.0f : (1.0f - T);
		OutPoints[i] = Center + Offsets[i] * TargetScale;
	}
}

void UParticleModuleBeamNoise::Spawn(const FSpawnContext& Context)
{
	if (!bEnabled || !Context.ParticleBase || Frequency <= 0)
	{
		return;
	}

	FParticleBeam2EmitterInstance& Beam =
		static_cast<FParticleBeam2EmitterInstance&>(Context.Owner);
	if (!EnsureArenaSize(Beam, Frequency))
	{
		return;
	}

	const int32 NoiseOffset = static_cast<int32>(Beam.GetModuleDataOffset(this));
	if (NoiseOffset <= 0)
	{
		return;
	}

	const int32 Slot = ParticleSlot(Beam, Context.ParticleBase);
	if (Slot < 0)
	{
		return;
	}

	FBeamNoisePayloadData* Payload = reinterpret_cast<FBeamNoisePayloadData*>(
		reinterpret_cast<uint8*>(Context.ParticleBase) + NoiseOffset);

	Payload->NoisePoints   = &Beam.NoisePointArena[Slot * Frequency];
	Payload->NoiseTimes    = &Beam.NoiseTimeArena[Slot * Frequency];
	Payload->NoiseIndex    = 0;
	const float CurrentTime = Beam.BeamTravelTime + Context.SpawnTime;
	Payload->NextNoiseTime = NextRollTime(CurrentTime, NoiseLockTime, NoiseSpeed);

	// Endpoints/tangents already written by TypeData/Source/Target Spawn (we run last).
	FBeam2TypeDataPayload* BeamPayload = reinterpret_cast<FBeam2TypeDataPayload*>(
		reinterpret_cast<uint8*>(Context.ParticleBase) + Context.Offset);
	FVector* Offsets = &Beam.NoiseOffsetArena[Slot * Frequency];
	BuildNoiseOffsets(Offsets, Payload->NoiseTimes, Frequency, CurrentTime);
	ApplyNoiseOffsets(Payload->NoisePoints, Offsets, Frequency, *BeamPayload);
}

void UParticleModuleBeamNoise::Update(const FUpdateContext& UpdateContext)
{
	if (!bEnabled || Frequency <= 0)
	{
		return;
	}

	FParticleBeam2EmitterInstance& Beam =
		static_cast<FParticleBeam2EmitterInstance&>(UpdateContext.Owner);
	if (!EnsureArenaSize(Beam, Frequency) || !Beam.ParticleIndices)
	{
		return;
	}

	const int32 NoiseOffset = static_cast<int32>(Beam.GetModuleDataOffset(this));
	if (NoiseOffset <= 0)
	{
		return;
	}

	for (int32 Index = 0; Index < Beam.ActiveParticles; ++Index)
	{
		FBaseParticle* Particle = Beam.GetParticleDirect(Beam.ParticleIndices[Index]);
		const int32 Slot = ParticleSlot(Beam, Particle);
		if (Slot < 0)
		{
			continue;
		}

		FBeamNoisePayloadData* Payload = reinterpret_cast<FBeamNoisePayloadData*>(
			reinterpret_cast<uint8*>(Particle) + NoiseOffset);

		// Re-derive pointer in case Resize relocated the data buffer this frame.
		FVector* NoisePoints = &Beam.NoisePointArena[Slot * Frequency];
		float* NoiseTimes = &Beam.NoiseTimeArena[Slot * Frequency];
		const bool bArenaChanged = Payload->NoisePoints != NoisePoints || Payload->NoiseTimes != NoiseTimes;
		Payload->NoisePoints = NoisePoints;
		Payload->NoiseTimes = NoiseTimes;
		FVector* Offsets = &Beam.NoiseOffsetArena[Slot * Frequency];

		const float CurrentTime = Beam.BeamTravelTime;
		if (bArenaChanged || CurrentTime >= Payload->NextNoiseTime)
		{
			++Payload->NoiseIndex;
			Payload->NextNoiseTime = NextRollTime(CurrentTime, NoiseLockTime, NoiseSpeed);
			BuildNoiseOffsets(Offsets, Payload->NoiseTimes, Frequency, CurrentTime);
		}

		FBeam2TypeDataPayload* BeamPayload = reinterpret_cast<FBeam2TypeDataPayload*>(
			reinterpret_cast<uint8*>(Particle) + UpdateContext.Offset);
		ApplyNoiseOffsets(Payload->NoisePoints, Offsets, Frequency, *BeamPayload);
	}
}
