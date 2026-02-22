// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBuildResourceConsumption.h"

#include "ArcBuildingDefinition.h"
#include "ArcBuilderComponent.h"
#include "ArcBuildIngredient.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Items/ArcItemData.h"

// -------------------------------------------------------------------
// FArcBuildResourceConsumption (base)
// -------------------------------------------------------------------

bool FArcBuildResourceConsumption::CheckAndConsumeResources(
	const UArcBuildingDefinition* BuildingDef,
	const UArcBuilderComponent* BuilderComponent,
	bool bConsume) const
{
	return true;
}

// -------------------------------------------------------------------
// FArcBuildResourceConsumption_ItemStore
// -------------------------------------------------------------------

bool FArcBuildResourceConsumption_ItemStore::CheckAndConsumeResources(
	const UArcBuildingDefinition* BuildingDef,
	const UArcBuilderComponent* BuilderComponent,
	bool bConsume) const
{
	if (!BuildingDef || !BuilderComponent)
	{
		return false;
	}

	// No ingredients means free placement.
	if (BuildingDef->Ingredients.Num() == 0)
	{
		return true;
	}

	AActor* Owner = BuilderComponent->GetOwner();
	if (!Owner)
	{
		return false;
	}

	UArcItemsStoreComponent* ItemStore = Owner->FindComponentByClass<UArcItemsStoreComponent>();
	if (!ItemStore)
	{
		return false;
	}

	TArray<const FArcItemData*> Items = ItemStore->GetItems();

	// For each ingredient, check if we have enough matching items.
	// We track which items we've "claimed" so we don't double-count.
	struct FClaimedItem
	{
		const FArcItemData* Item;
		int32 ClaimedAmount;
	};
	TArray<FClaimedItem> ClaimedItems;

	for (const FInstancedStruct& IngredientStruct : BuildingDef->Ingredients)
	{
		const FArcBuildIngredient* Ingredient = IngredientStruct.GetPtr<FArcBuildIngredient>();
		if (!Ingredient)
		{
			continue;
		}

		int32 Remaining = Ingredient->Amount;

		for (const FArcItemData* ItemData : Items)
		{
			if (Remaining <= 0)
			{
				break;
			}

			if (!Ingredient->DoesItemSatisfy(ItemData))
			{
				continue;
			}

			// Check how much of this item is available (minus already claimed).
			int32 Available = ItemData->Spec.Amount;
			for (const FClaimedItem& Claimed : ClaimedItems)
			{
				if (Claimed.Item == ItemData)
				{
					Available -= Claimed.ClaimedAmount;
				}
			}

			if (Available <= 0)
			{
				continue;
			}

			int32 ToClaim = FMath::Min(Available, Remaining);
			Remaining -= ToClaim;

			// Record claim.
			FClaimedItem* Existing = ClaimedItems.FindByPredicate(
				[ItemData](const FClaimedItem& C) { return C.Item == ItemData; });
			if (Existing)
			{
				Existing->ClaimedAmount += ToClaim;
			}
			else
			{
				ClaimedItems.Add({ ItemData, ToClaim });
			}
		}

		if (Remaining > 0)
		{
			return false;
		}
	}

	// If we're only checking, we're done.
	if (!bConsume)
	{
		return true;
	}

	// Consume claimed items.
	for (const FClaimedItem& Claimed : ClaimedItems)
	{
		ItemStore->RemoveItem(Claimed.Item->ItemId, Claimed.ClaimedAmount);
	}

	return true;
}
