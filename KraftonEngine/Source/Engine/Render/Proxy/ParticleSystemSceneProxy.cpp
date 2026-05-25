#include "ParticleSystemSceneProxy.h"
#include "Particle/ParticleHelper.h"
#include "Render/Types/FrameContext.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Particle/ParticleDynamicData.h"
#include "Component/ParticleSystemComponent.h"
#include "Materials/Material.h"
#include "Mesh/StaticMesh.h"

#include <algorithm>
#include <cmath>

namespace {
	bool ShouldSortEmitter(EBlendState BlendState) 
	{
		switch (BlendState)
		{
		case (EBlendState::Additive): 
		{
			return false;
		}
		case (EBlendState::AlphaBlend):
		case (EBlendState::Opaque):
		default:
			return true;
		}
	}

}

FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	// Per-frame CPU pack (sort + quad expansion) needs the FrameContext for
	// camera-dependent sort order. Required for RenderCollector to invoke
	// UpdatePerViewport on this proxy each frame.
	ProxyFlags |= EPrimitiveProxyFlags::PerViewportUpdate;

	// Bounds for particle systems are owned by UParticleSystem and FParticleEmitterInstance (dynamic per-frame bounds).
	// Until that's wired up on the CPU side, opt out of frustum + occlusion culling
	ProxyFlags |= EPrimitiveProxyFlags::NeverCull;
	ProxyFlags &= ~EPrimitiveProxyFlags::ShowAABB;
}

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

	if (EmitterDraws.size() != DynamicData.size())
	{
		bIsEmitterOrderDirty = true;
		EmitterDraws.resize(DynamicData.size());
	}
}

void FParticleSystemSceneProxy::UpdateTransform()
{
	FPrimitiveSceneProxy::UpdateTransform();
	ComponentToWorld = GetOwner()->GetWorldMatrix();
	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(FMatrix::Identity);
	MarkPerObjectCBDirty();
}

void FParticleSystemSceneProxy::UpdateMaterial()
{
	for (uint32 i = 0; i < DynamicData.size(); i++)
	{
		const auto& Source = static_cast<const FDynamicRenderableEmitterReplayDataBase&>(
			DynamicData[i]->GetSource());
		const FDynamicRenderableEmitterReplayDataBase* RenderableSource =
			dynamic_cast<const FDynamicRenderableEmitterReplayDataBase*>(&Source);

		auto& Draw = EmitterDraws[i];
		Draw.Material = RenderableSource && RenderableSource->MaterialInterface
			? RenderableSource->MaterialInterface->GetMaterial()
			: nullptr;
		Draw.Type	 = Source.eEmitterType;
		Draw.EmitterIndex = DynamicData[i]->EmitterIndex;
		if (Draw.SortingPriority != Source.EmitterSortPriority)
		{
			Draw.SortingPriority = Source.EmitterSortPriority;
			bIsEmitterOrderDirty = true;
		}
		Draw.SetParticleBlendRoute(Source.BlendMode);
		UpdateCB(EmitterDraws[i], Source);
	}
}

void FParticleSystemSceneProxy::UpdateVisibility()
{
	FPrimitiveSceneProxy::UpdateVisibility();
	bCastShadow = false;
}

void FParticleSystemSceneProxy::UpdateMesh()
{
	UpdateMaterial();

	for (size_t i = 0; i < DynamicData.size(); ++i)
	{
		FEmitterDraw& Draw = EmitterDraws[i];

		switch(Draw.Type)
		{
		case (DET_Sprite):
		{
			Draw.FirstIndex = 0;
			Draw.IndexCount = 0;
			break;
		}
		case (DET_Beam2):
		{
			Draw.FirstIndex = 0;
			Draw.IndexCount = 0;
			break;
		}
		case (DET_Mesh):
		{
			const auto& Source = static_cast<const FDynamicMeshEmitterReplayData&>(
				DynamicData[i]->GetSource());
			const uint32 ParticleCount = static_cast<uint32>(Source.ActiveParticleCount);

			// Mesh path: section's index range is the static mesh's own IB.
			// FirstIndex/IndexCount come from MeshGeom, and InstanceCount = ParticleCount.
			Draw.FirstIndex = 0;
			Draw.InstanceCount = ParticleCount;
			Draw.MeshGeom = Source.StaticMesh ? Source.StaticMesh->GetLODMeshBuffer(Source.LODLevel) : nullptr;
			Draw.IndexCount = Draw.MeshGeom ? Draw.MeshGeom->GetIndexBuffer().GetIndexCount() : 0;
			break;
		}
		}
	}

	if (bIsEmitterOrderDirty)
	{
		SortEmitters();
	}
}

