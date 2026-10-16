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

#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"
#include "Core/ArcCoreAssetManager.h"
#include "Items/ArcItemSpec.h"
#include "JsonAssets/ArcJsonAssetType.h"

namespace ArcRecipeLookupPrivate
{
	const TCHAR* RecipeExtension = TEXT("arcrecipe");

	FGameplayTagContainer GetIngredientTags(const FArcRecipeDefinitionData& Recipe)
	{
		FGameplayTagContainer IngredientTags;
		for (const FInstancedStruct& IngredientStruct : Recipe.Ingredients)
		{
			const FArcRecipeIngredient* Ingredient = IngredientStruct.GetPtr<FArcRecipeIngredient>();
			if (Ingredient != nullptr && Ingredient->bOptional)
			{
				continue;
			}

			if (const FArcRecipeIngredient_Tags* TagIngredient = IngredientStruct.GetPtr<FArcRecipeIngredient_Tags>())
			{
				IngredientTags.AppendTags(TagIngredient->RequiredTags);
			}
		}
		return IngredientTags;
	}

	TArray<FPrimaryAssetId> GetRecipeIds()
	{
		UArcCoreAssetManager& AssetManager = UArcCoreAssetManager::Get();

		TArray<FPrimaryAssetId> RecipeIds;
		for (const FPrimaryAssetType& Type : AssetManager.GetJsonRecordTypes())
		{
			const FArcJsonAssetTypeInfo* TypeInfo = AssetManager.FindJsonRecordType(Type);
			if (TypeInfo != nullptr && TypeInfo->Extension == RecipeExtension)
			{
				AssetManager.GetPrimaryAssetIdList(Type, RecipeIds);
			}
		}
		return RecipeIds;
	}
}

TArray<FPrimaryAssetId> FArcRecipeLookup::FindRecipesForStation(
	const FGameplayTagContainer& StationTypeTags,
	const FGameplayTagContainer& OfferedRecipeTags)
{
	const TArray<FPrimaryAssetId> RecipeIds = ArcRecipeLookupPrivate::GetRecipeIds();

	TArray<FPrimaryAssetId> Filtered;
	Filtered.Reserve(RecipeIds.Num());
	for (const FPrimaryAssetId& RecipeId : RecipeIds)
	{
		const FArcRecipeDefinitionData* Recipe = ArcCraftRecords::Find<FArcRecipeDefinitionData>(RecipeId);
		if (Recipe == nullptr)
		{
			continue;
		}

		const bool bTypeOk = Recipe->RequiredStationTags.IsEmpty() || StationTypeTags.HasAll(Recipe->RequiredStationTags);
		if (!bTypeOk)
		{
			continue;
		}

		// HasAny expands the RECEIVER's tags (adds parents). Receiver = RecipeTags so a station
		// offering a PARENT tag (Offered={Recipe.Tier}) matches a recipe tagged with a child
		// (RecipeTags={Recipe.Tier.0}). The reverse direction would silently fail parent offers.
		const bool bTierOk = !Recipe->RecipeTags.IsEmpty() && Recipe->RecipeTags.HasAny(OfferedRecipeTags);
		if (bTierOk)
		{
			Filtered.Add(RecipeId);
		}
	}
	return Filtered;
}

TArray<FPrimaryAssetId> FArcRecipeLookup::FilterByAvailableIngredients(
	const TArray<FPrimaryAssetId>& CandidateRecipes,
	const FGameplayTagContainer& AvailableItemTags)
{
	TArray<FPrimaryAssetId> Filtered;

	for (const FPrimaryAssetId& RecipeId : CandidateRecipes)
	{
		const FArcRecipeDefinitionData* Recipe = ArcCraftRecords::Find<FArcRecipeDefinitionData>(RecipeId);
		if (Recipe == nullptr)
		{
			continue;
		}

		const FGameplayTagContainer IngredientTags = ArcRecipeLookupPrivate::GetIngredientTags(*Recipe);

		// If recipe has no ingredient tags (e.g. all item-def based), include it
		if (IngredientTags.Num() == 0)
		{
			Filtered.Add(RecipeId);
			continue;
		}

		// Check if available items have any overlap with what the recipe needs
		if (AvailableItemTags.HasAny(IngredientTags))
		{
			Filtered.Add(RecipeId);
		}
	}

	return Filtered;
}

