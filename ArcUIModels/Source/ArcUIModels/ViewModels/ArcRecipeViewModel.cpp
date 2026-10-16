// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/ArcRecipeViewModel.h"

#include "Items/ArcItemDefinitionData.h"
#include "ViewModels/ArcItemViewModel.h"
#include "Items/Fragments/ArcItemFragment_UIDescription.h"
#include "Items/Fragments/ArcItemFragment_Icon.h"
#include "ArcCraft/Rarity/ArcCraftRarity.h"
#include "ArcCraft/Rarity/ArcCraftRulesData.h"
#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Station/ArcCraftItemSource.h"
#include "ViewModels/ArcRecipeIngredientViewModel.h"
#include "Display/ArcTagDisplayRegistry.h"
#include "Items/ArcItemSpec.h"

namespace ArcRecipeVMPrivate
{
	FText FormatCraftTimeHMS(float Seconds)
	{
		const int32 TotalSeconds = FMath::Max(0, FMath::RoundToInt(Seconds));
		const int32 Hours = TotalSeconds / 3600;
		const int32 Minutes = (TotalSeconds % 3600) / 60;
		const int32 Secs = TotalSeconds % 60;
		return FText::FromString(FString::Printf(TEXT("%02d:%02d:%02d"), Hours, Minutes, Secs));
	}
}

void UArcRecipeViewModel::RefreshFrom(
	const FPrimaryAssetId& InRecipeId,
	const FArcRecipeDefinitionData& Recipe,
	TConstArrayView<FArcCraftStoreItemRow> InventoryItems,
	TConstArrayView<FArcItemSpec> ServerInput,
	TConstArrayView<FArcCraftStoreItemRow> PreviewInput,
	const UArcTagDisplayRegistry* Registry,
	const UArcStatDisplayRegistry* StatReg,
	int32 MaxCraftable)
{
	// --- RecipeId ---
	FArcNamedPrimaryAssetId NewRecipeId;
	NewRecipeId.AssetId = InRecipeId;
	UE_MVVM_SET_PROPERTY_VALUE(RecipeId, NewRecipeId);

	// --- DisplayName ---
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, Recipe.GetDisplayName());

	// --- Icon, Description & OutputAmount from the main output ---
	TSoftObjectPtr<UObject> NewIcon;
	FText NewDescription;
	int32 NewOutputAmount = 0;

	const FArcRecipeOutput* MainOutput = Recipe.GetMainOutput();
	const FArcItemDefinitionData* OutputDef = MainOutput != nullptr
		? FArcItemDefinitionData::Find(MainOutput->ItemDefinition.AssetId)
		: nullptr;
	if (MainOutput != nullptr)
	{
		NewOutputAmount = MainOutput->Amount;
	}
	if (OutputDef != nullptr)
	{
		if (const FArcItemFragment_Icon* IconFrag = OutputDef->FindFragment<FArcItemFragment_Icon>())
		{
			NewIcon = IconFrag->ItemIcon;
		}
		if (const FArcItemFragment_UIDescription* UIDesc = OutputDef->FindFragment<FArcItemFragment_UIDescription>())
		{
			NewDescription = UIDesc->ItemDescription;
		}
	}
	UE_MVVM_SET_PROPERTY_VALUE(Icon, NewIcon);
	UE_MVVM_SET_PROPERTY_VALUE(Description, NewDescription);

	// --- OutputAmount & CraftTime ---
	UE_MVVM_SET_PROPERTY_VALUE(OutputAmount, NewOutputAmount);
	UE_MVVM_SET_PROPERTY_VALUE(CraftTime, Recipe.CraftTime);
	UE_MVVM_SET_PROPERTY_VALUE(CraftTimeText, ArcRecipeVMPrivate::FormatCraftTimeHMS(Recipe.CraftTime));

	FGameplayTag NewOutputRarity = Recipe.GuaranteedRarity;
	if (!NewOutputRarity.IsValid())
	{
		if (const FArcCraftRulesData* Rules = ArcCraftRarity::GetRules())
		{
			NewOutputRarity = Rules->DefaultRarity;
		}
	}
	UE_MVVM_SET_PROPERTY_VALUE(OutputRarity, NewOutputRarity);

	// --- Build/recycle per-ingredient child VMs + bCanCraft ---
	TArray<TObjectPtr<UArcRecipeIngredientViewModel>> NewIngredients;
	TArray<TObjectPtr<UArcRecipeIngredientViewModel>> NewRequired;
	TArray<TObjectPtr<UArcRecipeIngredientViewModel>> NewOptional;
	NewIngredients.Reserve(Recipe.Ingredients.Num());
	bool bAllSatisfied = true;

	for (int32 i = 0; i < Recipe.Ingredients.Num(); ++i)
	{
		UArcRecipeIngredientViewModel* Child = nullptr;
		if (Ingredients.IsValidIndex(i) && Ingredients[i] != nullptr)
		{
			Child = Ingredients[i].Get();
		}
		else
		{
			Child = NewObject<UArcRecipeIngredientViewModel>(this);
		}

		Child->RefreshFrom(Recipe.Ingredients[i], InventoryItems, ServerInput, PreviewInput, Registry, StatReg);

		if (!Child->GetbSatisfied())
		{
			bAllSatisfied = false;
		}

		if (Recipe.IsIngredientOptional(i))
		{
			NewOptional.Add(Child);
		}
		else
		{
			NewRequired.Add(Child);
		}
		NewIngredients.Add(Child);
	}

	SetIngredients(MoveTemp(NewIngredients), MoveTemp(NewRequired), MoveTemp(NewOptional));
	UE_MVVM_SET_PROPERTY_VALUE(bCanCraft, bAllSatisfied);

	TArray<FArcItemSpec> Scratch;
	Scratch.Reserve(ServerInput.Num() + PreviewInput.Num());
	Scratch.Append(ServerInput.GetData(), ServerInput.Num());
	for (const FArcCraftStoreItemRow& Row : PreviewInput)
	{
		FArcItemSpec& PreviewSpec = Scratch.Add_GetRef(Row.Spec);
		PreviewSpec.Amount = static_cast<uint16>(FMath::Clamp(Row.Stacks, 0, static_cast<int32>(MAX_uint16)));
	}
	int32 NewCraftable = 0;
	while (NewCraftable < MaxCraftable
		&& FArcCraftItemSource::MatchAndConsumeFromSpecs(Scratch, Recipe, /*bConsume=*/true))
	{
		++NewCraftable;
	}
	UE_MVVM_SET_PROPERTY_VALUE(CraftableFromInput, NewCraftable);
}

