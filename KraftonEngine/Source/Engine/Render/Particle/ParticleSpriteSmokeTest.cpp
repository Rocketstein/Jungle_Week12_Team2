#include "Render/Particle/ParticleSpriteSmokeTest.h"

#include "Component/ActorComponent.h"
#include "Component/ParticleSystemComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Materials/MaterialManager.h"
#include "Object/Object.h"
#include "Particle/ParticleHelper.h"
#include "Render/Particle/ParticleDynamicData.h"
#include "Render/Proxy/ParticleSystemSceneProxy.h"

#include <cmath>
#include <utility>

namespace
{
	constexpr float kTwoPi = 6.28318530718f;

	// Build one sprite emitter snapshot — N stationary, camera-facing quads
	// arranged on a unit circle of given radius around the component origin.
	// Returns a heap-allocated FDynamicSpriteEmitterData* whose ownership is
	// passed to FParticleSystemSceneProxy::UpdateDynamicData.
	FDynamicSpriteEmitterData* BuildRingEmitter(int32 N, UMaterial* Material, float Radius)
	{
		if (N <= 0) return nullptr;

		auto* Emitter = new FDynamicSpriteEmitterData();
		Emitter->EmitterIndex = 0;

		FDynamicSpriteEmitterReplayDataBase& Src = Emitter->Source;
		// eEmitterType is already DET_Sprite via the FDynamicSpriteEmitterReplayDataBase ctor.
		Src.MaterialInterface     = Material;
		Src.ActiveParticleCount   = N;
		Src.ParticleStride        = AlignParticleDataSize(static_cast<int32>(sizeof(FBaseParticle)), 16);
		Src.SortMode              = 0;
		Src.SubImages_Horizontal  = 1;
		Src.SubImages_Vertical    = 1;
		Src.BlendMode             = EBlendState::Additive;

		Src.DataContainer.Alloc(Src.ParticleStride * N, N);

		for (int32 i = 0; i < N; ++i)
		{
			FBaseParticle* P = reinterpret_cast<FBaseParticle*>(
				Src.DataContainer.ParticleData + Src.ParticleStride * i);

			const float t = (kTwoPi * static_cast<float>(i)) / static_cast<float>(N);
			P->Location           = FVector(Radius * std::cos(t), Radius * std::sin(t), 0.0f);
			P->OldLocation        = P->Location;
			P->Velocity           = FVector::ZeroVector;
			P->BaseVelocity       = FVector::ZeroVector;
			P->Size               = FVector(20.0f, 20.0f, 0.0f);
			P->BaseSize           = P->Size;
			P->Rotation           = 0.0f;
			P->BaseRotationRate   = 0.0f;
			P->RotationRate       = 0.0f;
			// Cycle hue around the ring so it is obvious the per-particle color
			// path is exercised, not just a single constant.
			P->Color              = FLinearColor(1.0f,
			                                     0.5f + 0.5f * std::cos(t),
			                                     0.5f + 0.5f * std::sin(t),
			                                     1.0f);
			P->BaseColor          = P->Color;
			P->RelativeTime       = 0.0f;
			P->OneOverMaxLifetime = 0.0f;
			P->Flags              = 0;

			Src.DataContainer.ParticleIndices[i] = static_cast<uint16>(i);
		}

		return Emitter;
	}
}

UMaterial* ParticleSpriteSmokeTest::GetDefaultMaterial()
{
	return FMaterialManager::Get().GetOrCreateMaterial(
		FString("Asset/Materials/Editor/DefaultParticleSprite.mat"));
}

int32 ParticleSpriteSmokeTest::InjectIntoWorld(UWorld* World, UMaterial* Material,
                                                int32 N, float Radius)
{
	if (!World || !Material) return 0;

	int32 Injected = 0;

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor) continue;

		for (UActorComponent* Comp : Actor->GetComponents())
		{
			UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(Comp);
			if (!PSC) continue;

			FParticleSystemSceneProxy* Proxy = PSC->GetSceneProxy();
			if (!Proxy) continue;

			TArray<FDynamicEmitterDataBase*> Data;
			if (FDynamicSpriteEmitterData* Emitter = BuildRingEmitter(N, Material, Radius))
			{
				Data.push_back(Emitter);
			}

			Proxy->UpdateDynamicData(std::move(Data));
			// FScene::UpdateDirtyProxies uses if/else-if on Mesh→Material, so marking
			// both still only fires UpdateMesh. ParticleSystemSceneProxy::UpdateMesh
			// reads EmitterDraws[i].{Type,Material} which only UpdateMaterial populates,
			// so we call them explicitly in the right order. Once the teammate's CPU
			// sim wires this up properly, this becomes their concern.
			Proxy->UpdateMaterial();
			Proxy->UpdateMesh();
			++Injected;
		}
	}

	return Injected;
}
