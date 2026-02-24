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

#include "ArcCraft/MaterialCraft/ArcMaterialCraftEvaluator.h"

#include "ArcCraft/MaterialCraft/ArcMaterialCraftContext.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyRule.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTable.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "Items/ArcItemSpec.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMaterialCraft, Log, All);

// -------------------------------------------------------------------
// ComputeQualityAndWeightBonus
// -------------------------------------------------------------------

void FArcMaterialCraftEvaluator::ComputeQualityAndWeightBonus(
	const UArcMaterialPropertyTable* InTable,
	FArcMaterialCraftContext& InOutContext)
{
	// Band eligibility = pure ingredient quality, NOT boosted by extra same-quality items
	InOutContext.BandEligibilityQuality = InOutContext.AverageQuality;

	float WeightBonus = 0.0f;
	if (InTable)
	{
		// Extra ingredient bonus → goes to weight bias, not eligibility
		const int32 ExtraIngredients = FMath::Max(0, InOutContext.IngredientCount - InOutContext.BaseIngredientCount);
		WeightBonus += ExtraIngredients * InTable->ExtraIngredientWeightBonus;

		// Extra craft time bonus → also weight bias
		WeightBonus += FMath::Min(InOutContext.ExtraCraftTimeBonus, InTable->ExtraTimeWeightBonusCap);
	}
	InOutContext.BandWeightBonus = WeightBonus;
}

// -------------------------------------------------------------------
// Rule Matching (per-slot union)
// -------------------------------------------------------------------

bool FArcMaterialCraftEvaluator::DoesRuleMatch(
	const FArcMaterialPropertyRule& Rule,
	const FArcMaterialCraftContext& Context)
{
	// Check tag query against union of per-slot tags.
	// Empty tag query = always matches (no tag requirement).
	if (!Rule.TagQuery.IsEmpty())
	{
		FGameplayTagContainer CombinedTags;
		for (const FGameplayTagContainer& SlotTags : Context.PerSlotTags)
		{
			CombinedTags.AppendTags(SlotTags);
		}
		
		if (CombinedTags.IsEmpty())
		{
			return true;
		}
		
		if (!Rule.TagQuery.Matches(CombinedTags))
		{
			return false;
		}
	}

	// Check required recipe tags
	if (!Rule.RequiredRecipeTags.IsEmpty())
	{
		if (!Context.RecipeTags.HasAll(Rule.RequiredRecipeTags))
		{
			return false;
		}
	}

	return true;
}

// -------------------------------------------------------------------
// Band Selection (separated eligibility vs weight)
// -------------------------------------------------------------------

void FArcMaterialCraftEvaluator::GetEligibleBands(
	const TArray<FArcMaterialQualityBand>& Bands,
	float BandEligibilityQuality,
	float BandWeightBonus,
	TArray<int32>& OutEligibleIndices,
	TArray<float>& OutWeights)
{
	for (int32 BandIdx = 0; BandIdx < Bands.Num(); ++BandIdx)
	{
		const FArcMaterialQualityBand& Band = Bands[BandIdx];

		// Eligibility check uses ONLY BandEligibilityQuality (pure ingredient quality)
		if (BandEligibilityQuality >= Band.MinQuality)
		{
			// Weight calculation incorporates BandWeightBonus
			// Formula: BaseWeight * (1 + (max(0, Quality - MinQuality) + WeightBonus) * QualityWeightBias)
			const float QualityAboveMin = FMath::Max(0.0f, BandEligibilityQuality - Band.MinQuality);
			const float EffWeight = Band.BaseWeight * (1.0f + (QualityAboveMin + BandWeightBonus) * Band.QualityWeightBias);

			if (EffWeight > 0.0f)
			{
				OutEligibleIndices.Add(BandIdx);
				OutWeights.Add(EffWeight);
			}
		}
	}
}

