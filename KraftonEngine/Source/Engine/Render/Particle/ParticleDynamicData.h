#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Particle/ParticleHelper.h"
#include "Render/Types/VertexTypes.h"

struct FParticleDataContainer
{
	int32 MemBlockSize = 0;
	int32 ParticleDataNumBytes = 0;
	int32 ParticleIndicesNumShorts = 0;
	uint8* ParticleData = nullptr;		// this is also the memory block we allocated
	uint16* ParticleIndices = nullptr;	// not allocated, this is at the end of the memory block

	FParticleDataContainer() = default;
	~FParticleDataContainer();

	FParticleDataContainer(const FParticleDataContainer&) = delete;
	FParticleDataContainer& operator=(const FParticleDataContainer&) = delete;

	FParticleDataContainer(FParticleDataContainer&& Other) noexcept;
	FParticleDataContainer& operator=(FParticleDataContainer&& Other) noexcept;

	void Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts);
	void Free();
};

struct FDynamicEmitterReplayDataBase
{
	/** The type of emitter. */
	EDynamicEmitterType eEmitterType = DET_Unknown;

	/** The number of particles currently active in this emitter. */
	int32 ActiveParticleCount = 0;

	int32 ParticleStride = 0;
	FParticleDataContainer DataContainer;

	FVector Scale = FVector::OneVector;

	int32 SortMode = 0;

	virtual ~FDynamicEmitterReplayDataBase() = default;
};

struct FDynamicSpriteEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	UMaterial* MaterialInterface = nullptr;

	int32 SubImages_Horizontal = 1;
	int32 SubImages_Vertical = 1;
	uint8 ScreenAlignment = 0;
	EBlendState BlendMode = EBlendState::AlphaBlend;

	FDynamicSpriteEmitterReplayDataBase()
	{
		eEmitterType = DET_Sprite;
	}
};

struct FDynamicMeshEmitterReplayData : public FDynamicSpriteEmitterReplayDataBase
{
	UStaticMesh* StaticMesh = nullptr;

	FDynamicMeshEmitterReplayData()
	{
		eEmitterType = DET_Mesh;
	}
};


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
