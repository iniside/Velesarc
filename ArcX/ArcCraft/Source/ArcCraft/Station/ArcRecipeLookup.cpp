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

#include "ArcCraft/Station/ArcRecipeLookup.h"

#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "Engine/AssetManager.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"

TArray<FAssetData> FArcRecipeLookup::FindRecipesForStation(
	const FGameplayTagContainer& StationTags)
{
	IAssetRegistry& AR = UAssetManager::Get().GetAssetRegistry();

	// Build filter for all UArcRecipeDefinition assets (and derived classes)
	FARFilter AssetFilter;
	AssetFilter.ClassPaths.Add(UArcRecipeDefinition::StaticClass()->GetClassPathName());

	TSet<FTopLevelAssetPath> DerivedClassNames;
	AR.GetDerivedClassNames(AssetFilter.ClassPaths, TSet<FTopLevelAssetPath>(), DerivedClassNames);
	AssetFilter.ClassPaths.Append(DerivedClassNames.Array());
	AssetFilter.bRecursiveClasses = true;

	TArray<FAssetData> AllRecipes;
	AR.GetAssets(AssetFilter, AllRecipes);

	// Filter by station tags using asset registry data
	return UArcRecipeDefinition::FilterByStationTags(AllRecipes, StationTags);
}

TArray<FAssetData> FArcRecipeLookup::FilterByAvailableIngredients(
	const TArray<FAssetData>& CandidateRecipes,
	const FGameplayTagContainer& AvailableItemTags)
{
	TArray<FAssetData> Filtered;

	for (const FAssetData& Data : CandidateRecipes)
	{
		// Get ingredient tags from asset registry
		FGameplayTagContainer IngredientTags = UArcRecipeDefinition::GetIngredientTagsFromAssetData(Data);

		// If recipe has no ingredient tags (e.g. all item-def based), include it
		if (IngredientTags.Num() == 0)
		{
			Filtered.Add(Data);
			continue;
		}

		// Check if available items have any overlap with what the recipe needs
		if (AvailableItemTags.HasAny(IngredientTags))
		{
			Filtered.Add(Data);
		}
	}

	return Filtered;
}

UArcRecipeDefinition* FArcRecipeLookup::FindBestRecipeForIngredients(
	const TArray<FAssetData>& CandidateRecipes,
	const TArray<const FArcItemData*>& AvailableItems,
	const FGameplayTagContainer& StationTags,
	float& OutMatchScore)
{
	OutMatchScore = 0.0f;
	UArcRecipeDefinition* BestRecipe = nullptr;

	for (const FAssetData& Data : CandidateRecipes)
	{
		// Load the recipe asset
		UArcRecipeDefinition* Recipe = Cast<UArcRecipeDefinition>(Data.GetAsset());
		if (!Recipe)
		{
			continue;
		}

		// Validate station tags again (in case of race condition)
		if (Recipe->RequiredStationTags.Num() > 0 && !StationTags.HasAll(Recipe->RequiredStationTags))
		{
			continue;
		}

		// Try to match all ingredient slots against available items
		TSet<int32> UsedItemIndices;
		int32 MatchedSlots = 0;
		const int32 TotalSlots = Recipe->Ingredients.Num();

		if (TotalSlots == 0)
		{
			continue;
		}

		for (int32 SlotIdx = 0; SlotIdx < TotalSlots; ++SlotIdx)
		{
			const FArcRecipeIngredient* Ingredient = Recipe->GetIngredientBase(SlotIdx);
			if (!Ingredient)
			{
				continue;
			}

			for (int32 ItemIdx = 0; ItemIdx < AvailableItems.Num(); ++ItemIdx)
			{
				if (UsedItemIndices.Contains(ItemIdx))
				{
					continue;
				}

				const FArcItemData* ItemData = AvailableItems[ItemIdx];
				if (!ItemData)
				{
					continue;
				}

				// We pass nullptr for tier table since we don't have it without loading more.
				// This is a best-effort match.
				if (Ingredient->DoesItemSatisfy(ItemData, nullptr) &&
					ItemData->GetStacks() >= static_cast<uint16>(Ingredient->Amount))
				{
					UsedItemIndices.Add(ItemIdx);
					MatchedSlots++;
					break;
				}
			}
		}

		// Only consider recipes where ALL slots are satisfied
		if (MatchedSlots < TotalSlots)
		{
			continue;
		}

		// Score: more ingredient slots = more specific recipe = better match.
		// Tie-break by closeness of available items count to required count.
		const float SlotScore = static_cast<float>(TotalSlots);
		const float Closeness = 1.0f / (1.0f + FMath::Abs(static_cast<float>(AvailableItems.Num() - TotalSlots)));
		const float Score = SlotScore + Closeness;

		if (Score > OutMatchScore)
		{
			OutMatchScore = Score;
			BestRecipe = Recipe;
		}
	}

	return BestRecipe;
}
