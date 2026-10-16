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

struct FArcRandomPoolEntry;
struct FArcRandomPoolDefinitionData;
struct FArcRandomPoolSelectionMode;
struct FArcRandomPoolWeightModifier;
struct FRandomStream;

/**
 * Result of evaluating one pool entry against a debug / runtime context.
 * Shared between the runtime evaluator and the editor debug overlay.
 */
struct FArcRandomPoolEntryEvalResult
{
	/** True if the entry passed all eligibility checks. */
	bool bEligible = false;

	/** Effective weight after weight-modifier math (0 if not eligible). */
	float EffectiveWeight = 0.0f;

	/** Human-readable failure reason (editor preview only; empty at runtime). */
	FText FailureReason;
};

/**
 * Shared eligibility + weight computation for random pool entries.
 * Called by both the runtime FArcRecipeModifierSource_RandomPool::Evaluate and
 * the editor debug overlay so the two can never diverge.
 *
 * Namespace: ArcRandomPoolEval (non-Impl — this header is included by both runtime and editor).
 */
namespace ArcRandomPoolEval
{

/**
 * Compute per-entry eligibility and effective weight for every entry in the pool.
 *
 * @param PoolDef          The pool record.
 * @param AverageR   Weighted average R from the ingredient slots.
 * @param AggregatedTags   Union of all ingredient tags (caller builds this).
 * @param bBuildFailureReason  If true, FArcRandomPoolEntryEvalResult::FailureReason is filled
 *                             (editor only; costs a FText format per ineligible entry).
 * @return One result per entry, parallel to PoolDef.Entries.
 */
ARCCRAFT_API TArray<FArcRandomPoolEntryEvalResult> ComputeEntryEvaluations(
	const FArcRandomPoolDefinitionData& PoolDef,
	float AverageR,
	const FGameplayTagContainer& AggregatedTags,
	bool bBuildFailureReason = false);

/**
 * Convenience: collect eligible indices and their effective weights from ComputeEntryEvaluations.
 * Used by the runtime Evaluate path.
 */
ARCCRAFT_API void CollectEligibleSets(
	const TArray<FArcRandomPoolEntryEvalResult>& EntryEvals,
	TArray<int32>& OutEligibleIndices,
	TArray<float>& OutEffectiveWeights);

/** Eligible entries and their weights (ComputeEntryEvaluations), then Mode.Select; indices into PoolDef.Entries. */
ARCCRAFT_API TArray<int32> SelectEntries(
	const FArcRandomPoolDefinitionData& PoolDef,
	const FArcRandomPoolSelectionMode& Mode,
	float AverageR,
	const FGameplayTagContainer& AggregatedTags,
	const FRandomStream& Stream);

/** True when AggregatedTags hold all Required tags and none of the Deny tags; an empty container never fails. */
ARCCRAFT_API bool PassesIngredientTags(
	const FGameplayTagContainer& RequiredTags,
	const FGameplayTagContainer& DenyTags,
	const FGameplayTagContainer& AggregatedTags);

/** (BaseWeight + sum of matching BonusWeight) * product of matching WeightMultiplier; not clamped. */
ARCCRAFT_API float ApplyWeightModifiers(
	float BaseWeight,
	TConstArrayView<FArcRandomPoolWeightModifier> WeightModifiers,
	const FGameplayTagContainer& AggregatedTags);

} // namespace ArcRandomPoolEval
