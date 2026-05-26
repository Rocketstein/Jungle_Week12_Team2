#include "ParticleBeamInstances.h"

#include "Component/ParticleSystemComponent.h"
#include "Materials/Material.h"
#include "Particle/ParticleHelper.h"
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
	const int32 SheetCount = std::max(1, BeamModule->Sheets);
	const int32 MaxBeamCount = std::max(1, BeamModule->MaxBeamCount);
	const int32 LogicalBeamCount = BeamModule->bAlwaysOn
		? MaxBeamCount
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
	NewEmitterReplayData->ActiveParticleCount = LogicalBeamCount * SheetCount;

	const FVector WorldSource = ComponentToWorld.TransformPositionWithW(LocalSource);
	const FVector WorldTarget = ComponentToWorld.TransformPositionWithW(LocalTarget);
	const FVector WorldBeamDelta = WorldTarget - WorldSource;
	const FVector WorldSourceTangent = ComponentToWorld.TransformVector(LocalSourceTangent);
	const FVector WorldTargetTangent = ComponentToWorld.TransformVector(LocalTargetTangent);
	const bool bUseParticleInstanceData = !BeamModule->bAlwaysOn;
	NewEmitterReplayData->Beams.reserve(LogicalBeamCount);
	for (int32 i = 0; i < LogicalBeamCount; ++i)
	{
		const FBaseParticle* Particle = (bUseParticleInstanceData && i < ActiveParticles && ParticleIndices)
			? GetParticleDirect(ParticleIndices[i])
			: nullptr;
		const FVector BeamOffset = Particle ? (Particle->Location - Location) : FVector::ZeroVector;
		const FVector BeamColor = Particle
			? FVector(Particle->Color.R * BeamModule->Color.X,
			          Particle->Color.G * BeamModule->Color.Y,
			          Particle->Color.B * BeamModule->Color.Z)
			: BeamModule->Color;
		const float BeamAlpha = Particle
			? Particle->Color.A * BeamModule->Alpha
			: BeamModule->Alpha;
		const float WidthScale = Particle ? std::max(0.0f, Particle->Size.X) : 1.0f;

		FBeamInstanceData Beam;
		Beam.Source       = WorldSource + BeamOffset;
		Beam.Target       = Beam.Source + WorldBeamDelta;
		Beam.SourceTangent = WorldSourceTangent;
		Beam.TargetTangent = WorldTargetTangent;
		Beam.bUseTangents = bUseTangents;
		Beam.Color        = BeamColor;
		Beam.Alpha        = std::clamp(BeamAlpha, 0.0f, 1.0f);
		Beam.Width        = BeamModule->Width * WidthScale;
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
