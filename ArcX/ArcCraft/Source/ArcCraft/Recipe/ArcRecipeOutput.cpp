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

#include "ArcCraft/Recipe/ArcRecipeOutput.h"

#include "Chooser.h"
#include "ChooserFunctionLibrary.h"
#include "GameplayEffect.h"
#include "IObjectChooser.h"
#include "Abilities/GameplayAbility.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Items/Fragments/ArcItemFragment_GrantedGameplayEffects.h"
#include "ArcCraft/Recipe/ArcCraftSlotResolver.h"
#include "ArcCraft/Recipe/ArcRecipeCraftContext.h"
#include "ArcCraft/Recipe/ArcRandomModifierEntry.h"
#include "ArcCraft/Recipe/ArcRandomPoolDefinition.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"

// -------------------------------------------------------------------
// Shared helpers
// -------------------------------------------------------------------

static bool HasTriggerTagsInIngredients(
	const FGameplayTagContainer& TriggerTags,
	const TArray<FArcItemSpec>& ConsumedIngredients)
{
	for (const FArcItemSpec& Ingredient : ConsumedIngredients)
	{
		const UArcItemDefinition* Def = Ingredient.GetItemDefinition();
		if (!Def)
		{
			continue;
		}

		const FArcItemFragment_Tags* TagsFrag = Def->FindFragment<FArcItemFragment_Tags>();
		if (TagsFrag && TagsFrag->AssetTags.HasAll(TriggerTags))
		{
			return true;
		}
	}
	return false;
}

static FGameplayTagContainer AggregateIngredientTags(
	const TArray<FArcItemSpec>& ConsumedIngredients)
{
	FGameplayTagContainer AggregatedTags;
	for (const FArcItemSpec& Ingredient : ConsumedIngredients)
	{
		const UArcItemDefinition* Def = Ingredient.GetItemDefinition();
		if (!Def)
		{
			continue;
		}

		const FArcItemFragment_Tags* TagsFrag = Def->FindFragment<FArcItemFragment_Tags>();
		if (TagsFrag)
		{
			AggregatedTags.AppendTags(TagsFrag->AssetTags);
		}
	}
	return AggregatedTags;
}

/**
 * Convert an array of FArcCraftModifier sub-types into flat FArcCraftModifierResult entries.
 * Handles Stats, Abilities, Effects, and nested RandomPool modifiers.
 */
