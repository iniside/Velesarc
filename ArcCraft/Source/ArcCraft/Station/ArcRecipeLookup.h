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
#include "UObject/PrimaryAssetId.h"

struct FArcItemSpec;

/**
 * Stateless utility class for discovering recipes among the indexed .arcrecipe records
 * (every record type registered for that extension, including custom RecipeType ids).
 *
 * Flow:
 *  1. FindRecipesForStation() — recipes matching station tags
 *  2. FilterByAvailableIngredients() — coarse filter using ingredient tags
 *  3. FindBestRecipeForIngredients() — score candidates against the available items
 */
class ARCCRAFT_API FArcRecipeLookup
{
public:
	/**
	 * Find recipe ids offered by a station, by two axes:
	 *  - type gate: recipe RequiredStationTags ⊆ StationTypeTags
	 *  - tier selector: recipe RecipeTags overlap OfferedRecipeTags (empty RecipeTags ⇒ never offered)
	 */
	static TArray<FPrimaryAssetId> FindRecipesForStation(
		const FGameplayTagContainer& StationTypeTags,
		const FGameplayTagContainer& OfferedRecipeTags);

	/**
	 * Coarse filter: keep only recipes whose tag-based ingredients have any overlap with available items' tags.
	 *
	 * @param CandidateRecipes  Ids from FindRecipesForStation().
	 * @param AvailableItemTags Union of all tags from available items.
	 * @return Filtered subset of candidates.
	 */
	static TArray<FPrimaryAssetId> FilterByAvailableIngredients(
		const TArray<FPrimaryAssetId>& CandidateRecipes,
		const FGameplayTagContainer& AvailableItemTags);

	/**
	 * Find the best match for the given ingredients among the candidate recipes.
	 * Scores by: number of satisfiable ingredient slots, closeness of ingredient count.
	 *
	 * @param CandidateRecipes  Pre-filtered ids (ideally from FilterByAvailableIngredients).
	 * @param AvailableItems    All items available for crafting.
	 * @param StationTags       Station tags for validation.
	 * @param OutMatchScore     Score of the best match (0 = no match).
	 * @return Best matching recipe id, or an invalid id if none match.
	 */
	static FPrimaryAssetId FindBestRecipeForIngredients(
		const TArray<FPrimaryAssetId>& CandidateRecipes,
		const TArray<FArcItemSpec>& AvailableItems,
		const FGameplayTagContainer& StationTags,
		float& OutMatchScore);

	/**
	 * Scan the recipe records for the member of BaseRecipeId's family (the base itself or a recipe whose BaseRecipe
	 * is BaseRecipeId) whose start rarity (GuaranteedRarity, else DefaultRarity) is Rarity.
	 *
	 * @return The first matching recipe id, or an invalid id if none.
	 */
	static FPrimaryAssetId FindRarityVariant(
		const FPrimaryAssetId& BaseRecipeId,
		const FGameplayTag& Rarity,
		const FGameplayTag& DefaultRarity);
};
