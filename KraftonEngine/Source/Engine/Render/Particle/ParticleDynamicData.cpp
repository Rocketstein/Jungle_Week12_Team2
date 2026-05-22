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