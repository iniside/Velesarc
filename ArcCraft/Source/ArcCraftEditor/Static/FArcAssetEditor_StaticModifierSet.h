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
#include "PropertyEditorModule.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/ArcModifierDebugContext.h"
#include "Widgets/ArcModifierTreeItem.h"

class IDetailsView;
class IStructureDetailsView;
class SBox;
class SDockTab;
class SArcModifierTreeView;
class SArcModifierDebugToolbar;
class SArcModifierResultsPanel;
class UArcCraftModifierRowProxy;
struct FArcStaticModifierSetData;
struct FPropertyChangedEvent;

/**
 * Record editor for .arcstaticmods files, editing the shared record edit object.
 * Three tabs: Details (struct view), ModifierTable (SArcModifierTreeView), Debug (SArcModifierDebugToolbar).
 */
class FArcAssetEditor_StaticModifierSet : public FAssetEditorToolkit, public FNotifyHook
{
public:
	static const FName DetailsTabId;
	static const FName ModifierTableTabId;
	static const FName DebugTabId;
	static const FName ToolkitFName;
	static const FName AppIdentifier;

	static bool OpenRecord(const FString& Filename);

	virtual ~FArcAssetEditor_StaticModifierSet() override;

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
	// No Save As: a copy would repeat the record GUID.
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

	TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_ModifierTable(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Debug(const FSpawnTabArgs& Args);

	FArcStaticModifierSetData* GetData() const;

	void HandleRecordChanged(UArcJsonRecordEditObject* Object, EArcJsonRecordEditChange Change);
	void RefreshFromRecord();
	void RefreshDetails();
	void ReleaseRecord();

	void OnDebugContextChanged(const FArcModifierDebugContext& NewContext);

	// Action handlers — bound to SArcModifierTreeView delegates
	void HandleAddModifier(TSharedPtr<FArcModifierTreeItem> Selected);
	void HandleDuplicateRow(TSharedPtr<FArcModifierTreeItem> Selected);
	void HandleDeleteRow(TSharedPtr<FArcModifierTreeItem> Selected);
	void HandleReorderTopLevel(int32 From, int32 To);
	void HandleRowSelected(TSharedPtr<FArcModifierTreeItem> Selected);
	void HandleRowPropertyChanged(const FPropertyChangedEvent& Event);

	/**
	 * Copies the selected row's data into ModifierProxy and calls
	 * RowDetailsView->SetObject. Null Selected clears the panel.
	 */
	void ShowRowSelection(TSharedPtr<FArcModifierTreeItem> Selected);
	void RefreshActiveRow();

	/** Row type active in the details panel — drives write-back in HandleRowPropertyChanged. */
	enum class ERowKind
	{
		None,
		Modifier
	};

	TWeakObjectPtr<UArcJsonRecordEditObject> Record;
	FDelegateHandle RecordChangedHandle;
	FPrimaryAssetId RecordId;
	bool bApplyingEdit = false;
	bool bClosing = false;
	uint64 DiscardOnCloseFrame = MAX_uint64;

	TSharedPtr<IStructureDetailsView> DetailsView;
	TSharedPtr<IDetailsView> RowDetailsView;
	TSharedPtr<SBox> RowDetailsHost;
	FDetailsViewArgs RowDetailsArgs;
	TSharedPtr<SArcModifierTreeView> ModifierTableWidget;
	TSharedPtr<SArcModifierDebugToolbar> DebugToolbar;
	TSharedPtr<SArcModifierResultsPanel> ResultsPanel;

	TStrongObjectPtr<UArcCraftModifierRowProxy> ModifierProxy;

	ERowKind ActiveKind = ERowKind::None;
	int32 ActiveChildIdx = INDEX_NONE;
};
