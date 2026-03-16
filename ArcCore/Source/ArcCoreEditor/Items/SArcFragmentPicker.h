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
#include "Widgets/Input/SSearchBox.h"

DECLARE_DELEGATE_OneParam(FArcOnFragmentPickerPicked, const UScriptStruct* /*PickedStruct*/);

/** A node in the fragment picker tree — either a category header or a pickable struct leaf. */
struct FArcFragmentPickerNode
{
	FString DisplayName;
	bool bIsCategory = false;
	TWeakObjectPtr<const UScriptStruct> Struct;
	TArray<TSharedPtr<FArcFragmentPickerNode>> Children;
};

/**
 * Lightweight struct picker that groups available fragment structs by their Category metadata.
 * Uncategorized structs appear first, sorted alphabetically. Category groups follow, also sorted.
 * Includes a search box for real-time filtering by display name.
 */
class SArcFragmentPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcFragmentPicker) {}
		/** Base struct — only children of this struct are shown. */
		SLATE_ARGUMENT(const UScriptStruct*, BaseStruct)
		/** Structs to exclude from the list (already used, current struct, etc.). */
		SLATE_ARGUMENT(TSet<const UScriptStruct*>, ExcludedStructs)
		/** Fired when the user clicks a struct leaf. */
		SLATE_EVENT(FArcOnFragmentPickerPicked, OnStructPicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void Populate();
	void RebuildFilteredTree();

	// Tree view callbacks
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FArcFragmentPickerNode> Node, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(TSharedPtr<FArcFragmentPickerNode> Node, TArray<TSharedPtr<FArcFragmentPickerNode>>& OutChildren);

	void OnSearchTextChanged(const FText& InFilterText);

	const UScriptStruct* BaseStruct = nullptr;
	TSet<const UScriptStruct*> ExcludedStructs;
	FArcOnFragmentPickerPicked OnStructPickedDelegate;
	FString SearchText;

	struct FStructEntry
	{
		FString DisplayName;
		FString Category;
		const UScriptStruct* Struct;
	};

	/** All valid structs (unfiltered). Built once in Populate(). */
	TArray<FStructEntry> AllEntries;

	/** Current tree nodes (rebuilt on search change). */
	TArray<TSharedPtr<FArcFragmentPickerNode>> RootNodes;

	TSharedPtr<STreeView<TSharedPtr<FArcFragmentPickerNode>>> TreeView;
};
