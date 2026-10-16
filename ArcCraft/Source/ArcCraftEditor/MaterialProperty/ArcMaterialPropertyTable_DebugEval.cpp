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
 * distributed under the LICENSE is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "MaterialProperty/ArcMaterialPropertyTable_DebugEval.h"

#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTableData.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyRule.h"
#include "ArcCraft/MaterialCraft/ArcMaterialCraftEvaluator.h"
#include "ArcCraft/MaterialCraft/ArcMaterialCraftContext.h"
#include "ArcCraft/Recipe/ArcRecipeModifierSource.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "StructUtils/InstancedStruct.h"
#include "Shared/ArcCraftDebugEvalShared.h"

// Runtime item types — needed by the ArcCraftDebugEvalApply.inl below.
#include "Items/ArcItemSpec.h"
#include "ItemFragments/ArcItemFragment_ItemStats.h"
#include "ItemFragments/ArcItemFragment_GrantedMassAbilities.h"
#include "ItemFragments/ArcItemFragment_GrantedMassEffects.h"
#include "ItemFragments/ArcItemFragment_MassAttributeModifier.h"
// Complete type needed for ->GetName() on TObjectPtr members.
#include "Abilities/ArcAbilityDefinition.h"
#include "Effects/ArcEffectDefinition.h"

// Defines ArcCraftDebugEval::CollectApplyOutputs (static, one copy per TU).
#include "Shared/ArcCraftDebugEvalApply.inl"

#define LOCTEXT_NAMESPACE "ArcMaterialPropertyTable_DebugEval"

namespace ArcMaterialPropertyTable_DebugEval_Impl
{

/** Encoded row key for a Rule-level row. */
static int32 RuleRowKey(int32 RuleIdx)
{
	return RuleIdx;
}

/** Encoded row key for a Modifier child row. */
static int32 ModifierRowKey(int32 RuleIdx, int32 ModIdx)
{
	return RuleIdx * 10000 + ModIdx;
}

/**
 * Build a minimal FArcMaterialCraftContext from the debug context.
 * When InputSlots is non-empty, each slot produces one PerSlotTags entry and the
 * SlotRaritySteps array mirrors the slot rarity steps.
 * AverageR is the arithmetic mean of the slot rarity steps (or the legacy Ctx.AverageR
 * scalar when InputSlots is empty).
 */
static FArcMaterialCraftContext BuildContextForDebug(const FArcModifierDebugContext& Ctx)
{
	FArcMaterialCraftContext CraftCtx;

	if (!Ctx.InputSlots.IsEmpty())
	{
		float StepSum = 0.0f;
		for (const FArcDebugIngredientSlot& Slot : Ctx.InputSlots)
		{
			CraftCtx.PerSlotTags.Add(Slot.Tags);
			CraftCtx.SlotRaritySteps.Add(Slot.RarityStep);
			StepSum += Slot.RarityStep;
		}
		CraftCtx.AverageR = StepSum / static_cast<float>(Ctx.InputSlots.Num());
		CraftCtx.BandEligibilityR = CraftCtx.AverageR;
		CraftCtx.IngredientCount = Ctx.InputSlots.Num();
		CraftCtx.BaseIngredientCount = Ctx.InputSlots.Num();
	}
	else
	{
		// Legacy single-R fallback: one combined slot.
		CraftCtx.AverageR = Ctx.AverageR;
		CraftCtx.BandEligibilityR = Ctx.AverageR;

		FGameplayTagContainer CombinedSlot;
		CombinedSlot.AppendTags(Ctx.SlotTags);
		CombinedSlot.AppendTags(Ctx.IngredientTags);
		CraftCtx.PerSlotTags.Add(MoveTemp(CombinedSlot));
		CraftCtx.SlotRaritySteps.Add(Ctx.AverageR);
		CraftCtx.IngredientCount = 1;
		CraftCtx.BaseIngredientCount = 1;
	}

	return CraftCtx;
}

} // namespace ArcMaterialPropertyTable_DebugEval_Impl

