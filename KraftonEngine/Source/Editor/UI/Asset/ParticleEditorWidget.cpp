#include "ParticleEditorWidget.h"

#include "Component/ParticleSystemComponent.h"
#include "Editor/UI/ContentBrowser/ContentItem.h"
#include "GameFramework/AActor.h"
#include "Input/InputSystem.h"
#include "GameFramework/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Object/ObjectFactory.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"
#include "Particle/ParticleSpriteEmitter.h"
#include "Particle/ParticleSystem.h"
#include "Particle/ParticleSystemManager.h"
#include "Runtime/Engine.h"
#include "Settings/EditorSettings.h"
#include "Slate/SlateApplication.h"
#include "UI/Toolbar/ViewportToolbar.h"
#include "Viewport/Viewport.h"

#include <algorithm>
#include <cstdio>
#include <imgui.h>

namespace
{
	static uint32 GNextParticleEditorInstanceId = 0;

	const char* GScreenAlignmentNames[] =
	{
		"PSA Square",
		"PSA Rectangle",
		"PSA Velocity",
		"PSA Away From Center",
		"PSA Type Specific",
		"PSA Facing Camera Position"
	};

	const char* GSortModeNames[] =
	{
		"None",
		"View Projection Depth",
		"Distance To View",
		"Age Oldest First",
		"Age Newest First"
	};

	const char* GParticleAlphaSourceNames[] =
	{
		"Texture Alpha",
		"Texture Luminance"
	};

	ImU32 GetModuleRowColor(bool bSelected, int32 ModuleIndex)
	{
		if (bSelected)
		{
			return IM_COL32(245, 215, 42, 255);
		}

		static constexpr ImU32 Colors[] =
		{
			IM_COL32(198, 92, 96, 255),
			IM_COL32(45, 47, 58, 255),
			IM_COL32(45, 47, 58, 255),
			IM_COL32(45, 47, 58, 255),
			IM_COL32(45, 47, 58, 255),
			IM_COL32(45, 47, 58, 255)
		};
		const int32 ColorIndex = std::clamp(ModuleIndex, 0, static_cast<int32>(IM_ARRAYSIZE(Colors)) - 1);
		return Colors[ColorIndex];
	}

	FString GetMaterialPath(UMaterialInterface* MaterialInterface)
	{
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : nullptr;
		return Material ? Material->GetAssetPathFileName() : FString();
	}

	UMaterial* AcceptMaterialDrop()
	{
		if (!ImGui::BeginDragDropTarget())
		{
			return nullptr;
		}

		UMaterial* Material = nullptr;
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("MaterialContentItem"))
		{
			const FContentItem* Item = static_cast<const FContentItem*>(Payload->Data);
			if (Item)
			{
				const FString MaterialPath = FPaths::MakeProjectRelative(
					FPaths::ToUtf8(Item->Path.generic_wstring()));
				Material = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);
			}
		}

		ImGui::EndDragDropTarget();
		return Material;
	}

	void DrawHorizontalSplitter(float& TopHeight, float MinTopHeight, float MinBottomHeight, float AvailableHeight, const char* Id)
	{
		const float SplitterHeight = 6.0f;
		ImGui::InvisibleButton(Id, ImVec2(-1.0f, SplitterHeight));
		if (ImGui::IsItemHovered())
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
		}
		if (ImGui::IsItemActive())
		{
			TopHeight += ImGui::GetIO().MouseDelta.y;
		}

		const float MaxTopHeight = (std::max)(MinTopHeight, AvailableHeight - SplitterHeight - MinBottomHeight);
		TopHeight = std::clamp(TopHeight, MinTopHeight, MaxTopHeight);
	}
}

FParticleEditorWidget::FParticleEditorWidget()
	: InstanceId(GNextParticleEditorInstanceId++)
{
	const FString Id = std::to_string(InstanceId);
	PreviewWorldHandle = FName("ParticleEditorPreview_" + Id);
	WindowIdSuffix = "###ParticleEditor_" + Id;
}

bool FParticleEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UParticleSystem>();
}

bool FParticleEditorWidget::IsEditingObject(UObject* Object) const
{
	return FAssetEditorWidget::IsEditingObject(Object);
}

void FParticleEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);
	if (!IsOpen())
	{
		return;
	}

	EditingParticleSystem = Cast<UParticleSystem>(EditedObject);
	if (!EditingParticleSystem)
	{
		Close();
		return;
	}

	EnsureDefaultSystem();
	InitializePreviewWorld();
}

