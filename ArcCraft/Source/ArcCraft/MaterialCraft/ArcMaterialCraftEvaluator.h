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

struct FArcMaterialPropertyRule;
struct FArcMaterialCraftContext;
struct FArcMaterialPropertyTableData;

/**
 * Result of evaluating a single rule — records which rule matched and which modifier
 * indices within that rule are eligible given the current R.
 * Not a USTRUCT — only used internally within evaluation logic.
 */
struct FArcMaterialRuleEvaluation
{
	int32 RuleIndex = INDEX_NONE;
	/** ComputeRuleWeight for the context. */
	float EffectiveWeight = 0.0f;
	const FArcMaterialPropertyRule* Rule = nullptr;
	/** Positive tags of the rule's TagQuery (GetPositiveQueryTags). */
	FGameplayTagContainer RequiredTags;
	/** RequiredTags present in the combined per-slot ingredient tags. */
	FGameplayTagContainer MatchedTags;
	/** Indices into Rule->Modifiers[] whose MinR was cleared. */
	TArray<int32> EligibleModifierIndices;
};

/**
 * Stateless utility class for evaluating material property rules.
 * Separated from data types for clarity and testability.
 *
 * Flow:
 *  1. ComputeEligibilityRAndWeightBonus() — separate eligibility R from weight bonus
 *  2. EvaluateRules() — match rules using per-slot tag union, weigh them, collect eligible modifier indices
 */
class ARCCRAFT_API FArcMaterialCraftEvaluator
{
public:
	/**
	 * Compute band eligibility R and band weight bonus separately.
	 * BandEligibilityR = AverageR (pure ingredient R, not boosted by extras).
	 * BandWeightBonus = bonus from extra ingredients + extra time (used in weight calc only).
	 * Updates InOutContext.BandEligibilityR and InOutContext.BandWeightBonus.
	 */
	static void ComputeEligibilityRAndWeightBonus(
		const FArcMaterialPropertyTableData& InTable,
		FArcMaterialCraftContext& InOutContext);

	/**
	 * Evaluate all rules against the context and collect eligible modifiers per rule.
	 * Returns one FArcMaterialRuleEvaluation per matched rule that has eligible modifiers, in table order.
	 * Each evaluation contains the indices into Rule->Modifiers[] whose MinR
	 * is cleared by Context.AverageR.
	 */
	static TArray<FArcMaterialRuleEvaluation> EvaluateRules(
		const FArcMaterialPropertyTableData& InTable,
		const FArcMaterialCraftContext& InContext);

	/**
	 * Check if a single rule matches: MinAverageR gate, then TagQuery against the slot tag union (Union)
	 * or against one non-empty slot's own tags (SingleIngredient).
	 * Exposed as public so editor debug evaluators can reuse the same predicate
	 * without reimplementing tag-query matching (single source of truth).
	 */
	static bool DoesRuleMatch(
		const FArcMaterialPropertyRule& Rule,
		const FArcMaterialCraftContext& Context);

	/**
	 * Per-slot tags the rule's MatchedTags and IngredientShare count, parallel to Context.PerSlotTags.
	 * Union (or an empty TagQuery): every slot. SingleIngredient: only the slots whose own tags satisfy TagQuery; the others are empty.
	 */
	static TArray<FGameplayTagContainer> GetRuleSlotTags(
		const FArcMaterialPropertyRule& Rule,
		const FArcMaterialCraftContext& Context);

	/**
	 * Rule weight = ArcRandomPoolEval::ApplyWeightModifiers(Weight, WeightModifiers, combined per-slot tags)
	 *   * (1 + (AverageR - 1) * RWeightScaling) * (1 + BandWeightBonus), never below 0.
	 * Expects ComputeEligibilityRAndWeightBonus to have run on the context.
	 */
	static float ComputeRuleWeight(
		const FArcMaterialPropertyRule& Rule,
		const FArcMaterialCraftContext& Context);

	/** Tags of the query's positive terms; tags under a NOT (NoTagsMatch, NoExprMatch) are left out. */
	static FGameplayTagContainer GetPositiveQueryTags(const FGameplayTagQuery& Query);
};
