// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

DECLARE_DELEGATE_OneParam(FArcOnMassStructPicked, const UScriptStruct* /*PickedStruct*/);

struct FArcMassStructPickerItem
{
	FString DisplayName;
	FText TooltipText;
	bool bIsCategory = false;
	TWeakObjectPtr<const UScriptStruct> Struct;
	TArray<TSharedPtr<FArcMassStructPickerItem>> Children;
};

/**
 * Struct picker for Mass fragment/tag types.
 * Shows all non-abstract children of a given base struct, grouped by Category metadata
 * (falling back to origin module name), with search filtering and alphabetical sorting.
 */
class SArcMassStructPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcMassStructPicker) {}
		SLATE_ARGUMENT(const UScriptStruct*, BaseStruct)
		SLATE_ARGUMENT(TSet<const UScriptStruct*>, ExcludedStructs)
		SLATE_EVENT(FArcOnMassStructPicked, OnStructPicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void Populate();
	void RebuildFilteredTree();

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FArcMassStructPickerItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(TSharedPtr<FArcMassStructPickerItem> Item, TArray<TSharedPtr<FArcMassStructPickerItem>>& OutChildren);

	const UScriptStruct* BaseStruct = nullptr;
	TSet<const UScriptStruct*> ExcludedStructs;
	FArcOnMassStructPicked OnStructPickedDelegate;
	FString SearchText;

	struct FStructEntry
	{
		FString DisplayName;
		FString Category;
		const UScriptStruct* Struct;
	};

	TArray<FStructEntry> AllEntries;
	TArray<TSharedPtr<FArcMassStructPickerItem>> RootItems;
	TSharedPtr<STreeView<TSharedPtr<FArcMassStructPickerItem>>> TreeView;
};