void FParticleEditorWidget::Close()
{
	FAssetEditorWidget::Close();
	ReleasePreviewWorld();
	EditingParticleSystem = nullptr;
	PreviewParticleComponent = nullptr;
	PreviewActor = nullptr;
	SelectedModule = nullptr;
}

void FParticleEditorWidget::Tick(float DeltaTime)
{
	if (ViewportClient.IsRenderable())
	{
		ViewportClient.Tick(DeltaTime);
	}

	if (PreviewActor)
	{
		PreviewActor->bTickInEditor = bSimulating;
	}
}

void FParticleEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (IsOpen())
	{
		OutClients.push_back(const_cast<FStaticMeshEditorViewportClient*>(&ViewportClient));
	}
}

void FParticleEditorWidget::EnsureDefaultSystem()
{
	if (!EditingParticleSystem || !EditingParticleSystem->Emitters.empty())
	{
		if (!SelectedModule)
		{
			SelectedModule = GetSelectedRequiredModule();
		}
		return;
	}

	if (UParticleEmitter* Emitter = CreateDefaultEmitter("Particle Emitter"))
	{
		EditingParticleSystem->Emitters.push_back(Emitter);
		SelectedEmitterIndex = 0;
		SelectedModule = GetSelectedRequiredModule();
	}
}

UParticleEmitter* FParticleEditorWidget::CreateDefaultEmitter(const FString& EmitterName)
{
	if (!EditingParticleSystem)
	{
		return nullptr;
	}

	UParticleSpriteEmitter* Emitter = GUObjectArray.CreateObject<UParticleSpriteEmitter>(EditingParticleSystem);
	Emitter->SetEmitterName(FName(EmitterName));
	Emitter->InitialAllocationCount = 128;
	Emitter->PeakActiveParticles = 128;

	UParticleLODLevel* LOD = GUObjectArray.CreateObject<UParticleLODLevel>(Emitter);
	LOD->bEnabled = true;
	LOD->SetLevelIndex(0);

	UParticleModuleRequired* Required = GUObjectArray.CreateObject<UParticleModuleRequired>(LOD);
	Required->Material = FMaterialManager::Get().GetOrCreateMaterial("Asset/Particle/Materials/M_Fire_B.mat");
	Required->ScreenAlignment = PSA_FacingCameraPosition;
	Required->SubImages_Horizontal = 1;
	Required->SubImages_Vertical = 1;
	Required->AlphaSource = 1;
	Required->AlphaThreshold = 0.08f;
	Required->AlphaPower = 1.0f;
	Required->ColorIntensity = 1.6f;
	LOD->RequiredModule = Required;

	UParticleModuleSpawn* Spawn = GUObjectArray.CreateObject<UParticleModuleSpawn>(LOD);
	Spawn->Rate = 20.0f;
	LOD->SpawnModule = Spawn;

	UParticleModuleLifetime* Lifetime = GUObjectArray.CreateObject<UParticleModuleLifetime>(LOD);
	Lifetime->Lifetime = 1.0f;
	LOD->Modules.push_back(Lifetime);

	UParticleModuleVelocity* Velocity = GUObjectArray.CreateObject<UParticleModuleVelocity>(LOD);
	Velocity->StartVelocity = FVector(0.0f, 0.0f, 35.0f);

	UParticleModuleSize* Size = GUObjectArray.CreateObject<UParticleModuleSize>(LOD);
	Size->StartSize = FVector(12.0f, 12.0f, 1.0f);
	LOD->Modules.push_back(Size);
	LOD->Modules.push_back(Velocity);

	UParticleModuleColor* Color = GUObjectArray.CreateObject<UParticleModuleColor>(LOD);
	Color->StartColor = FVector(1.0f, 1.0f, 1.0f);
	Color->StartAlpha = 1.0f;
	LOD->Modules.push_back(Color);

	LOD->UpdateModuleLists();
	Emitter->LODLevels.push_back(LOD);
	Emitter->UpdateModuleLists();
	return Emitter;
}

