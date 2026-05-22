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
		const FPrimitiveDrawOptions&, FDrawCommand&) const override;

	void PackSprites(const FFrameContext& Frame, ID3D11DeviceContext* InDeviceContext);
	void PackMeshes(const FFrameContext& Frame);

	const char* GetVertexShaderEntryName() const override { return "VS_ParticleSprite"; }

private:
	// Per-emitter draw range inside the shared dynamic VB/IB
	struct FEmitterDraw
	{
		EDynamicEmitterType Type;
		UMaterial* Material;
		uint32 FirstIndex;
		uint32 IndexCount;
		// mesh path: per-emitter instance buffer + base static-mesh VB/IB
		FDynamicVertexBuffer InstanceVB;
		FMeshBuffer*		 MeshGeom		= nullptr;   // borrowed from UStaticMesh
		uint32               InstanceCount  = 0;
	};

	void PackSpriteEmitter(const FFrameContext& Frame, FDynamicSpriteEmitterData& Emitter,
		TArray<FParticleSpriteVertex>& OutVerts,
		TArray<uint32>& OutIndices, uint32& IndexCursor);
	void PackMeshEmitter(const FFrameContext& Frame, FDynamicMeshEmitterData& Emitter);

	TArray<FDynamicEmitterDataBase*> DynamicData;   // owned, freed on next UpdateDynamicData
	TArray<FEmitterDraw>             EmitterDraws;

	// Sprite path — shared across all sprite emitters this proxy owns
	mutable FDynamicVertexBuffer SpriteVB;
	mutable FDynamicIndexBuffer  SpriteIB;

	// In Cascade particles, the Emitter Instance(specifically FParticleEmitterInstance and its associated FParticleSystemSceneProxy)
	// owns and manages the uniform buffers(constant buffers), not the individual particles.
	//FConstantBuffer      ParticleParamCB;   // b2: per-emitter (alignment mode, sub-uv dims)
};