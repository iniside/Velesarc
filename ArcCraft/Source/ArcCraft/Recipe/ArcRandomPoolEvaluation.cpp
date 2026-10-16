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

#include "ArcCraft/Recipe/ArcRandomPoolEvaluation.h"
#include "ArcCraft/Recipe/ArcRandomPoolDefinitionData.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"

#define LOCTEXT_NAMESPACE "ArcRandomPoolEvaluation"

namespace ArcRandomPoolEval
{

TArray<FArcRandomPoolEntryEvalResult> ComputeEntryEvaluations(
	const FArcRandomPoolDefinitionData& PoolDef,
	float AverageR,
	const FGameplayTagContainer& AggregatedTags,
	bool bBuildFailureReason)
{
	TArray<FArcRandomPoolEntryEvalResult> Results;

	Results.Reserve(PoolDef.Entries.Num());

	for (int32 Idx = 0; Idx < PoolDef.Entries.Num(); ++Idx)
	{
		const FArcRandomPoolEntry& Entry = PoolDef.Entries[Idx];

		FArcRandomPoolEntryEvalResult EvalResult;
		EvalResult.bEligible = false;
		EvalResult.EffectiveWeight = 0.0f;

		// --- Eligibility checks (same predicates as runtime Evaluate) ---

		if (Entry.MinR > 0.0f && AverageR < Entry.MinR)
		{
			if (bBuildFailureReason)
			{
				EvalResult.FailureReason = FText::Format(
					LOCTEXT("MinQFail", "Failed: MinR not met (need {0}, have {1})"),
					FText::AsNumber(Entry.MinR),
					FText::AsNumber(AverageR));
			}
			Results.Add(EvalResult);
			continue;
		}

		if (!Entry.RequiredIngredientTags.IsEmpty() &&
			!AggregatedTags.HasAll(Entry.RequiredIngredientTags))
		{
			if (bBuildFailureReason)
			{
				EvalResult.FailureReason = FText::Format(
					LOCTEXT("ReqTagsFail", "Failed: RequiredIngredientTags not satisfied (need {0})"),
					FText::FromString(Entry.RequiredIngredientTags.ToString()));
			}
			Results.Add(EvalResult);
			continue;
		}

		if (!Entry.DenyIngredientTags.IsEmpty() &&
			AggregatedTags.HasAny(Entry.DenyIngredientTags))
		{
			if (bBuildFailureReason)
			{
				EvalResult.FailureReason = FText::Format(
					LOCTEXT("DenyTagsFail", "Failed: DenyIngredientTags matched (deny {0})"),
					FText::FromString(Entry.DenyIngredientTags.ToString()));
			}
			Results.Add(EvalResult);
			continue;
		}

		// --- Eligible: compute effective weight ---

		float EntryWeight = ApplyWeightModifiers(Entry.BaseWeight, Entry.WeightModifiers, AggregatedTags);
		EntryWeight *= (1.0f + (AverageR - 1.0f) * Entry.RWeightScaling);
		EntryWeight = FMath::Max(EntryWeight, 0.0f);

		EvalResult.bEligible = true;
		EvalResult.EffectiveWeight = EntryWeight;

		Results.Add(EvalResult);
	}

	return Results;
}

void CollectEligibleSets(
	const TArray<FArcRandomPoolEntryEvalResult>& EntryEvals,
	TArray<int32>& OutEligibleIndices,
	TArray<float>& OutEffectiveWeights)
{
	OutEligibleIndices.Reset();
	OutEffectiveWeights.Reset();

	for (int32 Idx = 0; Idx < EntryEvals.Num(); ++Idx)
	{
		const FArcRandomPoolEntryEvalResult& Eval = EntryEvals[Idx];
		if (Eval.bEligible)
		{
			OutEligibleIndices.Add(Idx);
			OutEffectiveWeights.Add(Eval.EffectiveWeight);
		}
	}
}

TArray<int32> SelectEntries(
	const FArcRandomPoolDefinitionData& PoolDef,
	const FArcRandomPoolSelectionMode& Mode,
	float AverageR,
	const FGameplayTagContainer& AggregatedTags,
	const FRandomStream& Stream)
{
	const TArray<FArcRandomPoolEntryEvalResult> EntryEvals = ComputeEntryEvaluations(PoolDef, AverageR, AggregatedTags, /*bBuildFailureReason=*/false);

	TArray<int32> EligibleIndices;
	TArray<float> EffectiveWeights;
	CollectEligibleSets(EntryEvals, EligibleIndices, EffectiveWeights);

	TArray<int32> SelectedIndices;
	if (EligibleIndices.Num() == 0)
	{
		return SelectedIndices;
	}

	const FArcRandomPoolSelectionContext SelectionContext
	{
		PoolDef.Entries,
		EligibleIndices,
		EffectiveWeights,
		AverageR,
		Stream
	};
	Mode.Select(SelectionContext, SelectedIndices);
	return SelectedIndices;
}

bool PassesIngredientTags(
	const FGameplayTagContainer& RequiredTags,
	const FGameplayTagContainer& DenyTags,
	const FGameplayTagContainer& AggregatedTags)
{
	if (!RequiredTags.IsEmpty() && !AggregatedTags.HasAll(RequiredTags))
	{
		return false;
	}
	return DenyTags.IsEmpty() || !AggregatedTags.HasAny(DenyTags);
}

float ApplyWeightModifiers(
	float BaseWeight,
	TConstArrayView<FArcRandomPoolWeightModifier> WeightModifiers,
	const FGameplayTagContainer& AggregatedTags)
{
	float Weight = BaseWeight;
	float Multiplier = 1.0f;
	for (const FArcRandomPoolWeightModifier& WeightMod : WeightModifiers)
	{
		if (!WeightMod.RequiredTags.IsEmpty() &&
			AggregatedTags.HasAll(WeightMod.RequiredTags))
		{
			Weight += WeightMod.BonusWeight;
			Multiplier *= WeightMod.WeightMultiplier;
		}
	}
	return Weight * Multiplier;
}

} // namespace ArcRandomPoolEval

#undef LOCTEXT_NAMESPACE