static TArray<FArcCraftModifierResult> CollectResults(
	const TArray<FInstancedStruct>& Modifiers,
	const FGameplayTagContainer& AggregatedTags,
	float EffectiveQuality)
{
	TArray<FArcCraftModifierResult> Results;

	for (const FInstancedStruct& ModStruct : Modifiers)
	{
		const FArcCraftModifier* BaseMod = ModStruct.GetPtr<FArcCraftModifier>();
		if (!BaseMod)
		{
			continue;
		}

		// Eligibility checks (quality gate + trigger tags)
		if (BaseMod->MinQualityThreshold > 0.0f && EffectiveQuality < BaseMod->MinQualityThreshold)
		{
			continue;
		}
		if (!BaseMod->MatchesTriggerTags(AggregatedTags))
		{
			continue;
		}

		// Stats
		if (const FArcCraftModifier_Stats* StatMod = ModStruct.GetPtr<FArcCraftModifier_Stats>())
		{
			const float QualityScale = 1.0f + (EffectiveQuality - 1.0f) * StatMod->QualityScalingFactor;
			FArcCraftModifierResult R;
			R.Type = EArcCraftModifierResultType::Stat;
			R.Stat = StatMod->BaseStat;
			R.Stat.SetValue(StatMod->BaseStat.Value.GetValue() * QualityScale);
			Results.Add(MoveTemp(R));
			continue;
		}

		// Abilities
		if (const FArcCraftModifier_Abilities* AbilMod = ModStruct.GetPtr<FArcCraftModifier_Abilities>())
		{
			FArcCraftModifierResult R;
			R.Type = EArcCraftModifierResultType::Ability;
			R.Ability = AbilMod->AbilityToGrant;
			Results.Add(MoveTemp(R));
			continue;
		}

		// Effects
		if (const FArcCraftModifier_Effects* EffMod = ModStruct.GetPtr<FArcCraftModifier_Effects>())
		{
			FArcCraftModifierResult R;
			R.Type = EArcCraftModifierResultType::Effect;
			R.Effect = EffMod->EffectToGrant;
			Results.Add(MoveTemp(R));
			continue;
		}

		// RandomPool (nested — flatten recursively via the pool's selection)
		if (const FArcCraftModifier_RandomPool* PoolMod = ModStruct.GetPtr<FArcCraftModifier_RandomPool>())
		{
			UArcRandomPoolDefinition* PoolDef = PoolMod->PoolDefinition.LoadSynchronous();
			if (!PoolDef || PoolDef->Entries.Num() == 0)
			{
				continue;
			}

			const FArcRandomPoolSelectionMode* Mode = PoolMod->SelectionMode.GetPtr<FArcRandomPoolSelectionMode>();
			if (!Mode)
			{
				continue;
			}

			// Filter eligible entries and compute weights
			TArray<int32> EligibleIndices;
			TArray<float> EffectiveWeights;
			EligibleIndices.Reserve(PoolDef->Entries.Num());
			EffectiveWeights.Reserve(PoolDef->Entries.Num());

			for (int32 Idx = 0; Idx < PoolDef->Entries.Num(); ++Idx)
			{
				const FArcRandomPoolEntry& Entry = PoolDef->Entries[Idx];

				if (Entry.MinQualityThreshold > 0.0f && EffectiveQuality < Entry.MinQualityThreshold)
				{
					continue;
				}
				if (!Entry.RequiredIngredientTags.IsEmpty() &&
					!AggregatedTags.HasAll(Entry.RequiredIngredientTags))
				{
					continue;
				}
				if (!Entry.DenyIngredientTags.IsEmpty() &&
					AggregatedTags.HasAny(Entry.DenyIngredientTags))
				{
					continue;
				}

				float EntryWeight = Entry.BaseWeight;
				float Multiplier = 1.0f;
				for (const FArcRandomPoolWeightModifier& WeightMod : Entry.WeightModifiers)
				{
					if (!WeightMod.RequiredTags.IsEmpty() &&
						AggregatedTags.HasAll(WeightMod.RequiredTags))
					{
						EntryWeight += WeightMod.BonusWeight;
						Multiplier *= WeightMod.WeightMultiplier;
					}
				}
				EntryWeight *= Multiplier;
				EntryWeight *= (1.0f + (EffectiveQuality - 1.0f) * Entry.QualityWeightScaling);
				EntryWeight = FMath::Max(EntryWeight, 0.0f);

				EligibleIndices.Add(Idx);
				EffectiveWeights.Add(EntryWeight);
			}

			if (EligibleIndices.Num() == 0)
			{
				continue;
			}

			FArcRandomPoolSelectionContext SelectionContext
			{
				PoolDef->Entries,
				EligibleIndices,
				EffectiveWeights,
				EffectiveQuality
			};

			TArray<int32> SelectedIndices;
			Mode->Select(SelectionContext, SelectedIndices);

			for (const int32 SelectedIdx : SelectedIndices)
			{
				if (!PoolDef->Entries.IsValidIndex(SelectedIdx))
				{
					continue;
				}
				const FArcRandomPoolEntry& Entry = PoolDef->Entries[SelectedIdx];

				float EntryEffQ;
				if (Entry.bScaleByQuality)
				{
					EntryEffQ = EffectiveQuality * Entry.ValueScale + Entry.ValueSkew;
				}
				else
				{
					EntryEffQ = Entry.ValueScale + Entry.ValueSkew;
				}
				for (const FArcRandomPoolValueSkew& SkewRule : Entry.ValueSkewRules)
				{
					if (!SkewRule.RequiredTags.IsEmpty() &&
						AggregatedTags.HasAll(SkewRule.RequiredTags))
					{
						EntryEffQ = EntryEffQ * SkewRule.ValueScale + SkewRule.ValueOffset;
					}
				}

				// Recurse into the entry's terminal modifiers
				Results.Append(CollectResults(Entry.Modifiers, AggregatedTags, EntryEffQ));
			}
			continue;
		}
	}

	return Results;
}

// -------------------------------------------------------------------
// FArcRecipeOutputModifier (base)
// -------------------------------------------------------------------

bool FArcRecipeOutputModifier::IsEligible(
	const TArray<FArcItemSpec>& ConsumedIngredients,
	float AverageQuality) const
{
	// Quality gate
	if (MinQualityThreshold > 0.0f && AverageQuality < MinQualityThreshold)
	{
		return false;
	}

	// Trigger tag gate
	if (!TriggerTags.IsEmpty())
	{
		if (!HasTriggerTagsInIngredients(TriggerTags, ConsumedIngredients))
		{
			return false;
		}
	}

	return true;
}

