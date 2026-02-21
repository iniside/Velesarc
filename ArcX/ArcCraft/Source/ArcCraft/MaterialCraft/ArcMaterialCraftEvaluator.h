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

struct FArcItemSpec;
struct FArcItemData;
struct FArcMaterialPropertyRule;
struct FArcMaterialQualityBand;
struct FArcMaterialCraftContext;
struct FArcMaterialModifierSlotConfig;
class UArcMaterialPropertyTable;

/**
 * Result of evaluating a single rule — records which rule and band were selected.
 * Not a USTRUCT — only used internally within evaluation logic.
 */
struct FArcMaterialRuleEvaluation
{
	int32 RuleIndex = INDEX_NONE;
	int32 SelectedBandIndex = INDEX_NONE;
	float BandEligibilityQuality = 1.0f;
	float EffectiveWeight = 0.0f;
	const FArcMaterialPropertyRule* Rule = nullptr;
	const FArcMaterialQualityBand* Band = nullptr;
};

/**
 * Stateless utility class for evaluating material property rules.
 * Separated from data types for clarity and testability.
 *
 * Flow:
 *  1. ComputeQualityAndWeightBonus() — separate eligibility quality from weight bonus
 *  2. EvaluateRules() — match rules using per-slot tag union, select bands with separated quality/weight
 *  3. FilterBySlotConfig() — optional: limit evaluations per modifier slot type
 *  4. ApplyEvaluations() — apply selected band modifiers to the output item
 */
class ARCCRAFT_API FArcMaterialCraftEvaluator
{
public:
	/**
	 * Compute band eligibility quality and band weight bonus separately.
	 * BandEligibilityQuality = AverageQuality (pure ingredient quality, not boosted by extras).
	 * BandWeightBonus = bonus from extra ingredients + extra time (used in weight calc only).
	 * Updates InOutContext.BandEligibilityQuality and InOutContext.BandWeightBonus.
	 */
	static void ComputeQualityAndWeightBonus(
		const UArcMaterialPropertyTable* InTable,
		FArcMaterialCraftContext& InOutContext);

	/**
	 * Evaluate all rules against the context and select quality bands.
	 * Returns the list of rule+band pairs that should fire.
	 * BandEligibilityQuality and BandWeightBonus should already be computed.
	 */
	static TArray<FArcMaterialRuleEvaluation> EvaluateRules(
		const UArcMaterialPropertyTable* InTable,
		const FArcMaterialCraftContext& InContext);

	/**
	 * Filter evaluations through modifier slot configs.
	 * Rules' OutputTags are matched to SlotTag to determine routing.
	 * If SlotConfigs is empty, all evaluations pass through unchanged.
	 */
	static TArray<FArcMaterialRuleEvaluation> FilterBySlotConfig(
		const TArray<FArcMaterialRuleEvaluation>& Evaluations,
		const TArray<FArcMaterialModifierSlotConfig>& SlotConfigs);

	/**
	 * Apply all evaluated rule/band results to the output item spec.
	 * Each selected band's modifiers are applied with the evaluation's quality.
	 */
	static void ApplyEvaluations(
		const TArray<FArcMaterialRuleEvaluation>& Evaluations,
		FArcItemSpec& OutItemSpec,
		const TArray<const FArcItemData*>& ConsumedIngredients,
		const TArray<float>& IngredientQualityMults,
		float BandEligibilityQuality);

private:
	/** Check if a single rule matches using per-slot tag union and recipe tags. */
	static bool DoesRuleMatch(
		const FArcMaterialPropertyRule& Rule,
		const FArcMaterialCraftContext& Context);

	/**
	 * Select a band from the given bands via weighted random.
	 * Band eligibility uses BandEligibilityQuality. Weight bonus is separate.
	 * Returns the band index, or INDEX_NONE if no bands are eligible.
	 */
	static int32 SelectBandFromRule(
		const TArray<FArcMaterialQualityBand>& Bands,
		float BandEligibilityQuality,
		float BandWeightBonus);

	/**
	 * Get all eligible bands and their effective weights.
	 * A band is eligible if BandEligibilityQuality >= band.MinQuality.
	 * Weight formula: BaseWeight * (1 + (max(0, BandEligibilityQ - MinQ) + BandWeightBonus) * QualityWeightBias)
	 */
	static void GetEligibleBands(
		const TArray<FArcMaterialQualityBand>& Bands,
		float BandEligibilityQuality,
		float BandWeightBonus,
		TArray<int32>& OutEligibleIndices,
		TArray<float>& OutWeights);
};
