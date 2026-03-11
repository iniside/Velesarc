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

#include "SArcItemFragmentList.h"

#include "ArcCore/Items/ArcItemDefinition.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "Items/ArcEditorItemFragment.h"
#include "SArcFragmentPicker.h"
#include "ScopedTransaction.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "SArcItemFragmentList"

namespace ArcItemEditorPrivate
{
	static const UScriptStruct* GetBaseStructForSetType(EArcFragmentSetType SetType)
	{
		switch (SetType)
		{
		case EArcFragmentSetType::Runtime: return FArcItemFragment::StaticStruct();
		case EArcFragmentSetType::ScalableFloat: return FArcScalableFloatItemFragment::StaticStruct();
		case EArcFragmentSetType::Editor: return FArcEditorItemFragment::StaticStruct();
		}
		return nullptr;
	}

	static FName GetSetPropertyName(EArcFragmentSetType SetType)
	{
		switch (SetType)
		{
		case EArcFragmentSetType::Runtime: return TEXT("FragmentSet");
		case EArcFragmentSetType::ScalableFloat: return TEXT("ScalableFloatFragmentSet");
		case EArcFragmentSetType::Editor: return TEXT("EditorFragmentSet");
		}
		return NAME_None;
	}
}

// ---------- Construct ----------

void SArcItemFragmentList::Construct(const FArguments& InArgs, UArcItemDefinition* InItemDefinition)
{
	ItemDefinition = InItemDefinition;
	OnFragmentSelectedDelegate = InArgs._OnFragmentSelected;

	ChildSlot
	[
		SNew(SVerticalBox)

		// Toolbar: Add buttons + Remove
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 4, 0)
			[
				SNew(SComboButton)
				.ButtonContent()
				[
					SNew(STextBlock).Text(LOCTEXT("AddFragment", "+ Fragment"))
				]
				.OnGetMenuContent_Lambda([this]() -> TSharedRef<SWidget>
				{
					return CreateStructViewerWidget(EArcFragmentSetType::Runtime);
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 4, 0)
			[
				SNew(SComboButton)
				.ButtonContent()
				[
					SNew(STextBlock).Text(LOCTEXT("AddScalable", "+ Scalable"))
				]
				.OnGetMenuContent_Lambda([this]() -> TSharedRef<SWidget>
				{
					return CreateStructViewerWidget(EArcFragmentSetType::ScalableFloat);
				})
			]
#if WITH_EDITORONLY_DATA
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 4, 0)
			[
				SNew(SComboButton)
				.ButtonContent()
				[
					SNew(STextBlock).Text(LOCTEXT("AddEditor", "+ Editor"))
				]
				.OnGetMenuContent_Lambda([this]() -> TSharedRef<SWidget>
				{
					return CreateStructViewerWidget(EArcFragmentSetType::Editor);
				})
			]
#endif
		]

		// Tree view
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SAssignNew(TreeView, STreeView<TSharedPtr<FArcFragmentListItem>>)
			.TreeItemsSource(&RootItems)
			.OnGenerateRow(this, &SArcItemFragmentList::OnGenerateRow)
			.OnGetChildren(this, &SArcItemFragmentList::OnGetChildren)
			.OnSelectionChanged(this, &SArcItemFragmentList::OnSelectionChanged)
			.SelectionMode(ESelectionMode::Single)
		]
	];

	RebuildTree();
}

// ---------- Tree Building ----------

