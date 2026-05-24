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

	struct FSpriteParticlePacker
	{
		void ResetFrame() { PackedVertices.clear(); }
		void PackEmitter(const FFrameContext& Frame, FDynamicSpriteEmitterData& Emitter, uint32& IndexCursor);
		bool HasPackedSprites() const { return !PackedVertices.empty() && !IndexPattern.empty(); }
		void MarkGpuBuffersDirty() const { bGpuBuffersDirty = true; }
		bool PrepareDrawBuffer(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, FDrawCommandBuffer& Out) const;

	private:
		void EnsureIndexPattern(uint32 RequiredParticleCount);

		TArray<FParticleSpriteVertex> PackedVertices;
		TArray<uint32>                IndexPattern;
		uint32                        IndexPatternParticleCapacity = 0;

		mutable FDynamicVertexBuffer VertexBuffer;
		mutable FDynamicIndexBuffer  IndexBuffer;
		mutable bool bGpuBuffersDirty = true;
		mutable bool bIndexBufferDirty = true;
	};

	struct FMeshParticlePacker
	{
		void ResetFrame(TArray<FEmitterDraw>& EmitterDraws);
		void PackEmitter(const FFrameContext& Frame, FDynamicMeshEmitterData& Emitter, FEmitterDraw& Draw);
		bool HasPackedInstances(const TArray<FEmitterDraw>& EmitterDraws) const;
	};

	// Delegates type-specific CPU packing and refreshes per-emitter
	// (FirstIndex, IndexCount) for DrawCommandBuilder.
	void PackParticles(const FFrameContext& Frame);

	void UpdateCB(FEmitterDraw& EmitterDraw, const FDynamicEmitterReplayDataBase& Source);

private:
	TArray<FDynamicEmitterDataBase*> DynamicData;   // owned, freed on next UpdateDynamicData
	TArray<FEmitterDraw>             EmitterDraws;

	FMatrix ComponentToWorld = FMatrix::Identity;
	FSpriteParticlePacker SpritePacker;
	FMeshParticlePacker MeshPacker;

	bool bInstancePacked = false;
};
