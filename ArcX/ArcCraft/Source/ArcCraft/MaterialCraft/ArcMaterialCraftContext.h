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

#include "ArcMaterialCraftContext.generated.h"

struct FArcItemData;

/**
 * Evaluation context for the material crafting system.
 * Built from the consumed ingredients and recipe data,
 * then passed to FArcMaterialCraftEvaluator for rule evaluation.
 *
 * Tags are stored per ingredient slot (not merged), allowing the evaluator
 * to reason about which slot contributed which tags. Rule matching builds
 * a temporary union of per-slot tags for TagQuery evaluation.
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcMaterialCraftContext
{
	GENERATED_BODY()

public:
	/** Per-ingredient-slot tags. Parallel array to consumed ingredients.
	 *  Each entry contains the ItemTags from that slot's FArcItemFragment_Tags. */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	TArray<FGameplayTagContainer> PerSlotTags;

	/** Tags from the recipe definition. */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	FGameplayTagContainer RecipeTags;

	/** Per-ingredient quality multipliers (parallel to consumed ingredients). */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	TArray<float> IngredientQualityMultipliers;

	/** Weighted average quality across all ingredients. */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	float AverageQuality = 1.0f;

	/** Quality value used for band eligibility checks (MinQuality comparison).
	 *  Derived from weighted average ingredient quality.
	 *  Extra same-quality items do NOT boost this — they boost BandWeightBonus instead. */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	float BandEligibilityQuality = 1.0f;

	/** Bonus applied to band weight calculations from extra same-quality ingredients
	 *  and extra craft time. Does NOT affect band eligibility (MinQuality threshold).
	 *  Computed by FArcMaterialCraftEvaluator::ComputeQualityAndWeightBonus(). */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	float BandWeightBonus = 0.0f;

	/** Number of ingredient slots consumed. */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	int32 IngredientCount = 0;

	/** Base ingredient count expected by the recipe (for extra-material bonus calculation). */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	int32 BaseIngredientCount = 0;

	/** Designer-set quality bonus representing extra time spent crafting. */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	float ExtraCraftTimeBonus = 0.0f;

	/**
	 * Build a context from the consumed ingredients and recipe data.
	 * Uses FArcItemFragment_Tags::ItemTags for per-slot tag extraction (not GetItemAggregatedTags).
	 *
	 * @param ConsumedIngredients    Matched items per ingredient slot.
	 * @param QualityMultipliers     Quality multiplier per ingredient slot.
	 * @param InAverageQuality       Pre-computed weighted average quality.
	 * @param InRecipeTags           Tags from the recipe definition.
	 * @param InBaseIngredientCount  Expected ingredient count (for extra-material bonus).
	 * @param InExtraCraftTimeBonus  Designer-set extra-time quality bonus.
	 */
	static FArcMaterialCraftContext Build(
		const TArray<const FArcItemData*>& ConsumedIngredients,
		const TArray<float>& QualityMultipliers,
		float InAverageQuality,
		const FGameplayTagContainer& InRecipeTags,
		int32 InBaseIngredientCount = 0,
		float InExtraCraftTimeBonus = 0.0f);
};
