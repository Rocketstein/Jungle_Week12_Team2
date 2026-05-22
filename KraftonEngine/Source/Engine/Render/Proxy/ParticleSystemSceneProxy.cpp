#include "ParticleSystemSceneProxy.h"
#include "Particle/ParticleHelper.h"
#include "Render/Types/FrameContext.h"

FParticleSystemSceneProxy::~FParticleSystemSceneProxy()
{
	for (FDynamicEmitterDataBase* P : DynamicData)
	{
		delete P;
	}
	DynamicData.clear();
}

void FParticleSystemSceneProxy::UpdateDynamicData(TArray<FDynamicEmitterDataBase*>&& NewData)
{
	for (FDynamicEmitterDataBase* Old : DynamicData)
	{
		delete Old;
	}
	DynamicData = std::move(NewData);
}

void FParticleSystemSceneProxy::UpdateTransform()
{

}

void FParticleSystemSceneProxy::UpdateMaterial()
{
	EmitterDraws.resize(DynamicData.size());
	for (uint32 i = 0; i < DynamicData.size(); i++)
	{
		const FDynamicSpriteEmitterReplayDataBase& Source = static_cast<FDynamicSpriteEmitterReplayDataBase>(DynamicData[i]->GetSource());
		EmitterDraws[i].Material = Source.MaterialInterface;
		EmitterDraws[i].Type	 = Source.eEmitterType;
	}
}

void FParticleSystemSceneProxy::UpdateVisibility()
{

}

void FParticleSystemSceneProxy::UpdateMesh()
{
	SectionDraws.clear();
	uint32 IndexCursor = 0;

	for (size_t i = 0; i < DynamicData.size(); ++i)
	{
		FEmitterDraw& Draw = EmitterDraws[i];
		const auto& Source = static_cast<const FDynamicSpriteEmitterReplayDataBase&>(
			DynamicData[i]->GetSource());
		const uint32 ParticleCount = static_cast<uint32>(Source.ActiveParticleCount);

		if (Draw.Type == EDynamicEmitterType::Sprite)
		{
			// 6 indices per particle into the proxy's shared SpriteIB = Quad
			const uint32 IdxCount = ParticleCount * 6;
			Draw.FirstIndex = IndexCursor;
			Draw.IndexCount = IdxCount;
			IndexCursor += IdxCount;
		}
		else if (Draw.Type == EDynamicEmitterType::Mesh)
		{
			// Mesh path: section's index range is the static mesh's own IB.
			// FirstIndex/IndexCount come from MeshGeom, and InstanceCount = ParticleCount.
			Draw.FirstIndex = 0;
			Draw.IndexCount = Draw.MeshGeom ? Draw.MeshGeom->GetIndexBuffer().GetIndexCount() : 0;
			Draw.InstanceCount = ParticleCount;
		}
		SectionDraws.push_back({ Draw.Material, Draw.FirstIndex, Draw.IndexCount });
	}
}

void FParticleSystemSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{

}

bool FParticleSystemSceneProxy::PrepareDrawBuffer(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, FDrawCommandBuffer& Out) const
{
	
	return false;
}

bool FParticleSystemSceneProxy::PrepareDrawCommandBindings(ID3D11Device*, ID3D11DeviceContext*,
	const FPrimitiveDrawOptions&, FDrawCommand&) const
{
	return false;
}

void FParticleSystemSceneProxy::PackSpriteEmitter(const FFrameContext& Frame, FDynamicSpriteEmitterData& Emitter,
	TArray<FParticleSpriteVertex>& OutVerts,
	TArray<uint32>& OutIndices, uint32& IndexCursor)
{
	const FDynamicSpriteEmitterReplayDataBase& Source = Emitter.Source;
	const int32 Count = Source.ActiveParticleCount;
	if (Count <= 0) return;

	// Sort
	Emitter.SortSpriteParticles(Source.SortMode, Frame.CameraPosition, Frame.CameraForward, FMatrix::Identity, Source.DataContainer.ParticleIndices,
								Count, Source.DataContainer.ParticleData, Source.ParticleStride);

	// Expand each particle into a 4-vert quad
	const float SubUInv = (Source.SubImages_Horizontal > 0) ? 1.0f / Source.SubImages_Horizontal : 1.0f;
	const float SubVInv = (Source.SubImages_Vertical > 0) ? 1.0f / Source.SubImages_Vertical : 1.0f;

	for (int32 i = 0; i < Count; ++i)
	{
		const uint16 Idx = Source.DataContainer.ParticleIndices[i];
		const uint8* Bytes = Source.DataContainer.ParticleData + Idx * Source.ParticleStride;
		const FBaseParticle& P = *reinterpret_cast<const FBaseParticle*>(Bytes);

		// optional payload reads
		float SubImageIndex = 0.0f;
		if (Source.SubUVDataOffset >= 0)
			SubImageIndex = *reinterpret_cast<const float*>(Bytes + Source.SubUVDataOffset);

		const uint32 V0 = static_cast<uint32>(OutVerts.size());
		for (int corner = 0; corner < 4; ++corner)
		{
			FParticleSpriteVertex V;
			V.Position = P.Location;
			V.Size = FVector(P.Size.X, P.Size.Y, /*subImageLerp*/ 0.0f);
			V.UV = FVector2{ float(corner & 1), float((corner >> 1) & 1) };  // 0,0..1,1
			V.Color = FVector4(P.Color.R, P.Color.G, P.Color.B, P.Color.A);
			V.Rotation = P.Rotation;
			V.SubImageIndex = SubImageIndex;
			V.Velocity = P.Velocity;
			OutVerts.push_back(V);
		}

		// CCW quad
		OutIndices.push_back(V0 + 0); OutIndices.push_back(V0 + 1); OutIndices.push_back(V0 + 2);
		OutIndices.push_back(V0 + 2); OutIndices.push_back(V0 + 1); OutIndices.push_back(V0 + 3);
		IndexCursor += 6;
	}
}

void FParticleSystemSceneProxy::PackMeshEmitter(const FFrameContext& Frame, FDynamicMeshEmitterData& Emitter)
{
	
}