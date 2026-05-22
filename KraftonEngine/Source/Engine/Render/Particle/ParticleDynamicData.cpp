#include "ParticleDynamicData.h"
#include "Particle/ParticleHelper.h"

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

void FDynamicSpriteEmitterDataBase::SortSpriteParticles(int32 SortMode, const FVector& CameraOrigin, const FVector& CameraForward,
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
		float Depth = 0.0f;          // signed distance along the camera forward axis
		uint16 ParticleIndex = 0;
	};

	TArray<FParticleSortKey> SortKeys;
	SortKeys.reserve(Count);

	// Project each particle onto the camera forward axis. This gives true view-space
	// depth (a plane test), so two particles on the same depth plane sort equally
	// regardless of lateral offset.
	for (int32 i = 0; i < Count; ++i)
	{
		const uint16 ParticleIndex = InOutIndices[i];
		const uint8* ParticleBytes = ParticleData + static_cast<size_t>(ParticleIndex) * Stride;

		// FBaseParticle starts with Location; payload bytes follow the fixed header.
		const FVector& LocalLocation = *reinterpret_cast<const FVector*>(ParticleBytes + offsetof(FBaseParticle, Location));
		const FVector WorldLocation = LocalToWorld.TransformPositionWithW(LocalLocation);

		const float Depth = (WorldLocation - CameraOrigin).Dot(CameraForward);

		SortKeys.push_back({ Depth, ParticleIndex });
	}

	// Back-to-front: farthest depth first so alpha blending composites correctly.
	std::stable_sort(SortKeys.begin(), SortKeys.end(),
		[](const FParticleSortKey& A, const FParticleSortKey& B)
		{
			return A.Depth > B.Depth;
		});

	for (int32 i = 0; i < Count; ++i)
	{
		InOutIndices[i] = SortKeys[i].ParticleIndex;
	}
}
