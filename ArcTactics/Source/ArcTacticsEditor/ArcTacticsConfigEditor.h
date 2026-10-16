// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Toolkits/AssetEditorToolkit.h"

class FArcTacticsEditorViewModel;
class IDetailsView;
class IToolkitHost;
class SDockTab;
class SWidgetSwitcher;
class SArcTacticsView;
class UArcTacticsConfig;
class FSpawnTabArgs;
struct FArcTacticsEditorSelection;
struct FPropertyBindingPath;

class FArcTacticsConfigEditor final : public FAssetEditorToolkit
{
public:
	void Init(
		EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		UArcTacticsConfig* InConfig);

	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

	TSharedPtr<FArcTacticsEditorViewModel> GetViewModel() const { return ViewModel; }

private:
	TSharedRef<SDockTab> SpawnEditorTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnSimulatorTab(const FSpawnTabArgs& Args);
	void OpenSimulatorTab();
	void ExtendToolbar();
	void CompileConfig();
	void NavigateToBindingPath(bool bIsStruct, const FPropertyBindingPath& BindingPath);
	void NavigateToDiagnostic(FGuid NodeID);
	void NavigateToMemoryUser(FGuid StructID, FName PropertyName);
	void FocusSelectionProperty(
		const FArcTacticsEditorSelection& Selection,
		const FPropertyBindingPath* BindingPath,
		FName PropertyName);

	TSharedPtr<FArcTacticsEditorViewModel> ViewModel;
	TSharedPtr<SArcTacticsView> TacticsView;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SWidgetSwitcher> BottomPanelSwitcher;

	static const FName EditorTabID;
	static const FName SimulatorTabID;
};
