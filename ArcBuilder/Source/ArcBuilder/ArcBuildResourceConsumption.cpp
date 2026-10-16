// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBuildResourceConsumption.h"

#include "Mass/EntityManager.h"

#include "ArcBuildIngredient.h"
#include "ArcBuildingDefinition.h"
#include "Items/ArcItemData.h"
#include "Operations/ArcMassItemOperations.h"

// -------------------------------------------------------------------
// FArcBuildResourceConsumption (base) — free placement
// -------------------------------------------------------------------

bool FArcBuildResourceConsumption::CheckResources(
	const FMassEntityManager& EntityManager,
	FMassEntityHandle InstigatorEntity,
	const UArcBuildingDefinition* BuildingDef,
	TArray<FArcItemSpec>* OutMatched) const
{
	if (OutMatched != nullptr)
	{
		OutMatched->Reset();
	}
	return true;
}

bool FArcBuildResourceConsumption::ConsumeResources(
	FMassEntityManager& EntityManager,
	FMassEntityHandle InstigatorEntity,
	const UArcBuildingDefinition* BuildingDef,
	TArray<FArcItemSpec>& OutConsumed) const
{
	OutConsumed.Reset();
	return true;
}

// -------------------------------------------------------------------
// FArcBuildResourceConsumption_MassStore
// -------------------------------------------------------------------

namespace ArcBuildResourceConsumptionMassStoreLocal
{
	/** One planned removal: which item, how many stacks, and a spec snapshot of exactly that take. */
	struct FMatchedStack
	{
		FArcItemId ItemId;
		int32 StacksTaken = 0;
		FArcItemSpec SpecSnapshot;
	};

	const UScriptStruct* ResolveStoreType(const UScriptStruct* Picked)
	{
		const UScriptStruct* Resolved = Picked != nullptr ? Picked : FArcMassItemStoreFragment::StaticStruct();
		if (!Resolved->IsChildOf(FArcMassItemStoreFragment::StaticStruct()))
		{
			return nullptr;
		}
		return Resolved;
	}

	const FArcMassItemStoreFragment* GetStoreFragment(
		const FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType)
	{
		if (StoreType == nullptr || !EntityManager.IsEntityValid(Entity))
		{
			return nullptr;
		}
		const FConstStructView StoreView = EntityManager.GetSparseElementDataForEntity(StoreType, Entity);
		return StoreView.GetPtr<FArcMassItemStoreFragment>();
	}

	/**
	 * Greedy match of every ingredient slot against the store contents.
	 * One slot may be filled from multiple item stacks; per-item remaining stacks are
	 * tracked across slots so nothing is double-counted. Read-only — collects the
	 * (ItemId, StacksTaken) plan without mutating the store (two-pass consume).
	 */
	bool MatchIngredients(
		const FArcMassItemStoreFragment& Store,
		const UArcBuildingDefinition* BuildingDef,
		TArray<FMatchedStack>& OutMatches)
	{
		OutMatches.Reset();

		struct FAvailableStack
		{
			const FArcItemData* Item = nullptr;
			int32 Remaining = 0;
		};

		TArray<FAvailableStack> Available;
		Available.Reserve(Store.ReplicatedItems.Items.Num());
		for (const FArcMassReplicatedItem& ReplicatedItem : Store.ReplicatedItems.Items)
		{
			const FArcItemData* Item = ReplicatedItem.ToItem();
			if (Item != nullptr && Item->GetItemId().IsValid() && Item->Stacks > 0)
			{
				FAvailableStack& Entry = Available.AddDefaulted_GetRef();
				Entry.Item = Item;
				Entry.Remaining = static_cast<int32>(Item->Stacks);
			}
		}

		for (const FInstancedStruct& IngredientStruct : BuildingDef->Ingredients)
		{
			const FArcBuildIngredient* Ingredient = IngredientStruct.GetPtr<FArcBuildIngredient>();
			if (Ingredient == nullptr)
			{
				return false;
			}

			int32 Needed = Ingredient->Amount;
			for (FAvailableStack& Candidate : Available)
			{
				if (Needed <= 0)
				{
					break;
				}
				if (Candidate.Remaining <= 0 || !Ingredient->DoesItemSatisfy(Candidate.Item))
				{
					continue;
				}

				const int32 Taken = FMath::Min(Needed, Candidate.Remaining);
				Candidate.Remaining -= Taken;
				Needed -= Taken;

				FMatchedStack& Match = OutMatches.AddDefaulted_GetRef();
				Match.ItemId = Candidate.Item->GetItemId();
				Match.StacksTaken = Taken;
				Match.SpecSnapshot = Candidate.Item->Spec;
				Match.SpecSnapshot.Amount = static_cast<uint16>(Taken);
				Match.SpecSnapshot.SetItemId(Match.ItemId);
			}

			if (Needed > 0)
			{
				return false;
			}
		}

		return true;
	}
} // namespace ArcBuildResourceConsumptionMassStoreLocal

