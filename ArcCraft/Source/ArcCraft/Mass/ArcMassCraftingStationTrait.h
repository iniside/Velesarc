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

#include "ArcNamedPrimaryAssetId.h"
#include "GameplayTagContainer.h"
#include "MassEntityTraitBase.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"

#include "ArcMassCraftingStationTrait.generated.h"

/**
 * Configures a Mass entity as a crafting station. Add to a MassEntityConfig asset to make
 * the entity craft recipes — workbenches, NPC production stations, or the player's own
 * entity (hand-crafting).
 *
 * BuildTemplate seeds everything UArcCraftTickProcessor and the craft commands need:
 *   - FArcCraftQueueFragment / FArcCraftInputFragment / FArcCraftOutputFragment
 *     (craft queue, ingredient store, finished-item store)
 *   - FArcCraftStationTag, plus FArcCraftAutoTickTag when bAutoTick is set
 *     (the tick processor only iterates auto-tick stations)
 *   - FArcCraftStationConfigFragment   (const-shared: station/recipe tags, time mode,
 *     tick interval, queue size)
 *   - FArcMassAvailableRecipesSharedFragment  (const-shared: explicit recipe list for the
 *     UI recipe panel and server-side command validation)
 *   - FArcCraftDeliveryConfigFragment  (const-shared: output delivery + ingredient source
 *     policies; unset policies fall back to direct fragment writes)
 *
 * Recipes are offered through two axes: the explicit AvailableRecipes list, and tag
 * discovery (recipes whose RecipeTags match OfferedRecipeTags and whose
 * RequiredStationTags are a subset of StationTags).
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Mass Crafting Station"))
class ARCCRAFT_API UArcMassCraftingStationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	/** Recipes available on this crafting station (for UI display and server-side validation). */
	UPROPERTY(EditAnywhere, Category = "Crafting", meta = (AllowedTypes = "ArcRecipe"))
	TArray<FArcNamedPrimaryAssetId> AvailableRecipes;

	/** Tags that identify this station *type* (e.g. Station.Forge, Station.Hand).
	 *  Recipe RequiredStationTags must be a subset of these. */
	UPROPERTY(EditAnywhere, Category = "Crafting")
	FGameplayTagContainer StationTags;

	/** Recipe tier/category tags this station offers (matched against recipe RecipeTags;
	 *  empty ⇒ this station offers no recipes via tag discovery). */
	UPROPERTY(EditAnywhere, Category = "Crafting")
	FGameplayTagContainer OfferedRecipeTags;

	/** How crafted items are delivered (e.g. entity store, player item store). */
	UPROPERTY(EditAnywhere, Category = "Crafting",
		meta = (BaseStruct = "/Script/ArcCraft.ArcCraftOutputDelivery"))
	FInstancedStruct OutputDelivery;

	/** Where ingredients are sourced from (e.g. instigator item store, entity store). */
	UPROPERTY(EditAnywhere, Category = "Crafting",
		meta = (BaseStruct = "/Script/ArcCraft.ArcCraftItemSource"))
	FInstancedStruct IngredientSource;

	/** How time is tracked for craft completion. */
	UPROPERTY(EditAnywhere, Category = "Crafting")
	EArcCraftStationTimeMode TimeMode = EArcCraftStationTimeMode::AutoTick;

	/** Tick interval for AutoTick mode (seconds). */
	UPROPERTY(EditAnywhere, Category = "Crafting", meta = (ClampMin = "0.1"))
	float TickInterval = 0.5f;

	/** Queue entries that craft simultaneously on this station. */
	UPROPERTY(EditAnywhere, Category = "Crafting", meta = (ClampMin = "1"))
	int32 ConcurrentCraftSlots = 1;

	/** Queue length per slot. 0 = unlimited. Station capacity is the product of both. */
	UPROPERTY(EditAnywhere, Category = "Crafting", meta = (ClampMin = "0"))
	int32 QueueLengthPerSlot = 8;

	/** If true, adds FArcCraftAutoTickTag so the craft tick processor picks up this entity. */
	UPROPERTY(EditAnywhere, Category = "Crafting")
	bool bAutoTick = true;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