// UpdatePerViewport: per-frame CPU work
// Runs in RenderCollector::Collect, gated by EPrimitiveProxyFlags::PerViewportUpdate.
void FParticleSystemSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	// Reset scratch arrays. Sprite scratch is shared, mesh scratch is per-emitter.
	SpritePacker.ResetFrame();
	MeshPacker.ResetFrame(EmitterDraws);
	BeamPacker.ResetFrame();

	if (DynamicData.empty())
	{
		bVisible = false;
		return;
	}

	// CPU pack: sort, quad expansion, and mesh instance payloads.
	PackParticles(Frame);

	bVisible = bInstancePacked;
	bInstancePacked = false;
	if (!bVisible) return;

	SpritePacker.MarkGpuBuffersDirty();
}

// PrepareDrawBuffer: GPU upload + bind hand-off
bool FParticleSystemSceneProxy::PrepareDrawBuffer(
	ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, FDrawCommandBuffer& Out) const
{
	if (SpritePacker.HasPackedSprites())
	{
		return SpritePacker.PrepareDrawBuffer(InDevice, InDeviceContext, Out);
	}

	if (BeamPacker.HasReadyBeams() && BeamPacker.EnsureStaticIndexBuffer(InDevice))
	{
		Out.VB         = nullptr;
		Out.VBStride   = 0;
		Out.IB         = BeamPacker.GetStaticIndexBuffer();
		Out.BaseVertex = 0;
		return Out.IB != nullptr;
	}

	for (const FEmitterDraw& Draw : EmitterDraws)
	{
		if (Draw.Type == DET_Mesh && Draw.MeshGeom && Draw.InstanceCount > 0)
		{
			Out.VB         = Draw.MeshGeom->GetVertexBuffer().GetBuffer();
			Out.VBStride   = Draw.MeshGeom->GetVertexBuffer().GetStride();
			Out.IB         = Draw.MeshGeom->GetIndexBuffer().GetBuffer();
			Out.BaseVertex = 0;
			break;
		}
	}

	return Out.VB != nullptr && Out.IB != nullptr;
}

void FParticleSystemSceneProxy::SetEmitterSortingPriority(uint16 EmitterIndex, uint16 InPriority)
{
	if (EmitterIndex >= EmitterDraws.size()) return;
	EmitterDraws[EmitterIndex].SortingPriority = InPriority;
	bIsEmitterOrderDirty = true;
}

void FParticleSystemSceneProxy::SortEmitters()
{
	const uint16 Count = static_cast<uint16>(EmitterDraws.size());
	SectionToEmitterDrawIndex.resize(Count);
	for (uint16 i = 0; i < Count; ++i)
	{
		SectionToEmitterDrawIndex[i] = i;
	}

	// Ascending: lower SortingPriority draws first, higher draws on top.
	std::stable_sort(SectionToEmitterDrawIndex.begin(), SectionToEmitterDrawIndex.end(),
		[this](uint16 A, uint16 B)
		{
			return EmitterDraws[A].SortingPriority < EmitterDraws[B].SortingPriority;
		});

	bIsEmitterOrderDirty = false;
}

void FParticleSystemSceneProxy::RebuildSectionDraws()
{
	if (SectionToEmitterDrawIndex.size() != EmitterDraws.size())
	{
		SortEmitters();
	}

	SectionDraws.clear();
	SectionDraws.reserve(SectionToEmitterDrawIndex.size());

	for (uint16 DrawIndex : SectionToEmitterDrawIndex)
	{
		if (DrawIndex >= EmitterDraws.size())
		{
			continue;
		}

		const FEmitterDraw& Draw = EmitterDraws[DrawIndex];
		FMeshSectionDraw SectionDraw;
		SectionDraw.Material		= Draw.Material;
		SectionDraw.FirstIndex		= Draw.FirstIndex;
		SectionDraw.IndexCount		= Draw.IndexCount;
		SectionDraw.PassOverride	= Draw.ParticleBlendState.RenderPass;
		SectionDraw.BlendOverride	= Draw.ParticleBlendState.BlendState;
		SectionDraws.push_back(SectionDraw);
	}
}

