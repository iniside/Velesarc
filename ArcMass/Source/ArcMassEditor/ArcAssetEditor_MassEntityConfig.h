// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Toolkits/IToolkitHost.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "SArcMassTraitList.h"

class IDetailsView;
class IStructureDetailsView;
class SDockTab;
class SWidgetSwitcher;
class UMassEntityConfigAsset;

class ARCMASSEDITOR_API FArcAssetEditor_MassEntityConfig : public FSimpleAssetEditor
{
public:
	static const FName TraitListTabId;
	static const FName DetailsTabId;
	static const FName ToolkitFName;
	static const FName AppIdentifier;

	static TSharedRef<FArcAssetEditor_MassEntityConfig> CreateEditor(
		const EToolkitMode::Type Mode
		, const TSharedPtr<IToolkitHost>& InitToolkitHost
		, UObject* ObjectToEdit);

	// FAssetEditorToolkit overrides
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override { return ToolkitFName; }
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual void PostRegenerateMenusAndToolbars() override;

private:
	void InitEditor(
		const EToolkitMode::Type Mode
		, const TSharedPtr<IToolkitHost>& InitToolkitHost
		, UMassEntityConfigAsset* ObjectToEdit);

	// Tab spawners
	TSharedRef<SDockTab> SpawnTab_TraitList(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);

	// Trait selection from the left panel
	void OnTraitSelected(TSharedPtr<FArcMassTraitListItem> SelectedItem);

	// Config property changes (parent changed, etc.)
	void OnConfigPropertyChanged(const FPropertyChangedEvent& Event);

	// Toolbar
	void ExtendToolbar();
	void OnVerifyClicked();

	UMassEntityConfigAsset* GetEditingConfigAsset() const;

	// Show flattened fragments/tags from an assorted trait in the scroll box (switcher slot 3)
	void ShowAssortedFragmentsFlattened(UMassEntityTraitBase* AssortedTrait, bool bShowFragments = true, bool bShowTags = true);

	// Widgets
	TSharedPtr<SArcMassTraitList> TraitListWidget;
	TSharedPtr<IDetailsView> TraitDetailsView;
	TSharedPtr<IStructureDetailsView> StructDetailsView;
	TSharedPtr<IDetailsView> ConfigDetailsView;
	TSharedPtr<SWidgetSwitcher> DetailsSwitcher;
	TSharedPtr<SScrollBox> AssortedFragmentsScrollBox;

	// Keep struct data alive while displayed
	TArray<TSharedPtr<FStructOnScope>> ActiveStructScopes;
	TArray<TSharedPtr<IStructureDetailsView>> ActiveStructDetailViews;
};