TArray<FArcCraftPendingModifier> FArcRecipeOutputModifier::Evaluate(
	const TArray<FArcItemSpec>& ConsumedIngredients,
	const TArray<float>& IngredientQualityMults,
	float AverageQuality) const
{
	// Base returns empty — concrete subtypes override
	return {};
}

// -------------------------------------------------------------------
// FArcRecipeOutputModifier_Stats
// -------------------------------------------------------------------

TArray<FArcCraftPendingModifier> FArcRecipeOutputModifier_Stats::Evaluate(
	const TArray<FArcItemSpec>& ConsumedIngredients,
	const TArray<float>& IngredientQualityMults,
	float AverageQuality) const
{
	if (!IsEligible(ConsumedIngredients, AverageQuality))
	{
		return {};
	}

	const float QualityScale = 1.0f + (AverageQuality - 1.0f) * QualityScalingFactor;
	const float EffectiveWeight = Weight * QualityScale;

	FArcCraftPendingModifier Pending;
	Pending.SlotTag = SlotTag;
	Pending.EffectiveWeight = EffectiveWeight;

	Pending.Result.Type = EArcCraftModifierResultType::Stat;
	Pending.Result.Stat = BaseStat;
	Pending.Result.Stat.SetValue(BaseStat.Value.GetValue() * QualityScale);

	return { MoveTemp(Pending) };
}

// -------------------------------------------------------------------
// FArcRecipeOutputModifier_Abilities
// -------------------------------------------------------------------

TArray<FArcCraftPendingModifier> FArcRecipeOutputModifier_Abilities::Evaluate(
	const TArray<FArcItemSpec>& ConsumedIngredients,
	const TArray<float>& IngredientQualityMults,
	float AverageQuality) const
{
	if (!IsEligible(ConsumedIngredients, AverageQuality))
	{
		return {};
	}

	const float QualityScale = 1.0f + (AverageQuality - 1.0f) * QualityScalingFactor;
	const float EffectiveWeight = Weight * QualityScale;

	FArcCraftPendingModifier Pending;
	Pending.SlotTag = SlotTag;
	Pending.EffectiveWeight = EffectiveWeight;

	Pending.Result.Type = EArcCraftModifierResultType::Ability;
	Pending.Result.Ability = AbilityToGrant;

	return { MoveTemp(Pending) };
}

// -------------------------------------------------------------------
// FArcRecipeOutputModifier_Effects
// -------------------------------------------------------------------

TArray<FArcCraftPendingModifier> FArcRecipeOutputModifier_Effects::Evaluate(
	const TArray<FArcItemSpec>& ConsumedIngredients,
	const TArray<float>& IngredientQualityMults,
	float AverageQuality) const
{
	if (!IsEligible(ConsumedIngredients, AverageQuality))
	{
		return {};
	}

	const float QualityScale = 1.0f + (AverageQuality - 1.0f) * QualityScalingFactor;
	const float EffectiveWeight = Weight * QualityScale;

	FArcCraftPendingModifier Pending;
	Pending.SlotTag = SlotTag;
	Pending.EffectiveWeight = EffectiveWeight;

	Pending.Result.Type = EArcCraftModifierResultType::Effect;
	Pending.Result.Effect = EffectToGrant;

	return { MoveTemp(Pending) };
}

// -------------------------------------------------------------------
// FArcRecipeOutputModifier_TransferStats
// -------------------------------------------------------------------

TArray<FArcCraftPendingModifier> FArcRecipeOutputModifier_TransferStats::Evaluate(
	const TArray<FArcItemSpec>& ConsumedIngredients,
	const TArray<float>& IngredientQualityMults,
	float AverageQuality) const
{
	if (!IsEligible(ConsumedIngredients, AverageQuality))
	{
		return {};
	}

	// Validate ingredient slot index and definition
	if (!ConsumedIngredients.IsValidIndex(IngredientSlotIndex) ||
		!ConsumedIngredients[IngredientSlotIndex].GetItemDefinitionId().IsValid())
	{
		return {};
	}

	const UArcItemDefinition* Def = ConsumedIngredients[IngredientSlotIndex].GetItemDefinition();
	const FArcItemFragment_ItemStats* IngredientStats = Def ? Def->FindFragment<FArcItemFragment_ItemStats>() : nullptr;
	if (!IngredientStats || IngredientStats->DefaultStats.Num() == 0)
	{
		return {};
	}

	const float QualityScale = 1.0f + (AverageQuality - 1.0f) * QualityScalingFactor;
	const float EffectiveWeight = Weight * QualityScale;

	// Quality multiplier for the specific ingredient slot
	const float IngredientQuality = IngredientQualityMults.IsValidIndex(IngredientSlotIndex)
		? IngredientQualityMults[IngredientSlotIndex]
		: 1.0f;

	TArray<FArcCraftPendingModifier> Results;
	Results.Reserve(IngredientStats->DefaultStats.Num());

	for (const FArcItemAttributeStat& IngredientStat : IngredientStats->DefaultStats)
	{
		float ScaledValue = IngredientStat.Value.GetValue() * TransferScale;
		if (bScaleByQuality)
		{
			ScaledValue *= IngredientQuality;
		}

		FArcCraftPendingModifier Pending;
		Pending.SlotTag = SlotTag;
		Pending.EffectiveWeight = EffectiveWeight;

		Pending.Result.Type = EArcCraftModifierResultType::Stat;
		Pending.Result.Stat = IngredientStat;
		Pending.Result.Stat.SetValue(ScaledValue);

		Results.Add(MoveTemp(Pending));
	}

	return Results;
}

