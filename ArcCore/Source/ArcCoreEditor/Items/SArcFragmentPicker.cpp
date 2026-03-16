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

#include "SArcFragmentPicker.h"

#include "Styling/AppStyle.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "SArcFragmentPicker"

void SArcFragmentPicker::Construct(const FArguments& InArgs)
{
	BaseStruct = InArgs._BaseStruct;
	ExcludedStructs = InArgs._ExcludedStructs;
	OnStructPickedDelegate = InArgs._OnStructPicked;

	Populate();

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f)
		[
			SNew(SSearchBox)
			.OnTextChanged(this, &SArcFragmentPicker::OnSearchTextChanged)
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SAssignNew(TreeView, STreeView<TSharedPtr<FArcFragmentPickerNode>>)
			.TreeItemsSource(&RootNodes)
			.OnGenerateRow(this, &SArcFragmentPicker::OnGenerateRow)
			.OnGetChildren(this, &SArcFragmentPicker::OnGetChildren)
			.SelectionMode(ESelectionMode::None)
		]
	];

	RebuildFilteredTree();
}

void SArcFragmentPicker::Populate()
{
	AllEntries.Empty();

	if (!BaseStruct) return;

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		const UScriptStruct* Struct = *It;
		if (!Struct || Struct == BaseStruct) continue;
		if (!Struct->IsChildOf(BaseStruct)) continue;
		if (Struct->HasMetaData(TEXT("Hidden"))) continue;
		if (ExcludedStructs.Contains(Struct)) continue;

		FString Display = Struct->GetMetaData(TEXT("DisplayName"));
		FString Category = Struct->GetMetaData(TEXT("Category"));

		AllEntries.Add({
			Display.Len() > 0 ? Display : Struct->GetName(),
			Category,
			Struct
		});
	}
}

void SArcFragmentPicker::RebuildFilteredTree()
{
	RootNodes.Empty();

	TArray<FStructEntry> Uncategorized;
	TMap<FString, TArray<FStructEntry>> Categorized;

	for (const FStructEntry& Entry : AllEntries)
	{
		if (!SearchText.IsEmpty() && !Entry.DisplayName.Contains(SearchText))
		{
			continue;
		}

		if (Entry.Category.Len() > 0)
		{
			Categorized.FindOrAdd(Entry.Category).Add(Entry);
		}
		else
		{
			Uncategorized.Add(Entry);
		}
	}

	auto SortByName = [](const FStructEntry& A, const FStructEntry& B) { return A.DisplayName < B.DisplayName; };

	// Uncategorized leaves first
	Uncategorized.Sort(SortByName);
	for (const FStructEntry& Entry : Uncategorized)
	{
		TSharedPtr<FArcFragmentPickerNode> Node = MakeShared<FArcFragmentPickerNode>();
		Node->DisplayName = Entry.DisplayName;
		Node->Struct = Entry.Struct;
		RootNodes.Add(Node);
	}

	// Category groups sorted alphabetically
	TArray<FString> CategoryNames;
	Categorized.GetKeys(CategoryNames);
	CategoryNames.Sort();
	for (const FString& Cat : CategoryNames)
	{
		TSharedPtr<FArcFragmentPickerNode> CatNode = MakeShared<FArcFragmentPickerNode>();
		CatNode->DisplayName = Cat;
		CatNode->bIsCategory = true;

		TArray<FStructEntry>& Entries = Categorized[Cat];
		Entries.Sort(SortByName);
		for (const FStructEntry& Entry : Entries)
		{
			TSharedPtr<FArcFragmentPickerNode> Leaf = MakeShared<FArcFragmentPickerNode>();
			Leaf->DisplayName = Entry.DisplayName;
			Leaf->Struct = Entry.Struct;
			CatNode->Children.Add(Leaf);
		}
		RootNodes.Add(CatNode);
	}

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
		for (const TSharedPtr<FArcFragmentPickerNode>& Node : RootNodes)
		{
			if (Node->bIsCategory)
			{
				TreeView->SetItemExpansion(Node, true);
			}
		}
	}
}

TSharedRef<ITableRow> SArcFragmentPicker::OnGenerateRow(
	TSharedPtr<FArcFragmentPickerNode> Node,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	if (Node->bIsCategory)
	{
		return SNew(STableRow<TSharedPtr<FArcFragmentPickerNode>>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Node->DisplayName))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		];
	}

	TWeakPtr<FArcFragmentPickerNode> WeakNode = Node;
	return SNew(STableRow<TSharedPtr<FArcFragmentPickerNode>>, OwnerTable)
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(FMargin(4.f, 2.f))
		.OnClicked_Lambda([this, WeakNode]() -> FReply
		{
			TSharedPtr<FArcFragmentPickerNode> Pinned = WeakNode.Pin();
			if (Pinned.IsValid() && Pinned->Struct.IsValid())
			{
				OnStructPickedDelegate.ExecuteIfBound(Pinned->Struct.Get());
			}
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Text(FText::FromString(Node->DisplayName))
			.Font(FAppStyle::GetFontStyle("NormalFont"))
		]
	];
}

void SArcFragmentPicker::OnGetChildren(
	TSharedPtr<FArcFragmentPickerNode> Node,
	TArray<TSharedPtr<FArcFragmentPickerNode>>& OutChildren)
{
	if (Node.IsValid())
	{
		OutChildren = Node->Children;
	}
}

void SArcFragmentPicker::OnSearchTextChanged(const FText& InFilterText)
{
	SearchText = InFilterText.ToString();
	RebuildFilteredTree();
}

#undef LOCTEXT_NAMESPACE