void SArcItemFragmentList::RebuildTree()
{
	RootItems.Empty();

	UArcItemDefinition* ItemDef = ItemDefinition.Get();
	if (!ItemDef)
	{
		if (TreeView.IsValid()) TreeView->RequestTreeRefresh();
		return;
	}

	struct FragmentEntry
	{
		FString DisplayName;
		const UScriptStruct* Struct;
	};

	// Builds a set header with category sub-groups from a fragment set.
	// Uncategorized fragments appear first (sorted alphabetically), then category sub-groups (sorted alphabetically).
	auto BuildSetGroup = [](const TSet<FArcInstancedStruct>& Set, EArcFragmentSetType SetType, const FString& GroupName) -> TSharedPtr<FArcFragmentListItem>
	{
		if (Set.Num() == 0) return nullptr;

		TArray<FragmentEntry> Uncategorized;
		TMap<FString, TArray<FragmentEntry>> Categorized;

		for (const FArcInstancedStruct& IS : Set)
		{
			if (!IS.GetScriptStruct()) continue;
			FString Category = IS.GetScriptStruct()->GetMetaData(TEXT("Category"));
			FString Display = IS.GetScriptStruct()->GetMetaData(TEXT("DisplayName"));
			FString Name = Display.Len() > 0 ? Display : IS.GetScriptStruct()->GetName();

			if (Category.Len() > 0)
			{
				Categorized.FindOrAdd(Category).Add({Name, IS.GetScriptStruct()});
			}
			else
			{
				Uncategorized.Add({Name, IS.GetScriptStruct()});
			}
		}

		TSharedPtr<FArcFragmentListItem> SetHeader = MakeShared<FArcFragmentListItem>();
		SetHeader->DisplayName = GroupName;
		SetHeader->bIsGroup = true;

		// Uncategorized fragments as direct children, sorted alphabetically
		Uncategorized.Sort([](const FragmentEntry& A, const FragmentEntry& B) { return A.DisplayName < B.DisplayName; });
		for (const FragmentEntry& Entry : Uncategorized)
		{
			TSharedPtr<FArcFragmentListItem> Leaf = MakeShared<FArcFragmentListItem>();
			Leaf->DisplayName = Entry.DisplayName;
			Leaf->ScriptStruct = Entry.Struct;
			Leaf->SetType = SetType;
			SetHeader->Children.Add(Leaf);
		}

		// Category sub-groups, sorted alphabetically
		TArray<FString> CategoryNames;
		Categorized.GetKeys(CategoryNames);
		CategoryNames.Sort();
		for (const FString& Cat : CategoryNames)
		{
			TSharedPtr<FArcFragmentListItem> CatGroup = MakeShared<FArcFragmentListItem>();
			CatGroup->DisplayName = Cat;
			CatGroup->bIsGroup = true;

			TArray<FragmentEntry>& Entries = Categorized[Cat];
			Entries.Sort([](const FragmentEntry& A, const FragmentEntry& B) { return A.DisplayName < B.DisplayName; });
			for (const FragmentEntry& Entry : Entries)
			{
				TSharedPtr<FArcFragmentListItem> Leaf = MakeShared<FArcFragmentListItem>();
				Leaf->DisplayName = Entry.DisplayName;
				Leaf->ScriptStruct = Entry.Struct;
				Leaf->SetType = SetType;
				CatGroup->Children.Add(Leaf);
			}
			SetHeader->Children.Add(CatGroup);
		}

		return SetHeader;
	};

	// Runtime
	if (TSharedPtr<FArcFragmentListItem> RuntimeGroup = BuildSetGroup(ItemDef->GetFragmentSet(), EArcFragmentSetType::Runtime, TEXT("Runtime")))
	{
		RootItems.Add(RuntimeGroup);
	}

	// Scalable Float
	if (TSharedPtr<FArcFragmentListItem> ScalableGroup = BuildSetGroup(ItemDef->GetScalableFloatFragments(), EArcFragmentSetType::ScalableFloat, TEXT("Scalable Float")))
	{
		RootItems.Add(ScalableGroup);
	}

#if WITH_EDITORONLY_DATA
	// Editor fragments via reflection (no public getter)
	if (FProperty* EditorProp = UArcItemDefinition::StaticClass()->FindPropertyByName(TEXT("EditorFragmentSet")))
	{
		if (const TSet<FArcInstancedStruct>* EditorSet = EditorProp->ContainerPtrToValuePtr<TSet<FArcInstancedStruct>>(ItemDef))
		{
			if (TSharedPtr<FArcFragmentListItem> EditorGroup = BuildSetGroup(*EditorSet, EArcFragmentSetType::Editor, TEXT("Editor")))
			{
				RootItems.Add(EditorGroup);
			}
		}
	}
#endif

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();

		// Expand all group nodes (set headers + category sub-groups)
		TFunction<void(const TArray<TSharedPtr<FArcFragmentListItem>>&)> ExpandGroups =
			[this, &ExpandGroups](const TArray<TSharedPtr<FArcFragmentListItem>>& Items)
		{
			for (const TSharedPtr<FArcFragmentListItem>& Item : Items)
			{
				if (Item->bIsGroup)
				{
					TreeView->SetItemExpansion(Item, true);
					ExpandGroups(Item->Children);
				}
			}
		};
		ExpandGroups(RootItems);
	}
}