// -------------------------------------------------------------------
// FArcRecipeOutputModifier_Random (Chooser-based)
// -------------------------------------------------------------------

TArray<FArcCraftPendingModifier> FArcRecipeOutputModifier_Random::Evaluate(
	const TArray<FArcItemSpec>& ConsumedIngredients,
	const TArray<float>& IngredientQualityMults,
	float AverageQuality) const
{
	if (!IsEligible(ConsumedIngredients, AverageQuality))
	{
		return {};
	}

	UChooserTable* ChooserTable = ModifierChooserTable.LoadSynchronous();
	if (!ChooserTable)
	{
		return {};
	}

	const float QualityScale = 1.0f + (AverageQuality - 1.0f) * QualityScalingFactor;
	const float EffectiveWeight = Weight * QualityScale;

	// Build context for chooser evaluation
	FArcRecipeCraftContext CraftContext;
	CraftContext.IngredientTags = AggregateIngredientTags(ConsumedIngredients);
	CraftContext.AverageQuality = AverageQuality;
	CraftContext.IngredientCount = ConsumedIngredients.Num();
	for (const float& QM : IngredientQualityMults)
	{
		CraftContext.TotalQuality += QM;
	}

	// Compute effective number of rolls
	int32 EffectiveRolls = MaxRolls;
	if (bQualityAffectsRolls && QualityBonusRollThreshold > 0.0f)
	{
		const int32 BonusRolls = FMath::FloorToInt32(AverageQuality / QualityBonusRollThreshold);
		EffectiveRolls += BonusRolls;
	}

	// Evaluate the chooser table
	FChooserEvaluationContext EvalContext;
	EvalContext.AddStructParam(CraftContext);

	TArray<FArcCraftPendingModifier> PendingResults;
	TSet<UArcRandomModifierEntry*> AlreadySelected;

	for (int32 Roll = 0; Roll < EffectiveRolls; ++Roll)
	{
		UObject* SelectedObject = UChooserFunctionLibrary::EvaluateChooser(
			nullptr, ChooserTable, UArcRandomModifierEntry::StaticClass());

		UArcRandomModifierEntry* ModEntry = Cast<UArcRandomModifierEntry>(SelectedObject);
		if (!ModEntry)
		{
			continue;
		}

		// Duplicate check
		if (!bAllowDuplicates)
		{
			if (AlreadySelected.Contains(ModEntry))
			{
				continue;
			}
			AlreadySelected.Add(ModEntry);
		}

		// Compute effective quality for this entry
		float EntryEffQ;
		if (ModEntry->bScaleByQuality)
		{
			EntryEffQ = AverageQuality * ModEntry->ValueScale + ModEntry->ValueSkew;
		}
		else
		{
			EntryEffQ = ModEntry->ValueScale + ModEntry->ValueSkew;
		}

		// Convert sub-modifiers to results
		TArray<FArcCraftModifierResult> SubResults = CollectResults(
			ModEntry->Modifiers, CraftContext.IngredientTags, EntryEffQ);

		for (FArcCraftModifierResult& SubResult : SubResults)
		{
			FArcCraftPendingModifier Pending;
			Pending.SlotTag = SlotTag;
			Pending.EffectiveWeight = EffectiveWeight;
			Pending.Result = MoveTemp(SubResult);
			PendingResults.Add(MoveTemp(Pending));
		}
	}

	return PendingResults;
}

// -------------------------------------------------------------------
// FArcRecipeOutputModifier_RandomPool
// -------------------------------------------------------------------

