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

float DistanceBetween(const FVector& A, const FVector& B)
{
	return (B - A).Length();
}
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

	std::stable_sort(BuildPoints.begin(), BuildPoints.end(),
		[](const FRibbonBuildPoint& A, const FRibbonBuildPoint& B)
		{
			if (A.TrailIndex != B.TrailIndex)
			{
				return A.TrailIndex < B.TrailIndex;
			}
			return A.Payload->SpawnSequence < B.Payload->SpawnSequence;
		});

	const int32 MaxTrailCount = std::max(1, RibbonModule->MaxTrailCount);
	const int32 MaxPointsPerTrail = std::max(2, RibbonModule->MaxParticleInTrailCount);

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
	for (int32 StartIndex = 0; StartIndex < static_cast<int32>(BuildPoints.size()) && ConsumedTrailCount < MaxTrailCount;)
	{
		const int32 TrailIndex = BuildPoints[StartIndex].TrailIndex;
		int32 EndIndex = StartIndex + 1;
		while (EndIndex < static_cast<int32>(BuildPoints.size()) && BuildPoints[EndIndex].TrailIndex == TrailIndex)
		{
			++EndIndex;
		}

		const int32 AvailablePointCount = EndIndex - StartIndex;
		if (AvailablePointCount >= 2)
		{
			const int32 CopyStartIndex = EndIndex - std::min(AvailablePointCount, MaxPointsPerTrail);
			FRibbonTrailData TrailData;
			TrailData.FirstPoint = static_cast<int32>(ReplayData->Points.size());

			float DistanceFromStart = 0.0f;
			FVector PreviousPosition = FVector::ZeroVector;
			for (int32 BuildIndex = CopyStartIndex; BuildIndex < EndIndex; ++BuildIndex)
			{
				const FBaseParticle* Particle = BuildPoints[BuildIndex].Particle;
				const FRibbonParticlePayload* Payload = BuildPoints[BuildIndex].Payload;
				if (!Particle || !Payload)
				{
					continue;
				}

				if (TrailData.PointCount > 0)
				{
					DistanceFromStart += DistanceBetween(PreviousPosition, Particle->Location);
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
				ReplayData->Points.push_back(PointData);

				PreviousPosition = Particle->Location;
				++TrailData.PointCount;
			}

			if (TrailData.PointCount >= 2)
			{
				ReplayData->Trails.push_back(TrailData);
				++ConsumedTrailCount;
			}
			else
			{
				ReplayData->Points.resize(TrailData.FirstPoint);
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
