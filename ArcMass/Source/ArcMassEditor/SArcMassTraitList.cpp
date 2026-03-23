// Copyright Lukasz Baran. All Rights Reserved.

#include "SArcMassTraitList.h"

#include "MassEntityConfigAsset.h"
#include "MassEntityTraitBase.h"
#include "MassAssortedFragmentsTrait.h"
#include "SArcMassTraitPicker.h"
#include "SArcMassStructPicker.h"
#include "MassEntityElementTypes.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "SArcMassTraitList"

namespace ArcMassTraitListPrivate
{
    /** Access the protected Fragments array on UMassAssortedFragmentsTrait via reflection. */
    static TArray<FInstancedStruct>* GetAssortedFragments(UMassEntityTraitBase* Trait)
    {
        FProperty* Prop = UMassAssortedFragmentsTrait::StaticClass()->FindPropertyByName(TEXT("Fragments"));
        return Prop ? Prop->ContainerPtrToValuePtr<TArray<FInstancedStruct>>(Trait) : nullptr;
    }

    /** Access the protected Tags array on UMassAssortedFragmentsTrait via reflection. */
    static TArray<FInstancedStruct>* GetAssortedTags(UMassEntityTraitBase* Trait)
    {
        FProperty* Prop = UMassAssortedFragmentsTrait::StaticClass()->FindPropertyByName(TEXT("Tags"));
        return Prop ? Prop->ContainerPtrToValuePtr<TArray<FInstancedStruct>>(Trait) : nullptr;
    }

    /** Access the protected Traits array on FMassEntityConfig via reflection. */
    static TArray<TObjectPtr<UMassEntityTraitBase>>* GetMutableTraitsArray(FMassEntityConfig& Config)
    {
        FProperty* Prop = FMassEntityConfig::StaticStruct()->FindPropertyByName(TEXT("Traits"));
        return Prop ? Prop->ContainerPtrToValuePtr<TArray<TObjectPtr<UMassEntityTraitBase>>>(&Config) : nullptr;
    }
}

// ---------- Construct ----------

void SArcMassTraitList::Construct(const FArguments& InArgs, UMassEntityConfigAsset* InConfigAsset)
{
    ConfigAsset = InConfigAsset;
    OnTraitSelectedDelegate = InArgs._OnTraitSelected;

    ChildSlot
    [
        SNew(SVerticalBox)

        // Toolbar: Add trait button
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
                    SNew(STextBlock).Text(LOCTEXT("AddTrait", "+ Trait"))
                ]
                .OnGetMenuContent_Lambda([this]() -> TSharedRef<SWidget>
                {
                    // Build exclusion set from current traits
                    TSet<UClass*> ExcludedClasses;
                    UMassEntityConfigAsset* Asset = ConfigAsset.Get();
                    if (Asset)
                    {
                        TConstArrayView<UMassEntityTraitBase*> CurrentTraits = Asset->GetConfig().GetTraits();
                        for (UMassEntityTraitBase* Trait : CurrentTraits)
                        {
                            if (Trait)
                            {
                                ExcludedClasses.Add(Trait->GetClass());
                            }
                        }
                    }

                    return SNew(SBox)
                        .MinDesiredWidth(300.f)
                        .MinDesiredHeight(400.f)
                        [
                            SNew(SArcMassTraitPicker)
                            .ExcludedClasses(ExcludedClasses)
                            .OnTraitClassPicked_Lambda([this](TSubclassOf<UMassEntityTraitBase> TraitClass)
                            {
                                AddTrait(TraitClass);
                                FSlateApplication::Get().DismissAllMenus();
                            })
                        ];
                })
            ]
        ]

        // Tree view
        + SVerticalBox::Slot()
        .FillHeight(1.f)
        [
            SAssignNew(TreeView, STreeView<TSharedPtr<FArcMassTraitListItem>>)
            .TreeItemsSource(&RootItems)
            .OnGenerateRow(this, &SArcMassTraitList::OnGenerateRow)
            .OnGetChildren(this, &SArcMassTraitList::OnGetChildren)
            .OnSelectionChanged(this, &SArcMassTraitList::OnSelectionChanged)
            .SelectionMode(ESelectionMode::Single)
        ]
    ];

    RebuildTree();
}

// ---------- Tree Building ----------

FString SArcMassTraitList::GetTraitDisplayName(const UMassEntityTraitBase* Trait) const
{
    if (!Trait) return FString();

    FString DisplayName = Trait->GetClass()->GetMetaData(TEXT("DisplayName"));
    if (DisplayName.IsEmpty())
    {
        DisplayName = Trait->GetClass()->GetName();
        DisplayName.RemoveFromStart(TEXT("U"));
    }
    return DisplayName;
}

