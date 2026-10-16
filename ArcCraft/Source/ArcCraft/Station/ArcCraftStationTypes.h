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
#include "GameplayTagContainer.h"
#include "Items/ArcItemId.h"
#include "Items/ArcItemSpec.h"
#include "Mass/EntityHandle.h"
#include "UObject/PrimaryAssetId.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "UObject/PrimaryAssetId.h"

#include "ArcCraftStationTypes.generated.h"

/**
 * How the crafting station tracks time for craft completion.
 */
UENUM(BlueprintType)
enum class EArcCraftStationTimeMode : uint8
{
	/** Component ticks and auto-finishes when accumulated time reaches craft duration. */
	AutoTick,
	/** Stamps UTC time on start. On interaction, checks if enough time has passed. */
	InteractionCheck
};

/**
 * Lifecycle state of a craft queue entry.
 */
UENUM()
enum class EArcCraftQueueEntryState : uint8
{
	/** Queued but ingredients have not been consumed yet. */
	Pending,
	/** Ingredients consumed, craft timer is ticking. */
	Active
};

/**
 * A single entry in the crafting queue.
 */
USTRUCT(BlueprintType, meta = (Tooltip = "A single entry in the crafting queue."))
struct ARCCRAFT_API FArcCraftQueueEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** Unique identifier for this queue entry. */
	UPROPERTY(BlueprintReadOnly)
	FGuid EntryId;

	/** The recipe being crafted. */
	UPROPERTY(BlueprintReadOnly)
	FPrimaryAssetId RecipeId;

	/** Total number of items to produce. */
	UPROPERTY(BlueprintReadOnly)
	int32 Amount = 1;

	/** How many items have been completed so far. */
	UPROPERTY(BlueprintReadOnly)
	int32 CompletedAmount = 0;

	/** Priority — lower number = higher priority. */
	UPROPERTY(BlueprintReadOnly)
	int32 Priority = 0;

	/** Which FArcBuildingWorkforceFragment::Slots entry queued this, INDEX_NONE when nothing did. */
	UPROPERTY(BlueprintReadOnly)
	int32 WorkforceSlotIndex = INDEX_NONE;

	/** UTC unix timestamp when the current item started crafting. */
	UPROPERTY(BlueprintReadOnly)
	int64 StartTimestamp = 0;

	/** Accumulated tick time for AutoTick mode (seconds). */
	UPROPERTY(BlueprintReadOnly)
	float ElapsedTickTime = 0.0f;

	/** Per-slot rarity step captured at ingredient-consume time.
	 *  Parallel to the recipe Ingredients. A missing slot counts as 1. */
	UPROPERTY(BlueprintReadOnly)
	TArray<float> SlotRaritySteps;

	/** Item definition id of each consumed ingredient, parallel to SlotRaritySteps; invalid for an empty optional slot.
	 *  Populated at activation time. Consumed at output-build time by modifier sources that need
	 *  to inspect the actual consumed ingredients (e.g. FArcRecipeModifierSource_StatTransferTable). */
	UPROPERTY()
	TArray<FPrimaryAssetId> ConsumedIngredientDefs;

	/** Instance tags (FArcItemFragment_InstanceTags) of each consumed ingredient, parallel to ConsumedIngredientDefs; empty for an empty
	 *  optional slot. Captured at activation and restored on the rebuilt ingredient specs at output-build time. */
	UPROPERTY()
	TArray<FGameplayTagContainer> SlotInstanceTags;

	/** Average rarity step across the filled slots (weights = Ingredient->Amount), cached at consume time. */
	UPROPERTY(BlueprintReadOnly)
	float CachedAverageR = 1.0f;

	/** Time mode copied from the station at queue time. */
	UPROPERTY(BlueprintReadOnly)
	EArcCraftStationTimeMode TimeMode = EArcCraftStationTimeMode::AutoTick;

	/** Lifecycle state — Pending until CraftTick consumes ingredients. */
	UPROPERTY(BlueprintReadOnly)
	EArcCraftQueueEntryState State = EArcCraftQueueEntryState::Pending;

	/** True when a completed craft could not be delivered (e.g. bag full) and is being held.
	 *  While set, the station's active entry is paused — no accumulation, no completion. */
	UPROPERTY(BlueprintReadOnly)
	bool bOutputBlocked = false;

	/** The exact built output specs not yet delivered while delivery is blocked. Snapshotted on block so a
	 *  resumed craft delivers the same items (recipes may carry random output rolls). */
	UPROPERTY(BlueprintReadOnly)
	TArray<FArcItemSpec> HeldOutput;

	/** Mass entity that requested this queue entry. Used by the tick processor to route
	 *  ingredient consumption and output delivery to the correct entity. */
	UPROPERTY()
	FMassEntityHandle InstigatorEntity;
};

