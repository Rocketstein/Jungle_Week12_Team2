#include "Component/ParticleSystemComponent.h"

#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleEmitterInstances.h"
#include "Particle/ParticleSystem.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleBeamInstances.h"
#include "Render/Particle/ParticleDynamicData.h"
#include "Render/Proxy/ParticleSystemSceneProxy.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "GameFramework/World.h"
#include "Particle/ParticleLODContext.h"
#include "Particle/RibbonEmitterInstance.h"

namespace
{
void MoveReplayDataBase(FDynamicEmitterReplayDataBase& Dest, FDynamicEmitterReplayDataBase& Source)
{
	Dest.eEmitterType = Source.eEmitterType;
	Dest.ActiveParticleCount = Source.ActiveParticleCount;
	Dest.ParticleStride = Source.ParticleStride;
	Dest.DataContainer = std::move(Source.DataContainer);
	Dest.Scale = Source.Scale;
	Dest.SortMode = Source.SortMode;
	Dest.EmitterSortPriority = Source.EmitterSortPriority;
}

void MoveRenderableReplayData(FDynamicRenderableEmitterReplayDataBase& Dest, FDynamicRenderableEmitterReplayDataBase& Source)
{
	MoveReplayDataBase(Dest, Source);
	Dest.MaterialInterface = Source.MaterialInterface;
	Dest.BlendMode = Source.BlendMode;
}

void MoveSpriteReplayData(FDynamicSpriteEmitterReplayData& Dest, FDynamicSpriteEmitterReplayData& Source)
{
	MoveRenderableReplayData(Dest, Source);
	Dest.SubImages_Horizontal = Source.SubImages_Horizontal;
	Dest.SubImages_Vertical = Source.SubImages_Vertical;
	Dest.ScreenAlignment = Source.ScreenAlignment;
	Dest.EmitterOrigin = Source.EmitterOrigin;
	Dest.AlphaSource = Source.AlphaSource;
	Dest.AlphaThreshold = Source.AlphaThreshold;
	Dest.AlphaPower = Source.AlphaPower;
	Dest.ColorIntensity = Source.ColorIntensity;
}

void MoveMeshReplayData(FDynamicMeshEmitterReplayData& Dest, FDynamicMeshEmitterReplayData& Source)
{
	MoveRenderableReplayData(Dest, Source);
	Dest.LODLevel = Source.LODLevel;
	Dest.StaticMesh = Source.StaticMesh;
}

void MoveBeamReplayData(FDynamicBeamEmitterReplayData& Dest, FDynamicBeamEmitterReplayData& Source)
{
	MoveRenderableReplayData(Dest, Source);
	Dest.Beams = std::move(Source.Beams);
	Dest.InterpolationPoints = Source.InterpolationPoints;
	Dest.Sheets = Source.Sheets;
	Dest.LogicalBeamCount = Source.LogicalBeamCount;
	Dest.MaxBeamCount = Source.MaxBeamCount;
	Dest.UpVectorStepSize = Source.UpVectorStepSize;
	Dest.TextureTile = Source.TextureTile;
	Dest.TextureTileDistance = Source.TextureTileDistance;
	Dest.NoiseAmplitude = Source.NoiseAmplitude;
	Dest.NoiseFrequency = Source.NoiseFrequency;
	Dest.NoisePhase = Source.NoisePhase;
	Dest.NoiseSeed = Source.NoiseSeed;
	Dest.NoiseRangeMin = Source.NoiseRangeMin;
	Dest.NoiseRangeMax = Source.NoiseRangeMax;
	Dest.bRenderGeometry = Source.bRenderGeometry;
	Dest.bRenderDirectLine = Source.bRenderDirectLine;
	Dest.bRenderLines = Source.bRenderLines;
	Dest.bRenderTessellation = Source.bRenderTessellation;
	Dest.BranchParentName = Source.BranchParentName;
	Dest.TargetData = std::move(Source.TargetData);
}

void MoveRibbonReplayData(FDynamicRibbonEmitterReplayData& Dest, FDynamicRibbonEmitterReplayData& Source)
{
	MoveRenderableReplayData(Dest, Source);
	Dest.Points = std::move(Source.Points);
	Dest.Trails = std::move(Source.Trails);
	Dest.SheetsPerTrail = Source.SheetsPerTrail;
	Dest.MaxTessellationBetweenParticles = Source.MaxTessellationBetweenParticles;
	Dest.TilingDistance = Source.TilingDistance;
	Dest.DistanceTessellationStepSize = Source.DistanceTessellationStepSize;
	Dest.bRenderGeometry = Source.bRenderGeometry;
	Dest.bRenderSpawnPoints = Source.bRenderSpawnPoints;
	Dest.bRenderTangents = Source.bRenderTangents;
	Dest.bRenderTessellation = Source.bRenderTessellation;
}

FParticleEmitterInstance* CreateEmitterInstance(
	UParticleSystemComponent* Component,
	UParticleEmitter* Emitter)
{
	if (!Emitter)
	{
		return nullptr;
	}

	Emitter->UpdateModuleLists();
	UParticleLODLevel* LOD = Emitter ? Emitter->GetLODLevel(0) : nullptr;
	if (LOD && LOD->TypeDataModule)
	{
		if (LOD->TypeDataModule->IsABeamEmitter()) {
			return new FBeam2EmitterInstance(Component);
		}
		if (LOD->TypeDataModule->IsARibbonEmitter())
		{
			return new FRibbonEmitterInstance(Component);
		}
	}

	return new FParticleEmitterInstance(Component);
}

FDynamicEmitterDataBase* CreateDynamicEmitterData(int32 EmitterIndex, FDynamicEmitterReplayDataBase* ReplayData)
{
	if (!ReplayData)
	{
		return nullptr;
	}

	FDynamicEmitterDataBase* DynamicData = nullptr;

	if (ReplayData->eEmitterType == DET_Mesh)
	{
		FDynamicMeshEmitterData* MeshDynamicData = new FDynamicMeshEmitterData();
		MeshDynamicData->EmitterIndex = EmitterIndex;
		MoveMeshReplayData(MeshDynamicData->MeshSource, *static_cast<FDynamicMeshEmitterReplayData*>(ReplayData));
		DynamicData = MeshDynamicData;
	}
	else if (ReplayData->eEmitterType == DET_Beam2)
	{
		FDynamicBeamEmitterData* BeamDynamicData = new FDynamicBeamEmitterData();
		BeamDynamicData->EmitterIndex = EmitterIndex;
		MoveBeamReplayData(BeamDynamicData->BeamSource, *static_cast<FDynamicBeamEmitterReplayData*>(ReplayData));
		DynamicData = BeamDynamicData;
	}
	else if (ReplayData->eEmitterType == DET_Ribbon)
	{
		FDynamicRibbonEmitterData* RibbonDynamicData = new FDynamicRibbonEmitterData();
		RibbonDynamicData->EmitterIndex = EmitterIndex;
		MoveRibbonReplayData(RibbonDynamicData->RibbonSource, *static_cast<FDynamicRibbonEmitterReplayData*>(ReplayData));
		DynamicData = RibbonDynamicData;
	}
	else
	{
		FDynamicSpriteEmitterData* SpriteDynamicData = new FDynamicSpriteEmitterData();
		SpriteDynamicData->EmitterIndex = EmitterIndex;
		MoveSpriteReplayData(SpriteDynamicData->Source, *static_cast<FDynamicSpriteEmitterReplayData*>(ReplayData));
		DynamicData = SpriteDynamicData;
	}

	delete ReplayData;
	return DynamicData;
}

void DeleteDynamicEmitterData(TArray<FDynamicEmitterDataBase*>& DynamicData)
{
	for (FDynamicEmitterDataBase* EmitterData : DynamicData)
	{
		delete EmitterData;
	}
	DynamicData.clear();
}

uint16 ToTranslucencySortPriority(int32 SortPriority)
{
	return static_cast<uint16>(std::clamp(SortPriority, 0, 65535));
}
}