void FParticleSystemSceneProxy::PackParticles(const FFrameContext& Frame)
{
	// Rebuild Emitter order before packing vertices
	if (bIsEmitterOrderDirty) {
		SortEmitters();
	}

	uint32 IndexCursor = 0;
	for (size_t i = 0; i < SectionToEmitterDrawIndex.size(); ++i)
	{
		const uint16 DrawIndex = SectionToEmitterDrawIndex[i];
		if (DrawIndex >= EmitterDraws.size() || DrawIndex >= DynamicData.size()) continue;
		FEmitterDraw& Draw = EmitterDraws[DrawIndex];

		switch (Draw.Type)
		{
		case DET_Sprite:
		{
			const uint32 IndexBefore = IndexCursor;
			SpritePacker.PackEmitter(Frame,
				static_cast<FDynamicSpriteEmitterData&>(*DynamicData[DrawIndex]), Draw, IndexCursor);

			// Refresh section range so DrawCommandBuilder sees the right slice
			// even if ActiveParticleCount shrank since UpdateMesh.
			Draw.FirstIndex = IndexBefore;
			Draw.IndexCount = IndexCursor - IndexBefore;
			break;
		}
		case DET_Mesh:
		{
			MeshPacker.PackEmitter(Frame,
				static_cast<FDynamicMeshEmitterData&>(*DynamicData[DrawIndex]),
				Draw);
			// Mesh section range stays as UpdateMesh set it (static-mesh IB range
			// is fixed; InstanceCount is the per-frame variable).
			break;
		}
		case DET_Beam2:
		{
			BeamPacker.PackEmitter(static_cast<FDynamicBeamEmitterData&>(*DynamicData[DrawIndex]), Draw);
			break;
		}
		default:
			break;
		}

		if (!Draw.PackedInstances.empty() || SpritePacker.HasPackedSprites() || (Draw.Type == DET_Beam2 && Draw.IndexCount > 0))
		{
			bInstancePacked = true;
		}
	}

	RebuildSectionDraws();
}

bool FParticleSystemSceneProxy::PrepareDrawCommandBindings(ID3D11Device* InDevice,
	ID3D11DeviceContext* InDeviceContext,
	const FPrimitiveDrawOptions&, FDrawCommand& Cmd, int32 SectionIndex) const
{
	if (SectionIndex < 0 || SectionIndex >= static_cast<int32>(SectionToEmitterDrawIndex.size()))
	{
		return false;
	}

	const uint16 DrawIndex = SectionToEmitterDrawIndex[SectionIndex];
	if (DrawIndex >= EmitterDraws.size())
	{
		return false;
	}

	const FEmitterDraw& Hit = EmitterDraws[DrawIndex];

	// Upload CB if dirty
	if (Hit.bParticleParamCBDirty)
	{
		if (!Hit.ParticleParamCB.GetBuffer())
		{
			Hit.ParticleParamCB.Create(
				InDevice,
				sizeof(FParticleParamConstants),
				"ParticleParamCB");
		}

		Hit.ParticleParamCB.Update(
			InDeviceContext,
			&Hit.ParticleParams,
			sizeof(FParticleParamConstants));

		Hit.bParticleParamCBDirty = false;
	}
	Cmd.Bindings.PerShaderCB[0] = &Hit.ParticleParamCB;

	if (Hit.Type == DET_Mesh && Hit.MeshGeom && Hit.InstanceCount > 0)
	{
		if (Hit.bInstanceVBDirty && !Hit.PackedInstances.empty())
		{
			const uint32 Count = static_cast<uint32>(Hit.PackedInstances.size());
			if (Hit.InstanceVB.GetMaxCount() == 0)
			{
				Hit.InstanceVB.Create(InDevice, Count, sizeof(FMeshParticleInstanceVertex));
			}
			Hit.InstanceVB.EnsureCapacity(InDevice, Count);
			Hit.InstanceVB.Update(InDeviceContext, Hit.PackedInstances.data(), Count);
			Hit.bInstanceVBDirty = false;
		}

		Cmd.Buffer.VB                = Hit.MeshGeom->GetVertexBuffer().GetBuffer();
		Cmd.Buffer.VBStride          = Hit.MeshGeom->GetVertexBuffer().GetStride();
		Cmd.Buffer.IB                = Hit.MeshGeom->GetIndexBuffer().GetBuffer();
		Cmd.Buffer.FirstIndex        = 0;
		Cmd.Buffer.IndexCount        = Hit.IndexCount;
		Cmd.Buffer.BaseVertex        = 0;

		Cmd.Buffer.InstanceVB        = Hit.InstanceVB.GetBuffer();
		Cmd.Buffer.InstanceVBStride  = sizeof(FMeshParticleInstanceVertex);
		Cmd.Buffer.InstancedCount    = Hit.InstanceCount;
		Cmd.Buffer.InstanceStart     = 0;
	}
	else if (Hit.Type == DET_Beam2 && Hit.IndexCount > 0)
	{
		if (!BeamPacker.EnsureStaticIndexBuffer(InDevice))
		{
			return false;
		}

		if (Hit.bBeamParamCBDirty)
		{
			if (!Hit.BeamParamCB.GetBuffer())
			{
				Hit.BeamParamCB.Create(InDevice, sizeof(FBeamParamConstants), "BeamParamCB");
			}
			Hit.BeamParamCB.Update(InDeviceContext, &Hit.BeamParams, sizeof(FBeamParamConstants));
			Hit.bBeamParamCBDirty = false;
		}
		Cmd.Bindings.PerShaderCB[1] = &Hit.BeamParamCB;

		Cmd.Buffer.VB         = nullptr;
		Cmd.Buffer.VBStride   = 0;
		Cmd.Buffer.IB         = BeamPacker.GetStaticIndexBuffer();
		Cmd.Buffer.FirstIndex = 0;
		Cmd.Buffer.IndexCount = Hit.IndexCount;
		Cmd.Buffer.BaseVertex = 0;
	}
	return true;
}

