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
#include "Items/ArcItemSpec.h"

#include "ArcMaterialCraftContext.generated.h"

struct FArcItemData;

/**
 * Evaluation context for the material crafting system.
 * Built from the consumed ingredients and recipe data,
 * then passed to FArcMaterialCraftEvaluator for rule evaluation.
 *
 * Tags are stored per ingredient slot (not merged), allowing the evaluator
 * to reason about which slot contributed which tags. Rule matching evaluates
 * TagQuery on a temporary union of per-slot tags or on each slot alone (EArcMaterialRuleMatch).
 */
USTRUCT(BlueprintType, meta = (Tooltip = "Evaluation context for the material crafting system, built from consumed ingredients and recipe data, then passed to FArcMaterialCraftEvaluator for rule evaluation."))
struct ARCCRAFT_API FArcMaterialCraftContext
{
	GENERATED_BODY()

public:
	/** Per-ingredient-slot tags. Parallel array to consumed ingredients.
	 *  Each entry is ArcCraftItemTags::GetCraftTags of that slot's spec. */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	TArray<FGameplayTagContainer> PerSlotTags;

	/** Per-slot rarity steps (parallel to consumed ingredients). */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	TArray<float> SlotRaritySteps;

	/** Weighted average R across all ingredients. */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	float AverageR = 1.0f;

	/** R value used for band eligibility checks (MinAverageR comparison).
	 *  Derived from weighted average ingredient R.
	 *  Extra same-R items do NOT boost this — they boost BandWeightBonus instead. */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	float BandEligibilityR = 1.0f;

	/** Remedy bonus from extra ingredients and extra craft time; rule weight is scaled by (1 + BandWeightBonus).
	 *  Does NOT affect band eligibility (MinAverageR threshold).
	 *  Computed by FArcMaterialCraftEvaluator::ComputeEligibilityRAndWeightBonus(). */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	float BandWeightBonus = 0.0f;

	/** Number of filled ingredient slots. */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	int32 IngredientCount = 0;

	/** Base ingredient count expected by the recipe (for extra-material bonus calculation). */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	int32 BaseIngredientCount = 0;

	/** Designer-set weight bonus representing extra time spent crafting. */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	float ExtraCraftTimeBonus = 0.0f;

	/**
	 * Build a context from the consumed ingredients and recipe data.
	 * Per-slot tags come from ArcCraftItemTags::GetCraftTags (definition AssetTags plus instance tags).
	 *
	 * @param ConsumedIngredients    Matched items per ingredient slot; an empty slot holds a spec without a definition.
	 * @param InSlotRaritySteps      Rarity step per ingredient slot.
	 * @param InAverageR             Pre-computed weighted average R.
	 * @param InBaseIngredientCount  Expected ingredient count (for extra-material bonus).
	 * @param InExtraCraftTimeBonus  Designer-set extra-time weight bonus.
	 */
	static FArcMaterialCraftContext Build(
		const TArray<FArcItemSpec>& ConsumedIngredients,
		const TArray<float>& InSlotRaritySteps,
		float InAverageR,
		int32 InBaseIngredientCount = 0,
		float InExtraCraftTimeBonus = 0.0f);
};
