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

#include "RandomPool/ArcRandomPool_DebugEval.h"

#include "ArcCraft/Recipe/ArcRandomPoolDefinitionData.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"
#include "ArcCraft/Recipe/ArcRandomPoolEvaluation.h"
#include "ArcCraft/Recipe/ArcRecipeModifierSource.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "StructUtils/InstancedStruct.h"
#include "Math/RandomStream.h"
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

#define LOCTEXT_NAMESPACE "ArcRandomPool_DebugEval"

namespace ArcRandomPool_DebugEval_Impl
{

/** Encoded row key for an Entry-level row. */
static int32 EntryRowKey(int32 EntryIdx)
{
	return EntryIdx;
}

/** Encoded row key for a Modifier child row. */
static int32 ModifierRowKey(int32 EntryIdx, int32 ModIdx)
{
	return EntryIdx * 10000 + ModIdx;
}

} // namespace ArcRandomPool_DebugEval_Impl

namespace ArcRandomPool_DebugEval
{

FArcModifierDebugEvaluation EvaluateRandomPoolForDebug(
	const FArcRandomPoolDefinitionData& PoolDef,
	const FArcModifierDebugContext& Ctx)
{
	FArcModifierDebugEvaluation Result;

	// Derive AverageR and AggregatedTags from InputSlots when present;
	// fall back to legacy Ctx.AverageR / IngredientTags otherwise.
	float AverageR = 0.0f;
	FGameplayTagContainer AggregatedTags;
	if (!Ctx.InputSlots.IsEmpty())
	{
		AverageR = ArcCraftDebugEval::ComputeAverageR(Ctx.InputSlots);
		AggregatedTags = ArcCraftDebugEval::AggregateSlotTags(Ctx.InputSlots);
	}
	else
	{
		AverageR = Ctx.AverageR;
		AggregatedTags = Ctx.IngredientTags;
	}

	TArray<FGameplayTagContainer> PerSlotTags;
	if (!Ctx.InputSlots.IsEmpty())
	{
		for (const FArcDebugIngredientSlot& Slot : Ctx.InputSlots)
		{
			PerSlotTags.Add(Slot.Tags);
		}
	}
	else
	{
		PerSlotTags.Add(AggregatedTags);
	}
	TArray<int32> SlotAmounts;
	SlotAmounts.Init(1, PerSlotTags.Num());

	// Use the shared runtime evaluation helper (bBuildFailureReason = true for editor)
	TArray<FArcRandomPoolEntryEvalResult> EntryEvals = ArcRandomPoolEval::ComputeEntryEvaluations(
		PoolDef, AverageR, AggregatedTags, /*bBuildFailureReason=*/true);

	for (int32 EntryIdx = 0; EntryIdx < EntryEvals.Num(); ++EntryIdx)
	{
		const FArcRandomPoolEntryEvalResult& EntryEval = EntryEvals[EntryIdx];

		// --- Entry-level row ---
		FArcModifierRowEvaluation EntryRowEval;
		EntryRowEval.bRulePassed = EntryEval.bEligible;
		EntryRowEval.bModifierEligible = EntryEval.bEligible;
		EntryRowEval.EffectiveWeight = EntryEval.EffectiveWeight;
		EntryRowEval.FailureReason = EntryEval.FailureReason;

		if (PoolDef.Entries.IsValidIndex(EntryIdx))
		{
			const FGameplayTagContainer& RequiredTags = PoolDef.Entries[EntryIdx].RequiredIngredientTags;
			EntryRowEval.MatchedTags = ArcCraftProvenance::FilterHeldTags(RequiredTags, AggregatedTags);
			EntryRowEval.IngredientShare = ArcCraftProvenance::ComputeIngredientShare(RequiredTags, PerSlotTags, SlotAmounts);
		}

		Result.ByRowKey.Add(
			ArcRandomPool_DebugEval_Impl::EntryRowKey(EntryIdx),
			EntryRowEval);

		// --- Modifier child rows ---
		if (!PoolDef.Entries.IsValidIndex(EntryIdx))
		{
			continue;
		}

		const FArcRandomPoolEntry& Entry = PoolDef.Entries[EntryIdx];

		for (int32 ModIdx = 0; ModIdx < Entry.Modifiers.Num(); ++ModIdx)
		{
			const FInstancedStruct& ModStruct = Entry.Modifiers[ModIdx];
			const FArcCraftModifier* Mod = ModStruct.GetPtr<FArcCraftModifier>();

			FArcModifierRowEvaluation ModRowEval;

			if (Mod == nullptr)
			{
				ModRowEval.bRulePassed = false;
				ModRowEval.bModifierEligible = false;
				ModRowEval.FailureReason = LOCTEXT("NullModifier", "Modifier not set");
				Result.ByRowKey.Add(
					ArcRandomPool_DebugEval_Impl::ModifierRowKey(EntryIdx, ModIdx),
					ModRowEval);
				continue;
			}

			// TriggerTags checked against aggregated ingredient tags.
			const bool bTagsMatch = Mod->MatchesTriggerTags(AggregatedTags);
			ModRowEval.bRulePassed = bTagsMatch;

			const bool bRMet = AverageR >= Mod->MinR;
			ModRowEval.bModifierEligible = EntryEval.bEligible && bTagsMatch && bRMet;
			ArcCraftDebugEval::FillStatBand(ModStruct, Ctx, AverageR, ModRowEval);

			if (!bTagsMatch)
			{
				ModRowEval.FailureReason = FText::Format(
					LOCTEXT("ModTagsFail",
						"Failed: TriggerTags not satisfied (need {0})"),
					FText::FromString(Mod->TriggerTags.ToString()));
			}
			else if (!bRMet)
			{
				ModRowEval.FailureReason = FText::Format(
					LOCTEXT("ModRFail",
						"Failed: MinR not met (need {0}, have {1})"),
					FText::AsNumber(Mod->MinR),
					FText::AsNumber(AverageR));
			}
			else if (!EntryEval.bEligible)
			{
				ModRowEval.FailureReason = LOCTEXT("ModParentIneligible",
					"Parent entry is ineligible");
			}

			// When eligible, run the runtime conversion and collect human-readable output lines.
			if (ModRowEval.bModifierEligible)
			{
				ModRowEval.ResolvedOutputs =
					ArcCraftDebugEval::CollectApplyOutputs(ModStruct, AggregatedTags, Ctx, AverageR);
			}

			Result.ByRowKey.Add(
				ArcRandomPool_DebugEval_Impl::ModifierRowKey(EntryIdx, ModIdx),
				ModRowEval);
		}
	}

	return Result;
}

TArray<int32> RollOnce(
	const FArcRandomPoolDefinitionData& PoolDef,
	const FArcModifierDebugContext& Ctx,
	const FInstancedStruct& DebugSelectionMode)
{
	if (PoolDef.Entries.Num() == 0)
	{
		return {};
	}

	// Derive R and tags the same way EvaluateRandomPoolForDebug does.
	float AverageR = 0.0f;
	FGameplayTagContainer AggregatedTags;
	if (!Ctx.InputSlots.IsEmpty())
	{
		AverageR = ArcCraftDebugEval::ComputeAverageR(Ctx.InputSlots);
		AggregatedTags = ArcCraftDebugEval::AggregateSlotTags(Ctx.InputSlots);
	}
	else
	{
		AverageR = Ctx.AverageR;
		AggregatedTags = Ctx.IngredientTags;
	}

	TArray<FArcRandomPoolEntryEvalResult> EntryEvals = ArcRandomPoolEval::ComputeEntryEvaluations(
		PoolDef, AverageR, AggregatedTags, /*bBuildFailureReason=*/false);

	TArray<int32> EligibleIndices;
	TArray<float> EffectiveWeights;
	ArcRandomPoolEval::CollectEligibleSets(EntryEvals, EligibleIndices, EffectiveWeights);

	if (EligibleIndices.Num() == 0)
	{
		return {};
	}

	const FRandomStream Stream(FMath::Rand32());
	FArcRandomPoolSelectionContext SelectionCtx
	{
		PoolDef.Entries,
		EligibleIndices,
		EffectiveWeights,
		AverageR,
		Stream
	};

	// Resolve the selection mode: use caller-provided one if valid, else fallback
	const FArcRandomPoolSelectionMode* Mode =
		DebugSelectionMode.GetPtr<FArcRandomPoolSelectionMode>();

	TArray<int32> SelectedIndices;
	if (Mode != nullptr)
	{
		Mode->Select(SelectionCtx, SelectedIndices);
	}
	else
	{
		// Fallback: SimpleRandom with MaxSelections=1
		FArcRandomPoolSelection_SimpleRandom DefaultMode;
		DefaultMode.MaxSelections = 1;
		DefaultMode.Select(SelectionCtx, SelectedIndices);
	}

	return SelectedIndices;
}

} // namespace ArcRandomPool_DebugEval

#undef LOCTEXT_NAMESPACE
