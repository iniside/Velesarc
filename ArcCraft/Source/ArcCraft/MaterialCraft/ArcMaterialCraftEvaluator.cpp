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
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTableData.h"
#include "ArcCraft/Recipe/ArcRandomPoolEvaluation.h"
#include "ArcCraft/Recipe/ArcRecipeModifierSource.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMaterialCraft, Log, All);

namespace ArcMaterialCraftEvaluatorImpl
{

static FGameplayTagContainer CombineSlotTags(const TArray<FGameplayTagContainer>& PerSlotTags)
{
	FGameplayTagContainer CombinedTags;
	for (const FGameplayTagContainer& SlotTags : PerSlotTags)
	{
		CombinedTags.AppendTags(SlotTags);
	}
	return CombinedTags;
}

static bool DoesSlotSatisfyQuery(const FGameplayTagQuery& Query, const FGameplayTagContainer& SlotTags)
{
	return !SlotTags.IsEmpty() && Query.Matches(SlotTags);
}

static void CollectPositiveTags(const FGameplayTagQueryExpression& Expr, FGameplayTagContainer& OutTags)
{
	switch (Expr.ExprType)
	{
	case EGameplayTagQueryExprType::AnyTagsMatch:
	case EGameplayTagQueryExprType::AllTagsMatch:
	case EGameplayTagQueryExprType::AnyTagsExactMatch:
	case EGameplayTagQueryExprType::AllTagsExactMatch:
		for (const FGameplayTag& Tag : Expr.TagSet)
		{
			OutTags.AddTag(Tag);
		}
		break;
	case EGameplayTagQueryExprType::AnyExprMatch:
	case EGameplayTagQueryExprType::AllExprMatch:
		for (const FGameplayTagQueryExpression& SubExpr : Expr.ExprSet)
		{
			CollectPositiveTags(SubExpr, OutTags);
		}
		break;
	default:
		break;
	}
}

} // namespace ArcMaterialCraftEvaluatorImpl

// -------------------------------------------------------------------
// ComputeEligibilityRAndWeightBonus
// -------------------------------------------------------------------

void FArcMaterialCraftEvaluator::ComputeEligibilityRAndWeightBonus(
	const FArcMaterialPropertyTableData& InTable,
	FArcMaterialCraftContext& InOutContext)
{
	// Band eligibility = pure ingredient R, NOT boosted by extra same-R items
	InOutContext.BandEligibilityR = InOutContext.AverageR;

	float WeightBonus = 0.0f;
	// Extra ingredient bonus → goes to weight bias, not eligibility
	const int32 ExtraIngredients = FMath::Max(0, InOutContext.IngredientCount - InOutContext.BaseIngredientCount);
	WeightBonus += ExtraIngredients * InTable.ExtraIngredientWeightBonus;

	// Extra craft time bonus → also weight bias
	WeightBonus += FMath::Min(InOutContext.ExtraCraftTimeBonus, InTable.ExtraTimeWeightBonusCap);
	InOutContext.BandWeightBonus = WeightBonus;
}

// -------------------------------------------------------------------
// Rule Matching (match mode + MinAverageR gate)
// -------------------------------------------------------------------

bool FArcMaterialCraftEvaluator::DoesRuleMatch(
	const FArcMaterialPropertyRule& Rule,
	const FArcMaterialCraftContext& Context)
{
	// Rule-level R gate
	if (Context.AverageR < Rule.MinAverageR)
	{
		return false;
	}

	if (Rule.TagQuery.IsEmpty())
	{
		return true;
	}

	if (Rule.Match == EArcMaterialRuleMatch::SingleIngredient)
	{
		for (const FGameplayTagContainer& SlotTags : Context.PerSlotTags)
		{
			if (ArcMaterialCraftEvaluatorImpl::DoesSlotSatisfyQuery(Rule.TagQuery, SlotTags))
			{
				return true;
			}
		}
		return false;
	}

	return Rule.TagQuery.Matches(ArcMaterialCraftEvaluatorImpl::CombineSlotTags(Context.PerSlotTags));
}

