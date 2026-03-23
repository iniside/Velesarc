// Copyright Lukasz Baran. All Rights Reserved.

#include "SArcMassTraitPicker.h"
#include "MassEntityTraitBase.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"

#define LOCTEXT_NAMESPACE "SArcMassTraitPicker"

void SArcMassTraitPicker::Construct(const FArguments& InArgs)
{
    ExcludedClasses = InArgs._ExcludedClasses;
    OnTraitClassPickedDelegate = InArgs._OnTraitClassPicked;

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
                RebuildList(Text.ToString());
            })
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.f)
        [
            SAssignNew(TreeView, STreeView<TSharedPtr<FArcTraitPickerItem>>)
            .TreeItemsSource(&RootItems)
            .OnGenerateRow(this, &SArcMassTraitPicker::OnGenerateRow)
            .OnGetChildren(this, &SArcMassTraitPicker::OnGetChildren)
            .SelectionMode(ESelectionMode::Single)
        ]
    ];

    RebuildList(FString());
}

void SArcMassTraitPicker::RebuildList(const FString& FilterText)
{
    RootItems.Empty();

    TMap<FString, TArray<TSharedPtr<FArcTraitPickerItem>>> Categorized;

    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* Class = *It;
        if (!Class->IsChildOf(UMassEntityTraitBase::StaticClass())) continue;
        if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_Hidden)) continue;
        if (Class == UMassEntityTraitBase::StaticClass()) continue;
        if (ExcludedClasses.Contains(Class)) continue;

        FString DisplayName = Class->GetMetaData(TEXT("DisplayName"));
        if (DisplayName.IsEmpty())
        {
            DisplayName = Class->GetName();
            DisplayName.RemoveFromStart(TEXT("U"));
        }

        if (!FilterText.IsEmpty() && !DisplayName.Contains(FilterText))
        {
            continue;
        }

        FString Category = Class->GetMetaData(TEXT("Category"));
        if (Category.IsEmpty())
        {
            // Use origin module name as default category
            UPackage* ClassPackage = Class->GetOuterUPackage();
            if (ClassPackage)
            {
                FString PackageName = ClassPackage->GetName();
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

        // Build tooltip: class name + doc comment
        FString ClassName = Class->GetName();
        FText DocTooltip = Class->GetToolTipText();
        FText Tooltip;
        if (DocTooltip.IsEmpty() || DocTooltip.ToString() == ClassName)
        {
            Tooltip = FText::FromString(ClassName);
        }
        else
        {
            Tooltip = FText::Format(LOCTEXT("TraitPickerTooltipFmt", "{0}\n{1}"), FText::FromString(ClassName), DocTooltip);
        }

        TSharedPtr<FArcTraitPickerItem> Leaf = MakeShared<FArcTraitPickerItem>();
        Leaf->DisplayName = DisplayName;
        Leaf->TooltipText = Tooltip;
        Leaf->TraitClass = Class;
        Categorized.FindOrAdd(Category).Add(Leaf);
    }

    TArray<FString> CategoryNames;
    Categorized.GetKeys(CategoryNames);
    CategoryNames.Sort();

    for (const FString& CatName : CategoryNames)
    {
        TSharedPtr<FArcTraitPickerItem> CatItem = MakeShared<FArcTraitPickerItem>();
        CatItem->DisplayName = CatName;
        CatItem->bIsCategory = true;

        TArray<TSharedPtr<FArcTraitPickerItem>>& Entries = Categorized[CatName];
        Entries.Sort([](const TSharedPtr<FArcTraitPickerItem>& A, const TSharedPtr<FArcTraitPickerItem>& B)
        {
            return A->DisplayName < B->DisplayName;
        });
        CatItem->Children = MoveTemp(Entries);
        RootItems.Add(CatItem);
    }

    if (TreeView.IsValid())
    {
        TreeView->RequestTreeRefresh();
        for (const TSharedPtr<FArcTraitPickerItem>& Item : RootItems)
        {
            TreeView->SetItemExpansion(Item, true);
        }
    }
}

TSharedRef<ITableRow> SArcMassTraitPicker::OnGenerateRow(
    TSharedPtr<FArcTraitPickerItem> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    if (Item->bIsCategory)
    {
        return SNew(STableRow<TSharedPtr<FArcTraitPickerItem>>, OwnerTable)
        [
            SNew(STextBlock)
            .Text(FText::FromString(Item->DisplayName))
            .Font(FAppStyle::GetFontStyle("BoldFont"))
        ];
    }

    TWeakPtr<FArcTraitPickerItem> WeakItem = Item;
    return SNew(STableRow<TSharedPtr<FArcTraitPickerItem>>, OwnerTable)
    .ToolTipText(Item->TooltipText)
    [
        SNew(SButton)
        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
        .ContentPadding(FMargin(4.f, 2.f))
        .OnClicked_Lambda([this, WeakItem]() -> FReply
        {
            TSharedPtr<FArcTraitPickerItem> Pinned = WeakItem.Pin();
            if (Pinned.IsValid() && Pinned->TraitClass)
            {
                OnTraitClassPickedDelegate.ExecuteIfBound(Pinned->TraitClass);
            }
            return FReply::Handled();
        })
        [
            SNew(STextBlock)
            .Text(FText::FromString(Item->DisplayName))
        ]
    ];
}

void SArcMassTraitPicker::OnGetChildren(
    TSharedPtr<FArcTraitPickerItem> Item,
    TArray<TSharedPtr<FArcTraitPickerItem>>& OutChildren)
{
    if (Item.IsValid())
    {
        OutChildren = Item->Children;
    }
}

#undef LOCTEXT_NAMESPACE
