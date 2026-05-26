#include "ParticleBeamInstances.h"

#include "Component/ParticleSystemComponent.h"
#include "Materials/Material.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"
#include "Particle/TypeData/ParticleModuleTypeDataBeam2.h"

#include <algorithm>

namespace
{
	template <typename TModule>
	TModule* FindEnabledBeamModule(UParticleLODLevel* LOD)
	{
		if (!LOD)
		{
			return nullptr;
		}

		for (UParticleModule* Module : LOD->Modules)
		{
			TModule* TypedModule = Cast<TModule>(Module);
			if (TypedModule && TypedModule->bEnabled)
			{
				return TypedModule;
			}
		}
		return nullptr;
	}
}

void FBeam2EmitterInstance::Tick(float DeltaTime, int32 LODLevel, bool bSuppressSpawning)
{
	FParticleEmitterInstance::Tick(DeltaTime, LODLevel, bSuppressSpawning);
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
	FVector LocalSourceTangent = FVector::ZeroVector;
	FVector LocalTargetTangent = FVector::ZeroVector;
	bool bUseTangents = false;
	if (UParticleModuleBeamSource* SourceModule = FindEnabledBeamModule<UParticleModuleBeamSource>(CurrentLODLevel))
	{
		LocalSource = SourceModule->SourcePoint;
		if (SourceModule->SourceTangentMethod == PEBTANM_UserSet)
		{
			LocalSourceTangent = SourceModule->SourceTangent;
			bUseTangents = true;
		}
	}

	switch (BeamModule->BeamMethod)
	{
	case PEB2M_Distance:
		LocalTarget = LocalSource + FVector(BeamModule->Distance, 0.0f, 0.0f);
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
	if (UParticleModuleBeamTarget* TargetModule = FindEnabledBeamModule<UParticleModuleBeamTarget>(CurrentLODLevel))
	{
		LocalTarget = TargetModule->TargetPoint;
		if (TargetModule->TargetTangentMethod == PEBTANM_UserSet)
		{
			LocalTargetTangent = TargetModule->TargetTangent;
			bUseTangents = true;
		}
	}

	const FMatrix& ComponentToWorld = Component->GetWorldMatrix();
	FDynamicBeamEmitterReplayData* NewEmitterReplayData = new FDynamicBeamEmitterReplayData();
	NewEmitterReplayData->Source = ComponentToWorld.TransformPositionWithW(LocalSource);
	NewEmitterReplayData->Target = ComponentToWorld.TransformPositionWithW(LocalTarget);
	NewEmitterReplayData->SourceTangent = ComponentToWorld.TransformVector(LocalSourceTangent);
	NewEmitterReplayData->TargetTangent = ComponentToWorld.TransformVector(LocalTargetTangent);
	NewEmitterReplayData->bUseTangents = bUseTangents;
	NewEmitterReplayData->ActiveParticleCount = 1;
	NewEmitterReplayData->ParticleStride = 0;
	NewEmitterReplayData->Scale = FVector::OneVector;
	NewEmitterReplayData->Width = BeamModule->Width;
	NewEmitterReplayData->Color = BeamModule->Color;
	NewEmitterReplayData->Alpha = std::clamp(BeamModule->Alpha, 0.0f, 1.0f);
	NewEmitterReplayData->InterpolationPoints = std::max(0, BeamModule->InterpolationPoints);
	NewEmitterReplayData->Sheets = std::max(1, BeamModule->Sheets);
	NewEmitterReplayData->MaxBeamCount = std::max(1, BeamModule->MaxBeamCount);
	NewEmitterReplayData->Speed = std::max(0.0f, BeamModule->Speed);
	NewEmitterReplayData->UpVectorStepSize = std::max(0, BeamModule->UpVectorStepSize);
	NewEmitterReplayData->TaperFactor = BeamModule->TaperFactor;
	NewEmitterReplayData->TaperMethod = BeamModule->TaperMethod;
	NewEmitterReplayData->TaperScale = BeamModule->TaperScale;
	NewEmitterReplayData->TextureTile = std::max(1, BeamModule->TextureTile);
	NewEmitterReplayData->TextureTileDistance = std::max(0.0f, BeamModule->TextureTileDistance);
	if (UParticleModuleBeamNoise* NoiseModule = FindEnabledBeamModule<UParticleModuleBeamNoise>(CurrentLODLevel))
	{
		NewEmitterReplayData->NoiseAmplitude = std::max(0.0f, NoiseModule->NoiseAmplitude);
		NewEmitterReplayData->NoiseFrequency = std::max(0.0f, NoiseModule->NoiseFrequency);
		NewEmitterReplayData->NoisePhase = SecondsSinceCreation * std::max(0.0f, NoiseModule->NoiseSpeed);
		NewEmitterReplayData->NoiseSeed = NoiseModule->NoiseSeed;
		if (NoiseModule->bLowFreqEnabled)
		{
			NewEmitterReplayData->NoiseRangeMin = NoiseModule->NoiseRangeMin;
			NewEmitterReplayData->NoiseRangeMax = NoiseModule->NoiseRangeMax;
		}
	}
	NewEmitterReplayData->bRenderDirectLine = BeamModule->bRenderDirectLine;
	NewEmitterReplayData->bRenderGeometry = BeamModule->bRenderGeometry;
	NewEmitterReplayData->bRenderLines = BeamModule->bRenderLines;
	NewEmitterReplayData->bRenderTessellation = BeamModule->bRenderTessellation;
	NewEmitterReplayData->BranchParentName = BeamModule->BranchParentName;
	NewEmitterReplayData->TargetData = BeamModule->TargetData;

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