int32 FArcMaterialCraftEvaluator::SelectBandFromRule(
	const TArray<FArcMaterialQualityBand>& Bands,
	float BandEligibilityQuality,
	float BandWeightBonus)
{
	TArray<int32> EligibleIndices;
	TArray<float> Weights;
	GetEligibleBands(Bands, BandEligibilityQuality, BandWeightBonus, EligibleIndices, Weights);

	if (EligibleIndices.Num() == 0)
	{
		return INDEX_NONE;
	}

	// Compute total weight
	float TotalWeight = 0.0f;
	for (const float W : Weights)
	{
		TotalWeight += W;
	}

	if (TotalWeight <= 0.0f)
	{
		return INDEX_NONE;
	}

	// Weighted random pick
	const float Roll = FMath::FRandRange(0.0f, TotalWeight);
	float Accumulator = 0.0f;

	for (int32 i = 0; i < EligibleIndices.Num(); ++i)
	{
		Accumulator += Weights[i];
		if (Roll <= Accumulator)
		{
			return EligibleIndices[i];
		}
	}

	// Floating point edge case — return last eligible
	return EligibleIndices.Last();
}

// -------------------------------------------------------------------
// EvaluateRules
// -------------------------------------------------------------------

TArray<FArcMaterialRuleEvaluation> FArcMaterialCraftEvaluator::EvaluateRules(
	const UArcMaterialPropertyTable* InTable,
	const FArcMaterialCraftContext& InContext)
{
	TArray<FArcMaterialRuleEvaluation> Evaluations;

	if (!InTable || InTable->Rules.Num() == 0)
	{
		return Evaluations;
	}

	const float BandEligQ = InContext.BandEligibilityQuality;
	const float WeightBonus = InContext.BandWeightBonus;

	// 1. Find all matching rules
	struct FMatchedRule
	{
		int32 RuleIndex;
		int32 Priority;
	};

	TArray<FMatchedRule> MatchedRules;
	for (int32 RuleIdx = 0; RuleIdx < InTable->Rules.Num(); ++RuleIdx)
	{
		if (DoesRuleMatch(InTable->Rules[RuleIdx], InContext))
		{
			MatchedRules.Add({RuleIdx, InTable->Rules[RuleIdx].Priority});
		}
	}

	if (MatchedRules.Num() == 0)
	{
		return Evaluations;
	}

	// 2. Sort by priority (descending)
	MatchedRules.Sort([](const FMatchedRule& A, const FMatchedRule& B)
	{
		return A.Priority > B.Priority;
	});

	// 3. Apply MaxActiveRules limit
	if (InTable->MaxActiveRules > 0 && MatchedRules.Num() > InTable->MaxActiveRules)
	{
		MatchedRules.SetNum(InTable->MaxActiveRules);
	}

	// 4. Compute band budget if enabled
	const bool bUseBudget = InTable->BaseBandBudget > 0.0f;
	float RemainingBudget = 0.0f;
	if (bUseBudget)
	{
		RemainingBudget = InTable->BaseBandBudget
			+ FMath::Max(0.0f, BandEligQ - 1.0f) * InTable->BudgetPerQuality;
	}

	// 5. Select bands for each matching rule (one band per rule)
	for (const FMatchedRule& Matched : MatchedRules)
	{
		const FArcMaterialPropertyRule& Rule = InTable->Rules[Matched.RuleIndex];
		const TArray<FArcMaterialQualityBand>& Bands = Rule.GetEffectiveQualityBands();

		if (Bands.Num() == 0)
		{
			continue;
		}

		const int32 BandIdx = SelectBandFromRule(Bands, BandEligQ, WeightBonus);
		if (BandIdx == INDEX_NONE)
		{
			continue;
		}

		// Compute effective weight for this selection
		const FArcMaterialQualityBand& SelectedBand = Bands[BandIdx];
		const float QualityAboveMin = FMath::Max(0.0f, BandEligQ - SelectedBand.MinQuality);
		const float EffWeight = SelectedBand.BaseWeight
			* (1.0f + (QualityAboveMin + WeightBonus) * SelectedBand.QualityWeightBias);

		// Budget check: band cost = (bandIndex + 1)
		if (bUseBudget)
		{
			const float BandCost = static_cast<float>(BandIdx + 1);
			if (BandCost > RemainingBudget)
			{
				// Try to select a cheaper band instead
				TArray<int32> EligibleIndices;
				TArray<float> Weights;
				GetEligibleBands(Bands, BandEligQ, WeightBonus, EligibleIndices, Weights);

				// Filter to affordable bands
				TArray<int32> AffordableIndices;
				TArray<float> AffordableWeights;
				float AffordableTotalWeight = 0.0f;

				for (int32 i = 0; i < EligibleIndices.Num(); ++i)
				{
					const float Cost = static_cast<float>(EligibleIndices[i] + 1);
					if (Cost <= RemainingBudget)
					{
						AffordableIndices.Add(EligibleIndices[i]);
						AffordableWeights.Add(Weights[i]);
						AffordableTotalWeight += Weights[i];
					}
				}

				if (AffordableIndices.Num() == 0 || AffordableTotalWeight <= 0.0f)
				{
					continue; // Nothing affordable, skip this rule
				}

				// Weighted random among affordable bands
				const float Roll = FMath::FRandRange(0.0f, AffordableTotalWeight);
				float Accum = 0.0f;
				int32 AffordableBandIdx = AffordableIndices.Last();
				for (int32 i = 0; i < AffordableIndices.Num(); ++i)
				{
					Accum += AffordableWeights[i];
					if (Roll <= Accum)
					{
						AffordableBandIdx = AffordableIndices[i];
						break;
					}
				}

				RemainingBudget -= static_cast<float>(AffordableBandIdx + 1);

				const FArcMaterialQualityBand& AffBand = Bands[AffordableBandIdx];
				const float AffQAboveMin = FMath::Max(0.0f, BandEligQ - AffBand.MinQuality);
				const float AffEffWeight = AffBand.BaseWeight
					* (1.0f + (AffQAboveMin + WeightBonus) * AffBand.QualityWeightBias);

				FArcMaterialRuleEvaluation Eval;
				Eval.RuleIndex = Matched.RuleIndex;
				Eval.SelectedBandIndex = AffordableBandIdx;
				Eval.BandEligibilityQuality = BandEligQ;
				Eval.EffectiveWeight = AffEffWeight;
				Eval.Rule = &Rule;
				Eval.Band = &AffBand;
				Evaluations.Add(Eval);
			}
			else
			{
				RemainingBudget -= static_cast<float>(BandIdx + 1);

				FArcMaterialRuleEvaluation Eval;
				Eval.RuleIndex = Matched.RuleIndex;
				Eval.SelectedBandIndex = BandIdx;
				Eval.BandEligibilityQuality = BandEligQ;
				Eval.EffectiveWeight = EffWeight;
				Eval.Rule = &Rule;
				Eval.Band = &SelectedBand;
				Evaluations.Add(Eval);
			}
		}
		else
		{
			// No budget — just add the evaluation
			FArcMaterialRuleEvaluation Eval;
			Eval.RuleIndex = Matched.RuleIndex;
			Eval.SelectedBandIndex = BandIdx;
			Eval.BandEligibilityQuality = BandEligQ;
			Eval.EffectiveWeight = EffWeight;
			Eval.Rule = &Rule;
			Eval.Band = &SelectedBand;
			Evaluations.Add(Eval);
		}
	}

	UE_LOG(LogArcMaterialCraft, Verbose,
		TEXT("MaterialCraft: %d rules matched, %d evaluations produced (BandEligQ=%.2f, WeightBonus=%.2f, Budget=%.1f)"),
		MatchedRules.Num(), Evaluations.Num(), BandEligQ, WeightBonus,
		bUseBudget ? RemainingBudget : -1.0f);

	return Evaluations;
}

