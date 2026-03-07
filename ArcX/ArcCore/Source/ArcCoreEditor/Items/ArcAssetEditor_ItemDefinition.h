/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "Toolkits/IToolkitHost.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "SArcItemFragmentList.h"

class IDetailsView;
class IStructureDetailsView;
class SDockTab;
class SWidgetSwitcher;
class UArcItemDefinition;

class ARCCOREEDITOR_API FArcAssetEditor_ItemDefinition : public FSimpleAssetEditor
{
public:
	static const FName FragmentListTabId;
	static const FName DetailsTabId;
	static const FName ToolkitFName;
	static const FName AppIdentifier;

	static TSharedRef<FArcAssetEditor_ItemDefinition> CreateItemEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		UObject* ObjectToEdit);

	static TSharedRef<FArcAssetEditor_ItemDefinition> CreateItemEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		const TArray<UObject*>& ObjectsToEdit);

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
	void InitItemEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		UArcItemDefinition* ObjectToEdit);

	// Tab spawners
	TSharedRef<SDockTab> SpawnTab_FragmentList(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);

	// Fragment selection from the left panel
	void OnFragmentSelected(const UScriptStruct* SelectedStruct, EArcFragmentSetType SetType);
	void OnFragmentPropertyChanged(const FPropertyChangedEvent& Event);

	// Template operations (toolbar)
	void ExtendToolbar();
	void OnChangeTemplateClicked();
	void OnUpdateFromTemplateClicked();
	void OnPushToTemplateClicked();
	void RefreshAfterTemplateChange();
	FText GetTemplateNameText() const;
	bool HasSourceTemplate() const;

	UArcItemDefinition* GetEditingItemDefinition() const;

	// Widgets
	TSharedPtr<SArcItemFragmentList> FragmentListWidget;
	TSharedPtr<IDetailsView> GeneralDetailsView;
	TSharedPtr<IStructureDetailsView> FragmentDetailsView;
	TSharedPtr<SWidgetSwitcher> DetailsSwitcher;
};
