#include "Render/Particle/ParticleBeamSmokeTest.h"

#include "Component/ActorComponent.h"
#include "Component/ParticleSystemComponent.h"
#include "Component/SceneComponent.h"
#include "Core/Log.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Materials/Material.h"
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

	// Build one beam emitter snapshot — a single beam from WorldCenter outward to
	// a point on a circle of given radius at angle `t`. Each FDynamicBeamEmitterData
	// represents ONE beam (Source → Target); the caller composes N of these into
	// a starburst.
	//
	// Particle positions are world-space because FParticleSystemSceneProxy uses an
	// identity model matrix, so the caller must bake the owning component's world
	// location into both Source and Target.
	//
	// Per-beam variation:
	//   * Hue cycles around the ring so per-emitter color path is visibly exercised.
	//   * Even-indexed beams use PEBTM_Full taper so the beam-packer's taper math
	//     is hit on at least some beams without obscuring straight-beam debugging.
	//
	// Ownership of the returned ptr is transferred to UpdateDynamicData.
	FDynamicBeamEmitterData* BuildRadialBeamEmitter(int32 EmitterIndex, int32 N, UMaterial* Material,
		float Radius, const FVector& WorldCenter)
	{
		if (N <= 0) return nullptr;

		auto* Emitter = new FDynamicBeamEmitterData();
		Emitter->EmitterIndex = EmitterIndex;

		FDynamicBeamEmitterReplayData& Src = Emitter->BeamSource;
		// eEmitterType is already DET_Beam2 via the FDynamicBeamEmitterReplayData ctor.
		Src.MaterialInterface     = Material;
		Src.ParticleStride        = AlignParticleDataSize(static_cast<int32>(sizeof(FBaseParticle)), 16);
		Src.SortMode              = EParticleSortMode::PSORTMODE_None;
		Src.BlendMode             = Material ? Material->GetBlendState() : EBlendState::Additive;

		const float t = (kTwoPi * static_cast<float>(EmitterIndex)) / static_cast<float>(N);
		Src.Source = WorldCenter;
		Src.Target = WorldCenter + FVector(Radius * std::cos(t), Radius * std::sin(t), 0.0f);

		// Hue cycle per emitter (1 → green/blue → 1 around the ring).
		Src.Color = FVector(1.0f,
		                    0.5f + 0.5f * std::cos(t),
		                    0.5f + 0.5f * std::sin(t));
		Src.Alpha = 0.8f;
		Src.Width = 4.0f;

		Src.InterpolationPoints  = 8;
		Src.Sheets               = 1;
		Src.LogicalBeamCount     = 1;
		Src.ActiveParticleCount  = Src.LogicalBeamCount * Src.Sheets;
		Src.TextureTile          = 20;
		Src.TextureTileDistance  = 0.0f;

		// Exercise the taper path on half the beams; leave the rest straight so a
		// regression in width math is easy to spot.
		const bool bTaper = (EmitterIndex & 1) == 0;
		Src.TaperMethod = bTaper ? PEBTM_Full : PEBTM_None;
		Src.TaperFactor = 1.0f;
		Src.TaperScale  = 1.0f;

		Src.bRenderGeometry     = true;
		Src.bRenderDirectLine   = false;
		Src.bRenderLines        = false;
		Src.bRenderTessellation = false;

		// FBaseParticle payload is still allocated for consistency with the other
		// emitter types and any code path that reads ParticleStride / DataContainer.
		// The beam packer itself reads Source/Target/Color/Alpha/Width directly,
		// not the per-particle bytes — these are inert but kept non-null.
		Src.DataContainer.Alloc(Src.ParticleStride, 1);
		FBaseParticle* P = reinterpret_cast<FBaseParticle*>(Src.DataContainer.ParticleData);
		P->Location           = Src.Source;
		P->OldLocation        = P->Location;
		P->Velocity           = FVector::ZeroVector;
		P->BaseVelocity       = FVector::ZeroVector;
		P->Size               = FVector(Src.Width, Src.Width, 0.0f);
		P->BaseSize           = P->Size;
		P->Rotation           = 0.0f;
		P->BaseRotationRate   = 0.0f;
		P->RotationRate       = 0.0f;
		P->Color              = FLinearColor(Src.Color.X, Src.Color.Y, Src.Color.Z, Src.Alpha);
		P->BaseColor          = P->Color;
		P->RelativeTime       = 0.0f;
		P->OneOverMaxLifetime = 0.0f;
		P->Flags              = 0;
		Src.DataContainer.ParticleIndices[0] = 0;

		return Emitter;
	}
}

