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
	NewEmitterReplayData->LogicalBeamCount = LogicalBeamCount;
	NewEmitterReplayData->ParticleStride = 0;
	NewEmitterReplayData->Scale = FVector::OneVector;
	NewEmitterReplayData->InterpolationPoints = std::max(0, BeamModule->InterpolationPoints);
	NewEmitterReplayData->Sheets = SheetCount;
	NewEmitterReplayData->MaxBeamCount = MaxBeamCount;
	NewEmitterReplayData->UpVectorStepSize = std::max(0, BeamModule->UpVectorStepSize);
	NewEmitterReplayData->TextureTile = std::max(1, BeamModule->TextureTile);
	NewEmitterReplayData->TextureTileDistance = std::max(0.0f, BeamModule->TextureTileDistance);
	NewEmitterReplayData->bRenderDirectLine = BeamModule->bRenderDirectLine;
	NewEmitterReplayData->bRenderGeometry = BeamModule->bRenderGeometry;
	NewEmitterReplayData->bRenderLines = BeamModule->bRenderLines;
	NewEmitterReplayData->bRenderTessellation = BeamModule->bRenderTessellation;
	NewEmitterReplayData->BranchParentName = BeamModule->BranchParentName;
	NewEmitterReplayData->TargetData = BeamModule->TargetData;
	NewEmitterReplayData->ActiveParticleCount = LogicalBeamCount * SheetCount;

	// Per-beam state. Until per-particle beam modules feed individual variation,
	// every active beam shares the module's resolved Source/Target/Width/etc.
	const FVector WorldSource = ComponentToWorld.TransformPositionWithW(LocalSource);
	const FVector WorldTarget = ComponentToWorld.TransformPositionWithW(LocalTarget);
	NewEmitterReplayData->Beams.reserve(LogicalBeamCount);
	for (int32 i = 0; i < LogicalBeamCount; ++i)
	{
		FBeamInstanceData Beam;
		Beam.Source       = WorldSource;
		Beam.Target       = WorldTarget;
		Beam.Color        = BeamModule->Color;
		Beam.Alpha        = std::clamp(BeamModule->Alpha, 0.0f, 1.0f);
		Beam.Width        = BeamModule->Width;
		Beam.TaperMethod  = BeamModule->TaperMethod;
		Beam.TaperFactor  = BeamModule->TaperFactor;
		Beam.TaperScale   = BeamModule->TaperScale;
		Beam.BeamProgress = BeamProgress;
		NewEmitterReplayData->Beams.push_back(Beam);
	}

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