void FParticleEditorWidget::InitializePreviewWorld()
{
	if (!GEngine || !EditingParticleSystem)
	{
		return;
	}

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	PreviewActor = WorldContext.World->SpawnActor<AActor>();
	PreviewActor->bTickInEditor = true;
	PreviewActor->bNeedsTick = true;
	PreviewActor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

	PreviewParticleComponent = PreviewActor->AddComponent<UParticleSystemComponent>();
	PreviewActor->SetRootComponent(PreviewParticleComponent);
	PreviewParticleComponent->SetTemplate(EditingParticleSystem);
	PreviewParticleComponent->InitializeSystem();

	ViewportClient.Initialize(Device, 640, 480);
	ViewportClient.SetClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(PreviewActor);
	ViewportClient.SetPreviewMeshComponent(nullptr);
	ViewportClient.ResetCameraToPreviewBounds();

	WorldContext.World->SetEditorPOVProvider(&ViewportClient);
	FSlateApplication::Get().RegisterViewport(&ParticleViewportWindow, &ViewportClient);
}

void FParticleEditorWidget::ReleasePreviewWorld()
{
	if (UWorld* PreviewWorld = ViewportClient.GetPreviewWorld())
	{
		FScene& PreviewScene = PreviewWorld->GetScene();
		GEngine->GetRenderer().GetResources().ReleaseShadowResourcesForScene(&PreviewScene);

		if (PreviewWorldHandle.IsValid())
		{
			GEngine->DestroyWorldContext(PreviewWorldHandle);
		}
	}

	FSlateApplication::Get().UnregisterViewport(&ViewportClient);
	ViewportClient.Release();
}

void FParticleEditorWidget::RestartPreviewSystem()
{
	if (!PreviewParticleComponent)
	{
		return;
	}

	PreviewParticleComponent->ResetParticles(true);
	PreviewParticleComponent->InitializeSystem();
}

void FParticleEditorWidget::ApplyEmitterEdit()
{
	if (UParticleEmitter* Emitter = GetSelectedEmitter())
	{
		Emitter->UpdateModuleLists();
	}

	RestartPreviewSystem();
	MarkDirty();
}

UParticleEmitter* FParticleEditorWidget::GetSelectedEmitter() const
{
	if (!EditingParticleSystem || EditingParticleSystem->Emitters.empty())
	{
		return nullptr;
	}

	const int32 ClampedIndex = std::clamp(SelectedEmitterIndex, 0,
		static_cast<int32>(EditingParticleSystem->Emitters.size()) - 1);
	return EditingParticleSystem->Emitters[ClampedIndex];
}

UParticleModuleRequired* FParticleEditorWidget::GetSelectedRequiredModule() const
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	if (!Emitter)
	{
		return nullptr;
	}

	UParticleLODLevel* LOD = Emitter->GetLODLevel(0);
	return LOD ? LOD->RequiredModule : nullptr;
}

UParticleModule* FParticleEditorWidget::GetSelectedModule() const
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	if (!Emitter)
	{
		return nullptr;
	}

	UParticleLODLevel* LOD = Emitter->GetLODLevel(0);
	if (!LOD)
	{
		return nullptr;
	}

	if (SelectedModule == LOD->RequiredModule || SelectedModule == LOD->SpawnModule)
	{
		return SelectedModule;
	}

	for (UParticleModule* Module : LOD->Modules)
	{
		if (SelectedModule == Module)
		{
			return SelectedModule;
		}
	}

	if (LOD->RequiredModule)
	{
		return LOD->RequiredModule;
	}
	return LOD->SpawnModule;
}

FString FParticleEditorWidget::GetEmitterDisplayName(UParticleEmitter* Emitter, int32 Index) const
{
	if (Emitter && Emitter->GetEmitterName().IsValid())
	{
		return Emitter->GetEmitterName().ToString();
	}

	char Buffer[32] = {};
	std::snprintf(Buffer, sizeof(Buffer), "Emitter %d", Index);
	return FString(Buffer);
}

FString FParticleEditorWidget::GetModuleDisplayName(UParticleModule* Module) const
{
	if (!Module)
	{
		return "None";
	}
	if (Module->IsA<UParticleModuleRequired>())
	{
		return "Required";
	}
	if (Module->IsA<UParticleModuleSpawn>())
	{
		return "Spawn";
	}
	if (Module->IsA<UParticleModuleLifetime>())
	{
		return "Lifetime";
	}
	if (Module->IsA<UParticleModuleSize>())
	{
		return "Initial Size";
	}
	if (Module->IsA<UParticleModuleVelocity>())
	{
		return "Initial Velocity";
	}
	if (Module->IsA<UParticleModuleLocation>())
	{
		return "Initial Location";
	}
	if (Module->IsA<UParticleModuleColor>())
	{
		return "Color Over Life";
	}
	return Module->GetClass()->GetName();
}

void FParticleEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (!IsOpen() || !EditingParticleSystem)
	{
		return;
	}

	bool bWindowOpen = true;
	FString VisibleTitle = "Particle Editor";
	const FString AssetName = EditingParticleSystem->GetName();
	if (!AssetName.empty())
	{
		VisibleTitle += " - ";
		VisibleTitle += AssetName;
	}
	if (IsDirty())
	{
		VisibleTitle += " *";
	}

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_None;
	if (ViewportClient.IsMouseOverViewport())
	{
		WindowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
	}

	ImGui::SetNextWindowSize(ImVec2(1180.0f, 720.0f), ImGuiCond_Once);
	FString WindowTitle = VisibleTitle + WindowIdSuffix;
	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, WindowFlags))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		FSlateApplication::Get().BringViewportToFront(&ViewportClient);
	}

	RenderToolbar();
	ImGui::Separator();
	RenderEditorLayout();

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}

void FParticleEditorWidget::RenderEditorLayout()
{
	static float TopHeight = 310.0f;

	const float SplitterHeight = 6.0f;
	const ImVec2 Available = ImGui::GetContentRegionAvail();
	const float MinTopHeight = 180.0f;
	const float MinBottomHeight = 180.0f;
	TopHeight = std::clamp(TopHeight, MinTopHeight, (std::max)(MinTopHeight, Available.y - SplitterHeight - MinBottomHeight));

	if (!ImGui::BeginTable(
		"ParticleEditorLayout",
		2,
		ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
	{
		return;
	}

	ImGui::TableSetupColumn("PreviewAndDetails", ImGuiTableColumnFlags_WidthStretch, 0.34f);
	ImGui::TableSetupColumn("EmittersAndCurve", ImGuiTableColumnFlags_WidthStretch, 0.66f);
	ImGui::TableNextRow();

	ImGui::TableSetColumnIndex(0);
	ImGui::BeginChild("ParticlePreviewPane", ImVec2(0.0f, TopHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	RenderPreviewViewport(ImGui::GetContentRegionAvail());
	ImGui::EndChild();
	DrawHorizontalSplitter(TopHeight, MinTopHeight, MinBottomHeight, Available.y, "##ParticleHorizontalSplitterLeft");
	ImGui::BeginChild("ParticleDetailsPane", ImVec2(0.0f, 0.0f), true);
	if (RenderDetailsPanel())
	{
		ApplyEmitterEdit();
	}
	ImGui::EndChild();

	ImGui::TableSetColumnIndex(1);
	ImGui::BeginChild("ParticleEmitterPane", ImVec2(0.0f, TopHeight), true);
	RenderEmitterList();
	ImGui::EndChild();
	DrawHorizontalSplitter(TopHeight, MinTopHeight, MinBottomHeight, Available.y, "##ParticleHorizontalSplitterRight");
	ImGui::BeginChild("ParticleCurvePane", ImVec2(0.0f, 0.0f), true);
	RenderCurvePanel();
	ImGui::EndChild();

	ImGui::EndTable();
}

void FParticleEditorWidget::RenderToolbar()
{
	if (ImGui::Button("Save"))
	{
		if (FParticleSystemManager::Get().Save(EditingParticleSystem))
		{
			ClearDirty();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button(bSimulating ? "Pause" : "Play"))
	{
		bSimulating = !bSimulating;
	}
	ImGui::SameLine();
	if (ImGui::Button("Restart Sim"))
	{
		RestartPreviewSystem();
	}
	ImGui::SameLine();
	if (ImGui::Button("Frame Camera"))
	{
		ViewportClient.ResetCameraToPreviewBounds();
	}
	ImGui::SameLine();
	ImGui::TextDisabled("%zu emitters", EditingParticleSystem ? EditingParticleSystem->Emitters.size() : 0);
}

void FParticleEditorWidget::RenderPreviewViewport(const ImVec2& Size)
{
	ImGui::BeginGroup();
	{
		ImVec2 ViewportPos = ImGui::GetCursorScreenPos();
		ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y);

		FViewport* VP = ViewportClient.GetViewport();
		if (VP && Size.x > 0.0f && Size.y > 0.0f)
		{
			VP->RequestResize(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y));
			ParticleViewportWindow.SetRect(FRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y));

			if (VP->GetSRV())
			{
				ImGui::Image((ImTextureID)VP->GetSRV(), Size);
				const float ImGuiWheel = ImGui::GetIO().MouseWheel;
				if (ImGui::IsItemHovered() && ImGuiWheel != 0.0f && InputSystem::Get().GetScrollNotches() == 0.0f)
				{
					ViewportClient.QueueScrollInput(ImGuiWheel);
				}
			}

			constexpr float ToolbarHeight = 28.0f;
			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			DrawList->AddRectFilled(
				ViewportPos,
				ImVec2(ViewportPos.x + Size.x, ViewportPos.y + ToolbarHeight),
				IM_COL32(40, 40, 40, 255));

			FViewportToolbarContext Context;
			Context.Renderer = &GEngine->GetRenderer();
			Context.Settings = &FEditorSettings::Get().MeshEditorViewportSettings;
			Context.RenderOptions = &ViewportClient.GetRenderOptions();
			Context.ToolbarLeft = ViewportPos.x;
			Context.ToolbarTop = ViewportPos.y;
			Context.ToolbarWidth = Size.x;
			Context.bReservePlayStopSpace = false;
			Context.bShowAddActor = false;
			Context.bShowGizmoControls = false;

			FViewportToolbar::Render(Context);
		}
	}
	ImGui::EndGroup();
}

void FParticleEditorWidget::RenderEmitterList()
{
	ImGui::TextUnformatted("Emitters");
	ImGui::SameLine();
	if (ImGui::Button("+ Emitter"))
	{
		const int32 NewIndex = EditingParticleSystem ? static_cast<int32>(EditingParticleSystem->Emitters.size()) : 0;
		char NameBuffer[48] = {};
		std::snprintf(NameBuffer, sizeof(NameBuffer), "Particle Emitter %d", NewIndex + 1);
		if (UParticleEmitter* NewEmitter = CreateDefaultEmitter(NameBuffer))
		{
			EditingParticleSystem->Emitters.push_back(NewEmitter);
			SelectedEmitterIndex = NewIndex;
			SelectedModule = GetSelectedRequiredModule();
			RestartPreviewSystem();
			MarkDirty();
		}
	}

	ImGui::BeginChild("EmitterList", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);

	if (!EditingParticleSystem || EditingParticleSystem->Emitters.empty())
	{
		ImGui::TextDisabled("No emitters.");
		ImGui::EndChild();
		return;
	}

	for (int32 Index = 0; Index < static_cast<int32>(EditingParticleSystem->Emitters.size()); ++Index)
	{
		UParticleEmitter* Emitter = EditingParticleSystem->Emitters[Index];
		UParticleLODLevel* LOD = Emitter ? Emitter->GetLODLevel(0) : nullptr;
		if (!Emitter || !LOD)
		{
			continue;
		}

		ImGui::PushID(Index);
		ImGui::BeginGroup();
		ImGui::BeginChild("EmitterColumn", ImVec2(190.0f, 0.0f), true);

		const bool bEmitterSelected = Index == SelectedEmitterIndex;
		const FString Label = GetEmitterDisplayName(Emitter, Index);
		ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(255, 124, 0, 255));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(255, 146, 42, 255));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(255, 124, 0, 255));
		if (ImGui::Selectable(Label.c_str(), bEmitterSelected, 0, ImVec2(0.0f, 28.0f)))
		{
			SelectedEmitterIndex = Index;
			SelectedModule = LOD->RequiredModule;
		}
		ImGui::PopStyleColor(3);

		auto DrawModuleRow = [&](UParticleModule* Module, int32 ModuleIndex)
		{
			if (!Module)
			{
				return;
			}

			const bool bSelected = Index == SelectedEmitterIndex && Module == GetSelectedModule();
			const ImU32 RowColor = GetModuleRowColor(bSelected, ModuleIndex);
			ImGui::PushStyleColor(ImGuiCol_Header, RowColor);
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, bSelected ? RowColor : IM_COL32(78, 80, 92, 255));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, RowColor);
			const FString ModuleName = GetModuleDisplayName(Module);
			if (ImGui::Selectable(ModuleName.c_str(), bSelected, 0, ImVec2(0.0f, 24.0f)))
			{
				SelectedEmitterIndex = Index;
				SelectedModule = Module;
			}
			ImGui::PopStyleColor(3);
		};

		int32 ModuleIndex = 0;
		DrawModuleRow(LOD->RequiredModule, ModuleIndex++);
		DrawModuleRow(LOD->SpawnModule, ModuleIndex++);
		for (UParticleModule* Module : LOD->Modules)
		{
			DrawModuleRow(Module, ModuleIndex++);
		}

		ImGui::EndChild();
		ImGui::EndGroup();
		ImGui::PopID();

		if (Index + 1 < static_cast<int32>(EditingParticleSystem->Emitters.size()))
		{
			ImGui::SameLine();
		}
	}

	ImGui::EndChild();
}

