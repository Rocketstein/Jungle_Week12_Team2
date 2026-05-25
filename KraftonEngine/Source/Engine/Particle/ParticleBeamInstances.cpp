#include "ParticleBeamInstances.h"

#include "Component/ParticleSystemComponent.h"
#include "Materials/Material.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"
#include "Particle/TypeData/ParticleModuleTypeDataBeam2.h"

#include <algorithm>

void FBeam2EmitterInstance::Tick(float DeltaTime, bool bSuppressSpawning)
{
	(void)bSuppressSpawning;

	LastDeltaTime = DeltaTime;
	SecondsSinceCreation += DeltaTime;
	EmitterTime += DeltaTime;
	OldLocation = Location;
	Location = Component ? Component->GetWorldLocation() : FVector::ZeroVector;

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
	FDynamicBeamEmitterReplayData* NewEmitterReplayData = new FDynamicBeamEmitterReplayData();
	NewEmitterReplayData->Source = ComponentToWorld.TransformPositionWithW(LocalSource);
	NewEmitterReplayData->Target = ComponentToWorld.TransformPositionWithW(LocalTarget);
	NewEmitterReplayData->ActiveParticleCount = 1;
	NewEmitterReplayData->ParticleStride = 0;
	NewEmitterReplayData->Scale = FVector::OneVector;
	NewEmitterReplayData->Width = BeamModule->Width;
	NewEmitterReplayData->Color = BeamModule->Color;
	NewEmitterReplayData->Alpha = std::clamp(BeamModule->Alpha, 0.0f, 1.0f);
	NewEmitterReplayData->InterpolationPoints = std::max(0, BeamModule->InterpolationPoints);
	NewEmitterReplayData->Sheets = std::max(1, BeamModule->Sheets);
	NewEmitterReplayData->TaperFactor = BeamModule->TaperFactor;
	NewEmitterReplayData->TaperMethod = BeamModule->TaperMethod;
	NewEmitterReplayData->TaperScale = BeamModule->TaperScale;
	NewEmitterReplayData->TextureTile = std::max(1, BeamModule->TextureTile);
	NewEmitterReplayData->TextureTileDistance = std::max(0.0f, BeamModule->TextureTileDistance);
	NewEmitterReplayData->bRenderDirectLine = BeamModule->bRenderDirectLine;
	NewEmitterReplayData->bRenderGeometry = BeamModule->bRenderGeometry;
	NewEmitterReplayData->bRenderLines = BeamModule->bRenderLines;
	NewEmitterReplayData->bRenderTessellation = BeamModule->bRenderTessellation;

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