TArray<FGameplayTagContainer> FArcMaterialCraftEvaluator::GetRuleSlotTags(
	const FArcMaterialPropertyRule& Rule,
	const FArcMaterialCraftContext& Context)
{
	if (Rule.Match != EArcMaterialRuleMatch::SingleIngredient || Rule.TagQuery.IsEmpty())
	{
		return Context.PerSlotTags;
	}

	TArray<FGameplayTagContainer> RuleSlotTags;
	RuleSlotTags.SetNum(Context.PerSlotTags.Num());
	for (int32 SlotIdx = 0; SlotIdx < Context.PerSlotTags.Num(); ++SlotIdx)
	{
		if (ArcMaterialCraftEvaluatorImpl::DoesSlotSatisfyQuery(Rule.TagQuery, Context.PerSlotTags[SlotIdx]))
		{
			RuleSlotTags[SlotIdx] = Context.PerSlotTags[SlotIdx];
		}
	}
	return RuleSlotTags;
}

// -------------------------------------------------------------------
// Rule weight and query tags
// -------------------------------------------------------------------

float FArcMaterialCraftEvaluator::ComputeRuleWeight(
	const FArcMaterialPropertyRule& Rule,
	const FArcMaterialCraftContext& Context)
{
	float RuleWeight = ArcRandomPoolEval::ApplyWeightModifiers(
		Rule.Weight, Rule.WeightModifiers, ArcMaterialCraftEvaluatorImpl::CombineSlotTags(Context.PerSlotTags));
	RuleWeight *= 1.0f + (Context.AverageR - 1.0f) * Rule.RWeightScaling;
	RuleWeight *= 1.0f + Context.BandWeightBonus;
	return FMath::Max(RuleWeight, 0.0f);
}

FGameplayTagContainer FArcMaterialCraftEvaluator::GetPositiveQueryTags(const FGameplayTagQuery& Query)
{
	FGameplayTagContainer PositiveTags;
	if (!Query.IsEmpty())
	{
		FGameplayTagQueryExpression RootExpr;
		Query.GetQueryExpr(RootExpr);
		ArcMaterialCraftEvaluatorImpl::CollectPositiveTags(RootExpr, PositiveTags);
	}
	return PositiveTags;
}

// -------------------------------------------------------------------
// EvaluateRules
// -------------------------------------------------------------------

TArray<FArcMaterialRuleEvaluation> FArcMaterialCraftEvaluator::EvaluateRules(
	const FArcMaterialPropertyTableData& InTable,
	const FArcMaterialCraftContext& InContext)
{
	TArray<FArcMaterialRuleEvaluation> Evaluations;

	if (InTable.Rules.Num() == 0)
	{
		return Evaluations;
	}

	int32 MatchedCount = 0;
	for (int32 RuleIdx = 0; RuleIdx < InTable.Rules.Num(); ++RuleIdx)
	{
		const FArcMaterialPropertyRule& Rule = InTable.Rules[RuleIdx];
		if (!DoesRuleMatch(Rule, InContext))
		{
			continue;
		}
		++MatchedCount;

		TArray<int32> EligibleIndices;
		for (int32 ModIdx = 0; ModIdx < Rule.Modifiers.Num(); ++ModIdx)
		{
			const FArcCraftModifier* Mod = Rule.Modifiers[ModIdx].GetPtr<FArcCraftModifier>();
			if (!Mod)
			{
				continue;
			}

			// Per-modifier R threshold gate
			if (Mod->MinR > 0.0f && InContext.AverageR < Mod->MinR)
			{
				continue;
			}

			EligibleIndices.Add(ModIdx);
		}

		if (EligibleIndices.Num() == 0)
		{
			continue;
		}

		FArcMaterialRuleEvaluation Eval;
		Eval.RuleIndex = RuleIdx;
		Eval.EffectiveWeight = ComputeRuleWeight(Rule, InContext);
		Eval.Rule = &Rule;
		Eval.RequiredTags = GetPositiveQueryTags(Rule.TagQuery);
		Eval.MatchedTags = ArcCraftProvenance::FilterHeldTags(
			Eval.RequiredTags, ArcMaterialCraftEvaluatorImpl::CombineSlotTags(GetRuleSlotTags(Rule, InContext)));
		Eval.EligibleModifierIndices = MoveTemp(EligibleIndices);
		Evaluations.Add(MoveTemp(Eval));
	}

	UE_LOGF(LogArcMaterialCraft, Verbose,"MaterialCraft: %d rules matched, %d evaluations produced (AverageR=%.2f)",
		MatchedCount, Evaluations.Num(), InContext.AverageR);

	return Evaluations;
}