FPrimaryAssetId FArcRecipeLookup::FindBestRecipeForIngredients(
	const TArray<FPrimaryAssetId>& CandidateRecipes,
	const TArray<FArcItemSpec>& AvailableItems,
	const FGameplayTagContainer& StationTags,
	float& OutMatchScore)
{
	OutMatchScore = 0.0f;
	FPrimaryAssetId BestRecipe;

	for (const FPrimaryAssetId& RecipeId : CandidateRecipes)
	{
		const FArcRecipeDefinitionData* Recipe = ArcCraftRecords::Find<FArcRecipeDefinitionData>(RecipeId);
		if (Recipe == nullptr)
		{
			continue;
		}

		if (Recipe->RequiredStationTags.Num() > 0 && !StationTags.HasAll(Recipe->RequiredStationTags))
		{
			continue;
		}

		// Try to match all required ingredient slots against available items
		TSet<int32> UsedItemIndices;
		int32 MatchedSlots = 0;
		const int32 TotalSlots = Recipe->GetIngredientCount();
		const int32 RequiredSlots = Recipe->GetRequiredIngredientCount();

		if (RequiredSlots == 0)
		{
			continue;
		}

		for (int32 SlotIdx = 0; SlotIdx < TotalSlots; ++SlotIdx)
		{
			const FArcRecipeIngredient* Ingredient = Recipe->GetIngredientBase(SlotIdx);
			if (!Ingredient || Ingredient->bOptional)
			{
				continue;
			}

			for (int32 ItemIdx = 0; ItemIdx < AvailableItems.Num(); ++ItemIdx)
			{
				if (UsedItemIndices.Contains(ItemIdx))
				{
					continue;
				}

				const FArcItemSpec& ItemSpec = AvailableItems[ItemIdx];
				if (!ItemSpec.GetItemDefinitionId().IsValid())
				{
					continue;
				}

				if (Ingredient->DoesItemSatisfy(ItemSpec) &&
					ItemSpec.Amount >= static_cast<uint16>(Ingredient->Amount))
				{
					UsedItemIndices.Add(ItemIdx);
					MatchedSlots++;
					break;
				}
			}
		}

		// Only consider recipes where all required slots are satisfied
		if (MatchedSlots < RequiredSlots)
		{
			continue;
		}

		// Score: more required slots = more specific recipe = better match.
		// Tie-break by closeness of available items count to required count.
		const float SlotScore = static_cast<float>(RequiredSlots);
		const float Closeness = 1.0f / (1.0f + FMath::Abs(static_cast<float>(AvailableItems.Num() - RequiredSlots)));
		const float Score = SlotScore + Closeness;

		if (Score > OutMatchScore)
		{
			OutMatchScore = Score;
			BestRecipe = RecipeId;
		}
	}

	return BestRecipe;
}

FPrimaryAssetId FArcRecipeLookup::FindRarityVariant(
	const FPrimaryAssetId& BaseRecipeId,
	const FGameplayTag& Rarity,
	const FGameplayTag& DefaultRarity)
{
	if (!BaseRecipeId.IsValid() || !Rarity.IsValid())
	{
		return FPrimaryAssetId();
	}

	for (const FPrimaryAssetId& RecipeId : ArcRecipeLookupPrivate::GetRecipeIds())
	{
		const FArcRecipeDefinitionData* Recipe = ArcCraftRecords::Find<FArcRecipeDefinitionData>(RecipeId);
		if (Recipe == nullptr)
		{
			continue;
		}

		const bool bInFamily = RecipeId == BaseRecipeId || Recipe->BaseRecipe == BaseRecipeId;
		const FGameplayTag StartRarity = Recipe->GuaranteedRarity.IsValid() ? Recipe->GuaranteedRarity : DefaultRarity;
		if (bInFamily && StartRarity == Rarity)
		{
			return RecipeId;
		}
	}
	return FPrimaryAssetId();
}
