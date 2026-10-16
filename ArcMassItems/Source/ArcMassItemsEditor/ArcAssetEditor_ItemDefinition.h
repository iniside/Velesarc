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

#include "CoreMinimal.h"
#include "Editing/ArcJsonRecordEditing.h"
#include "Misc/NotifyHook.h"
#include "SArcItemFragmentList.h"
#include "Toolkits/AssetEditorToolkit.h"

class IStructureDetailsView;
class SDockTab;
class SWidgetSwitcher;
struct FArcItemDefinitionData;
struct FArcItemFragmentSelection;
struct FPropertyChangedEvent;

class FArcAssetEditor_ItemDefinition : public FAssetEditorToolkit, public FNotifyHook
{
public:
	static const FName FragmentListTabId;
	static const FName DetailsTabId;
	static const FName ToolkitFName;
	static const FName AppIdentifier;

	static bool OpenRecord(const FString& Filename);

	virtual ~FArcAssetEditor_ItemDefinition() override;

	// FAssetEditorToolkit overrides
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override { return ToolkitFName; }
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetTabSuffix() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual bool CanSaveAsset() const override;
	virtual void SaveAsset_Execute() override;
	virtual bool CanSaveAssetAs() const override { return false; }
	virtual bool IsSaveAssetAsVisible() const override { return false; }
	virtual bool CanFindInContentBrowser() const override { return Record.IsValid(); }
	virtual void FindInContentBrowser_Execute() override;
	virtual bool OnRequestClose(EAssetEditorCloseReason InCloseReason) override;
	virtual void OnClose() override;
	virtual bool IncludeAssetInRestoreOpenAssetsPrompt(UObject* Asset) const override { return false; }

	// FNotifyHook
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange) override;
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;

private:
	void InitEditor(UArcJsonRecordEditObject* InRecord);

	TSharedRef<SDockTab> SpawnTab_FragmentList(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);

	void OnFragmentSelected(const UScriptStruct* SelectedStruct, EArcFragmentSetType SetType);

	FArcItemDefinitionData* GetData() const;
	bool SaveRecord();
	void RunPreSave();

	void HandleRecordChanged(UArcJsonRecordEditObject* Object, EArcJsonRecordEditChange Change);
	void RefreshFromRecord();
	void RefreshDetails();
	void ReleaseRecord();

	TWeakObjectPtr<UArcJsonRecordEditObject> Record;
	FDelegateHandle RecordChangedHandle;
	bool bClosing = false;
	uint64 DiscardOnCloseFrame = MAX_uint64;

	TSharedPtr<SArcItemFragmentList> FragmentListWidget;
	TSharedPtr<IStructureDetailsView> GeneralDetailsView;
	TSharedPtr<IStructureDetailsView> FragmentDetailsView;
	TSharedPtr<FArcItemFragmentSelection> FragmentSelection;
	TSharedPtr<SWidgetSwitcher> DetailsSwitcher;
};