bool FArcBuildResourceConsumption_MassStore::CheckResources(
	const FMassEntityManager& EntityManager,
	FMassEntityHandle InstigatorEntity,
	const UArcBuildingDefinition* BuildingDef,
	TArray<FArcItemSpec>* OutMatched) const
{
	using namespace ArcBuildResourceConsumptionMassStoreLocal;

	if (OutMatched != nullptr)
	{
		OutMatched->Reset();
	}
	if (BuildingDef == nullptr)
	{
		return false;
	}

	// No ingredients — free placement.
	if (BuildingDef->Ingredients.IsEmpty())
	{
		return true;
	}

	const FArcMassItemStoreFragment* Store =
		GetStoreFragment(EntityManager, InstigatorEntity, ResolveStoreType(StoreType));
	if (Store == nullptr)
	{
		return false;
	}

	TArray<FMatchedStack> Matches;
	if (!MatchIngredients(*Store, BuildingDef, Matches))
	{
		return false;
	}

	if (OutMatched != nullptr)
	{
		OutMatched->Reserve(Matches.Num());
		for (const FMatchedStack& Match : Matches)
		{
			OutMatched->Add(Match.SpecSnapshot);
		}
	}
	return true;
}

bool FArcBuildResourceConsumption_MassStore::ConsumeResources(
	FMassEntityManager& EntityManager,
	FMassEntityHandle InstigatorEntity,
	const UArcBuildingDefinition* BuildingDef,
	TArray<FArcItemSpec>& OutConsumed) const
{
	using namespace ArcBuildResourceConsumptionMassStoreLocal;

	OutConsumed.Reset();
	if (BuildingDef == nullptr)
	{
		return false;
	}

	// No ingredients — free placement, nothing to consume.
	if (BuildingDef->Ingredients.IsEmpty())
	{
		return true;
	}

	const UScriptStruct* ResolvedStoreType = ResolveStoreType(StoreType);
	const FArcMassItemStoreFragment* Store =
		GetStoreFragment(EntityManager, InstigatorEntity, ResolvedStoreType);
	if (Store == nullptr)
	{
		return false;
	}

	// Re-run the match against the live store — never trust a stale check result.
	TArray<FMatchedStack> Matches;
	if (!MatchIngredients(*Store, BuildingDef, Matches))
	{
		return false;
	}

	// Two-pass: the plan above collected (ItemId, StacksTaken) without mutation;
	// now remove through the standard ops rail. The match just verified availability,
	// so a failing removal is unexpected — report failure but keep OutConsumed
	// accurate to what actually left the store.
	bool bAllRemoved = true;
	for (const FMatchedStack& Match : Matches)
	{
		const bool bRemoved = ArcMassItems::RemoveItem(
			EntityManager, InstigatorEntity, ResolvedStoreType, Match.ItemId, Match.StacksTaken);
		if (!bRemoved)
		{
			bAllRemoved = false;
			continue;
		}
		OutConsumed.Add(Match.SpecSnapshot);
	}

	return bAllRemoved;
}