void FParticleEditorWidget::RenderCurvePanel()
{
	ImGui::TextUnformatted("Curve Editor");
	ImGui::Separator();

	const ImVec2 Origin = ImGui::GetCursorScreenPos();
	const ImVec2 Size = ImGui::GetContentRegionAvail();
	if (Size.x <= 0.0f || Size.y <= 0.0f)
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 Max(Origin.x + Size.x, Origin.y + Size.y);
	DrawList->AddRectFilled(Origin, Max, IM_COL32(45, 45, 45, 255));

	const float TrackWidth = (std::min)(190.0f, Size.x * 0.45f);
	DrawList->AddRectFilled(Origin, ImVec2(Origin.x + TrackWidth, Max.y), IM_COL32(120, 120, 120, 255));

	const ImU32 GridColor = IM_COL32(155, 155, 155, 180);
	for (float X = Origin.x + TrackWidth; X < Max.x; X += 120.0f)
	{
		DrawList->AddLine(ImVec2(X, Origin.y), ImVec2(X, Max.y), GridColor);
	}
	for (float Y = Origin.y; Y < Max.y; Y += 28.0f)
	{
		DrawList->AddLine(ImVec2(Origin.x, Y), ImVec2(Max.x, Y), GridColor);
	}

	const ImU32 TextColor = ImGui::GetColorU32(ImGuiCol_Text);
	DrawList->AddText(ImVec2(Origin.x + 10.0f, Origin.y + 12.0f), TextColor, "SubImageIndex");
	DrawList->AddText(ImVec2(Origin.x + 10.0f, Origin.y + 42.0f), TextColor, "LifeMultiplier");
	ImGui::InvisibleButton("##CurveCanvas", Size);
}

