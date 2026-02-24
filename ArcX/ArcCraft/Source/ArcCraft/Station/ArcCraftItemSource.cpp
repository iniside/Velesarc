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

#include "ArcCraft/Station/ArcCraftItemSource.h"

#include "ArcCoreUtils.h"
#include "MassEntitySubsystem.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Mass/ArcCraftVisEntityComponent.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeQuality.h"
#include "ArcCraft/Station/ArcCraftStationComponent.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemId.h"
#include "Items/ArcItemsStoreComponent.h"

// -------------------------------------------------------------------
// FArcCraftItemSource (base)
// -------------------------------------------------------------------

TArray<const FArcItemData*> FArcCraftItemSource::GetAvailableItems(
	const UArcCraftStationComponent* Station,
	const UObject* Instigator) const
{
	return {};
}

bool FArcCraftItemSource::CanSatisfyRecipe(
	const UArcCraftStationComponent* Station,
	const UArcRecipeDefinition* Recipe,
	const UObject* Instigator) const
{
	return false;
}

bool FArcCraftItemSource::ConsumeIngredients(
	UArcCraftStationComponent* Station,
	const UArcRecipeDefinition* Recipe,
	const UObject* Instigator,
	TArray<const FArcItemData*>& OutMatchedItems,
	TArray<float>& OutQualityMults) const
{
	return false;
}

bool FArcCraftItemSource::DepositItem(
	UArcCraftStationComponent* Station,
	const FArcItemSpec& Item,
	const UObject* Instigator)
{
	return false;
}

bool FArcCraftItemSource::MatchAndConsumeFromStore(
	UArcItemsStoreComponent* ItemsStore,
	const UArcRecipeDefinition* Recipe,
	bool bConsume,
	TArray<const FArcItemData*>* OutMatchedItems,
	TArray<float>* OutQualityMults)
{
	if (!ItemsStore || !Recipe)
	{
		return false;
	}

	// Load quality tier table if available
	UArcQualityTierTable* TierTable = nullptr;
	if (!Recipe->QualityTierTable.IsNull())
	{
		TierTable = Recipe->QualityTierTable.LoadSynchronous();
	}

	TArray<TPair<FArcItemId, int32>> ItemsToConsume;
	TSet<FArcItemId> UsedItemIds;

	TArray<const FArcItemData*> MatchedIngredients;
	TArray<float> QualityMultipliers;
	MatchedIngredients.SetNum(Recipe->Ingredients.Num());
	QualityMultipliers.SetNum(Recipe->Ingredients.Num());

	const TArray<const FArcItemData*> AllItems = ItemsStore->GetItems();

	for (int32 SlotIdx = 0; SlotIdx < Recipe->Ingredients.Num(); ++SlotIdx)
	{
		const FArcRecipeIngredient* Ingredient = Recipe->GetIngredientBase(SlotIdx);
		if (!Ingredient)
		{
			return false;
		}

		const FArcItemData* MatchedItem = nullptr;
		for (const FArcItemData* ItemData : AllItems)
		{
			if (!ItemData || UsedItemIds.Contains(ItemData->GetItemId()))
			{
				continue;
			}

			if (!Ingredient->DoesItemSatisfy(ItemData, TierTable))
			{
				continue;
			}

			if (ItemData->GetStacks() < static_cast<uint16>(Ingredient->Amount))
			{
				continue;
			}

			MatchedItem = ItemData;
			break;
		}

		if (!MatchedItem)
		{
			return false;
		}

		MatchedIngredients[SlotIdx] = MatchedItem;
		QualityMultipliers[SlotIdx] = Ingredient->GetItemQualityMultiplier(MatchedItem, TierTable);

		UsedItemIds.Add(MatchedItem->GetItemId());

		if (Ingredient->bConsumeOnCraft)
		{
			ItemsToConsume.Add({MatchedItem->GetItemId(), Ingredient->Amount});
		}
	}

	if (bConsume)
	{
		for (const auto& Pair : ItemsToConsume)
		{
			ItemsStore->RemoveItem(Pair.Key, Pair.Value, true);
		}
	}

	if (OutMatchedItems)
	{
		*OutMatchedItems = MoveTemp(MatchedIngredients);
	}
	if (OutQualityMults)
	{
		*OutQualityMults = MoveTemp(QualityMultipliers);
	}

	return true;
}

// -------------------------------------------------------------------
// FArcCraftItemSource_InstigatorStore
// -------------------------------------------------------------------

UArcItemsStoreComponent* FArcCraftItemSource_InstigatorStore::GetItemsStore(const UObject* Instigator) const
{
	const AActor* OwnerActor = Cast<AActor>(Instigator);
	if (!OwnerActor)
	{
		return nullptr;
	}
	return Arcx::Utils::GetComponent(OwnerActor, ItemsStoreClass);
}

TArray<const FArcItemData*> FArcCraftItemSource_InstigatorStore::GetAvailableItems(
	const UArcCraftStationComponent* Station,
	const UObject* Instigator) const
{
	UArcItemsStoreComponent* Store = GetItemsStore(Instigator);
	if (!Store)
	{
		return {};
	}
	return Store->GetItems();
}

bool FArcCraftItemSource_InstigatorStore::CanSatisfyRecipe(
	const UArcCraftStationComponent* Station,
	const UArcRecipeDefinition* Recipe,
	const UObject* Instigator) const
{
	UArcItemsStoreComponent* Store = GetItemsStore(Instigator);
	return MatchAndConsumeFromStore(Store, Recipe, false);
}

