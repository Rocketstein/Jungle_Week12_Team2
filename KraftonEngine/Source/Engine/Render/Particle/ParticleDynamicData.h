#pragma once
#include "Math/Matrix.h"
#include "Core/CoreTypes.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/VertexTypes.h"

class UMaterial;
struct FDynamicEmitterReplayDataBase;

enum class EDynamicEmitterType : uint8
{
	None,
	Sprite,
	Mesh,
	Beam,
	Ribbon,
};


// Raw Byte block from CPU
struct FParticleDataContainer
{
	int32 ParticleDataNumBytes = 0;
	int32 ParticleIndicesNumShorts = 0;
	uint8* ParticleData = nullptr; // owned, single allocation
	uint16* ParticleIndices = nullptr; // points inside same block
	void Alloc(int32 DataBytes, int32 IndexCount);
	void Free();
};


struct FDynamicEmitterReplayDataBase
{
	EDynamicEmitterType eEmitterType = EDynamicEmitterType::None;
	int32 ActiveParticleCount = 0;
	int32 ParticleStride = 0;     // sizeof(FBaseParticle) + payload sum
	FParticleDataContainer DataContainer;
	FVector Scale = FVector(1, 1, 1);
	int32 SortMode = 0;                // 0 = none, 1 = view distance back-to-front
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
	void SortSpriteParticles(int32 SortMode, const FVector& CameraOrigin,
		const FMatrix& LocalToWorld,
		uint16* InOutIndices, int32 Count,
		const uint8* ParticleData, int32 Stride);

	virtual int32 GetDynamicVertexStride(/*ERHIFeatureLevel::Type InFeatureLevel*/) const = 0;
};


struct FDynamicSpriteEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	UMaterial* MaterialInterface = nullptr;
	// RequiredModule snapshot (screen alignment, blend, atlas dims):
	int32 SubImages_Horizontal = 1;
	int32 SubImages_Vertical = 1;
	uint8 ScreenAlignment = 0;    // PSA_Square/PSA_Velocity/PSA_FacingCameraPosition
	EBlendState BlendMode = EBlendState::AlphaBlend;

	// Offsets into FBaseParticle payload (filled by CacheEmitterModuleInfo on CPU)
	int32 SubUVDataOffset = -1;
	int32 DynamicParameterDataOffset = -1;
};


struct FDynamicMeshEmitterReplayData : public FDynamicSpriteEmitterReplayDataBase
{
	class UStaticMesh* StaticMesh = nullptr;
	int32 MeshAlignment = 0;
};


struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicSpriteEmitterReplayDataBase Source;
	const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	int32 GetDynamicVertexStride() const override { return sizeof(FParticleSpriteVertex); }
};


struct FDynamicMeshEmitterData : public FDynamicSpriteEmitterData
{
	FDynamicMeshEmitterReplayData MeshSource;
	int32 GetDynamicVertexStride() const override { return sizeof(FMeshParticleInstanceVertex); }
};