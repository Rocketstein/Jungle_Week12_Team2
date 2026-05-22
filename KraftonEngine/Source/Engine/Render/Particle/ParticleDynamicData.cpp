#include "ParticleDynamicData.h"

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
	if (SortMode == 0 || Count == 1)
	{
		// No sorting needed
		for (int32 i = 0; i < Count; ++i)
			InOutIndices[i] = static_cast<uint16>(i);
		return;
	}

	// Calculate distance from camera for each particle

}