FString SArcMassTraitList::GetTraitCategory(const UMassEntityTraitBase* Trait) const
{
    if (!Trait) return TEXT("Default");

    FString Category = Trait->GetClass()->GetMetaData(TEXT("Category"));
    if (Category.IsEmpty())
    {
        // Use the origin module name as the default category
        UPackage* ClassPackage = Trait->GetClass()->GetOuterUPackage();
        if (ClassPackage)
        {
            FString PackageName = ClassPackage->GetName();
            // Package names look like "/Script/ModuleName" — extract the module name
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
    return Category;
}

FText SArcMassTraitList::GetTraitTooltip(const UMassEntityTraitBase* Trait) const
{
    if (!Trait) return FText();

    UClass* TraitClass = Trait->GetClass();
    FString ClassName = TraitClass->GetName();
    FText DocTooltip = TraitClass->GetToolTipText();
    if (DocTooltip.IsEmpty() || DocTooltip.ToString() == ClassName)
    {
        return FText::FromString(ClassName);
    }
    return FText::Format(LOCTEXT("TraitTooltipFmt", "{0}\n{1}"), FText::FromString(ClassName), DocTooltip);
}

FText SArcMassTraitList::GetStructTooltip(const UScriptStruct* Struct) const
{
    if (!Struct) return FText();

    FString StructName = Struct->GetName();
    FText DocTooltip = Struct->GetToolTipText();
    if (DocTooltip.IsEmpty() || DocTooltip.ToString() == StructName)
    {
        return FText::FromString(StructName);
    }
    return FText::Format(LOCTEXT("StructTooltipFmt", "{0}\n{1}"), FText::FromString(StructName), DocTooltip);
}

TSharedPtr<FArcMassTraitListItem> SArcMassTraitList::BuildAssortedCategory(
    UMassEntityTraitBase* AssortedTrait
    , bool bIsParent)
{
    TSharedPtr<FArcMassTraitListItem> CategoryItem = MakeShared<FArcMassTraitListItem>();
    CategoryItem->DisplayName = GetTraitDisplayName(AssortedTrait);
    CategoryItem->ItemType = EArcMassTraitListItemType::Category;
    CategoryItem->Trait = AssortedTrait;
    CategoryItem->bIsAssortedCategory = true;
    CategoryItem->bIsParentTrait = bIsParent;
    CategoryItem->bIsParentCategory = bIsParent;

    // --- Fragments sub-category ---
    TArray<FInstancedStruct>* Fragments = ArcMassTraitListPrivate::GetAssortedFragments(AssortedTrait);
    {
        TSharedPtr<FArcMassTraitListItem> FragSubCat = MakeShared<FArcMassTraitListItem>();
        FragSubCat->DisplayName = TEXT("Fragments");
        FragSubCat->ItemType = EArcMassTraitListItemType::Category;
        FragSubCat->Trait = AssortedTrait;
        FragSubCat->bIsAssortedFragmentSubCategory = true;
        FragSubCat->bIsParentTrait = bIsParent;
        FragSubCat->bIsParentCategory = bIsParent;

        if (Fragments)
        {
            for (int32 Idx = 0; Idx < Fragments->Num(); ++Idx)
            {
                const FInstancedStruct& Fragment = (*Fragments)[Idx];
                TSharedPtr<FArcMassTraitListItem> Child = MakeShared<FArcMassTraitListItem>();
                Child->ItemType = EArcMassTraitListItemType::AssortedFragment;
                Child->AssortedIndex = Idx;
                Child->OwningAssortedTrait = AssortedTrait;
                Child->bIsParentTrait = bIsParent;

                if (const UScriptStruct* ScriptStruct = Fragment.GetScriptStruct())
                {
                    Child->DisplayName = ScriptStruct->GetName();
                    Child->TooltipText = GetStructTooltip(ScriptStruct);
                }
                else
                {
                    Child->DisplayName = TEXT("[Missing Fragment]");
                }
                FragSubCat->Children.Add(Child);
            }
        }
        CategoryItem->Children.Add(FragSubCat);
    }

    // --- Tags sub-category ---
    TArray<FInstancedStruct>* Tags = ArcMassTraitListPrivate::GetAssortedTags(AssortedTrait);
    {
        TSharedPtr<FArcMassTraitListItem> TagSubCat = MakeShared<FArcMassTraitListItem>();
        TagSubCat->DisplayName = TEXT("Tags");
        TagSubCat->ItemType = EArcMassTraitListItemType::Category;
        TagSubCat->Trait = AssortedTrait;
        TagSubCat->bIsAssortedTagSubCategory = true;
        TagSubCat->bIsParentTrait = bIsParent;
        TagSubCat->bIsParentCategory = bIsParent;

        if (Tags)
        {
            for (int32 Idx = 0; Idx < Tags->Num(); ++Idx)
            {
                const FInstancedStruct& TagRef = (*Tags)[Idx];
                TSharedPtr<FArcMassTraitListItem> Child = MakeShared<FArcMassTraitListItem>();
                Child->ItemType = EArcMassTraitListItemType::AssortedTag;
                Child->AssortedIndex = Idx;
                Child->OwningAssortedTrait = AssortedTrait;
                Child->bIsParentTrait = bIsParent;

                if (const UScriptStruct* ScriptStruct = TagRef.GetScriptStruct())
                {
                    Child->DisplayName = ScriptStruct->GetName();
                    Child->TooltipText = GetStructTooltip(ScriptStruct);
                }
                else
                {
                    Child->DisplayName = TEXT("[Missing Tag]");
                }
                TagSubCat->Children.Add(Child);
            }
        }
        CategoryItem->Children.Add(TagSubCat);
    }

    return CategoryItem;
}

void SArcMassTraitList::RebuildTree()
{
    RootItems.Empty();

    UMassEntityConfigAsset* Asset = ConfigAsset.Get();
    if (!Asset)
    {
        if (TreeView.IsValid()) TreeView->RequestTreeRefresh();
        return;
    }

    const FMassEntityConfig& Config = Asset->GetConfig();

    // --- Parent traits (from parent config, if any) ---
    const UMassEntityConfigAsset* ParentAsset = Config.GetParent();
    if (ParentAsset)
    {
        TSharedPtr<FArcMassTraitListItem> ParentCategory = MakeShared<FArcMassTraitListItem>();
        ParentCategory->DisplayName = FString::Printf(TEXT("Parent (%s)"), *ParentAsset->GetName());
        ParentCategory->ItemType = EArcMassTraitListItemType::Category;
        ParentCategory->bIsParentCategory = true;

        // Group parent traits by category, same as own traits
        TMap<FString, TSharedPtr<FArcMassTraitListItem>> ParentCategoryMap;

        TConstArrayView<UMassEntityTraitBase*> ParentTraits = ParentAsset->GetConfig().GetTraits();
        for (UMassEntityTraitBase* ParentTrait : ParentTraits)
        {
            if (!ParentTrait)
            {
                TSharedPtr<FArcMassTraitListItem> NullItem = MakeShared<FArcMassTraitListItem>();
                NullItem->DisplayName = TEXT("[Missing Trait]");
                NullItem->ItemType = EArcMassTraitListItemType::NullTrait;
                NullItem->bIsParentTrait = true;

                FString CatName = TEXT("Default");
                TSharedPtr<FArcMassTraitListItem>& SubCat = ParentCategoryMap.FindOrAdd(CatName);
                if (!SubCat.IsValid())
                {
                    SubCat = MakeShared<FArcMassTraitListItem>();
                    SubCat->DisplayName = CatName;
                    SubCat->ItemType = EArcMassTraitListItemType::Category;
                    SubCat->bIsParentCategory = true;
                }
                SubCat->Children.Add(NullItem);
                continue;
            }

            if (ParentTrait->IsA<UMassAssortedFragmentsTrait>())
            {
                FString CatName = GetTraitCategory(ParentTrait);
                TSharedPtr<FArcMassTraitListItem>& SubCat = ParentCategoryMap.FindOrAdd(CatName);
                if (!SubCat.IsValid())
                {
                    SubCat = MakeShared<FArcMassTraitListItem>();
                    SubCat->DisplayName = CatName;
                    SubCat->ItemType = EArcMassTraitListItemType::Category;
                    SubCat->bIsParentCategory = true;
                }

                TSharedPtr<FArcMassTraitListItem> AssortedCat = BuildAssortedCategory(ParentTrait, /*bIsParent=*/ true);
                SubCat->Children.Add(AssortedCat);
            }
            else
            {
                FString CatName = GetTraitCategory(ParentTrait);
                TSharedPtr<FArcMassTraitListItem>& SubCat = ParentCategoryMap.FindOrAdd(CatName);
                if (!SubCat.IsValid())
                {
                    SubCat = MakeShared<FArcMassTraitListItem>();
                    SubCat->DisplayName = CatName;
                    SubCat->ItemType = EArcMassTraitListItemType::Category;
                    SubCat->bIsParentCategory = true;
                }

                TSharedPtr<FArcMassTraitListItem> TraitItem = MakeShared<FArcMassTraitListItem>();
                TraitItem->DisplayName = GetTraitDisplayName(ParentTrait);
                TraitItem->ItemType = EArcMassTraitListItemType::Trait;
                TraitItem->Trait = ParentTrait;
                TraitItem->bIsParentTrait = true;
                TraitItem->TooltipText = GetTraitTooltip(ParentTrait);
                SubCat->Children.Add(TraitItem);
            }
        }

        // Sort sub-categories and add to parent
        TArray<FString> ParentCatNames;
        ParentCategoryMap.GetKeys(ParentCatNames);
        ParentCatNames.Sort();

        for (const FString& CatName : ParentCatNames)
        {
            TSharedPtr<FArcMassTraitListItem> SubCat = ParentCategoryMap[CatName];
            if (SubCat.IsValid() && SubCat->Children.Num() > 0)
            {
                SubCat->Children.Sort([](const TSharedPtr<FArcMassTraitListItem>& A, const TSharedPtr<FArcMassTraitListItem>& B)
                {
                    return A->DisplayName < B->DisplayName;
                });
                ParentCategory->Children.Add(SubCat);
            }
        }

        if (ParentCategory->Children.Num() > 0)
        {
            RootItems.Add(ParentCategory);
        }
    }

    // --- Own traits grouped by category ---
    TConstArrayView<UMassEntityTraitBase*> OwnTraits = Config.GetTraits();
    TMap<FString, TSharedPtr<FArcMassTraitListItem>> CategoryMap;

    for (UMassEntityTraitBase* Trait : OwnTraits)
    {
        if (!Trait)
        {
            // Null trait — add to "Default" category as NullTrait
            FString CatName = TEXT("Default");
            TSharedPtr<FArcMassTraitListItem>& CatItem = CategoryMap.FindOrAdd(CatName);
            if (!CatItem.IsValid())
            {
                CatItem = MakeShared<FArcMassTraitListItem>();
                CatItem->DisplayName = CatName;
                CatItem->ItemType = EArcMassTraitListItemType::Category;
            }

            TSharedPtr<FArcMassTraitListItem> NullItem = MakeShared<FArcMassTraitListItem>();
            NullItem->DisplayName = TEXT("[Missing Trait]");
            NullItem->ItemType = EArcMassTraitListItemType::NullTrait;
            CatItem->Children.Add(NullItem);
            continue;
        }

        // Special handling for UMassAssortedFragmentsTrait
        if (Trait->IsA<UMassAssortedFragmentsTrait>())
        {
            FString CatName = GetTraitCategory(Trait);
            TSharedPtr<FArcMassTraitListItem>& CatItem = CategoryMap.FindOrAdd(CatName);
            if (!CatItem.IsValid())
            {
                CatItem = MakeShared<FArcMassTraitListItem>();
                CatItem->DisplayName = CatName;
                CatItem->ItemType = EArcMassTraitListItemType::Category;
            }

            TSharedPtr<FArcMassTraitListItem> AssortedCat = BuildAssortedCategory(Trait, /*bIsParent=*/ false);
            CatItem->Children.Add(AssortedCat);
            continue;
        }

        FString CatName = GetTraitCategory(Trait);
        TSharedPtr<FArcMassTraitListItem>& CatItem = CategoryMap.FindOrAdd(CatName);
        if (!CatItem.IsValid())
        {
            CatItem = MakeShared<FArcMassTraitListItem>();
            CatItem->DisplayName = CatName;
            CatItem->ItemType = EArcMassTraitListItemType::Category;
        }

        TSharedPtr<FArcMassTraitListItem> TraitItem = MakeShared<FArcMassTraitListItem>();
        TraitItem->DisplayName = GetTraitDisplayName(Trait);
        TraitItem->ItemType = EArcMassTraitListItemType::Trait;
        TraitItem->Trait = Trait;
        TraitItem->TooltipText = GetTraitTooltip(Trait);
        CatItem->Children.Add(TraitItem);
    }

    // Sort categories alphabetically and add to root
    TArray<FString> CategoryNames;
    CategoryMap.GetKeys(CategoryNames);
    CategoryNames.Sort();

    for (const FString& CatName : CategoryNames)
    {
        TSharedPtr<FArcMassTraitListItem> CatItem = CategoryMap[CatName];
        if (CatItem.IsValid() && CatItem->Children.Num() > 0)
        {
            // Sort children alphabetically within each category
            CatItem->Children.Sort([](const TSharedPtr<FArcMassTraitListItem>& A, const TSharedPtr<FArcMassTraitListItem>& B)
            {
                return A->DisplayName < B->DisplayName;
            });
            RootItems.Add(CatItem);
        }
    }

    if (TreeView.IsValid())
    {
        TreeView->RequestTreeRefresh();

        // Expand all group nodes
        TFunction<void(const TArray<TSharedPtr<FArcMassTraitListItem>>&)> ExpandGroups =
            [this, &ExpandGroups](const TArray<TSharedPtr<FArcMassTraitListItem>>& Items)
        {
            for (const TSharedPtr<FArcMassTraitListItem>& Item : Items)
            {
                if (Item->IsGroup())
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

TSharedRef<ITableRow> SArcMassTraitList::OnGenerateRow(
    TSharedPtr<FArcMassTraitListItem> Item
    , const TSharedRef<STableViewBase>& OwnerTable)
{
    TWeakPtr<FArcMassTraitListItem> WeakItem = Item;

    // --- Assorted Fragments sub-category (own, not parent) ---
    if (Item->bIsAssortedFragmentSubCategory && !Item->bIsParentCategory)
    {
        return SNew(STableRow<TSharedPtr<FArcMassTraitListItem>>, OwnerTable)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.f)
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ContentPadding(FMargin(0.f))
                .OnClicked_Lambda([this, WeakItem]() -> FReply
                {
                    TSharedPtr<FArcMassTraitListItem> Pinned = WeakItem.Pin();
                    if (Pinned.IsValid()) OnTraitSelectedDelegate.ExecuteIfBound(Pinned);
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Item->DisplayName))
                    .Font(FAppStyle::GetFontStyle("BoldFont"))
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(2.f, 0.f)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ContentPadding(FMargin(4.f, 0.f))
                .ToolTipText(LOCTEXT("AddAssortedFragmentTooltip", "Add a fragment to this assorted trait"))
                .OnClicked_Lambda([this, WeakItem]() -> FReply
                {
                    TSharedPtr<FArcMassTraitListItem> Pinned = WeakItem.Pin();
                    if (Pinned.IsValid() && Pinned->Trait.IsValid())
                    {
                        AddAssortedFragment(Pinned->Trait.Get());
                    }
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("AddFragment", "+ Fragment"))
                    .Font(FAppStyle::GetFontStyle("SmallFont"))
                ]
            ]
        ];
    }

    // --- Assorted Tags sub-category (own, not parent) ---
    if (Item->bIsAssortedTagSubCategory && !Item->bIsParentCategory)
    {
        return SNew(STableRow<TSharedPtr<FArcMassTraitListItem>>, OwnerTable)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.f)
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ContentPadding(FMargin(0.f))
                .OnClicked_Lambda([this, WeakItem]() -> FReply
                {
                    TSharedPtr<FArcMassTraitListItem> Pinned = WeakItem.Pin();
                    if (Pinned.IsValid()) OnTraitSelectedDelegate.ExecuteIfBound(Pinned);
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Item->DisplayName))
                    .Font(FAppStyle::GetFontStyle("BoldFont"))
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(2.f, 0.f)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ContentPadding(FMargin(4.f, 0.f))
                .ToolTipText(LOCTEXT("AddAssortedTagTooltip", "Add a tag to this assorted trait"))
                .OnClicked_Lambda([this, WeakItem]() -> FReply
                {
                    TSharedPtr<FArcMassTraitListItem> Pinned = WeakItem.Pin();
                    if (Pinned.IsValid() && Pinned->Trait.IsValid())
                    {
                        AddAssortedTag(Pinned->Trait.Get());
                    }
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("AddTag", "+ Tag"))
                    .Font(FAppStyle::GetFontStyle("SmallFont"))
                ]
            ]
        ];
    }

    // --- Assorted category (own, not parent) ---
    if (Item->bIsAssortedCategory && !Item->bIsParentCategory)
    {
        return SNew(STableRow<TSharedPtr<FArcMassTraitListItem>>, OwnerTable)
        [
            SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .ContentPadding(FMargin(0.f))
            .OnClicked_Lambda([this, WeakItem]() -> FReply
            {
                TSharedPtr<FArcMassTraitListItem> Pinned = WeakItem.Pin();
                if (Pinned.IsValid()) OnTraitSelectedDelegate.ExecuteIfBound(Pinned);
                return FReply::Handled();
            })
            [
                SNew(STextBlock)
                .Text(FText::FromString(Item->DisplayName))
                .Font(FAppStyle::GetFontStyle("BoldFont"))
            ]
        ];
    }

    // --- Assorted category (parent) ---
    if (Item->bIsAssortedCategory && Item->bIsParentCategory)
    {
        return SNew(STableRow<TSharedPtr<FArcMassTraitListItem>>, OwnerTable)
        [
            SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .ContentPadding(FMargin(0.f))
            .OnClicked_Lambda([this, WeakItem]() -> FReply
            {
                TSharedPtr<FArcMassTraitListItem> Pinned = WeakItem.Pin();
                if (Pinned.IsValid()) OnTraitSelectedDelegate.ExecuteIfBound(Pinned);
                return FReply::Handled();
            })
            [
                SNew(STextBlock)
                .Text(FText::FromString(Item->DisplayName))
                .Font(FAppStyle::GetFontStyle("BoldFont"))
            ]
        ];
    }

    // --- Parent category ---
    if (Item->bIsParentCategory && Item->IsGroup())
    {
        return SNew(STableRow<TSharedPtr<FArcMassTraitListItem>>, OwnerTable)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.f)
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ContentPadding(FMargin(0.f))
                .OnClicked_Lambda([this, WeakItem]() -> FReply
                {
                    TSharedPtr<FArcMassTraitListItem> Pinned = WeakItem.Pin();
                    if (Pinned.IsValid()) OnTraitSelectedDelegate.ExecuteIfBound(Pinned);
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Item->DisplayName))
                    .Font(FAppStyle::GetFontStyle("BoldFont"))
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(2.f, 0.f)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ContentPadding(FMargin(4.f, 0.f))
                .ToolTipText_Lambda([this]() -> FText
                {
                    return bParentEditingEnabled
                        ? LOCTEXT("LockParentTooltip", "Lock parent traits (read-only)")
                        : LOCTEXT("UnlockParentTooltip", "Unlock parent traits for editing");
                })
                .OnClicked_Lambda([this]() -> FReply
                {
                    ToggleParentEditing();
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]() -> FText
                    {
                        return bParentEditingEnabled
                            ? LOCTEXT("LockButton", "Lock")
                            : LOCTEXT("UnlockButton", "Unlock");
                    })
                    .Font(FAppStyle::GetFontStyle("SmallFont"))
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(2.f, 0.f)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ContentPadding(FMargin(4.f, 0.f))
                .ToolTipText(LOCTEXT("OpenParentTooltip", "Open parent config asset in editor"))
                .OnClicked_Lambda([this]() -> FReply
                {
                    OpenParentAsset();
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("OpenParentButton", "Open"))
                    .Font(FAppStyle::GetFontStyle("SmallFont"))
                ]
            ]
        ];
    }

    // --- Regular category ---
    if (Item->IsGroup())
    {
        return SNew(STableRow<TSharedPtr<FArcMassTraitListItem>>, OwnerTable)
        [
            SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .ContentPadding(FMargin(0.f))
            .OnClicked_Lambda([this, WeakItem]() -> FReply
            {
                TSharedPtr<FArcMassTraitListItem> Pinned = WeakItem.Pin();
                if (Pinned.IsValid())
                {
                    OnTraitSelectedDelegate.ExecuteIfBound(Pinned);
                }
                return FReply::Handled();
            })
            [
                SNew(STextBlock)
                .Text(FText::FromString(Item->DisplayName))
                .Font(FAppStyle::GetFontStyle("BoldFont"))
            ]
        ];
    }

    // --- NullTrait ---
    if (Item->ItemType == EArcMassTraitListItemType::NullTrait)
    {
        return SNew(STableRow<TSharedPtr<FArcMassTraitListItem>>, OwnerTable)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.f)
            .VAlign(VAlign_Center)
            .Padding(2.f, 0.f, 0.f, 0.f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("MissingTrait", "[Missing Trait]"))
                .Font(FAppStyle::GetFontStyle("NormalFont"))
                .ColorAndOpacity(FLinearColor::Red)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ContentPadding(FMargin(2.f, 0.f))
                .ToolTipText(LOCTEXT("RemoveNullTraitTooltip", "Remove this null trait entry"))
                .Visibility(Item->bIsParentTrait ? EVisibility::Collapsed : EVisibility::Visible)
                .OnClicked_Lambda([this, WeakItem]() -> FReply
                {
                    TSharedPtr<FArcMassTraitListItem> Pinned = WeakItem.Pin();
                    if (Pinned.IsValid())
                    {
                        RemoveTrait(Pinned);
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

    // --- Parent trait (read-only with lock icon) ---
    if (Item->bIsParentTrait)
    {
        return SNew(STableRow<TSharedPtr<FArcMassTraitListItem>>, OwnerTable)
        .ToolTipText(Item->TooltipText)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(2.f, 0.f, 4.f, 0.f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("\U0001F512")))
                .Font(FAppStyle::GetFontStyle("NormalFont"))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Item->DisplayName))
                .Font(FAppStyle::GetFontStyle("NormalFont"))
            ]
        ];
    }

    // --- Normal trait or assorted child ---
    return SNew(STableRow<TSharedPtr<FArcMassTraitListItem>>, OwnerTable)
    .ToolTipText(Item->TooltipText)
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
            SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .ContentPadding(FMargin(2.f, 0.f))
            .ToolTipText(LOCTEXT("RemoveTraitTooltip", "Remove this entry"))
            .OnClicked_Lambda([this, WeakItem]() -> FReply
            {
                TSharedPtr<FArcMassTraitListItem> Pinned = WeakItem.Pin();
                if (Pinned.IsValid())
                {
                    if (Pinned->ItemType == EArcMassTraitListItemType::AssortedFragment
                        || Pinned->ItemType == EArcMassTraitListItemType::AssortedTag)
                    {
                        RemoveAssortedEntry(Pinned);
                    }
                    else
                    {
                        RemoveTrait(Pinned);
                    }
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

void SArcMassTraitList::OnGetChildren(
    TSharedPtr<FArcMassTraitListItem> Item
    , TArray<TSharedPtr<FArcMassTraitListItem>>& OutChildren)
{
    if (Item.IsValid())
    {
        OutChildren = Item->Children;
    }
}

void SArcMassTraitList::OnSelectionChanged(
    TSharedPtr<FArcMassTraitListItem> Item
    , ESelectInfo::Type SelectInfo)
{
    if (Item.IsValid())
    {
        // Fire for both selectable items and category nodes
        OnTraitSelectedDelegate.ExecuteIfBound(Item);
    }
    else
    {
        OnTraitSelectedDelegate.ExecuteIfBound(nullptr);
    }
}

// ---------- Trait Mutations ----------

void SArcMassTraitList::AddTrait(TSubclassOf<UMassEntityTraitBase> TraitClass)
{
    if (!TraitClass) return;

    UMassEntityConfigAsset* Asset = ConfigAsset.Get();
    if (!Asset) return;

    FScopedTransaction Transaction(LOCTEXT("AddTraitTransaction", "Add Mass Trait"));
    Asset->Modify();

    Asset->AddTrait(TraitClass);

    RebuildTree();
}

void SArcMassTraitList::RemoveTrait(TSharedPtr<FArcMassTraitListItem> Item)
{
    if (!Item.IsValid()) return;

    UMassEntityConfigAsset* Asset = ConfigAsset.Get();
    if (!Asset) return;

    FMassEntityConfig& MutableConfig = Asset->GetMutableConfig();
    TArray<TObjectPtr<UMassEntityTraitBase>>* TraitsArray = ArcMassTraitListPrivate::GetMutableTraitsArray(MutableConfig);
    if (!TraitsArray) return;

    // For null traits, find and remove the first null entry
    if (Item->ItemType == EArcMassTraitListItemType::NullTrait)
    {
        FScopedTransaction Transaction(LOCTEXT("RemoveNullTraitTransaction", "Remove Null Trait"));
        Asset->Modify();

        for (int32 Idx = 0; Idx < TraitsArray->Num(); ++Idx)
        {
            if ((*TraitsArray)[Idx] == nullptr)
            {
                TraitsArray->RemoveAt(Idx);
                break;
            }
        }

        RebuildTree();
        return;
    }

    // For regular traits, find by pointer
    UMassEntityTraitBase* TraitToRemove = Item->Trait.Get();
    if (!TraitToRemove) return;

    FScopedTransaction Transaction(LOCTEXT("RemoveTraitTransaction", "Remove Mass Trait"));
    Asset->Modify();

    TraitsArray->Remove(TraitToRemove);

    RebuildTree();
}

// ---------- Assorted Fragment/Tag Mutations ----------

void SArcMassTraitList::AddAssortedFragment(UMassEntityTraitBase* AssortedTrait)
{
    if (!AssortedTrait) return;

    UMassEntityConfigAsset* Asset = ConfigAsset.Get();
    if (!Asset) return;

    TArray<FInstancedStruct>* FragmentsArray = ArcMassTraitListPrivate::GetAssortedFragments(AssortedTrait);
    if (!FragmentsArray) return;

    // Build exclusion set from existing fragments
    TSet<const UScriptStruct*> Excluded;
    for (const FInstancedStruct& Existing : *FragmentsArray)
    {
        if (Existing.IsValid())
        {
            Excluded.Add(Existing.GetScriptStruct());
        }
    }

    TWeakObjectPtr<UMassEntityTraitBase> WeakTrait = AssortedTrait;
    TSharedRef<SWidget> PickerWidget = SNew(SBox)
        .MinDesiredWidth(300.f)
        .MinDesiredHeight(400.f)
        [
            SNew(SArcMassStructPicker)
            .BaseStruct(FMassFragment::StaticStruct())
            .ExcludedStructs(Excluded)
            .OnStructPicked_Lambda([this, WeakTrait](const UScriptStruct* PickedStruct)
            {
                FSlateApplication::Get().DismissAllMenus();

                UMassEntityTraitBase* Trait = WeakTrait.Get();
                if (!Trait || !PickedStruct) return;

                UMassEntityConfigAsset* ConfigAssetPtr = ConfigAsset.Get();
                if (!ConfigAssetPtr) return;

                TArray<FInstancedStruct>* Fragments = ArcMassTraitListPrivate::GetAssortedFragments(Trait);
                if (!Fragments) return;

                FScopedTransaction Transaction(LOCTEXT("AddAssortedFragmentTransaction", "Add Assorted Fragment"));
                ConfigAssetPtr->Modify();

                FInstancedStruct NewInstance;
                NewInstance.InitializeAs(PickedStruct);
                Fragments->Add(MoveTemp(NewInstance));

                RebuildTree();
            })
        ];

    FSlateApplication::Get().PushMenu(
        SharedThis(this)
        , FWidgetPath()
        , PickerWidget
        , FSlateApplication::Get().GetCursorPos()
        , FPopupTransitionEffect::TypeInPopup);
}

void SArcMassTraitList::AddAssortedTag(UMassEntityTraitBase* AssortedTrait)
{
    if (!AssortedTrait) return;

    UMassEntityConfigAsset* Asset = ConfigAsset.Get();
    if (!Asset) return;

    TArray<FInstancedStruct>* TagsArray = ArcMassTraitListPrivate::GetAssortedTags(AssortedTrait);
    if (!TagsArray) return;

    // Build exclusion set from existing tags
    TSet<const UScriptStruct*> Excluded;
    for (const FInstancedStruct& Existing : *TagsArray)
    {
        if (Existing.IsValid())
        {
            Excluded.Add(Existing.GetScriptStruct());
        }
    }

    TWeakObjectPtr<UMassEntityTraitBase> WeakTrait = AssortedTrait;
    TSharedRef<SWidget> PickerWidget = SNew(SBox)
        .MinDesiredWidth(300.f)
        .MinDesiredHeight(400.f)
        [
            SNew(SArcMassStructPicker)
            .BaseStruct(FMassTag::StaticStruct())
            .ExcludedStructs(Excluded)
            .OnStructPicked_Lambda([this, WeakTrait](const UScriptStruct* PickedStruct)
            {
                FSlateApplication::Get().DismissAllMenus();

                UMassEntityTraitBase* Trait = WeakTrait.Get();
                if (!Trait || !PickedStruct) return;

                UMassEntityConfigAsset* ConfigAssetPtr = ConfigAsset.Get();
                if (!ConfigAssetPtr) return;

                TArray<FInstancedStruct>* Tags = ArcMassTraitListPrivate::GetAssortedTags(Trait);
                if (!Tags) return;

                FScopedTransaction Transaction(LOCTEXT("AddAssortedTagTransaction", "Add Assorted Tag"));
                ConfigAssetPtr->Modify();

                FInstancedStruct NewInstance;
                NewInstance.InitializeAs(PickedStruct);
                Tags->Add(MoveTemp(NewInstance));

                RebuildTree();
            })
        ];

    FSlateApplication::Get().PushMenu(
        SharedThis(this)
        , FWidgetPath()
        , PickerWidget
        , FSlateApplication::Get().GetCursorPos()
        , FPopupTransitionEffect::TypeInPopup);
}

void SArcMassTraitList::RemoveAssortedEntry(TSharedPtr<FArcMassTraitListItem> Item)
{
    if (!Item.IsValid()) return;
    if (Item->AssortedIndex == INDEX_NONE) return;

    UMassEntityTraitBase* AssortedTrait = Item->OwningAssortedTrait.Get();
    if (!AssortedTrait) return;

    UMassEntityConfigAsset* Asset = ConfigAsset.Get();
    if (!Asset) return;

    TArray<FInstancedStruct>* TargetArray = nullptr;
    if (Item->ItemType == EArcMassTraitListItemType::AssortedFragment)
    {
        TargetArray = ArcMassTraitListPrivate::GetAssortedFragments(AssortedTrait);
    }
    else if (Item->ItemType == EArcMassTraitListItemType::AssortedTag)
    {
        TargetArray = ArcMassTraitListPrivate::GetAssortedTags(AssortedTrait);
    }

    if (!TargetArray) return;
    if (!TargetArray->IsValidIndex(Item->AssortedIndex)) return;

    // Clear selection before removing — FStructOnScope in details view holds non-owning pointer
    OnTraitSelectedDelegate.ExecuteIfBound(nullptr);

    FScopedTransaction Transaction(LOCTEXT("RemoveAssortedEntryTransaction", "Remove Assorted Entry"));
    Asset->Modify();

    TargetArray->RemoveAt(Item->AssortedIndex);

    RebuildTree();
}

// ---------- Parent Config ----------

void SArcMassTraitList::ToggleParentEditing()
{
    bParentEditingEnabled = !bParentEditingEnabled;
    RebuildTree();
}

void SArcMassTraitList::OpenParentAsset()
{
    UMassEntityConfigAsset* Asset = ConfigAsset.Get();
    if (!Asset) return;

    const UMassEntityConfigAsset* ParentAsset = Asset->GetConfig().GetParent();
    if (!ParentAsset) return;

    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (AssetEditorSubsystem)
    {
        AssetEditorSubsystem->OpenEditorForAsset(const_cast<UMassEntityConfigAsset*>(ParentAsset));
    }
}

#undef LOCTEXT_NAMESPACE
