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

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class UArcJsonRecordEditObject;
struct FArcInstancedStruct;

/** Which fragment set a fragment belongs to. */
enum class EArcFragmentSetType : uint8
{
	Runtime,       // FragmentSet (FArcItemFragment base)
	ScalableFloat  // ScalableFloatFragmentSet (FArcScalableFloatItemFragment base)
};

/** An item in the fragment tree - either a group header or a leaf fragment. */
struct FArcFragmentListItem
{
	FString DisplayName;
	bool bIsGroup = false;
	bool bIsInvalid = false;

	// For leaf nodes (fragments):
	EArcFragmentSetType SetType = EArcFragmentSetType::Runtime;
	TWeakObjectPtr<const UScriptStruct> ScriptStruct;

	// For invalid leaf nodes: the stored FName of a fragment whose UScriptStruct could not be resolved.
	FName InvalidStructName = NAME_None;

	// For group nodes:
	TArray<TSharedPtr<FArcFragmentListItem>> Children;
};

DECLARE_DELEGATE_TwoParams(FArcOnFragmentSelected, const UScriptStruct* /*SelectedStruct*/, EArcFragmentSetType /*SetType*/);

/**
 * Left panel widget for the Item Definition editor.
 * Shows a tree of all fragments from FragmentSet and ScalableFloatFragmentSet of the item record.
 * Supports add/remove and single selection.
 */
class SArcItemFragmentList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcItemFragmentList) {}
		SLATE_EVENT(FArcOnFragmentSelected, OnFragmentSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UArcJsonRecordEditObject* InRecord);

	/** Rebuild the tree from current item record state. */
	void RebuildTree();

	void RebuildTreeKeepingSelection();

private:
	// Tree view callbacks
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FArcFragmentListItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(TSharedPtr<FArcFragmentListItem> Item, TArray<TSharedPtr<FArcFragmentListItem>>& OutChildren);
	void OnSelectionChanged(TSharedPtr<FArcFragmentListItem> Item, ESelectInfo::Type SelectInfo);

	// Add fragment: creates struct viewer popup for the given base struct
	TSharedRef<SWidget> CreateStructViewerWidget(EArcFragmentSetType SetType);
	void OnStructPicked(const UScriptStruct* InStruct, EArcFragmentSetType SetType);

	// Per-item replace: dropdown on each leaf to swap fragment type in-place
	TSharedRef<SWidget> CreateReplacementViewerWidget(TSharedPtr<FArcFragmentListItem> Item);
	void OnReplacementPicked(const UScriptStruct* NewStruct, TSharedPtr<FArcFragmentListItem> OldItem);

	// Per-item remove
	void RemoveFragment(TSharedPtr<FArcFragmentListItem> Item);

	// Wipe every fragment from both sets (Runtime, ScalableFloat).
	void ClearAllFragments();

	// Helpers
	TSet<FArcInstancedStruct>* GetMutableSet(EArcFragmentSetType SetType);
	void RehashSet(EArcFragmentSetType SetType);

	// Re-select a fragment after tree rebuild (fires the selection delegate)
	void RestoreSelection(const UScriptStruct* StructToSelect, EArcFragmentSetType SetType);

	TSharedPtr<STreeView<TSharedPtr<FArcFragmentListItem>>> TreeView;
	TArray<TSharedPtr<FArcFragmentListItem>> RootItems;
	TWeakObjectPtr<UArcJsonRecordEditObject> Record;
	FArcOnFragmentSelected OnFragmentSelectedDelegate;

	// Track current selection for preserving it across add/remove/replace rebuilds
	TWeakObjectPtr<const UScriptStruct> SelectedScriptStruct;
	EArcFragmentSetType SelectedSetType = EArcFragmentSetType::Runtime;
};