UParticleSystemComponent::~UParticleSystemComponent()
{
	ResetParticles(true);
}

void UParticleSystemComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (!PropertyName)
	{
		return;
	}

	if (std::strcmp(PropertyName, "Particle System Priority") == 0 || std::strcmp(PropertyName, "SortPriority") == 0)
	{
		FParticleSystemSceneProxy* ParticleSceneProxy = GetSceneProxy();
		if (ParticleSceneProxy)
		{
			ParticleSceneProxy->SetTranslucencySortPriority(ToTranslucencySortPriority(SortPriority));
		}
	}
	else if (std::strcmp(PropertyName, "Template") == 0)
	{
		ResetParticles(true);
		InitializeSystem();
	}
}

void UParticleSystemComponent::EndPlay()
{
	ResetParticles(true);
	UFXSystemComponent::EndPlay();
}

UFXSystemAsset* UParticleSystemComponent::GetFXSystemAsset() const
{
	return Template.Get();
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* NewTemplate)
{
	if (Template.Get() == NewTemplate)
	{
		return;
	}

	ResetParticles(true);
	Template = NewTemplate;
	InitializeSystem();
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	FParticleSystemSceneProxy* ParticleSceneProxy = new FParticleSystemSceneProxy(this);
	ParticleSceneProxy->SetTranslucencySortPriority(ToTranslucencySortPriority(SortPriority));
	return ParticleSceneProxy;
}

