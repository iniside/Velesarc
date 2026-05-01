// Copyright Lukasz Baran. All Rights Reserved.

#include "Customizations/SArcAttributeRefPicker.h"
#include "Attributes/ArcAttribute.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/STableRow.h"

void SArcAttributeRefPicker::Construct(const FArguments& InArgs)
{
	OnAttributeSelected = InArgs._OnAttributeSelected;

	CollectAttributeNodes();
	FilteredRootNodes = RootNodes;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f)
		[
			SNew(SSearchBox)
			.OnTextChanged(this, &SArcAttributeRefPicker::OnFilterTextChanged)
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SAssignNew(TreeView, STreeView<TSharedPtr<FArcAttributePickerNode>>)
			.TreeItemsSource(&FilteredRootNodes)
			.OnGenerateRow(this, &SArcAttributeRefPicker::OnGenerateRow)
			.OnGetChildren(this, &SArcAttributeRefPicker::OnGetChildren)
			.OnSelectionChanged(this, &SArcAttributeRefPicker::OnSelectionChanged)
			.SelectionMode(ESelectionMode::Single)
		]
	];
}

void SArcAttributeRefPicker::CollectAttributeNodes()
{
	RootNodes.Empty();

	UScriptStruct* BaseStruct = FArcMassAttributesBase::StaticStruct();

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;
		if (!Struct->IsChildOf(BaseStruct) || Struct == BaseStruct)
		{
			continue;
		}

		TSharedPtr<FArcAttributePickerNode> ParentNode = MakeShared<FArcAttributePickerNode>();
		ParentNode->DisplayName = Struct->GetName();
		ParentNode->FragmentType = Struct;
		ParentNode->bIsLeaf = false;

		for (TFieldIterator<FStructProperty> PropIt(Struct); PropIt; ++PropIt)
		{
			FStructProperty* StructProp = *PropIt;
			if (StructProp->Struct == FArcAttribute::StaticStruct())
			{
				TSharedPtr<FArcAttributePickerNode> ChildNode = MakeShared<FArcAttributePickerNode>();
				ChildNode->DisplayName = StructProp->GetName();
				ChildNode->FragmentType = Struct;
				ChildNode->PropertyName = StructProp->GetFName();
				ChildNode->bIsLeaf = true;
				ParentNode->Children.Add(ChildNode);
			}
		}

		if (ParentNode->Children.Num() > 0)
		{
			RootNodes.Add(ParentNode);
		}
	}

	RootNodes.Sort([](const TSharedPtr<FArcAttributePickerNode>& A, const TSharedPtr<FArcAttributePickerNode>& B)
	{
		return A->DisplayName < B->DisplayName;
	});
}

TSharedRef<ITableRow> SArcAttributeRefPicker::OnGenerateRow(
	TSharedPtr<FArcAttributePickerNode> Node,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FArcAttributePickerNode>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Node->DisplayName))
		.Font(Node->bIsLeaf ? FCoreStyle::GetDefaultFontStyle("Regular", 9) : FCoreStyle::GetDefaultFontStyle("Bold", 9))
	];
}

void SArcAttributeRefPicker::OnGetChildren(
	TSharedPtr<FArcAttributePickerNode> Node,
	TArray<TSharedPtr<FArcAttributePickerNode>>& OutChildren)
{
	OutChildren = Node->Children;
}

void SArcAttributeRefPicker::OnSelectionChanged(
	TSharedPtr<FArcAttributePickerNode> SelectedNode,
	ESelectInfo::Type SelectInfo)
{
	if (SelectedNode.IsValid() && SelectedNode->bIsLeaf)
	{
		OnAttributeSelected.ExecuteIfBound(SelectedNode->FragmentType, SelectedNode->PropertyName);
	}
}

void SArcAttributeRefPicker::OnFilterTextChanged(const FText& InFilterText)
{
	FilterString = InFilterText.ToString();

	FilteredRootNodes.Empty();

	if (FilterString.IsEmpty())
	{
		FilteredRootNodes = RootNodes;
	}
	else
	{
		for (const TSharedPtr<FArcAttributePickerNode>& Root : RootNodes)
		{
			bool bRootMatches = Root->DisplayName.Contains(FilterString);

			TSharedPtr<FArcAttributePickerNode> FilteredRoot = MakeShared<FArcAttributePickerNode>();
			FilteredRoot->DisplayName = Root->DisplayName;
			FilteredRoot->FragmentType = Root->FragmentType;
			FilteredRoot->bIsLeaf = false;

			for (const TSharedPtr<FArcAttributePickerNode>& Child : Root->Children)
			{
				if (bRootMatches || Child->DisplayName.Contains(FilterString))
				{
					FilteredRoot->Children.Add(Child);
				}
			}

			if (FilteredRoot->Children.Num() > 0)
			{
				FilteredRootNodes.Add(FilteredRoot);
			}
		}
	}

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();

		for (const TSharedPtr<FArcAttributePickerNode>& Root : FilteredRootNodes)
		{
			TreeView->SetItemExpansion(Root, true);
		}
	}
}
