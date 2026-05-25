#include "ParticleBeamInstances.h"
#include "Particle/ParticleLODLevel.h"
#include "Component/ParticleSystemComponent.h"

void FBeam2EmitterInstance::Tick(float DeltaTime, bool bSuppressSpawning)
{
	
}

FDynamicEmitterReplayDataBase* FBeam2EmitterInstance::GetReplayData()
{
	UParticleModuleTypeDataBeam2* BeamModule = static_cast<UParticleModuleTypeDataBeam2*>(CurrentLODLevel->TypeDataModule);
	if (!BeamModule || !Component) return nullptr;

	FVector Source;
	FVector Target;

	switch (BeamModule->BeamMethod)
	{
	case (PEB2M_Distance):
	{
		// TODO: Add "Use Local Space" branch that adds emitter transformation to Source
		Source = Component->GetWorldMatrix().TransformVector(BeamModule->SourcePoint);
		FVector TargetDir = Component->GetForwardVector();
		Target = TargetDir * BeamModule->Distance;
		break;
	}
	case (PEB2M_Target):
	{
		Source = Component->GetWorldMatrix().TransformVector(BeamModule->SourcePoint);
		Target = BeamModule->TargetPoint;
		break;	
	}
	case(PEB2M_Branch):
	{
		// TODO
		return nullptr;
		break;
	}
	default:
	{
		return nullptr;
	}
	}

	FDynamicBeamEmitterReplayData* NewEmitterReplayData = new FDynamicBeamEmitterReplayData();

	if (!FillReplayData(*NewEmitterReplayData))
	{
		delete NewEmitterReplayData;
		return nullptr;
	}

	NewEmitterReplayData->Width = BeamModule->Width;
	NewEmitterReplayData->Color = BeamModule->Color;
	NewEmitterReplayData->TaperFactor = BeamModule->TaperFactor;
	NewEmitterReplayData->TaperMethod = BeamModule->TaperMethod;
	NewEmitterReplayData->TaperScale = BeamModule->TaperScale;
	NewEmitterReplayData->TextureTile = BeamModule->TextureTile;
	NewEmitterReplayData->TextureTileDistance = BeamModule->TextureTileDistance;
	NewEmitterReplayData->bRenderDirectLine = BeamModule->bRenderDirectLine;
	NewEmitterReplayData->bRenderGeometry = BeamModule->bRenderGeometry;
	NewEmitterReplayData->bRenderLines = BeamModule->bRenderLines;
	NewEmitterReplayData->bRenderTessellation = BeamModule->bRenderTessellation;

	return NewEmitterReplayData;
}