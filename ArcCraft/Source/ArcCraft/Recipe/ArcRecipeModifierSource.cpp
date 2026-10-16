/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
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

#include "ArcCraft/Recipe/ArcRecipeModifierSource.h"

#include "ArcCraft/MaterialCraft/ArcMaterialCraftContext.h"
#include "ArcCraft/MaterialCraft/ArcMaterialCraftEvaluator.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyRule.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTableData.h"
#include "ArcCraft/Recipe/ArcRandomPoolDefinitionData.h"
#include "ArcCraft/StatTransfer/ArcStatTransferTableData.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"
#include "ArcCraft/Recipe/ArcRandomPoolEvaluation.h"
#include "ArcCraft/Rarity/ArcCraftRarity.h"
#include "ArcCraft/Rarity/ArcCraftRulesData.h"
#include "ArcCraft/Shared/ArcCraftItemTags.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "ArcCraft/Shared/ArcItemFragment_StatRanges.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"
#include "ArcCraft/Static/ArcStaticModifierSetData.h"
#include "Items/ArcItemDefinitionData.h"

namespace ArcRecipeModifierSourceImpl
{

static FGameplayTagContainer AggregateIngredientTags(
	const TArray<FArcItemSpec>& ConsumedIngredients)
{
	FGameplayTagContainer AggregatedTags;
	for (const FArcItemSpec& Ingredient : ConsumedIngredients)
	{
		AggregatedTags.AppendTags(ArcCraftItemTags::GetCraftTags(Ingredient));
	}
	return AggregatedTags;
}

static TArray<FGameplayTagContainer> GetPerSlotAssetTags(
	const TArray<FArcItemSpec>& ConsumedIngredients)
{
	TArray<FGameplayTagContainer> PerSlotTags;
	PerSlotTags.SetNum(ConsumedIngredients.Num());
	for (int32 SlotIdx = 0; SlotIdx < ConsumedIngredients.Num(); ++SlotIdx)
	{
		PerSlotTags[SlotIdx] = ArcCraftItemTags::GetCraftTags(ConsumedIngredients[SlotIdx]);
	}
	return PerSlotTags;
}

struct FGroupProvenance
{
	int32 GroupIndex = INDEX_NONE;
	float Weight = 0.0f;
	FGameplayTagContainer MatchedTags;
	float IngredientShare = 0.0f;
};

/** Converts one modifier and gives every result the producing group's provenance. */
static void AppendConverted(
	const FInstancedStruct& ModStruct,
	const FGameplayTagContainer& AggregatedTags,
	const FArcCraftContext& Context,
	const FGroupProvenance& Group,
	TArray<FArcCraftPendingModifier>& OutPending)
{
	const int32 FirstNew = OutPending.Num();
	ArcCraftModifiers::ConvertModifier(ModStruct, AggregatedTags, Context, OutPending);
	for (int32 Idx = FirstNew; Idx < OutPending.Num(); ++Idx)
	{
		OutPending[Idx].EffectiveWeight = Group.Weight;
		OutPending[Idx].GroupIndex = Group.GroupIndex;
		OutPending[Idx].MatchedTags = Group.MatchedTags;
		OutPending[Idx].IngredientShare = Group.IngredientShare;
	}
}

} // namespace ArcRecipeModifierSourceImpl

FGameplayTagContainer ArcCraftProvenance::FilterHeldTags(
	const FGameplayTagContainer& RequiredTags,
	const FGameplayTagContainer& IngredientTags)
{
	FGameplayTagContainer HeldTags;
	for (const FGameplayTag& Tag : RequiredTags)
	{
		if (IngredientTags.HasTag(Tag))
		{
			HeldTags.AddTag(Tag);
		}
	}
	return HeldTags;
}