//============================================================================
//	Sprite Packer
//============================================================================
bool FParticleSystemSceneProxy::FSpriteParticlePacker::PrepareDrawBuffer(
	ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, FDrawCommandBuffer& Out) const
{
	if (!HasPackedSprites())
	{
		return false;
	}

	const uint32 VertCount  = static_cast<uint32>(PackedVertices.size());
	const uint32 IndexCount = static_cast<uint32>(IndexPattern.size());

	if (VertexBuffer.GetMaxCount() == 0)
	{
		VertexBuffer.Create(InDevice, VertCount, sizeof(FParticleSpriteVertex));
	}
	else
	{
		VertexBuffer.EnsureCapacity(InDevice, VertCount);
	}

	// NOTE: FDynamicVertexBuffer::Update takes ELEMENT count, not byte count.
	if (bGpuBuffersDirty)
	{
		VertexBuffer.Update(InDeviceContext, PackedVertices.data(), VertCount);
		bGpuBuffersDirty = false;
	}

	if (bIndexBufferDirty)
	{
		if (IndexBuffer.GetMaxCount() == 0)
		{
			IndexBuffer.Create(InDevice, IndexCount);
		}
		else
		{
			IndexBuffer.EnsureCapacity(InDevice, IndexCount);
		}

		IndexBuffer.Update(InDeviceContext, IndexPattern.data(), IndexCount);
		bIndexBufferDirty = false;
	}

	Out.VB         = VertexBuffer.GetBuffer();
	Out.VBStride   = sizeof(FParticleSpriteVertex);
	Out.IB         = IndexBuffer.GetBuffer();
	Out.BaseVertex = 0;
	return Out.VB != nullptr && Out.IB != nullptr;
}

