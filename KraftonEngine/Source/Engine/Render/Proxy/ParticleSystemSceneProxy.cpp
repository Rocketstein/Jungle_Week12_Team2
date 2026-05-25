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

	FVector SafeNormalizeBeam(const FVector& Value, const FVector& Fallback)
	{
		const float LenSq = Value.Dot(Value);
		return (LenSq > 1e-6f) ? Value * (1.0f / std::sqrt(LenSq)) : Fallback;
	}

	FVector RotateAroundAxis(const FVector& Value, const FVector& UnitAxis, float Radians)
	{
		const float S = std::sin(Radians);
		const float C = std::cos(Radians);
		return Value * C
			+ UnitAxis.Cross(Value) * S
			+ UnitAxis * (UnitAxis.Dot(Value) * (1.0f - C));
	}

	float ApplyBeamTaper(EBeamTaperMethod TaperMethod, float TaperFactor, float TaperScale, float Alpha)
	{
		Alpha = std::clamp(Alpha, 0.0f, 1.0f);
		TaperScale = std::max(0.0f, TaperScale);

		if (TaperMethod == PEBTM_Full)
		{
			return (1.0f - Alpha * (1.0f - TaperFactor)) * TaperScale;
		}

		if (TaperMethod == PEBTM_Partial)
		{
			if (Alpha <= TaperFactor)
			{
				return TaperScale;
			}
			const float Denom = std::max(1.0f - TaperFactor, 1e-6f);
			return (1.0f - (Alpha - TaperFactor) / Denom) * TaperScale;
		}

		return TaperScale;
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
	FDrawCommandBuffer SpriteBuffer;
	const bool bHasSpriteBuffer = SpritePacker.HasPackedSprites()
		&& SpritePacker.PrepareDrawBuffer(InDevice, InDeviceContext, SpriteBuffer);

	FDrawCommandBuffer BeamBuffer;
	const bool bHasBeamBuffer = BeamPacker.PrepareDrawBuffer(InDevice, InDeviceContext, BeamBuffer);

	if (SpritePacker.HasPackedSprites())
	{
		if (bHasSpriteBuffer)
		{
			Out = SpriteBuffer;
			return true;
		}
	}

	if (bHasBeamBuffer)
	{
		Out = BeamBuffer;
		return true;
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
			BeamPacker.PackEmitter(Frame, static_cast<FDynamicBeamEmitterData&>(*DynamicData[DrawIndex]), Draw);
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
		Cmd.Buffer.VB         = BeamPacker.GetVertexBuffer();
		Cmd.Buffer.VBStride   = sizeof(FBeamParticleInstanceVertex);
		Cmd.Buffer.IB         = BeamPacker.GetIndexBuffer();
		Cmd.Buffer.FirstIndex = Hit.FirstIndex;
		Cmd.Buffer.IndexCount = Hit.IndexCount;
		Cmd.Buffer.BaseVertex = 0;
		if (!Cmd.Buffer.VB || !Cmd.Buffer.IB)
		{
			return false;
		}
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
//	Beam Packer — CPU-expanded dynamic geometry.
//============================================================================
void FParticleSystemSceneProxy::FBeamParticlePacker::ResetFrame()
{
	PackedVertices.clear();
	PackedIndices.clear();
	bGpuBuffersDirty = true;
}

bool FParticleSystemSceneProxy::FBeamParticlePacker::PrepareDrawBuffer(
	ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, FDrawCommandBuffer& Out) const
{
	if (!HasPackedBeams())
	{
		return false;
	}

	const uint32 VertCount = static_cast<uint32>(PackedVertices.size());
	const uint32 IndexCount = static_cast<uint32>(PackedIndices.size());

	if (VertexBuffer.GetMaxCount() == 0)
	{
		VertexBuffer.Create(InDevice, VertCount, sizeof(FBeamParticleInstanceVertex));
	}
	else
	{
		VertexBuffer.EnsureCapacity(InDevice, VertCount);
	}

	if (IndexBuffer.GetMaxCount() == 0)
	{
		IndexBuffer.Create(InDevice, IndexCount);
	}
	else
	{
		IndexBuffer.EnsureCapacity(InDevice, IndexCount);
	}

	if (bGpuBuffersDirty)
	{
		if (!VertexBuffer.Update(InDeviceContext, PackedVertices.data(), VertCount))
		{
			return false;
		}
		if (!IndexBuffer.Update(InDeviceContext, PackedIndices.data(), IndexCount))
		{
			return false;
		}
		bGpuBuffersDirty = false;
	}

	Out.VB = VertexBuffer.GetBuffer();
	Out.VBStride = sizeof(FBeamParticleInstanceVertex);
	Out.IB = IndexBuffer.GetBuffer();
	Out.BaseVertex = 0;
	return Out.VB != nullptr && Out.IB != nullptr;
}

void FParticleSystemSceneProxy::FBeamParticlePacker::PackEmitter(const FFrameContext& Frame, FDynamicBeamEmitterData& Emitter, FEmitterDraw& Draw)
{
	const FDynamicBeamEmitterReplayData& Source = Emitter.BeamSource;
	Draw.FirstIndex = static_cast<uint32>(PackedIndices.size());
	Draw.IndexCount = 0;

	// For beams, ActiveParticleCount is Cascade/stat accounting
	// (logical beams * sheets), not a per-particle draw loop count.
	if (!Source.bRenderGeometry || Source.ActiveParticleCount == 0)
		return;

	const FVector BeamDelta = Source.Target - Source.Source;
	if (BeamDelta.Length() <= 1e-6f)
		return;

	const int32 SegmentCount = std::clamp(Source.InterpolationPoints + 1,
		1, static_cast<int32>(MaxSegmentsPerBeam));
	const int32 PointCount = SegmentCount + 1;
	const int32 SheetCount = std::clamp(Source.Sheets,
		1, static_cast<int32>(MaxSheetsPerBeam));

	const uint32 RequiredVertices = static_cast<uint32>(PointCount) * 2 * static_cast<uint32>(SheetCount);
	const uint32 RequiredIndices = static_cast<uint32>(SegmentCount) * 6 * static_cast<uint32>(SheetCount);
	PackedVertices.reserve(PackedVertices.size() + RequiredVertices);
	PackedIndices.reserve(PackedIndices.size() + RequiredIndices);

	const float BeamLen = BeamDelta.Length();
	const float Progress = std::clamp(Source.BeamProgress, 0.0f, 1.0f);
	const float VisibleLen = BeamLen * Progress;
	const FVector VisibleDelta = BeamDelta * Progress;
	const FVector BeamDir = (BeamLen > 1e-6f) ? BeamDelta * (1.0f / BeamLen) : FVector::ForwardVector;
	const FVector VertexColor(Source.Color.X, Source.Color.Y, Source.Color.Z);
	const FVector4 PackedColor(VertexColor, std::clamp(Source.Alpha, 0.0f, 1.0f));
	constexpr float Pi = 3.14159265358979323846f;

	for (int32 SheetIdx = 0; SheetIdx < SheetCount; ++SheetIdx)
	{
		const uint32 SheetVertexBase = static_cast<uint32>(PackedVertices.size());
		for (int32 PointIdx = 0; PointIdx < PointCount; ++PointIdx)
		{
			const float T = static_cast<float>(PointIdx) / static_cast<float>(std::max(PointCount - 1, 1));
			const FVector Center = Source.Source + VisibleDelta * T;
			const float Taper = ApplyBeamTaper(Source.TaperMethod, Source.TaperFactor, Source.TaperScale, T);
			const float HalfWidth = std::max(0.0f, Source.Width * Taper) * 0.5f;

			const FVector ToCamera = SafeNormalizeBeam(Frame.CameraPosition - Center, FVector::UpVector);
			FVector SideAxis = SafeNormalizeBeam(ToCamera.Cross(BeamDir), FVector::ForwardVector);
			if (SheetIdx > 0)
			{
				const float SheetAngle = Pi * static_cast<float>(SheetIdx) / static_cast<float>(SheetCount);
				SideAxis = SafeNormalizeBeam(RotateAroundAxis(SideAxis, BeamDir, SheetAngle), SideAxis);
			}

			const float U = (Source.TextureTileDistance > 0.0f)
				? (VisibleLen * T) / Source.TextureTileDistance
				: T * static_cast<float>(std::max(1, Source.TextureTile));

			FBeamParticleInstanceVertex Left;
			Left.Position = Center - SideAxis * HalfWidth;
			Left.UV = FVector2(U, 0.0f);
			Left.Color = PackedColor;
			PackedVertices.push_back(Left);

			FBeamParticleInstanceVertex Right;
			Right.Position = Center + SideAxis * HalfWidth;
			Right.UV = FVector2(U, 1.0f);
			Right.Color = PackedColor;
			PackedVertices.push_back(Right);
		}

		for (int32 SegIdx = 0; SegIdx < SegmentCount; ++SegIdx)
		{
			const uint32 P0Left = SheetVertexBase + static_cast<uint32>(SegIdx) * 2;
			const uint32 P0Right = P0Left + 1;
			const uint32 P1Left = P0Left + 2;
			const uint32 P1Right = P0Left + 3;

			PackedIndices.push_back(P0Left);
			PackedIndices.push_back(P1Left);
			PackedIndices.push_back(P0Right);
			PackedIndices.push_back(P1Left);
			PackedIndices.push_back(P1Right);
			PackedIndices.push_back(P0Right);
		}
	}

	Draw.IndexCount = RequiredIndices;
	bGpuBuffersDirty = true;
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
