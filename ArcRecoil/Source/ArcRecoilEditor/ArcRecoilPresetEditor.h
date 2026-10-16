// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Preview/ArcRecoilPreviewSim.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/WeakObjectPtr.h"

class IDetailsView;
class SArcRecoilPatternPanel;
class SDockTab;
class UArcRecoilPreset;
class UArcRecoilPreviewSettings;
struct FPropertyChangedEvent;

class FArcRecoilPresetEditor : public FSimpleAssetEditor
{
public:
	static const FName PresetDetailsTabId;
	static const FName PreviewSettingsTabId;
	static const FName PatternTabId;
	static const FName ToolkitFName;
	static const FName AppIdentifier;

	static TSharedRef<FArcRecoilPresetEditor> CreateEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		UArcRecoilPreset* PresetToEdit);

	virtual ~FArcRecoilPresetEditor() override;

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override { return ToolkitFName; }
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void PostRegenerateMenusAndToolbars() override;

private:
	void InitPresetEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		UArcRecoilPreset* PresetToEdit);

	TSharedRef<SDockTab> SpawnTab_PresetDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_PreviewSettings(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Pattern(const FSpawnTabArgs& Args);

	void HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event);
	void Resimulate(const ArcRecoil::Preview::FArcRecoilPreviewQuality& Quality);

	TWeakObjectPtr<UArcRecoilPreset> EditedPreset;
	TStrongObjectPtr<UArcRecoilPreviewSettings> PreviewSettings;
	TSharedPtr<const ArcRecoil::Preview::FArcRecoilPreviewResult> SimResult;
	TSharedPtr<IDetailsView> PresetDetailsView;
	TSharedPtr<IDetailsView> PreviewSettingsDetailsView;
	TSharedPtr<SArcRecoilPatternPanel> PatternPanel;
	FDelegateHandle PropertyChangedHandle;
};