// ---------- Tree View Callbacks ----------

TSharedRef<ITableRow> SArcItemFragmentList::OnGenerateRow(
	TSharedPtr<FArcFragmentListItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	if (Item->bIsGroup)
	{
		return SNew(STableRow<TSharedPtr<FArcFragmentListItem>>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->DisplayName))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		];
	}

	// Leaf row: [Name text (click selects row)] [▼ replace dropdown] [× delete]
	TWeakPtr<FArcFragmentListItem> WeakItem = Item;

	// Build tooltip: struct name + doc comment if available
	FText RowTooltip;
	if (const UScriptStruct* Struct = Item->ScriptStruct.Get())
	{
		FText DocTooltip = Struct->GetToolTipText();
		FString StructName = Struct->GetName();
		if (DocTooltip.IsEmpty() || DocTooltip.ToString() == StructName)
		{
			RowTooltip = FText::FromString(StructName);
		}
		else
		{
			RowTooltip = FText::Format(LOCTEXT("FragmentRowTooltipFmt", "{0}\n{1}"), FText::FromString(StructName), DocTooltip);
		}
	}

	return SNew(STableRow<TSharedPtr<FArcFragmentListItem>>, OwnerTable)
	.ToolTipText(RowTooltip)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		.Padding(2.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->DisplayName))
			.Font(FAppStyle::GetFontStyle("NormalFont"))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SComboButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(2.f, 0.f))
			.HasDownArrow(false)
			.ToolTipText(LOCTEXT("ReplaceFragmentTooltip", "Replace with a different fragment type"))
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("\u25BE")))
				.Font(FAppStyle::GetFontStyle("NormalFont"))
			]
			.OnGetMenuContent_Lambda([this, WeakItem]() -> TSharedRef<SWidget>
			{
				TSharedPtr<FArcFragmentListItem> Pinned = WeakItem.Pin();
				if (Pinned.IsValid())
				{
					return CreateReplacementViewerWidget(Pinned);
				}
				return SNullWidget::NullWidget;
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(2.f, 0.f))
			.ToolTipText(LOCTEXT("RemoveFragmentTooltip", "Remove this fragment"))
			.OnClicked_Lambda([this, WeakItem]() -> FReply
			{
				TSharedPtr<FArcFragmentListItem> Pinned = WeakItem.Pin();
				if (Pinned.IsValid())
				{
					RemoveFragment(Pinned);
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("\u00D7")))
				.Font(FAppStyle::GetFontStyle("NormalFont"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]
	];
}

void SArcItemFragmentList::OnGetChildren(
	TSharedPtr<FArcFragmentListItem> Item,
	TArray<TSharedPtr<FArcFragmentListItem>>& OutChildren)
{
	if (Item.IsValid())
	{
		OutChildren = Item->Children;
	}
}

void SArcItemFragmentList::OnSelectionChanged(
	TSharedPtr<FArcFragmentListItem> Item,
	ESelectInfo::Type SelectInfo)
{
	if (Item.IsValid() && !Item->bIsGroup && Item->ScriptStruct.IsValid())
	{
		SelectedScriptStruct = Item->ScriptStruct.Get();
		SelectedSetType = Item->SetType;
		OnFragmentSelectedDelegate.ExecuteIfBound(Item->ScriptStruct.Get(), Item->SetType);
	}
	else
	{
		SelectedScriptStruct = nullptr;
		OnFragmentSelectedDelegate.ExecuteIfBound(nullptr, EArcFragmentSetType::Runtime);
	}
}

// ---------- Add Fragment ----------

TSharedRef<SWidget> SArcItemFragmentList::CreateStructViewerWidget(EArcFragmentSetType SetType)
{
	return SNew(SBox)
		.MinDesiredWidth(300.f)
		.MinDesiredHeight(400.f)
		[
			SNew(SArcFragmentPicker)
			.BaseStruct(ArcItemEditorPrivate::GetBaseStructForSetType(SetType))
			.OnStructPicked_Lambda([this, SetType](const UScriptStruct* Picked)
			{
				OnStructPicked(Picked, SetType);
				FSlateApplication::Get().DismissAllMenus();
			})
		];
}

void SArcItemFragmentList::OnStructPicked(const UScriptStruct* InStruct, EArcFragmentSetType SetType)
{
	if (!InStruct) return;

	UArcItemDefinition* ItemDef = ItemDefinition.Get();
	if (!ItemDef) return;

	TSet<FArcInstancedStruct>* Set = GetMutableSet(SetType);
	if (!Set) return;

	// Don't add duplicates
	FName StructName = InStruct->GetFName();
	if (Set->FindByHash(GetTypeHash(StructName), StructName))
	{
		return;
	}

	// Save current selection to restore after rebuild
	const UScriptStruct* PrevSelected = SelectedScriptStruct.Get();
	EArcFragmentSetType PrevSetType = SelectedSetType;

	// Clear details view — TSet rehash invalidates any FStructOnScope pointers
	OnFragmentSelectedDelegate.ExecuteIfBound(nullptr, EArcFragmentSetType::Runtime);

	FScopedTransaction Transaction(LOCTEXT("AddFragmentTransaction", "Add Item Fragment"));
	ItemDef->Modify();

	FInstancedStruct NewInstance;
	NewInstance.InitializeAs(InStruct);
	Set->Add(FArcInstancedStruct(NewInstance));
	RehashSet(SetType);

	RebuildTree();

	// Restore previous selection (re-reads from the new set, creating valid FStructOnScope)
	if (PrevSelected)
	{
		RestoreSelection(PrevSelected, PrevSetType);
	}
}

// ---------- Remove Fragment ----------

void SArcItemFragmentList::RemoveFragment(TSharedPtr<FArcFragmentListItem> Item)
{
	if (!Item.IsValid() || Item->bIsGroup || !Item->ScriptStruct.IsValid()) return;

	UArcItemDefinition* ItemDef = ItemDefinition.Get();
	if (!ItemDef) return;

	TSet<FArcInstancedStruct>* Set = GetMutableSet(Item->SetType);
	if (!Set) return;

	const bool bRemovingSelected = (SelectedScriptStruct.Get() == Item->ScriptStruct.Get()
		&& SelectedSetType == Item->SetType);

	// Save selection to restore if we're not removing the selected fragment
	const UScriptStruct* PrevSelected = bRemovingSelected ? nullptr : SelectedScriptStruct.Get();
	EArcFragmentSetType PrevSetType = SelectedSetType;

	// Always clear details view before rehash (invalidates FStructOnScope pointers)
	OnFragmentSelectedDelegate.ExecuteIfBound(nullptr, EArcFragmentSetType::Runtime);

	FScopedTransaction Transaction(LOCTEXT("RemoveFragmentTransaction", "Remove Item Fragment"));
	ItemDef->Modify();

	FName StructName = Item->ScriptStruct->GetFName();
	if (const FArcInstancedStruct* Found = Set->FindByHash(GetTypeHash(StructName), StructName))
	{
		Set->Remove(*Found);
	}
	RehashSet(Item->SetType);

	RebuildTree();

	if (PrevSelected)
	{
		RestoreSelection(PrevSelected, PrevSetType);
	}
}

// ---------- Replace Fragment ----------

TSharedRef<SWidget> SArcItemFragmentList::CreateReplacementViewerWidget(TSharedPtr<FArcFragmentListItem> Item)
{
	// Build exclusion set: all structs already in the set + the current struct itself
	TSet<const UScriptStruct*> Excluded;
	if (const TSet<FArcInstancedStruct>* ExistingSet = GetMutableSet(Item->SetType))
	{
		for (const FArcInstancedStruct& IS : *ExistingSet)
		{
			if (IS.GetScriptStruct())
			{
				Excluded.Add(IS.GetScriptStruct());
			}
		}
	}

	TWeakPtr<FArcFragmentListItem> WeakItem = Item;
	return SNew(SBox)
		.MinDesiredWidth(300.f)
		.MinDesiredHeight(400.f)
		[
			SNew(SArcFragmentPicker)
			.BaseStruct(ArcItemEditorPrivate::GetBaseStructForSetType(Item->SetType))
			.ExcludedStructs(Excluded)
			.OnStructPicked_Lambda([this, WeakItem](const UScriptStruct* Picked)
			{
				TSharedPtr<FArcFragmentListItem> Pinned = WeakItem.Pin();
				if (Pinned.IsValid())
				{
					OnReplacementPicked(Picked, Pinned);
				}
				FSlateApplication::Get().DismissAllMenus();
			})
		];
}

void SArcItemFragmentList::OnReplacementPicked(const UScriptStruct* NewStruct, TSharedPtr<FArcFragmentListItem> OldItem)
{
	if (!NewStruct || !OldItem.IsValid() || !OldItem->ScriptStruct.IsValid()) return;

	UArcItemDefinition* ItemDef = ItemDefinition.Get();
	if (!ItemDef) return;

	TSet<FArcInstancedStruct>* Set = GetMutableSet(OldItem->SetType);
	if (!Set) return;

	const bool bReplacingSelected = (SelectedScriptStruct.Get() == OldItem->ScriptStruct.Get()
		&& SelectedSetType == OldItem->SetType);

	const UScriptStruct* PrevSelected = bReplacingSelected ? nullptr : SelectedScriptStruct.Get();
	EArcFragmentSetType PrevSetType = SelectedSetType;

	// Clear details view before rehash (invalidates FStructOnScope pointers)
	OnFragmentSelectedDelegate.ExecuteIfBound(nullptr, EArcFragmentSetType::Runtime);

	FScopedTransaction Transaction(LOCTEXT("ReplaceFragmentTransaction", "Replace Item Fragment"));
	ItemDef->Modify();

	// Remove old
	FName OldName = OldItem->ScriptStruct->GetFName();
	if (const FArcInstancedStruct* Found = Set->FindByHash(GetTypeHash(OldName), OldName))
	{
		Set->Remove(*Found);
	}

	// Add new
	FInstancedStruct NewInstance;
	NewInstance.InitializeAs(NewStruct);
	Set->Add(FArcInstancedStruct(NewInstance));
	RehashSet(OldItem->SetType);

	RebuildTree();

	if (bReplacingSelected)
	{
		// Select the replacement fragment
		RestoreSelection(NewStruct, OldItem->SetType);
	}
	else if (PrevSelected)
	{
		RestoreSelection(PrevSelected, PrevSetType);
	}
}

// ---------- Selection Restore ----------

void SArcItemFragmentList::RestoreSelection(const UScriptStruct* StructToSelect, EArcFragmentSetType SetType)
{
	if (!StructToSelect || !TreeView.IsValid()) return;

	TFunction<bool(const TArray<TSharedPtr<FArcFragmentListItem>>&)> FindAndSelect =
		[this, StructToSelect, SetType, &FindAndSelect](const TArray<TSharedPtr<FArcFragmentListItem>>& Items) -> bool
	{
		for (const TSharedPtr<FArcFragmentListItem>& Item : Items)
		{
			if (Item->bIsGroup)
			{
				if (FindAndSelect(Item->Children)) return true;
			}
			else if (Item->ScriptStruct.Get() == StructToSelect && Item->SetType == SetType)
			{
				TreeView->SetSelection(Item, ESelectInfo::Direct);
				return true;
			}
		}
		return false;
	};
	FindAndSelect(RootItems);
}

// ---------- Helpers ----------

TSet<FArcInstancedStruct>* SArcItemFragmentList::GetMutableSet(EArcFragmentSetType SetType)
{
	UArcItemDefinition* ItemDef = ItemDefinition.Get();
	if (!ItemDef) return nullptr;

	FName PropName = ArcItemEditorPrivate::GetSetPropertyName(SetType);
	FProperty* Prop = UArcItemDefinition::StaticClass()->FindPropertyByName(PropName);
	return Prop ? Prop->ContainerPtrToValuePtr<TSet<FArcInstancedStruct>>(ItemDef) : nullptr;
}

void SArcItemFragmentList::RehashSet(EArcFragmentSetType SetType)
{
	UArcItemDefinition* ItemDef = ItemDefinition.Get();
	if (!ItemDef) return;

	FName PropName = ArcItemEditorPrivate::GetSetPropertyName(SetType);
	FSetProperty* SetProp = CastField<FSetProperty>(UArcItemDefinition::StaticClass()->FindPropertyByName(PropName));
	if (!SetProp) return;

	void* SetData = SetProp->ContainerPtrToValuePtr<void>(ItemDef);
	FScriptSetHelper Helper(SetProp, SetData);
	Helper.Rehash();
}

#undef LOCTEXT_NAMESPACE
