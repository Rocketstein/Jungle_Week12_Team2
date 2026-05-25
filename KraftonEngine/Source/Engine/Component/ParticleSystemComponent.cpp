#include "Component/ParticleSystemComponent.h"

#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleEmitterInstances.h"
#include "Particle/ParticleSystem.h"
#include "Render/Particle/ParticleDynamicData.h"
#include "Render/Proxy/ParticleSystemSceneProxy.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "GameFramework/World.h"
#include "Particle/ParticleLODContext.h"

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

	UParticleSystem* ParticleTemplate = Template.Get();
	if (!ParticleTemplate)
	{
		return;
	}

	if (EmitterInstances.empty())
	{
		InitializeSystem();
	}

	UWorld* World = GetWorld();
	if (World)
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

void UParticleSystemComponent::InitParticles()
{
	ResetParticles(true);

	UParticleSystem* ParticleTemplate = Template.Get();
	if (!ParticleTemplate)
	{
		return;
	}

	ParticleTemplate->NormalizeLODData();
	EmitterInstances.reserve(ParticleTemplate->Emitters.size());
	for (UParticleEmitter* Emitter : ParticleTemplate->Emitters)
	{
		if (!Emitter)
		{
			EmitterInstances.push_back(nullptr);
			continue;
		}

		FParticleEmitterInstance* Instance = new FParticleEmitterInstance(this);
		Instance->InitParameters(Emitter);
		Instance->Init();
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
