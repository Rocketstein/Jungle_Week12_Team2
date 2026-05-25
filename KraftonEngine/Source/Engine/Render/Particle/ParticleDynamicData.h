#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Particle/ParticleHelper.h"
#include "Particle/ParticleModule.h"
#include "Particle/TypeData/ParticleModuleTypeDataBeam2.h"
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

	EParticleSortMode SortMode = EParticleSortMode::PSORTMODE_None;

	// Cross-Emitter sorting priority
	uint16 EmitterSortPriority = 0;

	virtual ~FDynamicEmitterReplayDataBase() = default;
};

struct FDynamicRenderableEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	UMaterialInterface* MaterialInterface = nullptr;

	EBlendState BlendMode = EBlendState::AlphaBlend;
};

struct FDynamicSpriteEmitterReplayData : public FDynamicRenderableEmitterReplayDataBase
{
	int32 SubImages_Horizontal = 1;
	int32 SubImages_Vertical = 1;
	uint8 ScreenAlignment = 0;
	FVector EmitterOrigin = FVector::ZeroVector;
	uint32 AlphaSource = 0;
	float AlphaThreshold = 0.0f;
	float AlphaPower = 1.0f;
	float ColorIntensity = 1.0f;

	FDynamicSpriteEmitterReplayData()
	{
		eEmitterType = DET_Sprite;
	}
};

struct FDynamicMeshEmitterReplayData : public FDynamicRenderableEmitterReplayDataBase
{
	uint8 LODLevel = 0;
	UStaticMesh* StaticMesh = nullptr;

	FDynamicMeshEmitterReplayData()
	{
		eEmitterType = DET_Mesh;
	}
};

struct FDynamicBeamEmitterReplayData : public FDynamicRenderableEmitterReplayDataBase
{
	FVector Source;
	FVector Target;
	FVector Color;
	float Alpha = 1.0f;
	float Width = 8.0f;

	int32 InterpolationPoints = 8;
	int32 Sheets = 1;

	int32 TextureTile = 1;
	float TExtureTileDistance = 0.0f;

	EBeamTaperMethod TaperMethod = PEBTM_None;
	float TaperFactor = 1.0f;
	float TaperScale = 1.0f;

	bool bRenderGeometry = true;
	bool bRenderDirectLine = false;
	bool bRenderLines = false;
	bool bRenderTessellation = false;

	FDynamicBeamEmitterReplayData()
	{
		eEmitterType =DET_Beam2;
	}
};

struct FDynamicRibbonEmitterReplayData : public FDynamicRenderableEmitterReplayDataBase
{
	
};


// Render-side wrapper
struct FDynamicEmitterDataBase
{
	int32 EmitterIndex = -1;
	virtual ~FDynamicEmitterDataBase() = default;
	virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;
	virtual int32 GetDynamicVertexStride(/*ERHIFeatureLevel::Type InFeatureLevel*/) const = 0;
	virtual void SortParticles(EParticleSortMode SortMode, const FVector& CameraOrigin, const FVector& CameraForward,
							const FMatrix& LocalToWorld,
							uint16* InOutIndices, int32 Count,
							const uint8* ParticleData, int32 Stride);
};

struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
	void SortParticles(EParticleSortMode SortMode, const FVector& CameraOrigin, const FVector& CameraForward,
		const FMatrix& LocalToWorld,
		uint16* InOutIndices, int32 Count,
		const uint8* ParticleData, int32 Stride) override;
};

struct FDynamicMeshEmitterDataBase : public FDynamicEmitterDataBase
{
	void SortParticles(EParticleSortMode SortMode, const FVector& CameraOrigin, const FVector& CameraForward,
		const FMatrix& LocalToWorld,
		uint16* InOutIndices, int32 Count,
		const uint8* ParticleData, int32 Stride) override;
};

struct FDynamicBeamEmitterDataBase : public FDynamicEmitterDataBase
{

};

struct FDynamicRibbonEmitterDataBase : public FDynamicEmitterDataBase
{

};

struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicSpriteEmitterReplayData Source;
	const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	int32 GetDynamicVertexStride() const override { return sizeof(FParticleSpriteVertex); }
};

struct FDynamicMeshEmitterData : public FDynamicMeshEmitterDataBase
{
	FDynamicMeshEmitterReplayData MeshSource;
	const FDynamicEmitterReplayDataBase& GetSource() const override { return MeshSource; }
	int32 GetDynamicVertexStride() const override { return sizeof(FMeshParticleInstanceVertex); }
};

struct FDynamicBeamEmitterData : public FDynamicBeamEmitterDataBase
{
	FDynamicBeamEmitterReplayData BeamSource;
	const FDynamicEmitterReplayDataBase& GetSource() const override { return BeamSource; }
};

struct FDynamicRibbonEmitterData : public FDynamicRibbonEmitterDataBase 
{
	FDynamicRibbonEmitterReplayData RibbonSource;
	const FDynamicEmitterReplayDataBase& GetSource() const override { return RibbonSource; }
};