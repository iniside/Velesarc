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

#include "Customizations/SArcScalableCurveFloatPicker.h"
#include "ArcMassItemsRuntime/Items/Fragments/ArcItemFragment.h"
#include "ArcScalableFloat.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/STableRow.h"

void SArcScalableCurveFloatPicker::Construct(const FArguments& InArgs)
{
	OnSelected = InArgs._OnSelected;

	CollectPickerNodes();
	FilteredRootNodes = RootNodes;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f)
		[
			SNew(SSearchBox)
			.OnTextChanged(this, &SArcScalableCurveFloatPicker::OnFilterTextChanged)
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SAssignNew(TreeView, STreeView<TSharedPtr<FArcScalableCurveFloatPickerNode>>)
			.TreeItemsSource(&FilteredRootNodes)
			.OnGenerateRow(this, &SArcScalableCurveFloatPicker::OnGenerateRow)
			.OnGetChildren(this, &SArcScalableCurveFloatPicker::OnGetChildren)
			.OnSelectionChanged(this, &SArcScalableCurveFloatPicker::OnSelectionChanged)
			.SelectionMode(ESelectionMode::Single)
		]
	];
}

void SArcScalableCurveFloatPicker::CollectPickerNodes()
{
	RootNodes.Empty();

	UScriptStruct* BaseStruct = FArcScalableFloatItemFragment::StaticStruct();
	UScriptStruct* LeafStruct = FArcScalableFloat::StaticStruct();

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;
		if (!Struct->IsChildOf(BaseStruct) || Struct == BaseStruct)
		{
			continue;
		}

		if (Struct->HasMetaData(TEXT("Hidden")))
		{
			continue;
		}

		TSharedPtr<FArcScalableCurveFloatPickerNode> ParentNode = MakeShared<FArcScalableCurveFloatPickerNode>();
		ParentNode->DisplayName = Struct->GetName();
		ParentNode->FragmentType = Struct;
		ParentNode->bIsLeaf = false;

		for (TFieldIterator<FStructProperty> PropIt(Struct); PropIt; ++PropIt)
		{
			FStructProperty* StructProp = *PropIt;
			if (StructProp->Struct == LeafStruct)
			{
				TSharedPtr<FArcScalableCurveFloatPickerNode> ChildNode = MakeShared<FArcScalableCurveFloatPickerNode>();
				ChildNode->DisplayName = StructProp->GetName();
				ChildNode->FragmentType = Struct;
				ChildNode->StructProperty = StructProp;
				ChildNode->bIsLeaf = true;
				ParentNode->Children.Add(ChildNode);
			}
		}

		if (ParentNode->Children.Num() > 0)
		{
			RootNodes.Add(ParentNode);
		}
	}

	RootNodes.Sort([](const TSharedPtr<FArcScalableCurveFloatPickerNode>& A, const TSharedPtr<FArcScalableCurveFloatPickerNode>& B)
	{
		return A->DisplayName < B->DisplayName;
	});
}

TSharedRef<ITableRow> SArcScalableCurveFloatPicker::OnGenerateRow(
	TSharedPtr<FArcScalableCurveFloatPickerNode> Node,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FArcScalableCurveFloatPickerNode>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Node->DisplayName))
		.Font(Node->bIsLeaf ? FCoreStyle::GetDefaultFontStyle("Regular", 9) : FCoreStyle::GetDefaultFontStyle("Bold", 9))
	];
}

void SArcScalableCurveFloatPicker::OnGetChildren(
	TSharedPtr<FArcScalableCurveFloatPickerNode> Node,
	TArray<TSharedPtr<FArcScalableCurveFloatPickerNode>>& OutChildren)
{
	OutChildren = Node->Children;
}

void SArcScalableCurveFloatPicker::OnSelectionChanged(
	TSharedPtr<FArcScalableCurveFloatPickerNode> SelectedNode,
	ESelectInfo::Type SelectInfo)
{
	if (SelectedNode.IsValid() && SelectedNode->bIsLeaf)
	{
		OnSelected.ExecuteIfBound(SelectedNode->FragmentType, SelectedNode->StructProperty);
	}
}

void SArcScalableCurveFloatPicker::OnFilterTextChanged(const FText& InFilterText)
{
	FilterString = InFilterText.ToString();

	FilteredRootNodes.Empty();

	if (FilterString.IsEmpty())
	{
		FilteredRootNodes = RootNodes;
	}
	else
	{
		for (const TSharedPtr<FArcScalableCurveFloatPickerNode>& Root : RootNodes)
		{
			bool bRootMatches = Root->DisplayName.Contains(FilterString);

			TSharedPtr<FArcScalableCurveFloatPickerNode> FilteredRoot = MakeShared<FArcScalableCurveFloatPickerNode>();
			FilteredRoot->DisplayName = Root->DisplayName;
			FilteredRoot->FragmentType = Root->FragmentType;
			FilteredRoot->bIsLeaf = false;

			for (const TSharedPtr<FArcScalableCurveFloatPickerNode>& Child : Root->Children)
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

		for (const TSharedPtr<FArcScalableCurveFloatPickerNode>& Root : FilteredRootNodes)
		{
			TreeView->SetItemExpansion(Root, true);
		}
	}
}
