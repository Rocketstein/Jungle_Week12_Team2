#include "ParticleDynamicData.h"

#include <algorithm>

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
		float Depth = 0.0f;
		uint16 ParticleIndex = 0;
	};

	TArray<FParticleSortKey> SortKeys;
	SortKeys.reserve(Count);   

	for (int32 i = 0; i < Count; ++i)
	{
		const uint16 ParticleIndex = InOutIndices[i];
		const uint8* ParticleBytes = ParticleData + static_cast<size_t>(ParticleIndex) * Stride;
		const FBaseParticle& Particle = *reinterpret_cast<const FBaseParticle*>(ParticleBytes);
		const FVector WorldLocation = LocalToWorld.TransformPositionWithW(Particle.Location);
		const float Depth = (WorldLocation - CameraOrigin).Dot(CameraForward);

		SortKeys.push_back({ Depth, ParticleIndex });
	}

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