UMaterial* ParticleBeamSmokeTest::GetDefaultMaterial()
{
	return FMaterialManager::Get().GetOrCreateMaterial(
		FString("Asset/Materials/Editor/DefaultParticleBeam.mat"));
}

int32 ParticleBeamSmokeTest::InjectIntoWorld(UWorld* World, UMaterial* Material,
                                              int32 N, float Radius)
{
	if (!World)
	{
		UE_LOG("[ParticleBeamSmokeTest] FAIL: World is null");
		return 0;
	}
	if (!Material)
	{
		UE_LOG("[ParticleBeamSmokeTest] FAIL: Material is null. Check Asset/Materials/Editor/DefaultParticleBeam.mat exists and parses.");
		return 0;
	}

	int32 ActorsScanned = 0;
	int32 PSCsFound = 0;
	int32 PSCsWithProxy = 0;
	int32 Injected = 0;

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor) continue;
		++ActorsScanned;

		for (UActorComponent* Comp : Actor->GetComponents())
		{
			UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(Comp);
			if (!PSC) continue;
			++PSCsFound;

			FParticleSystemSceneProxy* Proxy = PSC->GetSceneProxy();
			if (!Proxy)
			{
				UE_LOG("[ParticleBeamSmokeTest] PSC on '%s' has no SceneProxy (component not registered with FScene?)",
					Actor->GetFName().ToString().c_str());
				continue;
			}
			++PSCsWithProxy;

			// Same world-space convention as sprites / meshes — the proxy uses
			// identity PerObjectConstants, so we bake the component's world
			// location into Source and Target at snapshot time.
			const FVector RingCenter = PSC->GetWorldLocation();
			UE_LOG("[ParticleBeamSmokeTest]   '%s' PSC at world (%.1f, %.1f, %.1f); starburst R=%.1f N=%d",
				Actor->GetFName().ToString().c_str(),
				RingCenter.X, RingCenter.Y, RingCenter.Z, Radius, N);

			TArray<FDynamicEmitterDataBase*> Data;
			Data.reserve(N);
			for (int32 i = 0; i < N; ++i)
			{
				if (FDynamicBeamEmitterData* Emitter = BuildRadialBeamEmitter(i, N, Material, Radius, RingCenter))
				{
					Data.push_back(Emitter);
				}
			}

			Proxy->UpdateDynamicData(std::move(Data));
			Proxy->UpdateMaterial();
			Proxy->UpdateMesh();
			++Injected;
		}
	}

	const ID3D11ShaderResourceView* const* SRVs = Material->GetCachedSRVs();
	UE_LOG("[ParticleBeamSmokeTest] World=%p WorldType=%d  ActorsScanned=%d PSCsFound=%d WithProxy=%d Injected=%d  Material=%p Pass=%d Shader=%p t0SRV=%p  N=%d Radius=%.1f",
		World,
		static_cast<int32>(World->GetWorldType()),
		ActorsScanned, PSCsFound, PSCsWithProxy, Injected,
		Material,
		static_cast<int32>(Material->GetRenderPass()),
		Material->GetShader(),
		SRVs ? SRVs[0] : nullptr,
		N, Radius);

	return Injected;
}