/** Consumed amount of the slots holding any RequiredTags over the total consumed amount; 0 without requirements. */
float ArcCraftProvenance::ComputeIngredientShare(
	const FGameplayTagContainer& RequiredTags,
	const TArray<FGameplayTagContainer>& PerSlotTags,
	const TArray<int32>& SlotAmounts)
{
	if (RequiredTags.IsEmpty())
	{
		return 0.0f;
	}

	int64 TotalAmount = 0;
	int64 MatchingAmount = 0;
	for (int32 SlotIdx = 0; SlotIdx < SlotAmounts.Num(); ++SlotIdx)
	{
		const int32 Amount = FMath::Max(0, SlotAmounts[SlotIdx]);
		TotalAmount += Amount;
		if (PerSlotTags.IsValidIndex(SlotIdx) && PerSlotTags[SlotIdx].HasAny(RequiredTags))
		{
			MatchingAmount += Amount;
		}
	}
	return TotalAmount > 0 ? static_cast<float>(MatchingAmount) / static_cast<float>(TotalAmount) : 0.0f;
}

// -------------------------------------------------------------------
// FArcRecipeModifierSource (base)
// -------------------------------------------------------------------

TArray<FArcCraftPendingModifier> FArcRecipeModifierSource::Evaluate(
	const TArray<FArcItemSpec>& ConsumedIngredients,
	const FArcCraftContext& Context) const
{
	// Base returns empty — concrete subtypes override.
	// Not pure virtual so UHT can instantiate the base for reflection purposes.
	return {};
}

// -------------------------------------------------------------------
// FArcRecipeModifierSource_RandomPool
// -------------------------------------------------------------------

TArray<FArcCraftPendingModifier> FArcRecipeModifierSource_RandomPool::Evaluate(
	const TArray<FArcItemSpec>& ConsumedIngredients,
	const FArcCraftContext& Context) const
{
	// Eligibility gate
	if (MinAverageR > 0.0f && Context.AverageR < MinAverageR)
	{
		return {};
	}

	const FArcRandomPoolDefinitionData* PoolDef = ArcCraftRecords::Find<FArcRandomPoolDefinitionData>(PoolDefinition);
	if (!PoolDef || PoolDef->Entries.Num() == 0)
	{
		return {};
	}

	const FArcRandomPoolSelectionMode* Mode = SelectionMode.GetPtr<FArcRandomPoolSelectionMode>();
	if (!Mode)
	{
		return {};
	}

	const FGameplayTagContainer AggregatedTags = ArcRecipeModifierSourceImpl::AggregateIngredientTags(ConsumedIngredients);

	const TArray<int32> SelectedIndices = ArcRandomPoolEval::SelectEntries(*PoolDef, *Mode, Context.AverageR, AggregatedTags, Context.Stream);
	const TArray<FGameplayTagContainer> PerSlotTags = ArcRecipeModifierSourceImpl::GetPerSlotAssetTags(ConsumedIngredients);

	TArray<FArcCraftPendingModifier> PendingResults;
	for (const int32 SelectedIdx : SelectedIndices)
	{
		if (!PoolDef->Entries.IsValidIndex(SelectedIdx))
		{
			continue;
		}

		const FArcRandomPoolEntry& Entry = PoolDef->Entries[SelectedIdx];
		ArcRecipeModifierSourceImpl::FGroupProvenance Group;
		Group.GroupIndex = SelectedIdx;
		Group.Weight = Weight;
		Group.MatchedTags = ArcCraftProvenance::FilterHeldTags(Entry.RequiredIngredientTags, AggregatedTags);
		Group.IngredientShare = ArcCraftProvenance::ComputeIngredientShare(Entry.RequiredIngredientTags, PerSlotTags, Context.SlotAmounts);

		for (const FInstancedStruct& ModStruct : Entry.Modifiers)
		{
			ArcRecipeModifierSourceImpl::AppendConverted(ModStruct, AggregatedTags, Context, Group, PendingResults);
		}
	}

	return PendingResults;
}

// -------------------------------------------------------------------
// FArcRecipeModifierSource_MaterialProperties
// -------------------------------------------------------------------

