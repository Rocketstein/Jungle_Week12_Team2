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
	FVector Source = FVector::ZeroVector;	// World-space start point of the beam
	FVector Target = FVector::ZeroVector;	// World-space end point of the beam
	FVector SourceTangent = FVector::ZeroVector;	// World-space source tangent for curved beams
	FVector TargetTangent = FVector::ZeroVector;	// World-space target tangent for curved beams
	bool bUseTangents = false;				// Hermite path toggle
	FVector Color = FVector::OneVector;		// Base RGB tint applied to the beam
	float Alpha = 1.0f;						// Opacity multiplier for the beam
	float Width = 8.0f;						// Beam thickness in world units

	int32 InterpolationPoints = 8;			// Number of subdivisions along the beam for curve interpolation
	int32 Sheets = 1;						// Number of crossed quad sheets used to render the beam
	int32 MaxBeamCount = 1;					// Max beam instances requested by type data
	float Speed = 0.0f;						// Beam interpolation speed requested by type data
	int32 UpVectorStepSize = 0;				// UE-compatible up-vector step hint

	int32 TextureTile = 1;					// Number of times the texture tiles along the beam length
	float TextureTileDistance = 0.0f;		// Distance per texture tile (overrides TextureTile when non-zero)
	float NoiseAmplitude = 0.0f;			// World-space offset applied along generated beam noise axes
	float NoiseFrequency = 0.0f;			// Number of noise waves along the beam
	float NoisePhase = 0.0f;				// Time-driven phase offset for animated noise
	float NoiseSeed = 0.0f;				// Stable offset so beams can vary without changing their endpoints
	FVector NoiseRangeMin = FVector::ZeroVector;	// Low-frequency uniform noise range minimum
	FVector NoiseRangeMax = FVector::ZeroVector;	// Low-frequency uniform noise range maximum

	EBeamTaperMethod TaperMethod = PEBTM_None;	// Width taper mode along the beam (none/start/end/full)
	float TaperFactor = 1.0f;				// Strength of the taper effect
	float TaperScale = 1.0f;				// Additional scale applied on top of the taper

	bool bRenderGeometry = true;			// Whether to render the solid beam geometry
	bool bRenderDirectLine = false;			// Whether to render a debug straight line from Source to Target
	bool bRenderLines = false;				// Whether to render debug lines along the interpolated path
	bool bRenderTessellation = false;		// Whether to render debug tessellation wireframe
	FName BranchParentName;					// Parent emitter requested by branch beams
	TArray<FBeamTargetData> TargetData;		// Imported branch target metadata

	FDynamicBeamEmitterReplayData()
	{
		eEmitterType = DET_Beam2;
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
	void SortParticles(EParticleSortMode SortMode, const FVector& CameraOrigin, const FVector& CameraForward,
		const FMatrix& LocalToWorld,
		uint16* InOutIndices, int32 Count,
		const uint8* ParticleData, int32 Stride) override;
};

struct FDynamicRibbonEmitterDataBase : public FDynamicEmitterDataBase
{
	void SortParticles(EParticleSortMode SortMode, const FVector& CameraOrigin, const FVector& CameraForward,
		const FMatrix& LocalToWorld,
		uint16* InOutIndices, int32 Count,
		const uint8* ParticleData, int32 Stride) override;
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
	int32 GetDynamicVertexStride() const override { return sizeof(FBeamParticleInstanceVertex); }
};

struct FDynamicRibbonEmitterData : public FDynamicRibbonEmitterDataBase 
{
	FDynamicRibbonEmitterReplayData RibbonSource;
	const FDynamicEmitterReplayDataBase& GetSource() const override { return RibbonSource; }
	int32 GetDynamicVertexStride() const override { return sizeof(FRibbonParticleInstanceVertex); }
};
