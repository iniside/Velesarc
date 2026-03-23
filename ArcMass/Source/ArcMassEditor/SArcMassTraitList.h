// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class UMassEntityConfigAsset;
class UMassEntityTraitBase;

/** Which kind of item this tree node represents. */
enum class EArcMassTraitListItemType : uint8
{
    Category,
    Trait,
    AssortedFragment,
    AssortedTag,
    NullTrait
};

/** A single node in the trait list tree. */
struct FArcMassTraitListItem
{
    FString DisplayName;
    EArcMassTraitListItemType ItemType = EArcMassTraitListItemType::Trait;

    /** The trait object (valid for Trait rows, null for NullTrait). */
    TWeakObjectPtr<UMassEntityTraitBase> Trait;

    /** True when this item comes from a parent config (read-only by default). */
    bool bIsParentTrait = false;

    /** Index into the assorted Fragments/Tags array (for AssortedFragment/AssortedTag rows). */
    int32 AssortedIndex = INDEX_NONE;

    /** The UMassAssortedFragmentsTrait that owns this fragment/tag entry. */
    TWeakObjectPtr<UMassEntityTraitBase> OwningAssortedTrait;

    /** Child nodes (for category/group items). */
    TArray<TSharedPtr<FArcMassTraitListItem>> Children;

    /** Tooltip text shown on hover. */
    FText TooltipText;

    /** True when this is the "Assorted Fragments" expandable category. */
    bool bIsAssortedCategory = false;

    /** True when this is the "Fragments" sub-category under an assorted trait. */
    bool bIsAssortedFragmentSubCategory = false;

    /** True when this is the "Tags" sub-category under an assorted trait. */
    bool bIsAssortedTagSubCategory = false;

    /** True when this category groups parent config traits. */
    bool bIsParentCategory = false;

    bool IsGroup() const { return ItemType == EArcMassTraitListItemType::Category; }
    bool IsSelectable() const { return ItemType != EArcMassTraitListItemType::Category; }
};

DECLARE_DELEGATE_OneParam(FArcOnTraitSelected, TSharedPtr<FArcMassTraitListItem>);

/**
 * Tree widget that displays Mass entity traits grouped by category.
 * Supports assorted fragment/tag children, parent config traits, and null trait handling.
 */
class SArcMassTraitList : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SArcMassTraitList) {}
        SLATE_EVENT(FArcOnTraitSelected, OnTraitSelected)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UMassEntityConfigAsset* InConfigAsset);

    /** Rebuild the full tree from the config asset's current state. */
    void RebuildTree();

private:
    // --- Tree view callbacks ---
    TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FArcMassTraitListItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
    void OnGetChildren(TSharedPtr<FArcMassTraitListItem> Item, TArray<TSharedPtr<FArcMassTraitListItem>>& OutChildren);
    void OnSelectionChanged(TSharedPtr<FArcMassTraitListItem> Item, ESelectInfo::Type SelectInfo);

    // --- Trait mutations ---
    void AddTrait(TSubclassOf<UMassEntityTraitBase> TraitClass);
    void RemoveTrait(TSharedPtr<FArcMassTraitListItem> Item);

    // --- Assorted fragment/tag mutations (stubs for Task 6) ---
    void AddAssortedFragment(UMassEntityTraitBase* AssortedTrait);
    void AddAssortedTag(UMassEntityTraitBase* AssortedTrait);
    void RemoveAssortedEntry(TSharedPtr<FArcMassTraitListItem> Item);

    // --- Parent config ---
    void ToggleParentEditing();
    void OpenParentAsset();
    bool IsParentEditingEnabled() const { return bParentEditingEnabled; }

    // --- Tree building helpers ---
    TSharedPtr<FArcMassTraitListItem> BuildAssortedCategory(UMassEntityTraitBase* AssortedTrait, bool bIsParent);
    FString GetTraitDisplayName(const UMassEntityTraitBase* Trait) const;
    FString GetTraitCategory(const UMassEntityTraitBase* Trait) const;
    FText GetTraitTooltip(const UMassEntityTraitBase* Trait) const;
    FText GetStructTooltip(const UScriptStruct* Struct) const;

    TSharedPtr<STreeView<TSharedPtr<FArcMassTraitListItem>>> TreeView;
    TArray<TSharedPtr<FArcMassTraitListItem>> RootItems;
    TWeakObjectPtr<UMassEntityConfigAsset> ConfigAsset;
    FArcOnTraitSelected OnTraitSelectedDelegate;

    /** When true, parent traits become editable in this config. */
    bool bParentEditingEnabled = false;
};