TArray<FArcCraftPendingModifier> FArcRecipeModifierSource_MaterialProperties::Evaluate(
	const TArray<FArcItemSpec>& ConsumedIngredients,
	const FArcCraftContext& Context) const
{
	// Eligibility gate
	if (MinAverageR > 0.0f && Context.AverageR < MinAverageR)
	{
		return {};
	}

	TArray<FArcCraftPendingModifier> Result;

	// 1. Load property table
	const FArcMaterialPropertyTableData* Table = ArcCraftRecords::Find<FArcMaterialPropertyTableData>(PropertyTable);
	if (!Table || Table->Rules.Num() == 0)
	{
		return Result;
	}

	// 2. Build material craft context
	FArcMaterialCraftContext MaterialContext = FArcMaterialCraftContext::Build(
		ConsumedIngredients,
		Context.SlotRaritySteps,
		Context.AverageR,
		BaseIngredientCount,
		ExtraCraftTimeBonus);

	// 3. Compute separated eligibility R and weight bonus
	FArcMaterialCraftEvaluator::ComputeEligibilityRAndWeightBonus(*Table, MaterialContext);

	// 4. Evaluate rules and select bands
	TArray<FArcMaterialRuleEvaluation> Evaluations = FArcMaterialCraftEvaluator::EvaluateRules(*Table, MaterialContext);

	if (Evaluations.Num() == 0)
	{
		return Result;
	}

	const FGameplayTagContainer AggregatedTags = ArcRecipeModifierSourceImpl::AggregateIngredientTags(ConsumedIngredients);

	// 5. Convert every eligible modifier of each matched rule; results carry the rule as their group.
	for (const FArcMaterialRuleEvaluation& Eval : Evaluations)
	{
		if (!Eval.Rule)
		{
			continue;
		}

		ArcRecipeModifierSourceImpl::FGroupProvenance Group;
		Group.GroupIndex = Eval.RuleIndex;
		Group.Weight = Eval.EffectiveWeight;
		Group.MatchedTags = Eval.MatchedTags;
		Group.IngredientShare = ArcCraftProvenance::ComputeIngredientShare(
			Eval.RequiredTags, FArcMaterialCraftEvaluator::GetRuleSlotTags(*Eval.Rule, MaterialContext), Context.SlotAmounts);

		for (const int32 ModIdx : Eval.EligibleModifierIndices)
		{
			ArcRecipeModifierSourceImpl::AppendConverted(Eval.Rule->Modifiers[ModIdx], AggregatedTags, Context, Group, Result);
		}
	}

	return Result;
}

// -------------------------------------------------------------------
// FArcRecipeModifierSource_Static
// -------------------------------------------------------------------

TArray<FArcCraftPendingModifier> FArcRecipeModifierSource_Static::Evaluate(
	const TArray<FArcItemSpec>& ConsumedIngredients,
	const FArcCraftContext& Context) const
{
	// Eligibility gate
	if (MinAverageR > 0.0f && Context.AverageR < MinAverageR)
	{
		return {};
	}

	const FArcStaticModifierSetData* Set = ArcCraftRecords::Find<FArcStaticModifierSetData>(ModifierSet);
	if (!Set || Set->Modifiers.Num() == 0)
	{
		return {};
	}

	const FGameplayTagContainer AggregatedTags = ArcRecipeModifierSourceImpl::AggregateIngredientTags(ConsumedIngredients);

	FGameplayTagContainer RequiredTags;
	for (const FInstancedStruct& ModStruct : Set->Modifiers)
	{
		if (const FArcCraftModifier* Mod = ModStruct.GetPtr<FArcCraftModifier>())
		{
			RequiredTags.AppendTags(Mod->TriggerTags);
		}
	}

	ArcRecipeModifierSourceImpl::FGroupProvenance Group;
	Group.GroupIndex = 0;
	Group.Weight = Weight;
	Group.MatchedTags = ArcCraftProvenance::FilterHeldTags(RequiredTags, AggregatedTags);
	Group.IngredientShare = ArcCraftProvenance::ComputeIngredientShare(
		RequiredTags, ArcRecipeModifierSourceImpl::GetPerSlotAssetTags(ConsumedIngredients), Context.SlotAmounts);

	TArray<FArcCraftPendingModifier> PendingResults;
	for (const FInstancedStruct& ModStruct : Set->Modifiers)
	{
		ArcRecipeModifierSourceImpl::AppendConverted(ModStruct, AggregatedTags, Context, Group, PendingResults);
	}

	return PendingResults;
}

