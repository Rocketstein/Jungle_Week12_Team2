#include "ParticleBeamInstances.h"
#include "Particle/ParticleLODLevel.h"
#include "Component/ParticleSystemComponent.h"

void FBeam2EmitterInstance::Tick(float DeltaTime, bool bSuppressSpawning)
{
	
}

FDynamicEmitterReplayDataBase* FBeam2EmitterInstance::GetReplayData()
{
	UParticleModuleTypeDataBeam2* BeamModule = static_cast<UParticleModuleTypeDataBeam2*>(CurrentLODLevel->TypeDataModule);
	if (!BeamModule) return nullptr;

	FVector Source;
	FVector Target;

	switch (BeamModule->BeamMethod)
	{
	case (PEB2M_Distance):
	{
		// TODO: Add "Use Local Space" variant that adds Location to Source
		Source = BeamModule->SourcePoint;
		FVector TargetDir = Component ? Component->GetForwardVector() : FVector(1, 0, 0);
		Target = TargetDir * BeamModule->Distance;
		break;
	}
	case (PEB2M_Target):
	{
		Source = BeamModule->SourcePoint;
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

	FDynamicEmitterReplayDataBase* NewEmitterReplayData = new FDynamicBeamEmitterReplayData();

	if (!FillReplayData(*NewEmitterReplayData))
	{
		delete NewEmitterReplayData;
		return nullptr;
	}

	return NewEmitterReplayData;
}