void UArcRecipeViewModel::SetIngredients(
	TArray<TObjectPtr<UArcRecipeIngredientViewModel>>&& NewIngredients,
	TArray<TObjectPtr<UArcRecipeIngredientViewModel>>&& NewRequired,
	TArray<TObjectPtr<UArcRecipeIngredientViewModel>>&& NewOptional)
{
	UE_MVVM_SET_PROPERTY_VALUE(Ingredients, NewIngredients);
	UE_MVVM_SET_PROPERTY_VALUE(RequiredIngredients, NewRequired);
	UE_MVVM_SET_PROPERTY_VALUE(OptionalIngredients, NewOptional);
}

bool UArcRecipeViewModel::DoesItemSatisfyAnySlot(const UArcItemViewModel* Item) const
{
	if (Item == nullptr)
	{
		return false;
	}

	for (const TObjectPtr<UArcRecipeIngredientViewModel>& Slot : Ingredients)
	{
		if (Slot == nullptr)
		{
			continue;
		}

		const EArcRecipeIngredientSource SlotSource = Slot->GetSource();

		if (SlotSource == EArcRecipeIngredientSource::ByItemDef)
		{
			const FPrimaryAssetId& ItemAssetId = Item->GetItemDefinition();
			if (!ItemAssetId.IsValid())
			{
				continue;
			}
			if (ItemAssetId == Slot->GetRequiredItemId().AssetId)
			{
				return true;
			}
		}
		else if (SlotSource == EArcRecipeIngredientSource::ByTag)
		{
			const FGameplayTagContainer& ItemTags = Item->GetTags();
			const FGameplayTagContainer& Required = Slot->GetRequiredTags();
			const FGameplayTagContainer& Deny = Slot->GetDenyTags();

			if (Required.IsEmpty() && Deny.IsEmpty())
			{
				// A tag slot with no requirements is trivially satisfied by any item.
				return true;
			}

			const bool bHasRequired = Required.IsEmpty() || ItemTags.HasAll(Required);
			const bool bHasDenied = !Deny.IsEmpty() && ItemTags.HasAny(Deny);

			if (bHasRequired && !bHasDenied)
			{
				return true;
			}
		}
	}

	return false;
}