FParticleSystemSceneProxy* UParticleSystemComponent::GetSceneProxy() const
{
	return static_cast<FParticleSystemSceneProxy*>(SceneProxy);
}

void UParticleSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ClearParticleCollisionEvents();

	UParticleSystem* ParticleTemplate = Template.Get();
	if (!ParticleTemplate)
	{
		return;
	}

	if (EmitterInstances.empty())
	{
		InitParticles();
		//InitializeSystem();
	}

	if (ForcedLODLevel >= 0)
	{
		const int32 MaxLODIndex = LODDistances.empty() ? 0 : static_cast<int32>(LODDistances.size()) - 1;
		LODLevel = std::clamp(ForcedLODLevel, 0, MaxLODIndex);
	}
	else if (UWorld* World = GetWorld())
	{
		LODLevel = DecideLODLevel(World->GetParticleLODContext());
	}
	for (FParticleEmitterInstance* EmitterInstance : EmitterInstances)
	{
		if (EmitterInstance)
		{
			EmitterInstance->Tick(DeltaTime, LODLevel, false);
		}
	}

	DispatchParticleCollisionEvents();

	TArray<FDynamicEmitterDataBase*> NewRenderData;
	NewRenderData.reserve(EmitterInstances.size());

	for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(EmitterInstances.size()); ++EmitterIndex)
	{
		FParticleEmitterInstance* EmitterInstance = EmitterInstances[EmitterIndex];
		if (!EmitterInstance)
		{
			continue;
		}

		FDynamicEmitterDataBase* DynamicData = CreateDynamicEmitterData(EmitterIndex, EmitterInstance->GetReplayData());
		if (DynamicData)
		{
			NewRenderData.push_back(DynamicData);
		}
	}

	FParticleSystemSceneProxy* ParticleSceneProxy = GetSceneProxy();
	if (!ParticleSceneProxy)
	{
		DeleteDynamicEmitterData(NewRenderData);
		return;
	}

	ParticleSceneProxy->UpdateDynamicData(std::move(NewRenderData));
	ParticleSceneProxy->UpdateMesh();
}

int32 UParticleSystemComponent::DecideLODLevel(const FParticleLODContext& Context) const
{
	if (!Context.bValid || LODDistances.empty())
	{
		return 0;
	}

	const float Distance = (Context.ViewPosition - GetWorldLocation()).Length();
	int32 SelectedLOD = 0;
	for (int32 Index = 0; Index < static_cast<int32>(LODDistances.size()); ++Index)
	{
		if (Distance < LODDistances[Index])
		{
			break;
		}
		SelectedLOD = Index;
	}

	return std::clamp(SelectedLOD, 0, static_cast<int32>(LODDistances.size()) - 1);
}

void UParticleSystemComponent::SetForcedLODLevel(int32 InLODLevel)
{
	ForcedLODLevel = std::max(0, InLODLevel);
	if (!LODDistances.empty())
	{
		LODLevel = std::clamp(ForcedLODLevel, 0, static_cast<int32>(LODDistances.size()) - 1);
	}
}

