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
#include "AssetRegistry/AssetData.h"
#include "GameplayTagContainer.h"

class UArcRecipeDefinition;
struct FArcItemData;

/**
 * Stateless utility class for discovering recipes via the asset registry.
 *
 * Flow:
 *  1. FindRecipesForStation() — query asset registry for recipes matching station tags
 *  2. FilterByAvailableIngredients() — coarse filter using ingredient tags from registry
 *  3. FindBestRecipeForIngredients() — load candidates and score them
 */
class ARCCRAFT_API FArcRecipeLookup
{
public:
	/**
	 * Find all recipe asset data whose RequiredStationTags are satisfied by the station's tags.
	 * Uses asset registry only — does NOT load recipe assets.
	 */
	static TArray<FAssetData> FindRecipesForStation(
		const FGameplayTagContainer& StationTags);

	/**
	 * Coarse filter: keep only recipes whose ingredient tags have any overlap with available items' tags.
	 * Uses asset registry tags — does NOT load recipe assets.
	 *
	 * @param CandidateRecipes  Asset data from FindRecipesForStation().
	 * @param AvailableItemTags Union of all tags from available items.
	 * @return Filtered subset of candidates.
	 */
	static TArray<FAssetData> FilterByAvailableIngredients(
		const TArray<FAssetData>& CandidateRecipes,
		const FGameplayTagContainer& AvailableItemTags);

	/**
	 * Load candidate recipes and find the best match for the given ingredients.
	 * Scores by: number of satisfiable ingredient slots, closeness of ingredient count.
	 *
	 * @param CandidateRecipes  Pre-filtered asset data (ideally from FilterByAvailableIngredients).
	 * @param AvailableItems    All items available for crafting.
	 * @param StationTags       Station tags for validation.
	 * @param OutMatchScore     Score of the best match (0 = no match).
	 * @return Best matching recipe, or nullptr if none match.
	 */
	static UArcRecipeDefinition* FindBestRecipeForIngredients(
		const TArray<FAssetData>& CandidateRecipes,
		const TArray<const FArcItemData*>& AvailableItems,
		const FGameplayTagContainer& StationTags,
		float& OutMatchScore);
};
