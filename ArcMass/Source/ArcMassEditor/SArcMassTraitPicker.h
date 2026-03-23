// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Templates/SubclassOf.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class UMassEntityTraitBase;

DECLARE_DELEGATE_OneParam(FArcOnTraitClassPicked, TSubclassOf<UMassEntityTraitBase>);

struct FArcTraitPickerItem
{
    FString DisplayName;
    FText TooltipText;
    bool bIsCategory = false;
    TSubclassOf<UMassEntityTraitBase> TraitClass;
    TArray<TSharedPtr<FArcTraitPickerItem>> Children;
};

class SArcMassTraitPicker : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SArcMassTraitPicker) {}
        SLATE_ARGUMENT(TSet<UClass*>, ExcludedClasses)
        SLATE_EVENT(FArcOnTraitClassPicked, OnTraitClassPicked)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    void RebuildList(const FString& FilterText);
    TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FArcTraitPickerItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
    void OnGetChildren(TSharedPtr<FArcTraitPickerItem> Item, TArray<TSharedPtr<FArcTraitPickerItem>>& OutChildren);

    TSharedPtr<STreeView<TSharedPtr<FArcTraitPickerItem>>> TreeView;
    TArray<TSharedPtr<FArcTraitPickerItem>> RootItems;
    TSet<UClass*> ExcludedClasses;
    FArcOnTraitClassPicked OnTraitClassPickedDelegate;
};
