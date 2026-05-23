#pragma once
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Resource/Buffer.h"
#include "Render/Particle/ParticleDynamicData.h"

class UParticleSystemComponent;

class FParticleSystemSceneProxy : public FPrimitiveSceneProxy
{
public:
	FParticleSystemSceneProxy(UParticleSystemComponent* InComponent);
	~FParticleSystemSceneProxy() override;

	// Called by Component each frame after CPU sim. Proxy takes ownership.
	void UpdateDynamicData(TArray<FDynamicEmitterDataBase*>&& NewData);

	// Required overrides:
	void UpdateTransform()   override;  // Local-space proxy: just CachedWorldPos + identity model
	void UpdateMaterial()    override;  // Pull material from each emitter Source
	void UpdateVisibility()  override;
	void UpdateMesh()        override;  // Reset SectionDraws; sized by emitter count
	void UpdatePerViewport(const FFrameContext& Frame) override;

	bool PrepareDrawBuffer(ID3D11Device*, ID3D11DeviceContext*, FDrawCommandBuffer&) const override;
	bool PrepareDrawCommandBindings(ID3D11Device*, ID3D11DeviceContext*,
		const FPrimitiveDrawOptions&, FDrawCommand&, int32 SectionIndex) const override;

	//const char* GetVertexShaderEntryName() const override { return "VS_ParticleSprite"; }

private:
	// Per-emitter draw range inside the shared dynamic VB/IB.
	// EmitterDraws[i] is parallel to SectionDraws[i] and DynamicData[i]
	struct FEmitterDraw
	{
		int32  EmitterIndex                 = -1;
		EDynamicEmitterType Type           = DET_Unknown;
		UMaterial* Material                = nullptr;
		uint32 FirstIndex                  = 0;
		uint32 IndexCount                  = 0;

		// mesh path
		mutable FDynamicVertexBuffer InstanceVB;
		FMeshBuffer*		 MeshGeom		= nullptr;   // borrowed from UStaticMesh
		uint32               InstanceCount  = 0;
		TArray<FMeshParticleInstanceVertex> PackedInstances;
		mutable bool bInstanceVBDirty = true;

		// CBuffer, owned by the emitter
		mutable FConstantBuffer ParticleParamCB;
		mutable bool bParticleParamCBDirty = true;
		FParticleParamConstants ParticleParams;
	};

	// Fills the SpriteVert/IndexScratch member arrays and refreshes per-emitter
	// (FirstIndex, IndexCount). Sets bGpuBuffersDirty for PrepareDrawBuffer to consume.
	void PackParticles(const FFrameContext& Frame);
	void PackSpriteEmitter(const FFrameContext& Frame, FDynamicSpriteEmitterData& Emitter, uint32& IndexCursor);
	void PackMeshEmitter(const FFrameContext& Frame, FDynamicMeshEmitterData& Emitter, uint32 SectionIndex);

	void UpdateCB(FEmitterDraw& EmitterDraw, const FDynamicSpriteEmitterReplayDataBase& Source);
	void EnsureSpriteIndexPattern(uint32 RequiredParticleCount);

	TArray<FDynamicEmitterDataBase*> DynamicData;   // owned, freed on next UpdateDynamicData
	TArray<FEmitterDraw>             EmitterDraws;

	FMatrix ComponentToWorld = FMatrix::Identity;
	TArray<FParticleSpriteVertex> PackedSpriteVertices;
	TArray<uint32>                SpriteIndexPattern;
	uint32                        SpriteIndexPatternParticleCapacity = 0;

	// Sprite path: shared across all sprite emitters this proxy owns.
	mutable FDynamicVertexBuffer SpriteVB;
	mutable FDynamicIndexBuffer  SpriteIB;

	// Signals PrepareDrawBuffer that scratch arrays must be re-uploaded.
	mutable bool bGpuBuffersDirty = true;
	mutable bool bSpriteIBDirty = true;

	// In Cascade particles, the Emitter Instance(specifically FParticleEmitterInstance and its associated FParticleSystemSceneProxy)
	// owns and manages the uniform buffers(constant buffers), not the individual particles.
	//FConstantBuffer      ParticleParamCB;   // b2: per-emitter (alignment mode, sub-uv dims)
};
