#include "RibbonEmitterInstance.h"

#include "Materials/Material.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/TypeData/ParticleModuleTypeDataRibbon.h"

#include <algorithm>

namespace
{
struct FRibbonBuildPoint
{
	const FBaseParticle* Particle = nullptr;
	const FRibbonParticlePayload* Payload = nullptr;
	int32 TrailIndex = 0;
};
}

FRibbonEmitterInstance::FRibbonEmitterInstance(UParticleSystemComponent* InComponent)
	: FParticleEmitterInstance(InComponent)
{
}

void FRibbonEmitterInstance::PostSpawn(FBaseParticle* Particle, float Interp, float SpawnTime)
{
	FParticleEmitterInstance::PostSpawn(Particle, Interp, SpawnTime);

	if (!Particle || TypeDataOffset <= 0)
	{
		return;
	}

	auto* Payload = reinterpret_cast<FRibbonParticlePayload*>(reinterpret_cast<uint8*>(Particle) + TypeDataOffset);
	Payload->SpawnSequence = RibbonSpawnSequence++;
	Payload->TrailIndex = 0;
}

FDynamicEmitterReplayDataBase* FRibbonEmitterInstance::GetReplayData()
{
	if (!CurrentLODLevel
		|| !CurrentLODLevel->TypeDataModule
		|| !Component
		|| !ParticleIndices
		|| ActiveParticles < 2
		|| TypeDataOffset <= 0)
	{
		return nullptr;
	}

	UParticleModuleTypeDataRibbon* RibbonModule = Cast<UParticleModuleTypeDataRibbon>(CurrentLODLevel->TypeDataModule);
	if (!RibbonModule || !RibbonModule->bRenderGeometry)
	{
		return nullptr;
	}

	//Particle들을 모은다.(ControlPoint들을 모음)
	TArray<FRibbonBuildPoint> BuildPoints;
	BuildPoints.reserve(ActiveParticles);
	for (int32 ActiveIndex = 0; ActiveIndex < ActiveParticles; ++ActiveIndex)
	{
		const FBaseParticle* Particle = GetParticleDirect(ParticleIndices[ActiveIndex]);
		if (!Particle)
		{
			continue;
		}

		const FRibbonParticlePayload* Payload = reinterpret_cast<const FRibbonParticlePayload*>(
			reinterpret_cast<const uint8*>(Particle) + TypeDataOffset);

		FRibbonBuildPoint BuildPoint;
		BuildPoint.Particle = Particle;
		BuildPoint.Payload = Payload;
		BuildPoint.TrailIndex = Payload->TrailIndex;
		BuildPoints.push_back(BuildPoint);
	}

	if (BuildPoints.size() < 2)
	{
		return nullptr;
	}

	//모은 Particle들을 SpawnSequence에 따라 정렬함
	std::stable_sort(BuildPoints.begin(), BuildPoints.end(),
		[](const FRibbonBuildPoint& A, const FRibbonBuildPoint& B)
		{
			if (A.TrailIndex != B.TrailIndex)
			{
				return A.TrailIndex < B.TrailIndex;
			}
			return A.Payload->SpawnSequence < B.Payload->SpawnSequence;
		});

	//아래로부터 ReplayData(SnapShot)을 채우는 과정
	FDynamicRibbonEmitterReplayData* ReplayData = new FDynamicRibbonEmitterReplayData();
	ReplayData->ParticleStride = 0;
	ReplayData->Scale = FVector::OneVector;
	ReplayData->SheetsPerTrail = std::max(1, RibbonModule->SheetsPerTrail);
	ReplayData->MaxTessellationBetweenParticles = std::max(0, RibbonModule->MaxTessellationBetweenParticles);
	ReplayData->TilingDistance = std::max(0.0f, RibbonModule->TilingDistance);
	ReplayData->DistanceTessellationStepSize = std::max(0.0f, RibbonModule->DistanceTessellationStepSize);
	ReplayData->bRenderGeometry = RibbonModule->bRenderGeometry;
	ReplayData->bRenderSpawnPoints = RibbonModule->bRenderSpawnPoints;
	ReplayData->bRenderTangents = RibbonModule->bRenderTangents;
	ReplayData->bRenderTessellation = RibbonModule->bRenderTessellation;

	int32 ConsumedTrailCount = 0;
	for (int32 StartIndex = 0; StartIndex < BuildPoints.size() && ConsumedTrailCount < RibbonModule->MaxTrailCount ; /*Blank*/)
	{
		//[Trail 0] : p0 , [Trail ] : p1 , [Trail 0] : p2 ..몇개인지 찾는다.
		const int32 TrailIndex = BuildPoints[StartIndex].TrailIndex;
		int32 EndIndex = StartIndex + 1;
		while (EndIndex < BuildPoints.size() && BuildPoints[EndIndex].TrailIndex == TrailIndex)
		{
			++EndIndex;
		}

		const int32 AvailablePointCount = EndIndex - StartIndex;
		if (AvailablePointCount >= 2)
		{
			//최신 MaxPointsPerTrail개만쓴다
			//MaxPointPerTrail이 4개고 AvailablePointCount가 10이면 6 7 8 9만쓴다
			const int32 ClipedStartIndex = EndIndex - std::min(AvailablePointCount, RibbonModule->MaxParticleInTrailCount);
			FRibbonTrailSection TrailSection;
			TrailSection.FirstPoint = ReplayData->Points.size();//

			float DistanceFromStart = 0.0f;
			FVector PreviousPosition = FVector::ZeroVector;
			for (int32 BuildIndex = ClipedStartIndex; BuildIndex < EndIndex; ++BuildIndex)
			{
				const FBaseParticle* Particle = BuildPoints[BuildIndex].Particle;
				const FRibbonParticlePayload* Payload = BuildPoints[BuildIndex].Payload;
				if (!Particle || !Payload)
				{
					continue;
				}

				if (TrailSection.PointCount > 0)
				{
					DistanceFromStart += (PreviousPosition-Particle->Location).Length();
				}

				FRibbonPointData PointData;
				PointData.Position = Particle->Location;
				PointData.Color = FLinearColor(
					Particle->Color.R * RibbonModule->Color.X,
					Particle->Color.G * RibbonModule->Color.Y,
					Particle->Color.B * RibbonModule->Color.Z,
					std::clamp(Particle->Color.A * RibbonModule->Alpha, 0.0f, 1.0f));
				PointData.Width = std::max(0.0f, RibbonModule->Width * std::max(0.0f, Particle->Size.X));
				PointData.DistanceFromStart = DistanceFromStart;
				PointData.SpawnSequence = Payload->SpawnSequence;
				ReplayData->Points.push_back(PointData); //Point들 모음집에 넣는다

				PreviousPosition = Particle->Location;
				++TrailSection.PointCount;
			}

			if (TrailSection.PointCount >= 2)
			{
				ReplayData->Trails.push_back(TrailSection);//Point들을 어떻게 쓸지 section데이터에 넣는다
				++ConsumedTrailCount;
			}
			else
			{
				ReplayData->Points.resize(TrailSection.FirstPoint);
			}
		}

		StartIndex = EndIndex;
	}

	if (ReplayData->Trails.empty())
	{
		delete ReplayData;
		return nullptr;
	}

	ReplayData->ActiveParticleCount = static_cast<int32>(ReplayData->Points.size());

	if (CurrentLODLevel->RequiredModule)
	{
		ReplayData->SortMode = CurrentLODLevel->RequiredModule->SortMode;
		ReplayData->MaterialInterface = CurrentLODLevel->RequiredModule->Material;
		if (UMaterial* Material = CurrentLODLevel->RequiredModule->Material
			? CurrentLODLevel->RequiredModule->Material->GetMaterial()
			: nullptr)
		{
			ReplayData->BlendMode = Material->GetBlendState();
		}
	}

	return ReplayData;
}
