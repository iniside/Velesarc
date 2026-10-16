// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/ArcItemViewModel.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Items/Fragments/ArcItemFragment_UIDescription.h"
#include "Items/Fragments/ArcItemFragment_Icon.h"
#include "Items/Fragments/ArcItemFragment_Rarity.h"
#include "Items/Fragments/ArcItemFragment_InstanceTags.h"
#include "ArcCraft/Rarity/ArcCraftRarity.h"
#include "ArcCraft/Rarity/ArcCraftRulesData.h"
#include "ItemFragments/ArcItemFragment_ItemStats.h"
#include "Display/ArcStatDisplayRegistry.h"
#include "Display/ArcTagDisplayRegistry.h"
#include "ViewModels/ArcItemStatRowViewModel.h"
#include "NativeGameplayTags.h"

namespace ArcUIModelsItemVM
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Material_Property_Magic, "Material.Property.Magic");

	bool IsSpecEnchanted(const FArcItemSpec& Spec)
	{
		return ArcMassItems::GetSpecInstanceTags(Spec).HasTag(TAG_Material_Property_Magic);
	}

	FGameplayTag ResolveSpecRarity(const FArcItemSpec& Spec)
	{
		const FGameplayTag SpecRarity = ArcMassItems::GetSpecRarity(Spec);
		if (SpecRarity.IsValid())
		{
			return SpecRarity;
		}

		const FArcCraftRulesData* Rules = ArcCraftRarity::GetRules();
		return Rules != nullptr ? Rules->DefaultRarity : FGameplayTag();
	}
}

void UArcItemViewModel::RefreshFrom(const FArcItemData& Item, const UArcTagDisplayRegistry* TagReg)
{
	UE_MVVM_SET_PROPERTY_VALUE(ItemId, Item.ItemId);

	UE_MVVM_SET_PROPERTY_VALUE(ItemDefinition, Item.GetItemDefinitionId());

	const int32 NewStacks = static_cast<int32>(Item.Stacks);
	UE_MVVM_SET_PROPERTY_VALUE(Stacks, NewStacks);

	const int32 NewLevel = static_cast<int32>(Item.Level);
	UE_MVVM_SET_PROPERTY_VALUE(Level, NewLevel);

	UE_MVVM_SET_PROPERTY_VALUE(CurrentItemStoreSlot, Item.Slot);

	UE_MVVM_SET_PROPERTY_VALUE(Tags, Item.ItemAggregatedTags);

	FText NewDisplayName;
	FText NewDescription;
	if (const FArcItemFragment_UIDescription* UI = ArcItemsHelper::FindFragment<FArcItemFragment_UIDescription>(&Item))
	{
		NewDisplayName = UI->ItemName;
		NewDescription = UI->ItemDescription;
	}
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, NewDisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Description, NewDescription);

	TSoftObjectPtr<UObject> NewIcon;
	if (const FArcItemFragment_Icon* IconFrag = ArcItemsHelper::FindFragment<FArcItemFragment_Icon>(&Item))
	{
		NewIcon = IconFrag->ItemIcon;
	}
	UE_MVVM_SET_PROPERTY_VALUE(Icon, NewIcon);

	SetRarity(ArcUIModelsItemVM::ResolveSpecRarity(Item.Spec), TagReg);
	UE_MVVM_SET_PROPERTY_VALUE(bEnchanted, ArcUIModelsItemVM::IsSpecEnchanted(Item.Spec));
}

void UArcItemViewModel::RefreshFrom(const FArcItemSpec& Spec, int32 InStacks, const UArcTagDisplayRegistry* TagReg)
{
	UE_MVVM_SET_PROPERTY_VALUE(ItemId, Spec.ItemId);

	const FArcItemDefinitionData* NewDef = Spec.GetItemDefinition();
	UE_MVVM_SET_PROPERTY_VALUE(ItemDefinition, Spec.GetItemDefinitionId());

	UE_MVVM_SET_PROPERTY_VALUE(Stacks, InStacks);

	const int32 NewLevel = static_cast<int32>(Spec.Level);
	UE_MVVM_SET_PROPERTY_VALUE(Level, NewLevel);

	UE_MVVM_SET_PROPERTY_VALUE(CurrentItemStoreSlot, FGameplayTag());

	// Mirror FArcItemData::InitializeMass: aggregated tags are the definition's ItemTags.
	// The spec's definition may be unresolved — every fragment read is null-guarded.
	FGameplayTagContainer NewTags;
	FText NewDisplayName;
	FText NewDescription;
	TSoftObjectPtr<UObject> NewIcon;
	if (NewDef != nullptr)
	{
		if (const FArcItemFragment_Tags* TagsFrag = NewDef->FindFragment<FArcItemFragment_Tags>())
		{
			NewTags.AppendTags(TagsFrag->ItemTags);
		}
		if (const FArcItemFragment_UIDescription* UI = NewDef->FindFragment<FArcItemFragment_UIDescription>())
		{
			NewDisplayName = UI->ItemName;
			NewDescription = UI->ItemDescription;
		}
		if (const FArcItemFragment_Icon* IconFrag = NewDef->FindFragment<FArcItemFragment_Icon>())
		{
			NewIcon = IconFrag->ItemIcon;
		}
	}
	UE_MVVM_SET_PROPERTY_VALUE(Tags, NewTags);
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, NewDisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Description, NewDescription);
	UE_MVVM_SET_PROPERTY_VALUE(Icon, NewIcon);

	SetRarity(ArcUIModelsItemVM::ResolveSpecRarity(Spec), TagReg);
	UE_MVVM_SET_PROPERTY_VALUE(bEnchanted, ArcUIModelsItemVM::IsSpecEnchanted(Spec));
}

