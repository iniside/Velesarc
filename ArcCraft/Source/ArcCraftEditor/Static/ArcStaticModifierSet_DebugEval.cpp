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

#include "Static/ArcStaticModifierSet_DebugEval.h"

#include "ArcCraft/Static/ArcStaticModifierSetData.h"
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

#define LOCTEXT_NAMESPACE "ArcStaticModifierSet_DebugEval"

FArcModifierDebugEvaluation EvaluateStaticModifierSetForDebug(
	const FArcStaticModifierSetData& ModifierSet,
	const FArcModifierDebugContext& DebugContext)
{
	FArcModifierDebugEvaluation Result;

	// Derive AverageR and AggregatedTags from InputSlots when present;
	// fall back to legacy R / (IngredientTags + SlotTags) otherwise.
	float AverageR = 0.0f;
	FGameplayTagContainer AggregatedTags;
	if (!DebugContext.InputSlots.IsEmpty())
	{
		AverageR = ArcCraftDebugEval::ComputeAverageR(DebugContext.InputSlots);
		AggregatedTags = ArcCraftDebugEval::AggregateSlotTags(DebugContext.InputSlots);
	}
	else
	{
		AverageR = DebugContext.AverageR;
		AggregatedTags.AppendTags(DebugContext.IngredientTags);
		AggregatedTags.AppendTags(DebugContext.SlotTags);
	}

	for (int32 i = 0; i < ModifierSet.Modifiers.Num(); ++i)
	{
		const FInstancedStruct& ModStruct = ModifierSet.Modifiers[i];
		const FArcCraftModifier* Mod = ModStruct.GetPtr<FArcCraftModifier>();

		FArcModifierRowEvaluation RowEval;

		if (Mod == nullptr)
		{
			RowEval.bRulePassed = false;
			RowEval.bModifierEligible = false;
			RowEval.FailureReason = LOCTEXT("NullModifier", "Modifier not set");
			Result.ByRowKey.Add(i, RowEval);
			continue;
		}

		// TriggerTags check: empty = always match; otherwise HasAll.
		const bool bTagsMatch = Mod->MatchesTriggerTags(AggregatedTags);
		RowEval.bRulePassed = bTagsMatch;

		// MinR check against effective R.
		const bool bRMet = AverageR >= Mod->MinR;
		RowEval.bModifierEligible = bTagsMatch && bRMet;
		ArcCraftDebugEval::FillStatBand(ModStruct, DebugContext, AverageR, RowEval);

		if (!bTagsMatch)
		{
			RowEval.FailureReason = FText::Format(
				LOCTEXT("TagsFail", "Failed: TriggerTags not satisfied (need {0})"),
				FText::FromString(Mod->TriggerTags.ToString()));
		}
		else if (!bRMet)
		{
			RowEval.FailureReason = FText::Format(
				LOCTEXT("RFail", "Failed: MinR not met (need {0:.2f}, have {1:.2f})"),
				Mod->MinR,
				AverageR);
		}

		// When eligible, run the runtime conversion and collect human-readable output lines.
		if (RowEval.bModifierEligible)
		{
			RowEval.ResolvedOutputs =
				ArcCraftDebugEval::CollectApplyOutputs(ModStruct, AggregatedTags, DebugContext, AverageR);
		}

		Result.ByRowKey.Add(i, RowEval);
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
