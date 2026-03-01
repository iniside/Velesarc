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
#include "Engine/World.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemsArray.h"
#include "Items/ArcItemsStoreComponent.h"

// -------------------------------------------------------------------
// FArcCraftItemSource (base)
// -------------------------------------------------------------------

TArray<FArcItemSpec> FArcCraftItemSource::GetAvailableItems(
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
	TArray<FArcItemSpec>& OutMatchedItems,
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

bool FArcCraftItemSource::WithdrawItem(
	UArcCraftStationComponent* Station,
	int32 ItemIndex,
	int32 Stacks,
	FArcItemSpec& OutSpec,
	const UObject* Instigator)
{
	return false;
}

bool FArcCraftItemSource::MatchAndConsumeFromSpecs(
	TArray<FArcItemSpec>& Items,
	const UArcRecipeDefinition* Recipe,
	bool bConsume,
	TArray<FArcItemSpec>* OutMatchedItems,
	TArray<float>* OutQualityMults)
{
	if (!Recipe)
	{
		return false;
	}

	UArcQualityTierTable* TierTable = nullptr;
	if (!Recipe->QualityTierTable.IsNull())
	{
		TierTable = Recipe->QualityTierTable.LoadSynchronous();
	}

	TSet<int32> UsedIndices;
	TArray<FArcItemSpec> MatchedIngredients;
	TArray<float> QualityMultipliers;
	MatchedIngredients.SetNum(Recipe->Ingredients.Num());
	QualityMultipliers.SetNum(Recipe->Ingredients.Num());

	// Track items to consume (index + amount)
	TArray<TPair<int32, int32>> ItemsToConsume;

	for (int32 SlotIdx = 0; SlotIdx < Recipe->Ingredients.Num(); ++SlotIdx)
	{
		const FArcRecipeIngredient* Ingredient = Recipe->GetIngredientBase(SlotIdx);
		if (!Ingredient)
		{
			return false;
		}

		int32 MatchedIndex = INDEX_NONE;
		for (int32 ItemIdx = 0; ItemIdx < Items.Num(); ++ItemIdx)
		{
			if (UsedIndices.Contains(ItemIdx))
			{
				continue;
			}

			const FArcItemSpec& Spec = Items[ItemIdx];
			if (!Spec.GetItemDefinitionId().IsValid())
			{
				continue;
			}

			if (!Ingredient->DoesItemSatisfy(Spec, TierTable))
			{
				continue;
			}

			if (Spec.Amount < static_cast<uint16>(Ingredient->Amount))
			{
				continue;
			}

			MatchedIndex = ItemIdx;
			break;
		}

		if (MatchedIndex == INDEX_NONE)
		{
			return false;
		}

		MatchedIngredients[SlotIdx] = Items[MatchedIndex];
		QualityMultipliers[SlotIdx] = Ingredient->GetItemQualityMultiplier(Items[MatchedIndex], TierTable);

		UsedIndices.Add(MatchedIndex);

		if (Ingredient->bConsumeOnCraft)
		{
			ItemsToConsume.Add({MatchedIndex, Ingredient->Amount});
		}
	}

	if (bConsume)
	{
		// Remove consumed amounts from the Items array. Process in reverse index order
		// so that removals don't invalidate earlier indices.
		ItemsToConsume.Sort([](const TPair<int32, int32>& A, const TPair<int32, int32>& B)
		{
			return A.Key > B.Key;
		});

		for (const auto& Pair : ItemsToConsume)
		{
			FArcItemSpec& Spec = Items[Pair.Key];
			if (Spec.Amount <= static_cast<uint16>(Pair.Value))
			{
				Items.RemoveAt(Pair.Key);
			}
			else
			{
				Spec.Amount -= static_cast<uint16>(Pair.Value);
			}
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

bool FArcCraftItemSource::MatchAndConsumeFromStore(
	UArcItemsStoreComponent* ItemsStore,
	const UArcRecipeDefinition* Recipe,
	bool bConsume,
	TArray<FArcItemSpec>* OutMatchedItems,
	TArray<float>* OutQualityMults)
{
	if (!ItemsStore || !Recipe)
	{
		return false;
	}

	UArcQualityTierTable* TierTable = nullptr;
	if (!Recipe->QualityTierTable.IsNull())
	{
		TierTable = Recipe->QualityTierTable.LoadSynchronous();
	}

	TArray<TPair<FArcItemId, int32>> ItemsToConsume;
	TSet<FArcItemId> UsedItemIds;

	TArray<FArcItemSpec> MatchedIngredients;
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

			const FArcItemSpec ItemSpec = FArcItemCopyContainerHelper::ToSpec(ItemData);
			if (!Ingredient->DoesItemSatisfy(ItemSpec, TierTable))
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

		const FArcItemSpec MatchedSpec = FArcItemCopyContainerHelper::ToSpec(MatchedItem);
		MatchedIngredients[SlotIdx] = MatchedSpec;
		QualityMultipliers[SlotIdx] = Ingredient->GetItemQualityMultiplier(MatchedSpec, TierTable);

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

TArray<FArcItemSpec> FArcCraftItemSource_InstigatorStore::GetAvailableItems(
	const UArcCraftStationComponent* Station,
	const UObject* Instigator) const
{
	UArcItemsStoreComponent* Store = GetItemsStore(Instigator);
	if (!Store)
	{
		return {};
	}

	TArray<FArcItemSpec> Specs;
	const TArray<const FArcItemData*> Items = Store->GetItems();
	Specs.Reserve(Items.Num());
	for (const FArcItemData* ItemData : Items)
	{
		if (ItemData)
		{
			Specs.Add(FArcItemCopyContainerHelper::ToSpec(ItemData));
		}
	}
	return Specs;
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
	TArray<FArcItemSpec>& OutMatchedItems,
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

bool FArcCraftItemSource_InstigatorStore::WithdrawItem(
	UArcCraftStationComponent* Station,
	int32 ItemIndex,
	int32 Stacks,
	FArcItemSpec& OutSpec,
	const UObject* Instigator)
{
	// Instigator store doesn't support withdrawal — items are in the player's inventory, not the station.
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

TArray<FArcItemSpec> FArcCraftItemSource_StationStore::GetAvailableItems(
	const UArcCraftStationComponent* Station,
	const UObject* Instigator) const
{
	UArcItemsStoreComponent* Store = GetStationStore(Station);
	if (!Store)
	{
		return {};
	}

	TArray<FArcItemSpec> Specs;
	const TArray<const FArcItemData*> Items = Store->GetItems();
	Specs.Reserve(Items.Num());
	for (const FArcItemData* ItemData : Items)
	{
		if (ItemData)
		{
			Specs.Add(FArcItemCopyContainerHelper::ToSpec(ItemData));
		}
	}
	return Specs;
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
	TArray<FArcItemSpec>& OutMatchedItems,
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

bool FArcCraftItemSource_StationStore::WithdrawItem(
	UArcCraftStationComponent* Station,
	int32 ItemIndex,
	int32 Stacks,
	FArcItemSpec& OutSpec,
	const UObject* Instigator)
{
	UArcItemsStoreComponent* Store = GetStationStore(Station);
	if (!Store)
	{
		return false;
	}

	const TArray<const FArcItemData*> Items = Store->GetItems();
	if (!Items.IsValidIndex(ItemIndex))
	{
		return false;
	}

	const FArcItemData* ItemData = Items[ItemIndex];
	if (!ItemData)
	{
		return false;
	}

	const int32 ItemStacks = static_cast<int32>(ItemData->GetStacks());
	const int32 StacksToWithdraw = (Stacks <= 0 || Stacks >= ItemStacks) ? ItemStacks : Stacks;

	OutSpec = FArcItemCopyContainerHelper::ToSpec(ItemData);
	OutSpec.Amount = static_cast<uint16>(StacksToWithdraw);

	if (StacksToWithdraw >= ItemStacks)
	{
		Store->DestroyItem(ItemData->GetItemId());
	}
	else
	{
		Store->RemoveItem(ItemData->GetItemId(), StacksToWithdraw);
	}

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

FArcCraftInputFragment* FArcCraftItemSource_EntityStore::GetInputFragment(
	const UArcCraftStationComponent* Station) const
{
	UArcCraftVisEntityComponent* VisComp = GetVisComponent(Station);
	if (!VisComp)
	{
		return nullptr;
	}

	const FMassEntityHandle Entity = VisComp->GetEntityHandle();
	if (!Entity.IsValid())
	{
		return nullptr;
	}

	UWorld* World = Station->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem)
	{
		return nullptr;
	}

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityValid(Entity))
	{
		return nullptr;
	}

	return EntityManager.GetFragmentDataPtr<FArcCraftInputFragment>(Entity);
}

UArcItemsStoreComponent* FArcCraftItemSource_EntityStore::GetMirrorInputStore(
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

TArray<FArcItemSpec> FArcCraftItemSource_EntityStore::GetAvailableItems(
	const UArcCraftStationComponent* Station,
	const UObject* Instigator) const
{
	const FArcCraftInputFragment* InputFrag = GetInputFragment(Station);
	if (InputFrag)
	{
		return InputFrag->InputItems;
	}
	return {};
}

bool FArcCraftItemSource_EntityStore::CanSatisfyRecipe(
	const UArcCraftStationComponent* Station,
	const UArcRecipeDefinition* Recipe,
	const UObject* Instigator) const
{
	const FArcCraftInputFragment* InputFrag = GetInputFragment(Station);
	if (!InputFrag)
	{
		return false;
	}

	// Match against a copy — don't modify the fragment for a check
	TArray<FArcItemSpec> ItemsCopy = InputFrag->InputItems;
	return MatchAndConsumeFromSpecs(ItemsCopy, Recipe, false);
}

bool FArcCraftItemSource_EntityStore::ConsumeIngredients(
	UArcCraftStationComponent* Station,
	const UArcRecipeDefinition* Recipe,
	const UObject* Instigator,
	TArray<FArcItemSpec>& OutMatchedItems,
	TArray<float>& OutQualityMults) const
{
	FArcCraftInputFragment* InputFrag = GetInputFragment(Station);
	if (!InputFrag)
	{
		return false;
	}

	// Consume directly from the entity fragment (source of truth)
	if (!MatchAndConsumeFromSpecs(InputFrag->InputItems, Recipe, true, &OutMatchedItems, &OutQualityMults))
	{
		return false;
	}

	// Mirror to actor-side store if alive (for UI)
	UArcItemsStoreComponent* MirrorStore = GetMirrorInputStore(Station);
	if (MirrorStore)
	{
		// Re-sync the entire mirror store from the fragment
		// Remove all items from the store and re-add from fragment
		const TArray<const FArcItemData*> StoreItems = MirrorStore->GetItems();
		for (const FArcItemData* ItemData : StoreItems)
		{
			if (ItemData)
			{
				MirrorStore->RemoveItem(ItemData->GetItemId(), ItemData->GetStacks(), true);
			}
		}
		for (const FArcItemSpec& Spec : InputFrag->InputItems)
		{
			MirrorStore->AddItem(Spec, FArcItemId::InvalidId);
		}
	}

	return true;
}

bool FArcCraftItemSource_EntityStore::DepositItem(
	UArcCraftStationComponent* Station,
	const FArcItemSpec& Item,
	const UObject* Instigator)
{
	FArcCraftInputFragment* InputFrag = GetInputFragment(Station);
	if (!InputFrag)
	{
		return false;
	}

	// Add to entity fragment (source of truth)
	InputFrag->InputItems.Add(Item);

	// Mirror to actor-side store if alive (for UI)
	UArcItemsStoreComponent* MirrorStore = GetMirrorInputStore(Station);
	if (MirrorStore)
	{
		MirrorStore->AddItem(Item, FArcItemId::InvalidId);
	}

	return true;
}

bool FArcCraftItemSource_EntityStore::WithdrawItem(
	UArcCraftStationComponent* Station,
	int32 ItemIndex,
	int32 Stacks,
	FArcItemSpec& OutSpec,
	const UObject* Instigator)
{
	FArcCraftInputFragment* InputFrag = GetInputFragment(Station);
	if (!InputFrag)
	{
		return false;
	}

	if (!InputFrag->InputItems.IsValidIndex(ItemIndex))
	{
		return false;
	}

	FArcItemSpec& SourceSpec = InputFrag->InputItems[ItemIndex];
	const int32 AvailableStacks = static_cast<int32>(SourceSpec.Amount);
	const int32 StacksToWithdraw = (Stacks <= 0 || Stacks >= AvailableStacks) ? AvailableStacks : Stacks;

	// Build the output spec
	OutSpec = SourceSpec;
	OutSpec.Amount = static_cast<uint16>(StacksToWithdraw);

	// Remove from entity fragment (source of truth)
	if (StacksToWithdraw >= AvailableStacks)
	{
		InputFrag->InputItems.RemoveAt(ItemIndex);
	}
	else
	{
		SourceSpec.Amount = static_cast<uint16>(AvailableStacks - StacksToWithdraw);
	}

	// Re-sync mirror store from fragment
	UArcItemsStoreComponent* MirrorStore = GetMirrorInputStore(Station);
	if (MirrorStore)
	{
		const TArray<const FArcItemData*> StoreItems = MirrorStore->GetItems();
		for (const FArcItemData* ItemData : StoreItems)
		{
			if (ItemData)
			{
				MirrorStore->RemoveItem(ItemData->GetItemId(), ItemData->GetStacks(), true);
			}
		}
		for (const FArcItemSpec& Spec : InputFrag->InputItems)
		{
			MirrorStore->AddItem(Spec, FArcItemId::InvalidId);
		}
	}

	return true;
}