void FParticleSystemSceneProxy::FSpriteParticlePacker::PackEmitter(const FFrameContext& Frame, FDynamicSpriteEmitterData& Emitter, const FEmitterDraw& Draw, uint32& IndexCursor)
{
	const FDynamicSpriteEmitterReplayData& Source = Emitter.Source;
	const int32 Count = Source.ActiveParticleCount;
	if (Count <= 0 ||
		!Source.DataContainer.ParticleData ||
		!Source.DataContainer.ParticleIndices ||
		Source.ParticleStride < static_cast<int32>(sizeof(FBaseParticle)))
	{
		return;
	}

	TArray<uint16> SortedParticleIndices(Source.DataContainer.ParticleIndices, Source.DataContainer.ParticleIndices + Count);

	if (ShouldSortEmitter(Emitter.Source.BlendMode)) {
		Emitter.SortParticles(Source.SortMode, Frame.CameraPosition, Frame.CameraForward, FMatrix::Identity,
			SortedParticleIndices.data(), Count, Source.DataContainer.ParticleData, Source.ParticleStride);
	}

	const uint32 ParticleCount = static_cast<uint32>(Count);
	const uint32 FirstParticle = IndexCursor / 6;
	EnsureIndexPattern(FirstParticle + ParticleCount);

	const int32 SubImageColumns = std::max(1, static_cast<int32>(Draw.ParticleParams.SubUVCols));
	const int32 SubImageRows = std::max(1, static_cast<int32>(Draw.ParticleParams.SubUVRows));
	const int32 SubImageCount = std::max(1, SubImageColumns * SubImageRows);

	PackedVertices.reserve(PackedVertices.size() + Count * 4);
	for (int32 i = 0; i < Count; ++i)
	{
		const uint16 Idx = SortedParticleIndices[i];
		const uint8* Bytes = Source.DataContainer.ParticleData + Idx * Source.ParticleStride;
		const FBaseParticle& P = *reinterpret_cast<const FBaseParticle*>(Bytes);
		const float NormalizedAge = std::clamp(P.RelativeTime, 0.0f, 1.0f);
		const int32 SubImageIndex = std::clamp(
			static_cast<int32>(std::floor(NormalizedAge * static_cast<float>(SubImageCount))),
			0,
			SubImageCount - 1);

		for (int corner = 0; corner < 4; ++corner)
		{
			FParticleSpriteVertex V;
			V.Position = P.Location;
			V.Size = FVector(P.Size.X, P.Size.Y, /*subImageLerp*/ 0.0f);
			V.UV = FVector2{ float(corner & 1), float((corner >> 1) & 1) };  // 0,0..1,1
			V.Color = FVector4(P.Color.R, P.Color.G, P.Color.B, P.Color.A);
			V.Rotation = P.Rotation;
			V.SubImageIndex = static_cast<float>(SubImageIndex);
			V.Velocity = P.Velocity;
			PackedVertices.push_back(V);
		}
	}

	IndexCursor += ParticleCount * 6;
}

//============================================================================
//	Mesh Packer
//============================================================================
void FParticleSystemSceneProxy::FMeshParticlePacker::ResetFrame(TArray<FEmitterDraw>& EmitterDraws)
{
	for (FEmitterDraw& Draw : EmitterDraws)
	{
		if (Draw.Type == DET_Mesh)
		{
			Draw.PackedInstances.clear();
		}
	}
}

bool FParticleSystemSceneProxy::FMeshParticlePacker::HasPackedInstances(const TArray<FEmitterDraw>& EmitterDraws) const
{
	for (const FEmitterDraw& Draw : EmitterDraws)
	{
		if (Draw.Type == DET_Mesh && !Draw.PackedInstances.empty())
		{
			return true;
		}
	}
	return false;
}

void FParticleSystemSceneProxy::FMeshParticlePacker::PackEmitter(const FFrameContext& Frame,
	FDynamicMeshEmitterData& Emitter, FEmitterDraw& Draw)
{
	const FDynamicMeshEmitterReplayData& Source = Emitter.MeshSource;
	const int32 Count = Source.ActiveParticleCount;
	Draw.InstanceCount = static_cast<uint32>(Count > 0 ? Count : 0);

	if (Count <= 0 ||
		!Source.DataContainer.ParticleData ||
		!Source.DataContainer.ParticleIndices ||
		Source.ParticleStride < static_cast<int32>(sizeof(FBaseParticle)))
	{
		return;
	}

	Draw.PackedInstances.clear();
	Draw.PackedInstances.reserve(Count);

	TArray<uint16> SortedParticleIndices(Source.DataContainer.ParticleIndices, Source.DataContainer.ParticleIndices + Count);

	if (ShouldSortEmitter(Emitter.MeshSource.BlendMode)) {
		Emitter.SortParticles(Source.SortMode, Frame.CameraPosition, Frame.CameraForward, FMatrix::Identity,
			SortedParticleIndices.data(), Count, Source.DataContainer.ParticleData, Source.ParticleStride);
	}

	for (int32 i = 0; i < Count; ++i)
	{
		const uint16 Idx = SortedParticleIndices[i];
		const uint8* Bytes = Source.DataContainer.ParticleData + Idx * Source.ParticleStride;
		const FBaseParticle& P = *reinterpret_cast<const FBaseParticle*>(Bytes);

		const FMatrix Model = FMatrix::MakeScaleMatrix(P.Size)
		                    * FMatrix::MakeRotationZ(P.Rotation)
		                    * FMatrix::MakeTranslationMatrix(P.Location);

		FMeshParticleInstanceVertex V;
		V.Transform    = Model;
		V.Color        = FVector4(P.Color.R, P.Color.G, P.Color.B, P.Color.A);
		V.DynamicParam = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
		Draw.PackedInstances.push_back(V);
	}

	Draw.bInstanceVBDirty = true;
}


