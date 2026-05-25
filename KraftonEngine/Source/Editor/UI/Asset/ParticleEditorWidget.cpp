#include "ParticleEditorWidget.h"

#include "Component/ParticleSystemComponent.h"
#include "Editor/UI/ContentBrowser/ContentItem.h"
#include "Editor/UI/EditorTextureManager.h"
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
#include "Platform/Paths.h"
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

	const char* GCollisionChannelNames[] =
	{
		"World Static",
		"World Dynamic",
		"Pawn",
		"Projectile",
		"Trigger",
		"Foot IK"
	};

	const char* GParticleCollisionResponseNames[] =
	{
		"Bounce",
		"Stop",
		"Kill"
	};

	FString GetParticleEditorIconPath(const wchar_t* FileName)
	{
		return FPaths::ToUtf8(FPaths::Combine(
			FPaths::AssetDir(),
			L"Editor/Icons/ParticleEditor",
			FileName));
	}

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

	bool DrawParticleToolbarButton(const char* Id, const wchar_t* IconFileName, const char* Label, bool bDisabled)
	{
		bool bClicked = false;
		ID3D11ShaderResourceView* Icon = FEditorTextureManager::Get().GetOrLoadIcon(GetParticleEditorIconPath(IconFileName));

		ImGui::PushID(Id);
		ImGui::BeginDisabled(bDisabled);
		if (Icon)
		{
			bClicked |= ImGui::ImageButton("##Icon", reinterpret_cast<ImTextureID>(Icon), ImVec2(18.0f, 18.0f));
			ImGui::SameLine(0.0f, 4.0f);
		}
		bClicked |= ImGui::Button(Label);
		ImGui::EndDisabled();

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("%s", Label);
		}
		ImGui::PopID();

		return bClicked && !bDisabled;
	}

	void DestroyEmitterTree(UParticleEmitter* Emitter)
	{
		if (!Emitter)
		{
			return;
		}

		for (UParticleLODLevel* LOD : Emitter->LODLevels)
		{
			if (!LOD)
			{
				continue;
			}

			for (UParticleModule* Module : LOD->Modules)
			{
				if (Module)
				{
					if (Module == LOD->TypeDataModule)
					{
						LOD->TypeDataModule = nullptr;
					}
					GUObjectArray.DestroyObject(Module);
				}
			}
			LOD->Modules.clear();

			if (LOD->RequiredModule)
			{
				GUObjectArray.DestroyObject(LOD->RequiredModule);
				LOD->RequiredModule = nullptr;
			}
			if (LOD->SpawnModule)
			{
				GUObjectArray.DestroyObject(LOD->SpawnModule);
				LOD->SpawnModule = nullptr;
			}
			if (LOD->TypeDataModule)
			{
				GUObjectArray.DestroyObject(LOD->TypeDataModule);
				LOD->TypeDataModule = nullptr;
			}

			GUObjectArray.DestroyObject(LOD);
		}
		Emitter->LODLevels.clear();

		GUObjectArray.DestroyObject(Emitter);
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

	SelectedEmitterIndex = 0;
	SelectedLODIndex = 0;
	SelectedModule = nullptr;
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
	SelectedLODIndex = 0;
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
		if (EditingParticleSystem)
		{
			EditingParticleSystem->NormalizeLODData();
			SelectedLODIndex = ClampLODIndex(SelectedLODIndex);
		}
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
		EditingParticleSystem->NormalizeLODData();
		SelectedLODIndex = ClampLODIndex(SelectedLODIndex);
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

	if (UParticleModule* Lifetime = CreateModule(EAddableModuleType::Lifetime, LOD))
	{
		LOD->Modules.push_back(Lifetime);
	}
	if (UParticleModule* Size = CreateModule(EAddableModuleType::Size, LOD))
	{
		LOD->Modules.push_back(Size);
	}
	if (UParticleModule* Velocity = CreateModule(EAddableModuleType::Velocity, LOD))
	{
		LOD->Modules.push_back(Velocity);
	}
	if (UParticleModule* Location = CreateModule(EAddableModuleType::Location, LOD))
	{
		LOD->Modules.push_back(Location);
	}
	if (UParticleModule* Color = CreateModule(EAddableModuleType::Color, LOD))
	{
		LOD->Modules.push_back(Color);
	}
	if (UParticleModule* ColorOverLife = CreateModule(EAddableModuleType::ColorOverLife, LOD))
	{
		LOD->Modules.push_back(ColorOverLife);
	}

	LOD->UpdateModuleLists();
	Emitter->LODLevels.push_back(LOD);
	Emitter->UpdateModuleLists();
	return Emitter;
}

