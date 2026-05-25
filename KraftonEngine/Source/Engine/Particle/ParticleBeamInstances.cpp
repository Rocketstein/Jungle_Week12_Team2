#include "ParticleBeamInstances.h"

#include "Component/ParticleSystemComponent.h"
#include "Materials/Material.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"
#include "Particle/TypeData/ParticleModuleTypeDataBeam2.h"

#include <algorithm>

void FBeam2EmitterInstance::Tick(float DeltaTime, int32 LODLevel, bool bSuppressSpawning)
{
	FParticleEmitterInstance::Tick(DeltaTime, LODLevel, bSuppressSpawning);
	BeamTravelTime += DeltaTime;
}

FDynamicEmitterReplayDataBase* FBeam2EmitterInstance::GetReplayData()
{
	if (!CurrentLODLevel || !Component)
	{
		return nullptr;
	}

	UParticleModuleTypeDataBeam2* BeamModule = Cast<UParticleModuleTypeDataBeam2>(CurrentLODLevel->TypeDataModule);
	if (!BeamModule || (!BeamModule->bAlwaysOn && ActiveParticles <= 0))
	{
		return nullptr;
	}

	FVector LocalSource = BeamModule->SourcePoint;
	FVector LocalTarget = BeamModule->TargetPoint;

	switch (BeamModule->BeamMethod)
	{
	case PEB2M_Distance:
		LocalTarget = BeamModule->SourcePoint + FVector(BeamModule->Distance, 0.0f, 0.0f);
		break;
	case PEB2M_Target:
		break;
	case PEB2M_Branch:
		// Branching needs parent-emitter lookup. Until that exists, render this
		// with the explicit target so imported assets still produce a beam.
		break;
	default:
		return nullptr;
	}

	const FMatrix& ComponentToWorld = Component->GetWorldMatrix();
	const int32 SheetCount = std::max(1, BeamModule->Sheets);
	const int32 MaxBeamCount = std::max(1, BeamModule->MaxBeamCount);
	const int32 LogicalBeamCount = BeamModule->bAlwaysOn
		? std::clamp(std::max(1, ActiveParticles), 1, MaxBeamCount)
		: std::clamp(ActiveParticles, 0, MaxBeamCount);
	const float FullBeamLength = (ComponentToWorld.TransformPositionWithW(LocalTarget)
		- ComponentToWorld.TransformPositionWithW(LocalSource)).Length();
	const float BeamProgress = (BeamModule->Speed > 0.0f && FullBeamLength > 1e-6f)
		? std::clamp((BeamTravelTime * BeamModule->Speed) / FullBeamLength, 0.0f, 1.0f)
		: 1.0f;

	FDynamicBeamEmitterReplayData* NewEmitterReplayData = new FDynamicBeamEmitterReplayData();
	NewEmitterReplayData->Source = ComponentToWorld.TransformPositionWithW(LocalSource);
	NewEmitterReplayData->Target = ComponentToWorld.TransformPositionWithW(LocalTarget);
	NewEmitterReplayData->LogicalBeamCount = LogicalBeamCount;
	NewEmitterReplayData->ParticleStride = 0;
	NewEmitterReplayData->Scale = FVector::OneVector;
	NewEmitterReplayData->Width = BeamModule->Width;
	NewEmitterReplayData->Color = BeamModule->Color;
	NewEmitterReplayData->Alpha = std::clamp(BeamModule->Alpha, 0.0f, 1.0f);
	NewEmitterReplayData->InterpolationPoints = std::max(0, BeamModule->InterpolationPoints);
	NewEmitterReplayData->Sheets = SheetCount;
	NewEmitterReplayData->MaxBeamCount = MaxBeamCount;
	NewEmitterReplayData->UpVectorStepSize = std::max(0, BeamModule->UpVectorStepSize);
	NewEmitterReplayData->TaperFactor = BeamModule->TaperFactor;
	NewEmitterReplayData->TaperMethod = BeamModule->TaperMethod;
	NewEmitterReplayData->TaperScale = BeamModule->TaperScale;
	NewEmitterReplayData->TextureTile = std::max(1, BeamModule->TextureTile);
	NewEmitterReplayData->TextureTileDistance = std::max(0.0f, BeamModule->TextureTileDistance);
	NewEmitterReplayData->bRenderDirectLine = BeamModule->bRenderDirectLine;
	NewEmitterReplayData->bRenderGeometry = BeamModule->bRenderGeometry;
	NewEmitterReplayData->bRenderLines = BeamModule->bRenderLines;
	NewEmitterReplayData->bRenderTessellation = BeamModule->bRenderTessellation;
	NewEmitterReplayData->BranchParentName = BeamModule->BranchParentName;
	NewEmitterReplayData->TargetData = BeamModule->TargetData;
	// Cascade reports active beam particles as logical beams multiplied by
	// crossed sheets. The current renderer still expands one resolved beam path.
	NewEmitterReplayData->ActiveParticleCount = LogicalBeamCount * SheetCount;
	NewEmitterReplayData->BeamProgress = BeamProgress;

	if (CurrentLODLevel->RequiredModule)
	{
		NewEmitterReplayData->SortMode = CurrentLODLevel->RequiredModule->SortMode;
		NewEmitterReplayData->MaterialInterface = CurrentLODLevel->RequiredModule->Material;
		if (UMaterial* Material = CurrentLODLevel->RequiredModule->Material
			? CurrentLODLevel->RequiredModule->Material->GetMaterial()
			: nullptr)
		{
			NewEmitterReplayData->BlendMode = Material->GetBlendState();
		}
	}

	return NewEmitterReplayData;
}
