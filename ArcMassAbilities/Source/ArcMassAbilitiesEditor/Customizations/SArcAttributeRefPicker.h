// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STreeView.h"

struct FArcAttributePickerNode
{
	FString DisplayName;
	UScriptStruct* FragmentType = nullptr;
	FName PropertyName = NAME_None;
	bool bIsLeaf = false;
	TArray<TSharedPtr<FArcAttributePickerNode>> Children;
};

DECLARE_DELEGATE_TwoParams(FOnArcAttributeSelected, UScriptStruct*, FName);

class SArcAttributeRefPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcAttributeRefPicker) {}
		SLATE_EVENT(FOnArcAttributeSelected, OnAttributeSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void CollectAttributeNodes();

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FArcAttributePickerNode> Node, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(TSharedPtr<FArcAttributePickerNode> Node, TArray<TSharedPtr<FArcAttributePickerNode>>& OutChildren);
	void OnSelectionChanged(TSharedPtr<FArcAttributePickerNode> SelectedNode, ESelectInfo::Type SelectInfo);
	void OnFilterTextChanged(const FText& InFilterText);

	TArray<TSharedPtr<FArcAttributePickerNode>> RootNodes;
	TArray<TSharedPtr<FArcAttributePickerNode>> FilteredRootNodes;
	TSharedPtr<STreeView<TSharedPtr<FArcAttributePickerNode>>> TreeView;
	FOnArcAttributeSelected OnAttributeSelected;
	FString FilterString;
};