UParticleModule* FParticleEditorWidget::CreateModule(EAddableModuleType ModuleType, UObject* Outer)
{
	if (!Outer)
	{
		return nullptr;
	}

	switch (ModuleType)
	{
	case EAddableModuleType::Lifetime:
	{
		UParticleModuleLifetime* Lifetime = GUObjectArray.CreateObject<UParticleModuleLifetime>(Outer);
		Lifetime->bEnabled = true;
		Lifetime->Lifetime = 1.0f;
		Lifetime->LifetimeMin = Lifetime->Lifetime;
		Lifetime->LifetimeMax = Lifetime->Lifetime;
		return Lifetime;
	}
	case EAddableModuleType::Size:
	{
		UParticleModuleSize* Size = GUObjectArray.CreateObject<UParticleModuleSize>(Outer);
		Size->bEnabled = true;
		Size->StartSize = FVector(12.0f, 12.0f, 1.0f);
		Size->StartSizeMin = Size->StartSize;
		Size->StartSizeMax = Size->StartSize;
		return Size;
	}
	case EAddableModuleType::Velocity:
	{
		UParticleModuleVelocity* Velocity = GUObjectArray.CreateObject<UParticleModuleVelocity>(Outer);
		Velocity->bEnabled = true;
		Velocity->StartVelocity = FVector(0.0f, 0.0f, 35.0f);
		Velocity->StartVelocityMin = Velocity->StartVelocity;
		Velocity->StartVelocityMax = Velocity->StartVelocity;
		return Velocity;
	}
	case EAddableModuleType::Location:
	{
		UParticleModuleLocation* Location = GUObjectArray.CreateObject<UParticleModuleLocation>(Outer);
		Location->bEnabled = true;
		Location->StartLocation = FVector::ZeroVector;
		Location->StartLocationMin = Location->StartLocation;
		Location->StartLocationMax = Location->StartLocation;
		return Location;
	}
	case EAddableModuleType::Color:
	{
		UParticleModuleColor* Color = GUObjectArray.CreateObject<UParticleModuleColor>(Outer);
		Color->bEnabled = true;
		Color->StartColor = FVector(1.0f, 1.0f, 1.0f);
		Color->StartAlpha = 1.0f;
		Color->StartAlphaMin = Color->StartAlpha;
		Color->StartAlphaMax = Color->StartAlpha;
		return Color;
	}
	case EAddableModuleType::ColorOverLife:
	{
		UParticleModuleColorOverLife* ColorOverLife = GUObjectArray.CreateObject<UParticleModuleColorOverLife>(Outer);
		ColorOverLife->bEnabled = true;
		ColorOverLife->ColorOverLife = FVector(1.0f, 1.0f, 1.0f);
		ColorOverLife->AlphaOverLife = 0.0f;
		return ColorOverLife;
	}
	case EAddableModuleType::Collision:
	{
		UParticleModuleCollision* Collision = GUObjectArray.CreateObject<UParticleModuleCollision>(Outer);
		Collision->bEnabled = true;
		return Collision;
	}
	}

	return nullptr;
}

void FParticleEditorWidget::AddModuleToEmitter(int32 EmitterIndex, EAddableModuleType ModuleType)
{
	if (!EditingParticleSystem || EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(EditingParticleSystem->Emitters.size()))
	{
		return;
	}

	UParticleEmitter* Emitter = EditingParticleSystem->Emitters[EmitterIndex];
	UParticleLODLevel* LOD = GetSelectedLODLevel(Emitter);
	if (!LOD)
	{
		return;
	}

	UParticleModule* NewModule = CreateModule(ModuleType, LOD);
	if (!NewModule)
	{
		return;
	}

	LOD->Modules.push_back(NewModule);
	SelectedEmitterIndex = EmitterIndex;
	SelectedModule = NewModule;
	ApplyEmitterEdit();
}