void UArcItemViewModel::RefreshFromDefinition(
	const FArcItemDefinitionData* Def,
	const UArcStatDisplayRegistry* StatReg)
{
	// Def may be unresolved — clear the definition-derived fields and bail.
	if (Def == nullptr)
	{
		UE_MVVM_SET_PROPERTY_VALUE(ItemDefinition, FPrimaryAssetId());
		UE_MVVM_SET_PROPERTY_VALUE(DisplayName, FText());
		UE_MVVM_SET_PROPERTY_VALUE(Description, FText());
		UE_MVVM_SET_PROPERTY_VALUE(Icon, TSoftObjectPtr<UObject>());
		UE_MVVM_SET_PROPERTY_VALUE(Tags, FGameplayTagContainer());
		SetRarity(FGameplayTag(), nullptr);
		UE_MVVM_SET_PROPERTY_VALUE(bEnchanted, false);
		UE_MVVM_SET_PROPERTY_VALUE(CategoryLabel, FText());
		SetStatRows(TArray<TObjectPtr<UArcItemStatRowViewModel>>());
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(ItemDefinition, Def->GetPrimaryAssetId());

	// Name / Description / Icon / Tags from the definition fragments.
	FText NewDisplayName;
	FText NewDescription;
	TSoftObjectPtr<UObject> NewIcon;
	FGameplayTagContainer NewTags;

	if (const FArcItemFragment_UIDescription* UI = Def->FindFragment<FArcItemFragment_UIDescription>())
	{
		NewDisplayName = UI->ItemName;
		NewDescription = UI->ItemDescription;
	}
	if (const FArcItemFragment_Icon* IconFrag = Def->FindFragment<FArcItemFragment_Icon>())
	{
		NewIcon = IconFrag->ItemIcon;
	}

	if (const FArcItemFragment_Tags* TagsFrag = Def->FindFragment<FArcItemFragment_Tags>())
	{
		NewTags.AppendTags(TagsFrag->AssetTags);
	}

	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, NewDisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Description, NewDescription);
	UE_MVVM_SET_PROPERTY_VALUE(Icon, NewIcon);
	UE_MVVM_SET_PROPERTY_VALUE(Tags, NewTags);
	SetRarity(FGameplayTag(), nullptr);
	UE_MVVM_SET_PROPERTY_VALUE(bEnchanted, false);
	// CategoryLabel intentionally left untouched/empty — no Item.Category.* convention yet.
	UE_MVVM_SET_PROPERTY_VALUE(CategoryLabel, FText());

	// Stat rows from the definition's DefaultStats (design-time). Recycle rows by index, two-pass prune.
	TArray<TObjectPtr<UArcItemStatRowViewModel>> NewStatRows;
	if (const FArcItemFragment_ItemStats* StatsFrag = Def->FindFragment<FArcItemFragment_ItemStats>())
	{
		NewStatRows.Reserve(StatsFrag->DefaultStats.Num());
		for (int32 i = 0; i < StatsFrag->DefaultStats.Num(); ++i)
		{
			UArcItemStatRowViewModel* Row = nullptr;
			if (StatRows.IsValidIndex(i) && StatRows[i] != nullptr)
			{
				Row = StatRows[i].Get();
			}
			else
			{
				Row = NewObject<UArcItemStatRowViewModel>(this);
			}

			Row->RefreshFrom(StatsFrag->DefaultStats[i], StatReg);
			NewStatRows.Add(Row);
		}
	}
	SetStatRows(MoveTemp(NewStatRows));
}

void UArcItemViewModel::SetRarity(FGameplayTag RarityTag, const UArcTagDisplayRegistry* TagReg)
{
	FText NewRarityLabel;
	FLinearColor NewRarityColor = FLinearColor::White;
	if (RarityTag.IsValid() && TagReg != nullptr)
	{
		if (const FArcTagDisplayData* Disp = TagReg->FindDisplay(RarityTag))
		{
			NewRarityLabel = Disp->DisplayName;
			NewRarityColor = Disp->Color;
		}
	}
	UE_MVVM_SET_PROPERTY_VALUE(RarityLabel, NewRarityLabel);
	UE_MVVM_SET_PROPERTY_VALUE(RarityColor, NewRarityColor);
}

void UArcItemViewModel::SetStatRows(TArray<TObjectPtr<UArcItemStatRowViewModel>>&& NewStatRows)
{
	UE_MVVM_SET_PROPERTY_VALUE(StatRows, NewStatRows);
}