//============================================================================
//	Beam Packer — VS-driven: only fills the per-emitter CB + section index range.
//============================================================================
void FParticleSystemSceneProxy::FBeamParticlePacker::PackEmitter(FDynamicBeamEmitterData& Emitter, FEmitterDraw& Draw)
{
	const FDynamicBeamEmitterReplayData& Source = Emitter.BeamSource;
	Draw.FirstIndex = 0;
	Draw.IndexCount = 0;

	if (!Source.bRenderGeometry)
		return;

	const FVector BeamDelta = Source.Target - Source.Source;
	if (BeamDelta.Length() <= 1e-6f)
		return;

	const int32 SegmentCount = std::clamp(Source.InterpolationPoints + 1,
		1, static_cast<int32>(MaxSegmentsPerBeam));
	const int32 PointCount = SegmentCount + 1;
	const int32 SheetCount = std::clamp(Source.Sheets,
		1, static_cast<int32>(MaxSheetsPerBeam));

	FBeamParamConstants& P = Draw.BeamParams;
	P.Source              = Source.Source;
	P.Target              = Source.Target;
	P.Width               = Source.Width;
	P.Color               = Source.Color;
	P.Alpha               = std::clamp(Source.Alpha, 0.0f, 1.0f);
	P.TaperFactor         = Source.TaperFactor;
	P.TaperScale          = Source.TaperScale;
	P.TaperMethod         = static_cast<uint32>(Source.TaperMethod);
	P.PointCount          = static_cast<uint32>(PointCount);
	P.TextureTile         = static_cast<uint32>(std::max(1, Source.TextureTile));
	P.TextureTileDistance = std::max(0.0f, Source.TextureTileDistance);
	P.SheetCount          = static_cast<uint32>(SheetCount);
	Draw.bBeamParamCBDirty = true;

	Draw.IndexCount = static_cast<uint32>(SegmentCount) * 6 * static_cast<uint32>(SheetCount);
	bAnyBeamReady = true;
}

bool FParticleSystemSceneProxy::FBeamParticlePacker::EnsureStaticIndexBuffer(ID3D11Device* InDevice) const
{
	if (StaticIB.GetBuffer())
		return true;

	TArray<uint32> Indices;
	Indices.resize(MaxIndexCount);
	for (uint32 i = 0; i < MaxIndexCount; ++i)
	{
		Indices[i] = i;
	}

	StaticIB.Create(InDevice, Indices.data(), MaxIndexCount, MaxIndexCount * sizeof(uint32));
	return StaticIB.GetBuffer() != nullptr;
}

