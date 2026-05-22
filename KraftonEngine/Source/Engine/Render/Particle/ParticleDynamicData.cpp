#include "ParticleDynamicData.h"

#include <algorithm>

void FParticleDataContainer::Alloc(int32 DataBytes, int32 IndexCount)
{
	Free();
	ParticleDataNumBytes = DataBytes;
	ParticleIndicesNumShorts = IndexCount;
	if (DataBytes > 0)
	{
		ParticleData = new uint8[DataBytes + IndexCount * sizeof(uint16)];
		ParticleIndices = reinterpret_cast<uint16*>(ParticleData + DataBytes);
	}
}

void FParticleDataContainer::Free()
{
	delete[] ParticleData;
	ParticleData = nullptr;
	ParticleIndices = nullptr;
	ParticleDataNumBytes = 0;
	ParticleIndicesNumShorts = 0;
}

void FDynamicSpriteEmitterDataBase::SortSpriteParticles(int32 SortMode, const FVector& CameraOrigin,
	const FMatrix& LocalToWorld,
	uint16* InOutIndices, int32 Count,
	const uint8* ParticleData, int32 Stride)
{
	if (SortMode == 0 || Count <= 1 || !InOutIndices || !ParticleData || Stride < sizeof(FVector))
	{
		return;
	}

	struct FParticleSortKey
	{
		float DistanceSquared = 0.0f;
		uint16 ParticleIndex = 0;
	};

	TArray<FParticleSortKey> SortKeys;
	SortKeys.reserve(Count);

	for (int32 i = 0; i < Count; ++i)
	{
		const uint16 ParticleIndex = InOutIndices[i];
		const uint8* ParticleBytes = ParticleData + static_cast<size_t>(ParticleIndex) * Stride;

		// FBaseParticle starts with Location; payload bytes follow the fixed header.
		const FVector& LocalLocation = *reinterpret_cast<const FVector*>(ParticleBytes);
		const FVector WorldLocation = LocalToWorld.TransformPositionWithW(LocalLocation);

		SortKeys.push_back({
			FVector::DistSquared(WorldLocation, CameraOrigin),
			ParticleIndex
		});
	}

	std::stable_sort(SortKeys.begin(), SortKeys.end(),
		[](const FParticleSortKey& A, const FParticleSortKey& B)
		{
			return A.DistanceSquared > B.DistanceSquared;
		});

	for (int32 i = 0; i < Count; ++i)
	{
		InOutIndices[i] = SortKeys[i].ParticleIndex;
	}

}
