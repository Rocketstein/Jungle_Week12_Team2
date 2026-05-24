#pragma once

#include "AssetEditorWidget.h"
#include "Editor/Viewport/StaticMeshEditorViewportClient.h"
#include "Object/FName.h"
#include "Slate/SWindow.h"

#include <imgui.h>

class AActor;
class UParticleEmitter;
class UParticleModule;
class UParticleModuleRequired;
class UParticleSystem;
class UParticleSystemComponent;

class FParticleEditorWidget : public FAssetEditorWidget
{
public:
	FParticleEditorWidget();

	bool CanEdit(UObject* Object) const override;
	bool IsEditingObject(UObject* Object) const override;

	void Open(UObject* Object) override;
	void Close() override;
	void Tick(float DeltaTime) override;
	void Render(float DeltaTime) override;

	void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const override;
	bool AllowsMultipleInstances() const override { return true; }

private:
	void EnsureDefaultSystem();
	UParticleEmitter* CreateDefaultEmitter(const FString& EmitterName);
	void InitializePreviewWorld();
	void ReleasePreviewWorld();
	void RestartPreviewSystem();

	void RenderToolbar();
	void RenderEditorLayout();
	void RenderPreviewViewport(const ImVec2& Size);
	void RenderEmitterList();
	bool RenderDetailsPanel();
	bool RenderRequiredDetails(UParticleModuleRequired* Required);
	bool RenderModuleDetails(UParticleModule* Module);
	void RenderCurvePanel();

	UParticleEmitter* GetSelectedEmitter() const;
	UParticleModuleRequired* GetSelectedRequiredModule() const;
	UParticleModule* GetSelectedModule() const;
	FString GetEmitterDisplayName(UParticleEmitter* Emitter, int32 Index) const;
	FString GetModuleDisplayName(UParticleModule* Module) const;
	void ApplyEmitterEdit();

private:
	SWindow ParticleViewportWindow;
	FStaticMeshEditorViewportClient ViewportClient;
	UParticleSystem* EditingParticleSystem = nullptr;
	UParticleSystemComponent* PreviewParticleComponent = nullptr;
	AActor* PreviewActor = nullptr;

	int32 SelectedEmitterIndex = 0;
	UParticleModule* SelectedModule = nullptr;
	bool bSimulating = true;

	uint32 InstanceId = 0;
	FName PreviewWorldHandle = FName::None;
	FString WindowIdSuffix;
};