namespace ArcMaterialPropertyTable_DebugEval
{

FArcModifierDebugEvaluation EvaluateMaterialPropertyTableForDebug(
	const FArcMaterialPropertyTableData& Table,
	const FArcModifierDebugContext& Ctx)
{
	FArcModifierDebugEvaluation Result;

	// Build a craft context from the debug inputs so DoesRuleMatch runs exactly as
	// the runtime evaluator would — single source of truth for the predicate.
	FArcMaterialCraftContext CraftCtx =
		ArcMaterialPropertyTable_DebugEval_Impl::BuildContextForDebug(Ctx);
	FArcMaterialCraftEvaluator::ComputeEligibilityRAndWeightBonus(Table, CraftCtx);

	TArray<int32> SlotAmounts;
	SlotAmounts.Init(1, CraftCtx.PerSlotTags.Num());

	// Aggregated tags and effective R used for modifier-level checks.
	// When InputSlots is non-empty, derive from the slot set; otherwise fall back to legacy fields.
	FGameplayTagContainer AggregatedTags;
	float EffectiveR = 0.0f;
	if (!Ctx.InputSlots.IsEmpty())
	{
		AggregatedTags = ArcCraftDebugEval::AggregateSlotTags(Ctx.InputSlots);
		EffectiveR = ArcCraftDebugEval::ComputeAverageR(Ctx.InputSlots);
	}
	else
	{
		AggregatedTags.AppendTags(Ctx.IngredientTags);
		AggregatedTags.AppendTags(Ctx.SlotTags);
		EffectiveR = Ctx.AverageR;
	}

	for (int32 RuleIdx = 0; RuleIdx < Table.Rules.Num(); ++RuleIdx)
	{
		const FArcMaterialPropertyRule& Rule = Table.Rules[RuleIdx];

		// --- Rule-level row ---
		FArcModifierRowEvaluation RuleRowEval;

		const bool bRuleMatches = FArcMaterialCraftEvaluator::DoesRuleMatch(Rule, CraftCtx);
		RuleRowEval.bRulePassed = bRuleMatches;
		RuleRowEval.bModifierEligible = bRuleMatches;
		RuleRowEval.EffectiveWeight = FArcMaterialCraftEvaluator::ComputeRuleWeight(Rule, CraftCtx);

		const FGameplayTagContainer RequiredTags = FArcMaterialCraftEvaluator::GetPositiveQueryTags(Rule.TagQuery);
		const TArray<FGameplayTagContainer> RuleSlotTags = FArcMaterialCraftEvaluator::GetRuleSlotTags(Rule, CraftCtx);
		FGameplayTagContainer RuleTags;
		for (const FGameplayTagContainer& SlotTags : RuleSlotTags)
		{
			RuleTags.AppendTags(SlotTags);
		}
		RuleRowEval.MatchedTags = ArcCraftProvenance::FilterHeldTags(RequiredTags, RuleTags);
		RuleRowEval.IngredientShare = ArcCraftProvenance::ComputeIngredientShare(RequiredTags, RuleSlotTags, SlotAmounts);

		if (!bRuleMatches)
		{
			// Determine which gate caused the failure for a useful tooltip.
			if (EffectiveR < Rule.MinAverageR)
			{
				RuleRowEval.FailureReason = FText::Format(
					LOCTEXT("RuleMinQFail", "MinAverageR not met (need {0}, have {1})"),
					FText::AsNumber(Rule.MinAverageR),
					FText::AsNumber(EffectiveR));
			}
			else
			{
				RuleRowEval.FailureReason = Rule.Match == EArcMaterialRuleMatch::SingleIngredient
					? LOCTEXT("RuleTagQuerySingleFail", "TagQuery does not match any single ingredient")
					: LOCTEXT("RuleTagQueryFail", "TagQuery does not match");
			}
		}

		Result.ByRowKey.Add(
			ArcMaterialPropertyTable_DebugEval_Impl::RuleRowKey(RuleIdx),
			RuleRowEval);

		// --- Modifier child rows ---
		for (int32 ModIdx = 0; ModIdx < Rule.Modifiers.Num(); ++ModIdx)
		{
			const FInstancedStruct& ModStruct = Rule.Modifiers[ModIdx];
			const FArcCraftModifier* Mod = ModStruct.GetPtr<FArcCraftModifier>();

			FArcModifierRowEvaluation ModRowEval;

			if (Mod == nullptr)
			{
				ModRowEval.bRulePassed = false;
				ModRowEval.bModifierEligible = false;
				ModRowEval.FailureReason = LOCTEXT("NullModifier", "Modifier not set");
				Result.ByRowKey.Add(
					ArcMaterialPropertyTable_DebugEval_Impl::ModifierRowKey(RuleIdx, ModIdx),
					ModRowEval);
				continue;
			}

			// TriggerTags checked against aggregated ingredient tags.
			const bool bTagsMatch = Mod->MatchesTriggerTags(AggregatedTags);
			ModRowEval.bRulePassed = bTagsMatch;

			// MinR checked against EffectiveR —
			// derived from InputSlots when present, or Ctx.AverageR as legacy fallback.
			const bool bRMet = EffectiveR >= Mod->MinR;
			ModRowEval.bModifierEligible = bRuleMatches && bTagsMatch && bRMet;
			ArcCraftDebugEval::FillStatBand(ModStruct, Ctx, EffectiveR, ModRowEval);

			if (!bTagsMatch)
			{
				ModRowEval.FailureReason = FText::Format(
					LOCTEXT("ModTagsFail",
						"Trigger tags not satisfied (need {0})"),
					FText::FromString(Mod->TriggerTags.ToString()));
			}
			else if (!bRMet)
			{
				ModRowEval.FailureReason = FText::Format(
					LOCTEXT("ModRFail",
						"MinR not met (need {0}, have {1})"),
					FText::AsNumber(Mod->MinR),
					FText::AsNumber(EffectiveR));
			}
			else if (!bRuleMatches)
			{
				ModRowEval.FailureReason = LOCTEXT("ModParentRuleFailed",
					"Parent rule did not match");
			}

			// When eligible, run the runtime conversion and collect human-readable output lines.
			if (ModRowEval.bModifierEligible)
			{
				ModRowEval.ResolvedOutputs =
					ArcCraftDebugEval::CollectApplyOutputs(ModStruct, AggregatedTags, Ctx, EffectiveR);
			}

			Result.ByRowKey.Add(
				ArcMaterialPropertyTable_DebugEval_Impl::ModifierRowKey(RuleIdx, ModIdx),
				ModRowEval);
		}
	}

	return Result;
}

} // namespace ArcMaterialPropertyTable_DebugEval

#undef LOCTEXT_NAMESPACE