bool FParticleEditorWidget::RenderDetailsPanel()
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	UParticleModule* Module = GetSelectedModule();
	SelectedModule = Module;
	if (!Emitter || !Module)
	{
		ImGui::TextDisabled("Select a module.");
		return false;
	}

	bool bChanged = false;
	ImGui::TextUnformatted(GetModuleDisplayName(Module).c_str());
	ImGui::TextDisabled("%s", GetEmitterDisplayName(Emitter, SelectedEmitterIndex).c_str());
	ImGui::Separator();

	bChanged |= RenderModuleDetails(Module);

	return bChanged;
}

bool FParticleEditorWidget::RenderModuleDetails(UParticleModule* Module)
{
	if (!Module)
	{
		return false;
	}

	if (UParticleModuleRequired* Required = Cast<UParticleModuleRequired>(Module))
	{
		return RenderRequiredDetails(Required);
	}

	bool bChanged = false;
	bool bEnabled = Module->bEnabled != 0;
	if (ImGui::Checkbox("Enabled", &bEnabled))
	{
		Module->bEnabled = bEnabled;
		bChanged = true;
	}

	if (UParticleModuleSpawn* Spawn = Cast<UParticleModuleSpawn>(Module))
	{
		float Rate = Spawn->Rate;
		if (ImGui::DragFloat("Rate", &Rate, 1.0f, 0.0f, 10000.0f))
		{
			Spawn->Rate = (std::max)(0.0f, Rate);
			bChanged = true;
		}
	}
	else if (UParticleModuleLifetime* Lifetime = Cast<UParticleModuleLifetime>(Module))
	{
		float LifetimeValue = Lifetime->Lifetime;
		if (ImGui::DragFloat("Lifetime", &LifetimeValue, 0.05f, 0.0f, 1000.0f))
		{
			Lifetime->Lifetime = (std::max)(0.0f, LifetimeValue);
			bChanged = true;
		}
	}
	else if (UParticleModuleSize* Size = Cast<UParticleModuleSize>(Module))
	{
		FVector StartSize = Size->StartSize;
		if (ImGui::DragFloat3("Start Size", &StartSize.X, 0.25f, 0.0f, 10000.0f))
		{
			Size->StartSize = StartSize;
			bChanged = true;
		}
	}
	else if (UParticleModuleVelocity* Velocity = Cast<UParticleModuleVelocity>(Module))
	{
		FVector StartVelocity = Velocity->StartVelocity;
		if (ImGui::DragFloat3("Start Velocity", &StartVelocity.X, 0.5f, -10000.0f, 10000.0f))
		{
			Velocity->StartVelocity = StartVelocity;
			bChanged = true;
		}
	}
	else if (UParticleModuleLocation* Location = Cast<UParticleModuleLocation>(Module))
	{
		FVector StartLocation = Location->StartLocation;
		if (ImGui::DragFloat3("Start Location", &StartLocation.X, 0.25f))
		{
			Location->StartLocation = StartLocation;
			bChanged = true;
		}
	}
	else if (UParticleModuleColor* Color = Cast<UParticleModuleColor>(Module))
	{
		float StartColor[3] = { Color->StartColor.X, Color->StartColor.Y, Color->StartColor.Z };
		if (ImGui::ColorEdit3("Start Color", StartColor))
		{
			Color->StartColor = FVector(StartColor[0], StartColor[1], StartColor[2]);
			bChanged = true;
		}

		float StartAlpha = Color->StartAlpha;
		if (ImGui::DragFloat("Start Alpha", &StartAlpha, 0.01f, 0.0f, 1.0f))
		{
			Color->StartAlpha = std::clamp(StartAlpha, 0.0f, 1.0f);
			bChanged = true;
		}
	}
	else
	{
		ImGui::TextDisabled("No editable fields for this module yet.");
	}

	return bChanged;
}