bool FArcCraftItemSource_InstigatorStore::ConsumeIngredients(
	UArcCraftStationComponent* Station,
	const UArcRecipeDefinition* Recipe,
	const UObject* Instigator,
	TArray<const FArcItemData*>& OutMatchedItems,
	TArray<float>& OutQualityMults) const
{
	UArcItemsStoreComponent* Store = GetItemsStore(Instigator);
	return MatchAndConsumeFromStore(Store, Recipe, true, &OutMatchedItems, &OutQualityMults);
}

bool FArcCraftItemSource_InstigatorStore::DepositItem(
	UArcCraftStationComponent* Station,
	const FArcItemSpec& Item,
	const UObject* Instigator)
{
	// Instigator store doesn't support deposit — items are already in the player's inventory.
	return false;
}

// -------------------------------------------------------------------
// FArcCraftItemSource_StationStore
// -------------------------------------------------------------------

UArcItemsStoreComponent* FArcCraftItemSource_StationStore::GetStationStore(
	const UArcCraftStationComponent* Station) const
{
	if (!Station)
	{
		return nullptr;
	}

	AActor* Owner = Station->GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	return Arcx::Utils::GetComponent(Owner, ItemsStoreClass);
}

TArray<const FArcItemData*> FArcCraftItemSource_StationStore::GetAvailableItems(
	const UArcCraftStationComponent* Station,
	const UObject* Instigator) const
{
	UArcItemsStoreComponent* Store = GetStationStore(Station);
	if (!Store)
	{
		return {};
	}
	return Store->GetItems();
}

bool FArcCraftItemSource_StationStore::CanSatisfyRecipe(
	const UArcCraftStationComponent* Station,
	const UArcRecipeDefinition* Recipe,
	const UObject* Instigator) const
{
	UArcItemsStoreComponent* Store = GetStationStore(Station);
	return MatchAndConsumeFromStore(Store, Recipe, false);
}

bool FArcCraftItemSource_StationStore::ConsumeIngredients(
	UArcCraftStationComponent* Station,
	const UArcRecipeDefinition* Recipe,
	const UObject* Instigator,
	TArray<const FArcItemData*>& OutMatchedItems,
	TArray<float>& OutQualityMults) const
{
	UArcItemsStoreComponent* Store = GetStationStore(Station);
	return MatchAndConsumeFromStore(Store, Recipe, true, &OutMatchedItems, &OutQualityMults);
}

bool FArcCraftItemSource_StationStore::DepositItem(
	UArcCraftStationComponent* Station,
	const FArcItemSpec& Item,
	const UObject* Instigator)
{
	UArcItemsStoreComponent* Store = GetStationStore(Station);
	if (!Store)
	{
		return false;
	}

	Store->AddItem(Item, FArcItemId::InvalidId);
	return true;
}

// -------------------------------------------------------------------
// FArcCraftItemSource_EntityStore
// -------------------------------------------------------------------

UArcCraftVisEntityComponent* FArcCraftItemSource_EntityStore::GetVisComponent(
	const UArcCraftStationComponent* Station) const
{
	if (!Station || !Station->GetOwner())
	{
		return nullptr;
	}
	return Station->GetOwner()->FindComponentByClass<UArcCraftVisEntityComponent>();
}

UArcItemsStoreComponent* FArcCraftItemSource_EntityStore::GetInputStore(
	const UArcCraftStationComponent* Station) const
{
	UArcCraftVisEntityComponent* VisComp = GetVisComponent(Station);
	if (!VisComp || !VisComp->InputStoreClass || !Station->GetOwner())
	{
		return nullptr;
	}

	TArray<UArcItemsStoreComponent*> Stores;
	Station->GetOwner()->GetComponents<UArcItemsStoreComponent>(Stores);
	for (UArcItemsStoreComponent* Store : Stores)
	{
		if (Store && Store->IsA(VisComp->InputStoreClass))
		{
			return Store;
		}
	}
	return nullptr;
}

TArray<const FArcItemData*> FArcCraftItemSource_EntityStore::GetAvailableItems(
	const UArcCraftStationComponent* Station,
	const UObject* Instigator) const
{
	// When actor is alive, read from the mirrored input store
	UArcItemsStoreComponent* Store = GetInputStore(Station);
	if (Store)
	{
		return Store->GetItems();
	}
	return {};
}

bool FArcCraftItemSource_EntityStore::CanSatisfyRecipe(
	const UArcCraftStationComponent* Station,
	const UArcRecipeDefinition* Recipe,
	const UObject* Instigator) const
{
	UArcItemsStoreComponent* Store = GetInputStore(Station);
	return MatchAndConsumeFromStore(Store, Recipe, false);
}

bool FArcCraftItemSource_EntityStore::ConsumeIngredients(
	UArcCraftStationComponent* Station,
	const UArcRecipeDefinition* Recipe,
	const UObject* Instigator,
	TArray<const FArcItemData*>& OutMatchedItems,
	TArray<float>& OutQualityMults) const
{
	UArcItemsStoreComponent* Store = GetInputStore(Station);
	return MatchAndConsumeFromStore(Store, Recipe, true, &OutMatchedItems, &OutQualityMults);
}

bool FArcCraftItemSource_EntityStore::DepositItem(
	UArcCraftStationComponent* Station,
	const FArcItemSpec& Item,
	const UObject* Instigator)
{
	// When actor is alive, deposit into the mirrored input store.
	// On deactivation, UArcCraftVisEntityComponent syncs store → entity fragment.
	UArcItemsStoreComponent* Store = GetInputStore(Station);
	if (!Store)
	{
		return false;
	}

	Store->AddItem(Item, FArcItemId::InvalidId);
	return true;
}