// -------------------------------------------------------------------
// FArcRecipeModifierSource_StatTransferTable
// -------------------------------------------------------------------

TArray<FArcCraftPendingModifier> FArcRecipeModifierSource_StatTransferTable::Evaluate(
	const TArray<FArcItemSpec>& ConsumedIngredients,
	const FArcCraftContext& Context) const
{
	// Eligibility gate (inherited MinAverageR)
	if (MinAverageR > 0.0f && Context.AverageR < MinAverageR)
	{
		return {};
	}

	const FArcStatTransferTableData* TablePtr = ArcCraftRecords::Find<FArcStatTransferTableData>(Table);
	if (!TablePtr)
	{
		return {};
	}

	const FArcCraftRulesData* Rules = ArcCraftRarity::GetRules();

	TArray<FArcCraftPendingModifier> PendingResults;

	for (const FArcStatTransferEntry& Entry : TablePtr->Entries)
	{
		if (Entry.IngredientQuery.IsEmpty()) { continue; }   // inert row

		const bool bBandExempt = Rules == nullptr || Rules->RarityBandExempt.Contains(Entry.Stat);
		const FArcCraftRulesData* BandRules = bBandExempt ? nullptr : Rules;
		const float BiasPerSkillMargin = bBandExempt ? 0.0f : Rules->BandBiasPerSkillMargin;

		const int32 FirstSlot = Entry.SlotIndex >= 0 ? Entry.SlotIndex : 0;
		const int32 EndSlot = Entry.SlotIndex >= 0 ? FMath::Min(Entry.SlotIndex + 1, ConsumedIngredients.Num()) : ConsumedIngredients.Num();

		for (int32 SlotIdx = FirstSlot; SlotIdx < EndSlot; ++SlotIdx)
		{
			const FArcItemDefinitionData* Def = ConsumedIngredients[SlotIdx].GetItemDefinition();
			if (!Def) { continue; }

			if (!Entry.IngredientQuery.Matches(ArcCraftItemTags::GetCraftTags(ConsumedIngredients[SlotIdx]))) { continue; }

			const FArcItemFragment_ItemStatRanges* RangesFrag = Def->FindFragment<FArcItemFragment_ItemStatRanges>();
			if (!RangesFrag) { continue; }
			const FArcItemAttributeStatRange* Range = RangesFrag->FindRange(Entry.Stat);
			if (!Range || !Range->IsValid()) { continue; }

			const float Step   = Context.SlotRaritySteps.IsValidIndex(SlotIdx) ? Context.SlotRaritySteps[SlotIdx] : 1.0f;
			const float Rolled = ArcCraftRarity::ComputeBandValue(
				Range->Min,
				Range->Max,
				BandRules,
				Context.OutputRarity,
				Step,
				Entry.RBias,
				BiasPerSkillMargin,
				Context.SkillMargin,
				Context.Stream.FRand());

			const float Contribution = Rolled * Entry.Share;

			FArcCraftPendingModifier* Summed = PendingResults.FindByPredicate([&Entry](const FArcCraftPendingModifier& Existing)
			{
				return Existing.Result.Stat.Stat == Entry.Stat && Existing.Result.Stat.Type == Entry.OutputModType;
			});
			if (Summed)
			{
				Summed->Result.Stat.Value.Value += Contribution;
				continue;
			}

			FArcCraftModifierResult ModResult;
			ModResult.Type      = EArcCraftModifierResultType::Stat;
			ModResult.Stat      = FArcItemAttributeStat(Entry.Stat, Contribution);
			ModResult.Stat.Type = Entry.OutputModType;

			FArcCraftPendingModifier Pending;
			Pending.ModifierIndex   = INDEX_NONE;
			Pending.Result          = MoveTemp(ModResult);
			PendingResults.Add(MoveTemp(Pending));
		}
	}

	return PendingResults;
}
