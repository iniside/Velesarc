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

#pragma once

#include "CoreMinimal.h"
#include "Items/ArcItemId.h"
#include "Items/ArcItemSpec.h"
#include "Mass/EntityHandle.h"

#include "ArcCraftItemSource.generated.h"

struct FArcRecipeDefinitionData;
struct FArcCraftInputFragment;
struct FMassEntityManager;
class UScriptStruct;

/**
 * Base instanced struct for ingredient sourcing.
 * Defines how a crafting station obtains items for recipes.
 *
 * Override this to create custom item sources (e.g. searching nearby containers,
 * pulling from a shared storage network, etc.).
 */
USTRUCT(BlueprintType, meta = (Tooltip = "Base instanced struct for ingredient sourcing. Defines how a crafting station obtains items for recipes."))
struct ARCCRAFT_API FArcCraftItemSource
{
	GENERATED_BODY()

public:
	virtual bool AcceptsDeposits() const
	{
		return false;
	}

	/** Moves Stacks of ItemId from the instigator's StoreType store into the station input, synchronously. Moving every stack keeps ItemId; moving part of the stack splits it and the moved spec gets a new ItemId. */
	virtual bool DepositItem_Mass(
		FMassEntityManager& EntityManager,
		FMassEntityHandle StationEntity,
		FMassEntityHandle InstigatorEntity,
		const UScriptStruct* StoreType,
		FArcItemId ItemId,
		int32 Stacks) const
	{
		return false;
	}

	/**
	 * Mass-side: move an item from this source's station input back to the instigator's store.
	 * Pushes a deferred command via EntityManager.Defer(); callable outside a processor.
	 * Withdrawing part of the spec splits it: the withdrawn part gets a new ItemId.
	 * Default: false (source has no station-side input to withdraw from).
	 */
	virtual bool WithdrawItem_Mass(
		FMassEntityManager& EntityManager,
		FMassEntityHandle StationEntity,
		FMassEntityHandle InstigatorEntity,
		FArcItemId ItemId,
		int32 Stacks) const
	{
		return false;
	}

	virtual ~FArcCraftItemSource() = default;

public:
	/** Match and optionally consume recipe ingredients from a spec array.
	 *  When bConsume is true, matched items are removed from the Items array.
	 *  Required slots are matched first, then optional slots, each by first match from the specs left.
	 *  A slot draws its Amount from one or more specs of one item definition and equal instance tags
	 *  (rarities may differ); the first such group whose specs together cover Amount wins. An unmatched required
	 *  slot fails the call; an unmatched optional slot stays empty. Outputs are in slot order:
	 *  OutMatchedItems the first drawn spec (default spec when empty), OutSlotRaritySteps the
	 *  amount-weighted step of the drawn specs (1 when empty), OutSlotFilled whether the slot drew. */
	static bool MatchAndConsumeFromSpecs(
		TArray<FArcItemSpec>& Items,
		const FArcRecipeDefinitionData& Recipe,
		bool bConsume,
		TArray<FArcItemSpec>* OutMatchedItems = nullptr,
		TArray<float>* OutSlotRaritySteps = nullptr,
		TArray<bool>* OutSlotFilled = nullptr);

};

/**
 * Item source that reads/writes directly to the entity's FArcCraftInputFragment.
 * The source of truth is the Mass entity fragment — the actor-side mirror retired
 * with UArcItemsStoreComponent (plan step 8b-craft).
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Entity Storage", Tooltip = "Item source that reads and writes directly to the entity's FArcCraftInputFragment."))
struct ARCCRAFT_API FArcCraftItemSource_EntityStore : public FArcCraftItemSource
{
	GENERATED_BODY()

public:
	virtual bool AcceptsDeposits() const override
	{
		return true;
	}

	virtual bool DepositItem_Mass(
		FMassEntityManager& EntityManager,
		FMassEntityHandle StationEntity,
		FMassEntityHandle InstigatorEntity,
		const UScriptStruct* StoreType,
		FArcItemId ItemId,
		int32 Stacks) const override;

	virtual bool WithdrawItem_Mass(
		FMassEntityManager& EntityManager,
		FMassEntityHandle StationEntity,
		FMassEntityHandle InstigatorEntity,
		FArcItemId ItemId,
		int32 Stacks) const override;

	virtual ~FArcCraftItemSource_EntityStore() override = default;
};

