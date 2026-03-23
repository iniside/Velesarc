// Copyright Lukasz Baran. All Rights Reserved.

#include "SArcMassStructPicker.h"

#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SSearchBox.h"

#define LOCTEXT_NAMESPACE "SArcMassStructPicker"

void SArcMassStructPicker::Construct(const FArguments& InArgs)
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
			.OnTextChanged_Lambda([this](const FText& Text)
			{
				SearchText = Text.ToString();
				RebuildFilteredTree();
			})
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SAssignNew(TreeView, STreeView<TSharedPtr<FArcMassStructPickerItem>>)
			.TreeItemsSource(&RootItems)
			.OnGenerateRow(this, &SArcMassStructPicker::OnGenerateRow)
			.OnGetChildren(this, &SArcMassStructPicker::OnGetChildren)
			.SelectionMode(ESelectionMode::Single)
		]
	];

	RebuildFilteredTree();
}

void SArcMassStructPicker::Populate()
{
	AllEntries.Empty();

	if (!BaseStruct) return;

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;
		if (!Struct->IsChildOf(BaseStruct)) continue;
		if (Struct == BaseStruct) continue;
		if (Struct->HasMetaData(TEXT("Hidden"))) continue;
		if (ExcludedStructs.Contains(Struct)) continue;

		FString DisplayName = Struct->GetMetaData(TEXT("DisplayName"));
		if (DisplayName.IsEmpty())
		{
			DisplayName = Struct->GetName();
			// Strip common prefixes
			DisplayName.RemoveFromStart(TEXT("F"));
		}

		FString Category = Struct->GetMetaData(TEXT("Category"));
		if (Category.IsEmpty())
		{
			// Use origin module name as default category
			UPackage* StructPackage = Struct->GetOutermost();
			if (StructPackage)
			{
				FString PackageName = StructPackage->GetName();
				int32 LastSlash = INDEX_NONE;
				if (PackageName.FindLastChar(TEXT('/'), LastSlash))
				{
					Category = PackageName.Mid(LastSlash + 1);
				}
				else
				{
					Category = PackageName;
				}
			}
			else
			{
				Category = TEXT("Default");
			}
		}

		FStructEntry Entry;
		Entry.DisplayName = DisplayName;
		Entry.Category = Category;
		Entry.Struct = Struct;
		AllEntries.Add(Entry);
	}
}

void SArcMassStructPicker::RebuildFilteredTree()
{
	RootItems.Empty();

	TMap<FString, TArray<TSharedPtr<FArcMassStructPickerItem>>> Categorized;

	for (const FStructEntry& Entry : AllEntries)
	{
		if (!SearchText.IsEmpty() && !Entry.DisplayName.Contains(SearchText))
		{
			continue;
		}

		// Build tooltip: struct name + doc comment
		FText Tooltip;
		if (Entry.Struct)
		{
			FString StructName = Entry.Struct->GetName();
			FText DocTooltip = Entry.Struct->GetToolTipText();
			if (DocTooltip.IsEmpty() || DocTooltip.ToString() == StructName)
			{
				Tooltip = FText::FromString(StructName);
			}
			else
			{
				Tooltip = FText::Format(LOCTEXT("StructPickerTooltipFmt", "{0}\n{1}"), FText::FromString(StructName), DocTooltip);
			}
		}

		TSharedPtr<FArcMassStructPickerItem> Leaf = MakeShared<FArcMassStructPickerItem>();
		Leaf->DisplayName = Entry.DisplayName;
		Leaf->TooltipText = Tooltip;
		Leaf->Struct = Entry.Struct;
		Categorized.FindOrAdd(Entry.Category).Add(Leaf);
	}

	TArray<FString> CategoryNames;
	Categorized.GetKeys(CategoryNames);
	CategoryNames.Sort();

	for (const FString& CatName : CategoryNames)
	{
		TSharedPtr<FArcMassStructPickerItem> CatItem = MakeShared<FArcMassStructPickerItem>();
		CatItem->DisplayName = CatName;
		CatItem->bIsCategory = true;

		TArray<TSharedPtr<FArcMassStructPickerItem>>& Entries = Categorized[CatName];
		Entries.Sort([](const TSharedPtr<FArcMassStructPickerItem>& A, const TSharedPtr<FArcMassStructPickerItem>& B)
		{
			return A->DisplayName < B->DisplayName;
		});
		CatItem->Children = MoveTemp(Entries);
		RootItems.Add(CatItem);
	}

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
		for (const TSharedPtr<FArcMassStructPickerItem>& Item : RootItems)
		{
			TreeView->SetItemExpansion(Item, true);
		}
	}
}

TSharedRef<ITableRow> SArcMassStructPicker::OnGenerateRow(
	TSharedPtr<FArcMassStructPickerItem> Item
	, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (Item->bIsCategory)
	{
		return SNew(STableRow<TSharedPtr<FArcMassStructPickerItem>>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->DisplayName))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		];
	}

	TWeakPtr<FArcMassStructPickerItem> WeakItem = Item;
	return SNew(STableRow<TSharedPtr<FArcMassStructPickerItem>>, OwnerTable)
	.ToolTipText(Item->TooltipText)
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(FMargin(4.f, 2.f))
		.OnClicked_Lambda([this, WeakItem]() -> FReply
		{
			TSharedPtr<FArcMassStructPickerItem> Pinned = WeakItem.Pin();
			if (Pinned.IsValid() && Pinned->Struct.IsValid())
			{
				OnStructPickedDelegate.ExecuteIfBound(Pinned->Struct.Get());
			}
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->DisplayName))
		]
	];
}

void SArcMassStructPicker::OnGetChildren(
	TSharedPtr<FArcMassStructPickerItem> Item
	, TArray<TSharedPtr<FArcMassStructPickerItem>>& OutChildren)
{
	if (Item.IsValid())
	{
		OutChildren = Item->Children;
	}
}

#undef LOCTEXT_NAMESPACE