//============================================================================
//	CB
//============================================================================
void FParticleSystemSceneProxy::UpdateCB(FEmitterDraw& EmitterDraw, const FDynamicEmitterReplayDataBase& Source)
{
	const FDynamicSpriteEmitterReplayData* SpriteSource = dynamic_cast<const FDynamicSpriteEmitterReplayData*>(&Source);
	uint32 SubUVCols = SpriteSource ? static_cast<uint32>(SpriteSource->SubImages_Horizontal) : 1;
	uint32 SubUVRows = SpriteSource ? static_cast<uint32>(SpriteSource->SubImages_Vertical) : 1;
	const uint32 ScreenAlignment = SpriteSource ? static_cast<uint32>(SpriteSource->ScreenAlignment) : 0;
	const FVector EmitterOrigin = SpriteSource ? SpriteSource->EmitterOrigin : FVector::ZeroVector;
	const uint32 AlphaSource = SpriteSource ? SpriteSource->AlphaSource : 0;
	const float AlphaThreshold = SpriteSource ? SpriteSource->AlphaThreshold : 0.0f;
	const float AlphaPower = SpriteSource ? SpriteSource->AlphaPower : 1.0f;
	const float ColorIntensity = SpriteSource ? SpriteSource->ColorIntensity : 1.0f;

	if (EmitterDraw.Material)
	{
		const FMaterialParticleSettings& ParticleSettings = EmitterDraw.Material->GetParticleSettings();
		if (ParticleSettings.bUseSubUV)
		{
			SubUVCols = std::max(1u, ParticleSettings.SubUVColumns);
			SubUVRows = std::max(1u, ParticleSettings.SubUVRows);
		}
	}

	if (EmitterDraw.ParticleParams.SubUVCols == SubUVCols &&
		EmitterDraw.ParticleParams.SubUVRows == SubUVRows &&
		EmitterDraw.ParticleParams.ScreenAlignment == ScreenAlignment &&
		EmitterDraw.ParticleParams.EmitterOrigin.X == EmitterOrigin.X &&
		EmitterDraw.ParticleParams.EmitterOrigin.Y == EmitterOrigin.Y &&
		EmitterDraw.ParticleParams.EmitterOrigin.Z == EmitterOrigin.Z &&
		EmitterDraw.ParticleParams.AlphaSource == AlphaSource &&
		EmitterDraw.ParticleParams.AlphaThreshold == AlphaThreshold &&
		EmitterDraw.ParticleParams.AlphaPower == AlphaPower &&
		EmitterDraw.ParticleParams.ColorIntensity == ColorIntensity)
	{
		return;
	}

	EmitterDraw.ParticleParams.SubUVCols = SubUVCols;
	EmitterDraw.ParticleParams.SubUVRows = SubUVRows;
	EmitterDraw.ParticleParams.ScreenAlignment = ScreenAlignment;
	EmitterDraw.ParticleParams.EmitterOrigin = EmitterOrigin;
	EmitterDraw.ParticleParams.AlphaSource = AlphaSource;
	EmitterDraw.ParticleParams.AlphaThreshold = AlphaThreshold;
	EmitterDraw.ParticleParams.AlphaPower = AlphaPower;
	EmitterDraw.ParticleParams.ColorIntensity = ColorIntensity;
	EmitterDraw.bParticleParamCBDirty = true;
}

// Lazily grows the persistent sprite quad index buffer to cover RequiredParticleCount quads; no-op if already large enough.
void FParticleSystemSceneProxy::FSpriteParticlePacker::EnsureIndexPattern(uint32 RequiredParticleCount)
{
	if (RequiredParticleCount <= IndexPatternParticleCapacity)
	{
		return;
	}

	/* Quad = 6 Indices */
	IndexPattern.reserve(RequiredParticleCount * 6);
	for (uint32 ParticleIndex = IndexPatternParticleCapacity; ParticleIndex < RequiredParticleCount; ++ParticleIndex)
	{
		const uint32 V0 = ParticleIndex * 4;
		IndexPattern.push_back(V0 + 0);
		IndexPattern.push_back(V0 + 2);
		IndexPattern.push_back(V0 + 1);
		IndexPattern.push_back(V0 + 2);
		IndexPattern.push_back(V0 + 3);
		IndexPattern.push_back(V0 + 1);
	}

	IndexPatternParticleCapacity = RequiredParticleCount;
	bIndexBufferDirty = true;
}

void FParticleSystemSceneProxy::FEmitterDraw::SetParticleBlendRoute(EBlendState Mode)
{
	switch (Mode)
	{
	case EBlendState::Opaque:        ParticleBlendState = { ERenderPass::Opaque,     EBlendState::Opaque };   break;
	case EBlendState::Additive:      ParticleBlendState = { ERenderPass::AlphaBlend, EBlendState::Additive }; break;
	case EBlendState::AlphaBlend:
	default:                         ParticleBlendState = { ERenderPass::AlphaBlend, EBlendState::AlphaBlend };
	}
}