void UParticleSystemComponent::ClearForcedLODLevel()
{
	ForcedLODLevel = -1;
}

//각 Instance를 채워넣는다
void UParticleSystemComponent::InitParticles()
{
	ResetParticles(true);

	UParticleSystem* ParticleTemplate = Template.Get();
	if (!ParticleTemplate)
	{
		return;
	}
	//ParticleSystem과 Emitter가 가지는 LODLevels의 갯수를 맞춘다
	ParticleTemplate->NormalizeLODData();
	LODDistances = ParticleTemplate->GetLODDistances();
	const int32 MaxLODIndex = LODDistances.empty() ? 0 : static_cast<int32>(LODDistances.size()) - 1;
	if (ForcedLODLevel >= 0)
	{
		LODLevel = std::clamp(ForcedLODLevel, 0, MaxLODIndex);
	}
	else
	{
		LODLevel = std::clamp(LODLevel, 0, MaxLODIndex);
	}
	
	EmitterInstances.reserve(ParticleTemplate->Emitters.size());
	//Particle System(원본)과 같은 크기로 Isntance를 만든다.
	for (int32 EmitterInstanceIdx = 0; EmitterInstanceIdx < static_cast<int32>(ParticleTemplate->Emitters.size()); ++EmitterInstanceIdx)
	{
		//Particle System안의 Emitter
		UParticleEmitter* Emitter = ParticleTemplate->Emitters[EmitterInstanceIdx];
		if (!Emitter)
		{
			EmitterInstances.push_back(nullptr);
			continue;
		}

		Emitter->CalculateMaxActiveParticleCount(); //최대 몇개의 Particle가질지 계산
		FParticleEmitterInstance* Instance = CreateEmitterInstance(this, Emitter); //TypeDataModule보고 알맞은 Emitter만든다
		Instance->EmitterIndex = EmitterInstanceIdx; //Component의 몇번째 EmitterInstance인지 가르키는 Idx
		Instance->InitParameters(Emitter); //Instance의 ParticleSize를 계산한다.
		Instance->SetCurrentLODLevel(LODLevel); //Instance의 LODLevel설정한다
//		Instance->RebuildTemplateModuleList(); //InitParameters에서 이미 한번하는데 왜 굳이?
		EmitterInstances.push_back(Instance);
	}

	LODDistances = ParticleTemplate->GetLODDistances();
}

void UParticleSystemComponent::ResetParticles(bool bEmptyInstances)
{
	for (FParticleEmitterInstance* EmitterInstance : EmitterInstances)
	{
		delete EmitterInstance;
	}

	if (bEmptyInstances)
	{
		EmitterInstances.clear();
	}
	else
	{
		for (FParticleEmitterInstance*& EmitterInstance : EmitterInstances)
		{
			EmitterInstance = nullptr;
		}
	}
}

void UParticleSystemComponent::InitializeSystem()
{
	InitParticles();
}

void UParticleSystemComponent::QueueParticleCollisionEvent(const FParticleEventCollideData& EventData)
{
	if (MaxParticleCollisionEventsPerFrame >= 0
		&& static_cast<int32>(ParticleEventCollideDatas.size()) >= MaxParticleCollisionEventsPerFrame)
	{
		return;
	}

	ParticleEventCollideDatas.push_back(EventData);
}

void UParticleSystemComponent::DispatchParticleCollisionEvents()
{
	if (bDispatchingParticleCollisionEvents || ParticleEventCollideDatas.empty() || !OnParticleCollide.IsBound())
	{
		return;
	}

	bDispatchingParticleCollisionEvents = true;
	const TArray<FParticleEventCollideData> EventsToDispatch = ParticleEventCollideDatas;
	for (const FParticleEventCollideData& EventData : EventsToDispatch)
	{
		OnParticleCollide.Broadcast(this, EventData);
	}
	bDispatchingParticleCollisionEvents = false;
}

void UParticleSystemComponent::ClearParticleCollisionEvents()
{
	ParticleEventCollideDatas.clear();
}