// -------------------------------------------------------------------
// ApplyEvaluations
// -------------------------------------------------------------------

void FArcMaterialCraftEvaluator::ApplyEvaluations(
	const TArray<FArcMaterialRuleEvaluation>& Evaluations,
	FArcItemSpec& OutItemSpec,
	const FGameplayTagContainer& AggregatedIngredientTags,
	float BandEligibilityQuality)
{
	for (const FArcMaterialRuleEvaluation& Eval : Evaluations)
	{
		if (!Eval.Band)
		{
			continue;
		}

		UE_LOG(LogArcMaterialCraft, Verbose,
			TEXT("  Applying rule '%s' band '%s' (index %d, BandEligQ=%.2f, Weight=%.2f)"),
			Eval.Rule ? *Eval.Rule->RuleName.ToString() : TEXT("?"),
			*Eval.Band->BandName.ToString(),
			Eval.SelectedBandIndex,
			Eval.BandEligibilityQuality,
			Eval.EffectiveWeight);

		// Apply each terminal modifier in the selected band
		for (const FInstancedStruct& ModifierStruct : Eval.Band->Modifiers)
		{
			const FArcCraftModifier* Modifier = ModifierStruct.GetPtr<FArcCraftModifier>();
			if (Modifier)
			{
				Modifier->Apply(OutItemSpec, AggregatedIngredientTags, BandEligibilityQuality);
			}
		}
	}
}