void FParticleEditorWidget::DeleteModuleFromEmitter(int32 EmitterIndex, UParticleModule* Module)
{
	if (!EditingParticleSystem || !Module || EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(EditingParticleSystem->Emitters.size()))
	{
		return;
	}

	UParticleEmitter* Emitter = EditingParticleSystem->Emitters[EmitterIndex];
	UParticleLODLevel* LOD = GetSelectedLODLevel(Emitter);
	if (!LOD)
	{
		return;
	}

	auto ModuleIt = std::find(LOD->Modules.begin(), LOD->Modules.end(), Module);
	if (ModuleIt == LOD->Modules.end())
	{
		return;
	}

	LOD->Modules.erase(ModuleIt);
	if (Module == LOD->TypeDataModule)
	{
		LOD->TypeDataModule = nullptr;
	}
	if (SelectedModule == Module)
	{
		SelectedEmitterIndex = EmitterIndex;
		SelectedModule = LOD->RequiredModule;
		if (!SelectedModule)
		{
			SelectedModule = LOD->SpawnModule;
		}
	}

	GUObjectArray.DestroyObject(Module);
	ApplyEmitterEdit();
}

void FParticleEditorWidget::DeleteEmitter(int32 EmitterIndex)
{
	if (!EditingParticleSystem || EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(EditingParticleSystem->Emitters.size()))
	{
		return;
	}

	UParticleEmitter* EmitterToDelete = EditingParticleSystem->Emitters[EmitterIndex];
	EditingParticleSystem->Emitters.erase(EditingParticleSystem->Emitters.begin() + EmitterIndex);

	SelectedModule = nullptr;
	if (EditingParticleSystem->Emitters.empty())
	{
		SelectedEmitterIndex = 0;
	}
	else
	{
		SelectedEmitterIndex = std::clamp(EmitterIndex, 0, static_cast<int32>(EditingParticleSystem->Emitters.size()) - 1);
		SelectedModule = GetSelectedRequiredModule();
	}

	DestroyEmitterTree(EmitterToDelete);
	RestartPreviewSystem();
	MarkDirty();
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
	PreviewParticleComponent->SetForcedLODLevel(SelectedLODIndex);
	PreviewParticleComponent->SetTemplate(EditingParticleSystem);

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

	PreviewParticleComponent->SetForcedLODLevel(SelectedLODIndex);
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

int32 FParticleEditorWidget::GetLODCount() const
{
	return EditingParticleSystem ? EditingParticleSystem->GetLODCount() : 1;
}

int32 FParticleEditorWidget::ClampLODIndex(int32 LODIndex) const
{
	return std::clamp(LODIndex, 0, (std::max)(0, GetLODCount() - 1));
}

void FParticleEditorWidget::SetSelectedLODIndex(int32 LODIndex)
{
	SelectedLODIndex = ClampLODIndex(LODIndex);
	SelectedModule = GetSelectedRequiredModule();
	ApplySelectedLODToPreview(true);
}

UParticleLODLevel* FParticleEditorWidget::GetSelectedLODLevel(UParticleEmitter* Emitter) const
{
	if (!Emitter)
	{
		return nullptr;
	}

	return Emitter->GetLODLevel(ClampLODIndex(SelectedLODIndex));
}

void FParticleEditorWidget::AddLOD()
{
	if (!EditingParticleSystem)
	{
		return;
	}

	const int32 NewLODIndex = EditingParticleSystem->CreateLOD();
	SetSelectedLODIndex(NewLODIndex);
	MarkDirty();
}

void FParticleEditorWidget::DeleteSelectedLOD()
{
	if (!EditingParticleSystem || SelectedLODIndex <= 0)
	{
		return;
	}

	const int32 OldLODIndex = SelectedLODIndex;
	if (!EditingParticleSystem->RemoveLOD(SelectedLODIndex))
	{
		return;
	}

	SetSelectedLODIndex(OldLODIndex - 1);
	MarkDirty();
}

void FParticleEditorWidget::ApplySelectedLODToPreview(bool bRestart)
{
	if (!PreviewParticleComponent)
	{
		return;
	}

	PreviewParticleComponent->SetForcedLODLevel(SelectedLODIndex);
	if (bRestart)
	{
		RestartPreviewSystem();
	}
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

	UParticleLODLevel* LOD = GetSelectedLODLevel(Emitter);
	return LOD ? LOD->RequiredModule : nullptr;
}

UParticleModule* FParticleEditorWidget::GetSelectedModule() const
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	if (!Emitter)
	{
		return nullptr;
	}

	UParticleLODLevel* LOD = GetSelectedLODLevel(Emitter);
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
		return "Initial Color";
	}
	if (Module->IsA<UParticleModuleColorOverLife>())
	{
		return "Color Over Life";
	}
	if (Module->IsA<UParticleModuleCollision>())
	{
		return "Collision";
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
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	const int32 LODCount = GetLODCount();
	if (DrawParticleToolbarButton("LowerLOD", L"Cascade_LowerLOD_512x.png", "Lower LOD", SelectedLODIndex <= 0))
	{
		SetSelectedLODIndex(SelectedLODIndex - 1);
	}
	ImGui::SameLine();
	if (DrawParticleToolbarButton("AddLODLeft", L"Cascade_AddLOD1_512x.png", "Add LOD", false))
	{
		AddLOD();
	}
	ImGui::SameLine();
	ImGui::TextUnformatted("LOD :");
	ImGui::SameLine();
	int32 EditableLODIndex = SelectedLODIndex;
	ImGui::SetNextItemWidth(42.0f);
	if (ImGui::InputInt("##ParticleSelectedLOD", &EditableLODIndex, 0, 0))
	{
		SetSelectedLODIndex(EditableLODIndex);
	}
	ImGui::SameLine();
	if (DrawParticleToolbarButton("AddLODRight", L"Cascade_AddLOD2_512x.png", "Add LOD", false))
	{
		AddLOD();
	}
	ImGui::SameLine();
	if (DrawParticleToolbarButton("HigherLOD", L"Cascade_HigherLOD_512x.png", "Higher LOD", SelectedLODIndex >= LODCount - 1))
	{
		SetSelectedLODIndex(SelectedLODIndex + 1);
	}
	ImGui::SameLine();
	if (DrawParticleToolbarButton("DeleteLOD", L"Cascade_DeleteLOD_512x.png", "Delete LOD", SelectedLODIndex <= 0 || LODCount <= 1))
	{
		DeleteSelectedLOD();
	}
	ImGui::SameLine();
	ImGui::TextDisabled("|");
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
			EditingParticleSystem->NormalizeLODData();
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

	int32 EmitterToDelete = -1;
	int32 EmitterToAddModule = -1;
	int32 EmitterToDeleteModule = -1;
	UParticleModule* ModuleToDelete = nullptr;
	EAddableModuleType ModuleTypeToAdd = EAddableModuleType::Lifetime;
	auto QueueAddModule = [&](int32 EmitterIndex, EAddableModuleType ModuleType)
	{
		EmitterToAddModule = EmitterIndex;
		ModuleTypeToAdd = ModuleType;
	};
	auto DrawEmitterContextMenu = [&](int32 EmitterIndex)
	{
		if (ImGui::BeginMenu("Add Module"))
		{
			if (ImGui::MenuItem("Lifetime"))
			{
				QueueAddModule(EmitterIndex, EAddableModuleType::Lifetime);
			}
			if (ImGui::MenuItem("Initial Size"))
			{
				QueueAddModule(EmitterIndex, EAddableModuleType::Size);
			}
			if (ImGui::MenuItem("Initial Velocity"))
			{
				QueueAddModule(EmitterIndex, EAddableModuleType::Velocity);
			}
			if (ImGui::MenuItem("Initial Location"))
			{
				QueueAddModule(EmitterIndex, EAddableModuleType::Location);
			}
			if (ImGui::MenuItem("Initial Color"))
			{
				QueueAddModule(EmitterIndex, EAddableModuleType::Color);
			}
			if (ImGui::MenuItem("Color Over Life"))
			{
				QueueAddModule(EmitterIndex, EAddableModuleType::ColorOverLife);
			}
			if (ImGui::MenuItem("Collision"))
			{
				QueueAddModule(EmitterIndex, EAddableModuleType::Collision);
			}
			ImGui::EndMenu();
		}

		ImGui::Separator();
		if (ImGui::MenuItem("Delete Emitter"))
		{
			EmitterToDelete = EmitterIndex;
		}
	};
	auto DrawModuleContextMenu = [&](int32 EmitterIndex, UParticleLODLevel* LOD, UParticleModule* Module)
	{
		DrawEmitterContextMenu(EmitterIndex);

		const bool bCanDeleteModule = LOD && Module &&
			std::find(LOD->Modules.begin(), LOD->Modules.end(), Module) != LOD->Modules.end();
		ImGui::Separator();
		if (ImGui::MenuItem("Delete Module", nullptr, false, bCanDeleteModule))
		{
			EmitterToDeleteModule = EmitterIndex;
			ModuleToDelete = Module;
		}
	};

	for (int32 Index = 0; Index < static_cast<int32>(EditingParticleSystem->Emitters.size()); ++Index)
	{
		UParticleEmitter* Emitter = EditingParticleSystem->Emitters[Index];
		UParticleLODLevel* LOD = GetSelectedLODLevel(Emitter);
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
		if (ImGui::BeginPopupContextItem("EmitterHeaderContext"))
		{
			SelectedEmitterIndex = Index;
			SelectedModule = LOD->RequiredModule;
			DrawEmitterContextMenu(Index);
			ImGui::EndPopup();
		}

		auto DrawModuleRow = [&](UParticleModule* Module, int32 ModuleIndex)
		{
			if (!Module)
			{
				return;
			}

			ImGui::PushID(Module);
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
			if (ImGui::BeginPopupContextItem("ModuleRowContext"))
			{
				SelectedEmitterIndex = Index;
				SelectedModule = Module;
				DrawModuleContextMenu(Index, LOD, Module);
				ImGui::EndPopup();
			}
			ImGui::PopID();
		};

		int32 ModuleIndex = 0;
		DrawModuleRow(LOD->RequiredModule, ModuleIndex++);
		DrawModuleRow(LOD->SpawnModule, ModuleIndex++);
		for (UParticleModule* Module : LOD->Modules)
		{
			DrawModuleRow(Module, ModuleIndex++);
		}

		if (ImGui::BeginPopupContextWindow("EmitterColumnContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			SelectedEmitterIndex = Index;
			SelectedModule = GetSelectedModule();
			DrawEmitterContextMenu(Index);
			ImGui::EndPopup();
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

	if (EmitterToDelete >= 0)
	{
		DeleteEmitter(EmitterToDelete);
	}
	else if (EmitterToDeleteModule >= 0)
	{
		DeleteModuleFromEmitter(EmitterToDeleteModule, ModuleToDelete);
	}
	else if (EmitterToAddModule >= 0)
	{
		AddModuleToEmitter(EmitterToAddModule, ModuleTypeToAdd);
	}
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
		float LifetimeMin = Lifetime->LifetimeMin;
		if (ImGui::DragFloat("Lifetime Min", &LifetimeMin, 0.05f, 0.0f, 1000.0f))
		{
			Lifetime->LifetimeMin = (std::max)(0.0f, LifetimeMin);
			Lifetime->Lifetime = Lifetime->LifetimeMax;
			bChanged = true;
		}

		float LifetimeMax = Lifetime->LifetimeMax;
		if (ImGui::DragFloat("Lifetime Max", &LifetimeMax, 0.05f, 0.0f, 1000.0f))
		{
			Lifetime->LifetimeMax = (std::max)(0.0f, LifetimeMax);
			Lifetime->Lifetime = Lifetime->LifetimeMax;
			bChanged = true;
		}
	}
	else if (UParticleModuleSize* Size = Cast<UParticleModuleSize>(Module))
	{
		FVector StartSizeMin = Size->StartSizeMin;
		if (ImGui::DragFloat3("Start Size Min", &StartSizeMin.X, 0.25f, 0.0f, 10000.0f))
		{
			Size->StartSizeMin = StartSizeMin;
			Size->StartSize = Size->StartSizeMax;
			bChanged = true;
		}

		FVector StartSizeMax = Size->StartSizeMax;
		if (ImGui::DragFloat3("Start Size Max", &StartSizeMax.X, 0.25f, 0.0f, 10000.0f))
		{
			Size->StartSizeMax = StartSizeMax;
			Size->StartSize = Size->StartSizeMax;
			bChanged = true;
		}
	}
	else if (UParticleModuleVelocity* Velocity = Cast<UParticleModuleVelocity>(Module))
	{
		FVector StartVelocityMin = Velocity->StartVelocityMin;
		if (ImGui::DragFloat3("Start Velocity Min", &StartVelocityMin.X, 0.5f, -10000.0f, 10000.0f))
		{
			Velocity->StartVelocityMin = StartVelocityMin;
			Velocity->StartVelocity = Velocity->StartVelocityMax;
			bChanged = true;
		}

		FVector StartVelocityMax = Velocity->StartVelocityMax;
		if (ImGui::DragFloat3("Start Velocity Max", &StartVelocityMax.X, 0.5f, -10000.0f, 10000.0f))
		{
			Velocity->StartVelocityMax = StartVelocityMax;
			Velocity->StartVelocity = Velocity->StartVelocityMax;
			bChanged = true;
		}
	}
	else if (UParticleModuleLocation* Location = Cast<UParticleModuleLocation>(Module))
	{
		FVector StartLocationMin = Location->StartLocationMin;
		if (ImGui::DragFloat3("Start Location Min", &StartLocationMin.X, 0.25f))
		{
			Location->StartLocationMin = StartLocationMin;
			Location->StartLocation = Location->StartLocationMax;
			bChanged = true;
		}

		FVector StartLocationMax = Location->StartLocationMax;
		if (ImGui::DragFloat3("Start Location Max", &StartLocationMax.X, 0.25f))
		{
			Location->StartLocationMax = StartLocationMax;
			Location->StartLocation = Location->StartLocationMax;
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

		float StartAlphaMin = Color->StartAlphaMin;
		if (ImGui::DragFloat("Start Alpha Min", &StartAlphaMin, 0.01f, 0.0f, 1.0f))
		{
			Color->StartAlphaMin = std::clamp(StartAlphaMin, 0.0f, 1.0f);
			Color->StartAlpha = Color->StartAlphaMax;
			bChanged = true;
		}

		float StartAlphaMax = Color->StartAlphaMax;
		if (ImGui::DragFloat("Start Alpha Max", &StartAlphaMax, 0.01f, 0.0f, 1.0f))
		{
			Color->StartAlphaMax = std::clamp(StartAlphaMax, 0.0f, 1.0f);
			Color->StartAlpha = Color->StartAlphaMax;
			bChanged = true;
		}
	}
	else if (UParticleModuleColorOverLife* ColorOverLife = Cast<UParticleModuleColorOverLife>(Module))
	{
		float EndColor[3] = { ColorOverLife->ColorOverLife.X, ColorOverLife->ColorOverLife.Y, ColorOverLife->ColorOverLife.Z };
		if (ImGui::ColorEdit3("Color Over Life", EndColor))
		{
			ColorOverLife->ColorOverLife = FVector(EndColor[0], EndColor[1], EndColor[2]);
			bChanged = true;
		}

		float EndAlpha = ColorOverLife->AlphaOverLife;
		if (ImGui::DragFloat("Alpha Over Life", &EndAlpha, 0.01f, 0.0f, 1.0f))
		{
			ColorOverLife->AlphaOverLife = std::clamp(EndAlpha, 0.0f, 1.0f);
			bChanged = true;
		}
	}
	else if (UParticleModuleCollision* Collision = Cast<UParticleModuleCollision>(Module))
	{
		int TraceChannel = static_cast<int>(Collision->TraceChannel);
		if (ImGui::Combo("Trace Channel", &TraceChannel, GCollisionChannelNames, IM_ARRAYSIZE(GCollisionChannelNames)))
		{
			Collision->TraceChannel = static_cast<ECollisionChannel>(std::clamp(TraceChannel, 0, NumActiveCollisionChannels - 1));
			bChanged = true;
		}

		int ResponseMode = static_cast<int>(Collision->ResponseMode);
		if (ImGui::Combo("Response Mode", &ResponseMode, GParticleCollisionResponseNames, IM_ARRAYSIZE(GParticleCollisionResponseNames)))
		{
			ResponseMode = std::clamp(ResponseMode, 0, static_cast<int>(IM_ARRAYSIZE(GParticleCollisionResponseNames)) - 1);
			Collision->ResponseMode = static_cast<EParticleCollisionResponseMode>(ResponseMode);
			bChanged = true;
		}

		float DampingFactor = Collision->DampingFactor;
		if (ImGui::DragFloat("Damping Factor", &DampingFactor, 0.01f, 0.0f, 1.0f))
		{
			Collision->DampingFactor = std::clamp(DampingFactor, 0.0f, 1.0f);
			bChanged = true;
		}

		float CollisionOffset = Collision->CollisionOffset;
		if (ImGui::DragFloat("Collision Offset", &CollisionOffset, 0.01f, 0.0f, 100.0f))
		{
			Collision->CollisionOffset = (std::max)(0.0f, CollisionOffset);
			bChanged = true;
		}

		int MaxCollisions = Collision->MaxCollisions;
		if (ImGui::DragInt("Max Collisions", &MaxCollisions, 1.0f, 0, 128))
		{
			Collision->MaxCollisions = (std::max)(0, MaxCollisions);
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
