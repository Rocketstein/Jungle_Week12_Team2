#include "Component/ParticleSystemComponent.h"

#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleEmitterInstances.h"
#include "Particle/ParticleSystem.h"
#include "Render/Particle/ParticleDynamicData.h"
#include "Render/Proxy/ParticleSystemSceneProxy.h"

#include <utility>

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
}

UParticleSystemComponent::~UParticleSystemComponent()
{
	ResetParticles(true);
}

UFXSystemAsset* UParticleSystemComponent::GetFXSystemAsset() const
{
	return Template;
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* NewTemplate)
{
	if (Template == NewTemplate)
	{
		return;
	}

	ResetParticles(true);
	Template = NewTemplate;
	InitializeSystem();
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	return new FParticleSystemSceneProxy(this);
}

FParticleSystemSceneProxy* UParticleSystemComponent::GetSceneProxy() const
{
	return static_cast<FParticleSystemSceneProxy*>(SceneProxy);
}

void UParticleSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Template)
	{
		return;
	}

	if (EmitterInstances.empty())
	{
		InitializeSystem();
	}

	for (FParticleEmitterInstance* EmitterInstance : EmitterInstances)
	{
		if (EmitterInstance)
		{
			EmitterInstance->Tick(DeltaTime, false);
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

void UParticleSystemComponent::InitParticles()
{
	ResetParticles(true);

	if (!Template)
	{
		return;
	}

	EmitterInstances.reserve(Template->Emitters.size());
	for (UParticleEmitter* Emitter : Template->Emitters)
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
