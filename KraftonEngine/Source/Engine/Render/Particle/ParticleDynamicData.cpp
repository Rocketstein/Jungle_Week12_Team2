#include "ParticleDynamicData.h"

#include <algorithm>

FParticleDataContainer::~FParticleDataContainer()
{
	Free();
}

FParticleDataContainer::FParticleDataContainer(FParticleDataContainer&& Other) noexcept
{
	*this = std::move(Other);
}

FParticleDataContainer& FParticleDataContainer::operator=(FParticleDataContainer&& Other) noexcept
{
	if (this != &Other)
	{
		Free();

		MemBlockSize = Other.MemBlockSize;
		ParticleDataNumBytes = Other.ParticleDataNumBytes;
		ParticleIndicesNumShorts = Other.ParticleIndicesNumShorts;
		ParticleData = Other.ParticleData;
		ParticleIndices = Other.ParticleIndices;

		Other.MemBlockSize = 0;
		Other.ParticleDataNumBytes = 0;
		Other.ParticleIndicesNumShorts = 0;
		Other.ParticleData = nullptr;
		Other.ParticleIndices = nullptr;
	}
	return *this;
}

void FParticleDataContainer::Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts)
{
	Free();

	ParticleDataNumBytes = AlignParticleDataSize(std::max(0, InParticleDataNumBytes), 16);
	ParticleIndicesNumShorts = std::max(0, InParticleIndicesNumShorts);
	MemBlockSize = ParticleDataNumBytes + ParticleIndicesNumShorts * static_cast<int32>(sizeof(uint16));

	if (MemBlockSize > 0)
	{
		ParticleData = static_cast<uint8*>(_aligned_malloc(MemBlockSize, 16));
		std::memset(ParticleData, 0, MemBlockSize);
		ParticleIndices = reinterpret_cast<uint16*>(ParticleData + ParticleDataNumBytes);
	}
}

void FParticleDataContainer::Free()
{
	if (ParticleData)
	{
		_aligned_free(ParticleData);
	}

	MemBlockSize = 0;
	ParticleDataNumBytes = 0;
	ParticleIndicesNumShorts = 0;
	ParticleData = nullptr;
	ParticleIndices = nullptr;
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
