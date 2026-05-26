#pragma once
#include "ParticleModuleBeamBase.h"
#include "ParticleModuleBeamNoise.generated.h"

struct FBeamNoisePayloadData {
	FVector* NoisePoints;
	float* NoiseTimes;
	int32 NoiseIndex;
	float NextNoiseTime;
};

UCLASS()
class UParticleModuleBeamNoise : public UParticleModuleBeamBase
{
public:
	GENERATED_BODY(UParticleModuleBeamNoise)

	UPROPERTY(Edit, Category = "LowFreq")
	bool bLowFreq_Enabled = false;

	UPROPERTY(Edit, Category = "LowFreq", Min = 0, Max = 64)
	int32 Frequency = 0;

	UPROPERTY(Edit, Category = "LowFreq")
	float FrequencyDistance = 0.0f;

	UPROPERTY(Edit, Category = "LowFreq")
	FVector NoiseRange = FVector::ZeroVector;

	UPROPERTY(Edit, Category = "LowFreq", Min = 0.0f)
	float NoiseLockTime = 0.0f;

	UPROPERTY(Edit, Category = "LowFreq")
	bool bTargetNoise = false;

	// Fill OutPoints[0..NumPoints) with perturbed midpoints between Source and
	// Target (in payload/local space). Caller owns the buffer.
	void BuildNoisePoints(FVector* OutPoints, int32 NumPoints,
		const FVector& SourceLocal, const FVector& TargetLocal) const;
	void BuildNoiseOffsets(FVector* OutOffsets, float* OutTimes,
		int32 NumPoints, float CurrentTime) const;
	void ApplyNoiseOffsets(FVector* OutPoints, const FVector* Offsets,
		int32 NumPoints, const FVector& SourceLocal, const FVector& TargetLocal) const;

	void Update(const FUpdateContext& UpdateContext) override;
	void Spawn(const FSpawnContext& Context) override;
	uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData = nullptr) override { (void)TypeData; return sizeof(FBeamNoisePayloadData); }
	UParticleModule* CloneForLOD(UParticleLODLevel* NewOuter) const override;
};