TArray<FArcCraftPendingModifier> FArcRecipeOutputModifier_RandomPool::Evaluate(
	const TArray<FArcItemSpec>& ConsumedIngredients,
	const TArray<float>& IngredientQualityMults,
	float AverageQuality) const
{
	if (!IsEligible(ConsumedIngredients, AverageQuality))
	{
		return {};
	}

	UArcRandomPoolDefinition* PoolDef = PoolDefinition.LoadSynchronous();
	if (!PoolDef || PoolDef->Entries.Num() == 0)
	{
		return {};
	}

	const FArcRandomPoolSelectionMode* Mode = SelectionMode.GetPtr<FArcRandomPoolSelectionMode>();
	if (!Mode)
	{
		return {};
	}

	const float QualityScale = 1.0f + (AverageQuality - 1.0f) * QualityScalingFactor;
	const float EffectiveWeight = Weight * QualityScale;

	FGameplayTagContainer AggregatedTags = AggregateIngredientTags(ConsumedIngredients);

	// Filter eligible entries and compute weights
	TArray<int32> EligibleIndices;
	TArray<float> EffectiveWeights;
	EligibleIndices.Reserve(PoolDef->Entries.Num());
	EffectiveWeights.Reserve(PoolDef->Entries.Num());

	for (int32 Idx = 0; Idx < PoolDef->Entries.Num(); ++Idx)
	{
		const FArcRandomPoolEntry& Entry = PoolDef->Entries[Idx];

		if (Entry.MinQualityThreshold > 0.0f && AverageQuality < Entry.MinQualityThreshold)
		{
			continue;
		}
		if (!Entry.RequiredIngredientTags.IsEmpty() &&
			!AggregatedTags.HasAll(Entry.RequiredIngredientTags))
		{
			continue;
		}
		if (!Entry.DenyIngredientTags.IsEmpty() &&
			AggregatedTags.HasAny(Entry.DenyIngredientTags))
		{
			continue;
		}

		float EntryWeight = Entry.BaseWeight;
		float Multiplier = 1.0f;
		for (const FArcRandomPoolWeightModifier& WeightMod : Entry.WeightModifiers)
		{
			if (!WeightMod.RequiredTags.IsEmpty() &&
				AggregatedTags.HasAll(WeightMod.RequiredTags))
			{
				EntryWeight += WeightMod.BonusWeight;
				Multiplier *= WeightMod.WeightMultiplier;
			}
		}
		EntryWeight *= Multiplier;
		EntryWeight *= (1.0f + (AverageQuality - 1.0f) * Entry.QualityWeightScaling);
		EntryWeight = FMath::Max(EntryWeight, 0.0f);

		EligibleIndices.Add(Idx);
		EffectiveWeights.Add(EntryWeight);
	}

	if (EligibleIndices.Num() == 0)
	{
		return {};
	}

	FArcRandomPoolSelectionContext SelectionContext
	{
		PoolDef->Entries,
		EligibleIndices,
		EffectiveWeights,
		AverageQuality
	};

	TArray<int32> SelectedIndices;
	Mode->Select(SelectionContext, SelectedIndices);

	TArray<FArcCraftPendingModifier> PendingResults;

	for (const int32 SelectedIdx : SelectedIndices)
	{
		if (!PoolDef->Entries.IsValidIndex(SelectedIdx))
		{
			continue;
		}

		const FArcRandomPoolEntry& Entry = PoolDef->Entries[SelectedIdx];

		// Compute effective quality for this entry's sub-modifiers
		float EntryEffQ;
		if (Entry.bScaleByQuality)
		{
			EntryEffQ = AverageQuality * Entry.ValueScale + Entry.ValueSkew;
		}
		else
		{
			EntryEffQ = Entry.ValueScale + Entry.ValueSkew;
		}
		for (const FArcRandomPoolValueSkew& SkewRule : Entry.ValueSkewRules)
		{
			if (!SkewRule.RequiredTags.IsEmpty() &&
				AggregatedTags.HasAll(SkewRule.RequiredTags))
			{
				EntryEffQ = EntryEffQ * SkewRule.ValueScale + SkewRule.ValueOffset;
			}
		}

		// Convert to flat results
		TArray<FArcCraftModifierResult> SubResults = CollectResults(
			Entry.Modifiers, AggregatedTags, EntryEffQ);

		for (FArcCraftModifierResult& SubResult : SubResults)
		{
			FArcCraftPendingModifier Pending;
			Pending.SlotTag = SlotTag;
			Pending.EffectiveWeight = EffectiveWeight;
			Pending.Result = MoveTemp(SubResult);
			PendingResults.Add(MoveTemp(Pending));
		}
	}

	return PendingResults;
}
