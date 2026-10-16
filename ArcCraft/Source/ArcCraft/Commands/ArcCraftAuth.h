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
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Mass/ArcMassAvailableRecipesSharedFragment.h"
#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"

/**
 * Shared authorisation gate for crafting queue commands: station-tag gate, tier/pinned gate
 * and queue-capacity gate.
 *
 * Usage (inside Execute, after resolving EM and the recipe record):
 *   if (!ArcCraftAuth::CanQueue(EM, StationEntity, RecipeId, *Recipe, nullptr)) return false;
 */
namespace ArcCraftAuth
{
	/**
	 * Checks 1-3 of CanQueue:
	 *   1. Station entity is valid and carries FArcCraftStationConfigFragment.
	 *   2. RequiredStationTags gate  (StationConfig.StationTags ⊇ Recipe.RequiredStationTags).
	 *   3. Pinned/tier gate          (recipe pinned OR StationConfig.OfferedRecipeTags ∩ Recipe.RecipeTags).
	 *
	 * Does NOT check queue capacity — that is left to CanQueue.
	 */
	inline bool CanUseRecipeAtStation(
		FMassEntityManager& EM,
		FMassEntityHandle StationEntity,
		const FPrimaryAssetId& RecipeId,
		const FArcRecipeDefinitionData& Recipe,
		FText* OutReason)
	{
		if (!EM.IsEntityValid(StationEntity))
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("ArcCraftAuth", "InvalidStation", "Station entity is not valid.");
			}
			return false;
		}

		const FMassEntityView StationView(EM, StationEntity);

		const FArcCraftStationConfigFragment* StationConfig =
			StationView.GetConstSharedFragmentDataPtr<FArcCraftStationConfigFragment>();
		if (StationConfig == nullptr)
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("ArcCraftAuth", "NoConfig", "Station entity lacks FArcCraftStationConfigFragment.");
			}
			return false;
		}

		// -----------------------------------------------------------------------
		// 1. Type gate: station must carry all tags the recipe requires.
		// -----------------------------------------------------------------------
		const bool bTypeGate = StationConfig->StationTags.HasAll(Recipe.RequiredStationTags);

		// -----------------------------------------------------------------------
		// 2. Pinned / tier gate.
		//    Pinned recipes bypass the tier selector; dynamic recipes require their
		//    RecipeTags to overlap the station's OfferedRecipeTags.
		// -----------------------------------------------------------------------
		bool bPinned = false;
		const FArcMassAvailableRecipesSharedFragment* RecipesFrag =
			StationView.GetConstSharedFragmentDataPtr<FArcMassAvailableRecipesSharedFragment>();
		if (RecipesFrag != nullptr)
		{
			bPinned = RecipesFrag->Recipes.Contains(RecipeId);
		}

		const bool bTierGate = !Recipe.RecipeTags.IsEmpty()
			&& StationConfig->OfferedRecipeTags.HasAny(Recipe.RecipeTags);

		if (!bTypeGate || !(bPinned || bTierGate))
		{
			if (OutReason)
			{
				*OutReason = FText::Format(
					NSLOCTEXT("ArcCraftAuth", "AuthFailed",
						"Recipe '{0}' is not authorised at this station (pinned={1}, typeGate={2}, tierGate={3})."),
					Recipe.GetDisplayName(),
					FText::AsNumber(bPinned ? 1 : 0),
					FText::AsNumber(bTypeGate ? 1 : 0),
					FText::AsNumber(bTierGate ? 1 : 0));
			}
			return false;
		}

		return true;
	}

	/**
	 * Returns true if the station entity is authorised to queue the given recipe and has
	 * room in its queue. Writes a human-readable reason into OutReason when provided and
	 * the check fails.
	 *
	 * Checks (in order):
	 *   1. Station entity is valid and carries FArcCraftStationConfigFragment.
	 *   2. RequiredStationTags gate  (StationConfig.StationTags ⊇ Recipe.RequiredStationTags).
	 *   3. Pinned/tier gate          (recipe pinned OR StationConfig.OfferedRecipeTags ∩ Recipe.RecipeTags).
	 *   4. Queue-capacity gate       (0 = unlimited).
	 *
	 * Note: ingredient availability is NOT checked here — that is the caller's responsibility.
	 */
	inline bool CanQueue(
		FMassEntityManager& EM,
		FMassEntityHandle StationEntity,
		const FPrimaryAssetId& RecipeId,
		const FArcRecipeDefinitionData& Recipe,
		FText* OutReason)
	{
		if (!CanUseRecipeAtStation(EM, StationEntity, RecipeId, Recipe, OutReason))
		{
			return false;
		}

		// -----------------------------------------------------------------------
		// 4. Queue-capacity gate (0 = unlimited).
		// -----------------------------------------------------------------------
		const FMassEntityView StationView(EM, StationEntity);
		const FArcCraftStationConfigFragment* StationConfig =
			StationView.GetConstSharedFragmentDataPtr<FArcCraftStationConfigFragment>();

		const int32 QueueCapacity = StationConfig->GetQueueCapacity();

		const FArcCraftQueueFragment* QueueFrag =
			StationView.GetFragmentDataPtr<FArcCraftQueueFragment>();
		if (QueueFrag != nullptr
			&& QueueCapacity > 0
			&& QueueFrag->Entries.Num() >= QueueCapacity)
		{
			if (OutReason)
			{
				*OutReason = FText::Format(
					NSLOCTEXT("ArcCraftAuth", "QueueFull", "Craft queue is full ({0}/{1})."),
					FText::AsNumber(QueueFrag->Entries.Num()),
					FText::AsNumber(QueueCapacity));
			}
			return false;
		}

		return true;
	}
} // namespace ArcCraftAuth
