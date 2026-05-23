#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Particle/ParticleHelper.h"
#include "Render/Types/VertexTypes.h"

// Render-side wrapper
struct FDynamicEmitterDataBase
{
	int32 EmitterIndex = -1;
	virtual ~FDynamicEmitterDataBase() = default;
	virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;
};

struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
	void SortSpriteParticles(int32 SortMode, const FVector& CameraOrigin, const FVector& CameraForward,
		const FMatrix& LocalToWorld,
		uint16* InOutIndices, int32 Count,
		const uint8* ParticleData, int32 Stride);

	virtual int32 GetDynamicVertexStride(/*ERHIFeatureLevel::Type InFeatureLevel*/) const = 0;
};

struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicSpriteEmitterReplayDataBase Source;
	const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	int32 GetDynamicVertexStride() const override { return sizeof(FParticleSpriteVertex); }
};

struct FDynamicMeshEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicMeshEmitterReplayData MeshSource;
	const FDynamicEmitterReplayDataBase& GetSource() const override { return MeshSource; }
	int32 GetDynamicVertexStride() const override { return sizeof(FMeshParticleInstanceVertex); }
};
