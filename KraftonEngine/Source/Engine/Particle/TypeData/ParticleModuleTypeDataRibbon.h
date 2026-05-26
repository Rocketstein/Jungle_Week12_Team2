#pragma once

#include "Particle/ParticleModule.h"
#include "ParticleModuleTypeDataRibbon.generated.h"

UENUM()
enum ETrailsRenderAxisOption : int
{
	Trails_CameraUp,
	Trails_SourceUp,
	Trails_WorldUp,
	Trails_MAX,
};

UCLASS()
class UParticleModuleTypeDataRibbon : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY(UParticleModuleTypeDataRibbon)

	bool IsARibbonEmitter() const override { return true; }
	bool SupportsSpecificScreenAlignmentFlags() const override { return true; }
	uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;
	UParticleModule* CloneForLOD(UParticleLODLevel* NewOuter) const override;

	// Maximum interpolation points considered between two spawned trail particles.
	UPROPERTY(Edit, Category="Trail", Min=0, Max=32, Speed=1.0f)
	int32 MaxTessellationBetweenParticles = 1;

	UPROPERTY(Edit, Category="Trail", Min=1, Max=16, Speed=1.0f)
	int32 SheetsPerTrail = 1;

	UPROPERTY(Edit, Category="Trail", Min=1, Max=64, Speed=1.0f)
	int32 MaxTrailCount = 1;

	UPROPERTY(Edit, Category="Trail", Min=2, Max=1024, Speed=1.0f)
	int32 MaxParticleInTrailCount = 64;

	UPROPERTY(Edit, Category="Trail")
	bool bDeadTrailsOnDeactivate = true;

	UPROPERTY(Edit, Category="Trail")
	bool bDeadTrailsOnSourceLoss = true;

	// Unreal keeps the misspelled "Segement" property. Use the corrected engine
	// name while preserving the same meaning: do not join the trail to source.
	UPROPERTY(Edit, Category="Trail")
	bool bClipSourceSegment = false;

	UPROPERTY(Edit, Category="Trail")
	bool bEnablePreviousTangentRecalculation = false;

	UPROPERTY(Edit, Category="Trail")
	bool bTangentRecalculationEveryFrame = false;

	UPROPERTY(Edit, Category="Trail")
	bool bSpawnInitialParticle = true;

	UPROPERTY(Edit, Category="Trail")
	ETrailsRenderAxisOption RenderAxis = Trails_CameraUp;

	UPROPERTY(Edit, Category="Spawn", Min=0.0f, Max=1000.0f, Speed=0.1f)
	float TangentSpawningScalar = 0.0f;

	UPROPERTY(Edit, Category="Rendering")
	bool bRenderGeometry = true;

	UPROPERTY(Edit, Category="Rendering")
	bool bRenderSpawnPoints = false;

	UPROPERTY(Edit, Category="Rendering")
	bool bRenderTangents = false;

	UPROPERTY(Edit, Category="Rendering")
	bool bRenderTessellation = false;

	UPROPERTY(Edit, Category="Rendering", Min=0.0f, Max=10000.0f, Speed=1.0f)
	float TilingDistance = 0.0f;

	UPROPERTY(Edit, Category="Rendering", Min=0.0f, Max=10000.0f, Speed=1.0f)
	float DistanceTessellationStepSize = 0.0f;

	UPROPERTY(Edit, Category="Rendering")
	bool bEnableTangentDiffInterpScale = false;

	UPROPERTY(Edit, Category="Rendering", Min=0.0f, Max=1000.0f, Speed=0.1f)
	float TangentTessellationScalar = 0.0f;

	// Local constant-render defaults for the first ribbon renderer pass. Cascade
	// commonly drives these through modules/materials; keeping them here lets the
	// type data produce a visible ribbon before those paths are fully mirrored.
	UPROPERTY(Edit, Category="Rendering", Min=0.0f, Max=1000.0f, Speed=0.25f)
	float Width = 8.0f;

	UPROPERTY(Edit, Category="Rendering")
	FVector Color = FVector::OneVector;

	UPROPERTY(Edit, Category="Rendering", Min=0.0f, Max=1.0f, Speed=0.01f)
	float Alpha = 1.0f;
};