bool FParticleEditorWidget::RenderRequiredDetails(UParticleModuleRequired* Required)
{
	bool bChanged = false;

	const FString CurrentMaterialPath = GetMaterialPath(Required->Material);
	const char* PreviewLabel = CurrentMaterialPath.empty() ? "None" : CurrentMaterialPath.c_str();
	if (ImGui::BeginCombo("Material", PreviewLabel))
	{
		const TArray<FMaterialAssetListItem>& Materials = FMaterialManager::Get().GetAvailableMaterialFiles();
		for (const FMaterialAssetListItem& Item : Materials)
		{
			const bool bSelected = CurrentMaterialPath == Item.FullPath;
			if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
			{
				if (UMaterial* NewMaterial = FMaterialManager::Get().GetOrCreateMaterial(Item.FullPath))
				{
					Required->Material = NewMaterial;
					bChanged = true;
				}
			}
		}
		ImGui::EndCombo();
	}
	if (UMaterial* DroppedMaterial = AcceptMaterialDrop())
	{
		Required->Material = DroppedMaterial;
		bChanged = true;
	}

	int Alignment = static_cast<int>(Required->ScreenAlignment);
	if (ImGui::Combo("Screen Alignment", &Alignment, GScreenAlignmentNames, IM_ARRAYSIZE(GScreenAlignmentNames)))
	{
		Required->ScreenAlignment = static_cast<EParticleScreenAlignment>(std::clamp(Alignment, 0, static_cast<int>(PSA_MAX) - 1));
		bChanged = true;
	}

	int SortMode = static_cast<int>(Required->SortMode);
	if (ImGui::Combo("Sort Mode", &SortMode, GSortModeNames, IM_ARRAYSIZE(GSortModeNames)))
	{
		Required->SortMode = static_cast<EParticleSortMode>(std::clamp(SortMode, 0, static_cast<int>(PSORTMODE_MAX) - 1));
		bChanged = true;
	}

	UMaterial* RequiredMaterial = Required->Material ? Required->Material->GetMaterial() : nullptr;
	const FMaterialParticleSettings* MaterialParticleSettings = RequiredMaterial ? &RequiredMaterial->GetParticleSettings() : nullptr;
	const bool bMaterialControlsSubUV = MaterialParticleSettings && MaterialParticleSettings->bUseSubUV;

	if (bMaterialControlsSubUV)
	{
		const uint32 Columns = (std::max)(1u, MaterialParticleSettings->SubUVColumns);
		const uint32 Rows = (std::max)(1u, MaterialParticleSettings->SubUVRows);
		ImGui::TextDisabled("SubUV Source: Material (%u x %u)", Columns, Rows);
		ImGui::BeginDisabled();
		int SubImagesHorizontal = static_cast<int>(Columns);
		ImGui::DragInt("SubUV Columns", &SubImagesHorizontal, 1.0f, 1, 64);
		int SubImagesVertical = static_cast<int>(Rows);
		ImGui::DragInt("SubUV Rows", &SubImagesVertical, 1.0f, 1, 64);
		ImGui::EndDisabled();
	}
	else
	{
		int SubImagesHorizontal = (std::max)(1, Required->SubImages_Horizontal);
		if (ImGui::DragInt("SubUV Columns", &SubImagesHorizontal, 1.0f, 1, 64))
		{
			Required->SubImages_Horizontal = (std::max)(1, SubImagesHorizontal);
			bChanged = true;
		}

		int SubImagesVertical = (std::max)(1, Required->SubImages_Vertical);
		if (ImGui::DragInt("SubUV Rows", &SubImagesVertical, 1.0f, 1, 64))
		{
			Required->SubImages_Vertical = (std::max)(1, SubImagesVertical);
			bChanged = true;
		}
	}

	int AlphaSource = std::clamp(Required->AlphaSource, 0, static_cast<int>(IM_ARRAYSIZE(GParticleAlphaSourceNames)) - 1);
	if (ImGui::Combo("Alpha Source", &AlphaSource, GParticleAlphaSourceNames, IM_ARRAYSIZE(GParticleAlphaSourceNames)))
	{
		Required->AlphaSource = AlphaSource;
		bChanged = true;
	}

	float AlphaThreshold = Required->AlphaThreshold;
	if (ImGui::DragFloat("Alpha Threshold", &AlphaThreshold, 0.005f, 0.0f, 1.0f, "%.3f"))
	{
		Required->AlphaThreshold = std::clamp(AlphaThreshold, 0.0f, 1.0f);
		bChanged = true;
	}

	float AlphaPower = Required->AlphaPower;
	if (ImGui::DragFloat("Alpha Power", &AlphaPower, 0.01f, 0.001f, 8.0f, "%.3f"))
	{
		Required->AlphaPower = (std::max)(0.001f, AlphaPower);
		bChanged = true;
	}

	float ColorIntensity = Required->ColorIntensity;
	if (ImGui::DragFloat("Color Intensity", &ColorIntensity, 0.01f, 0.0f, 8.0f, "%.3f"))
	{
		Required->ColorIntensity = (std::max)(0.0f, ColorIntensity);
		bChanged = true;
	}

	FVector Origin = Required->EmitterOrigin;
	if (ImGui::DragFloat3("Emitter Origin", &Origin.X, 0.1f))
	{
		Required->EmitterOrigin = Origin;
		bChanged = true;
	}

	float Duration = Required->EmitterDuration;
	if (ImGui::DragFloat("Emitter Duration", &Duration, 0.01f, 0.0f, 1000.0f))
	{
		Required->EmitterDuration = (std::max)(0.0f, Duration);
		bChanged = true;
	}

	int MaxDrawCount = Required->MaxDrawCount;
	if (ImGui::DragInt("Max Draw Count", &MaxDrawCount, 1.0f, 0, 100000))
	{
		Required->MaxDrawCount = (std::max)(0, MaxDrawCount);
		bChanged = true;
	}

	bool bUseLocalSpace = Required->bUseLocalSpace != 0;
	if (ImGui::Checkbox("Use Local Space", &bUseLocalSpace))
	{
		Required->bUseLocalSpace = bUseLocalSpace;
		bChanged = true;
	}

	bool bKillOnDeactivate = Required->bKillOnDeactivate != 0;
	if (ImGui::Checkbox("Kill On Deactivate", &bKillOnDeactivate))
	{
		Required->bKillOnDeactivate = bKillOnDeactivate;
		bChanged = true;
	}

	bool bKillOnCompleted = Required->bKillOnCompleted != 0;
	if (ImGui::Checkbox("Kill On Completed", &bKillOnCompleted))
	{
		Required->bKillOnCompleted = bKillOnCompleted;
		bChanged = true;
	}

	return bChanged;
}
