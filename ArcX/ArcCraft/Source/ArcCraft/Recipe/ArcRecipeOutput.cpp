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
#include "IObjectChooser.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Items/Fragments/ArcItemFragment_GrantedGameplayEffects.h"
#include "ArcCraft/Recipe/ArcRecipeCraftContext.h"
#include "ArcCraft/Recipe/ArcRandomModifierEntry.h"
#include "ArcCraft/Recipe/ArcRandomPoolDefinition.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"

// -------------------------------------------------------------------
// FArcRecipeOutputModifier (base)
// -------------------------------------------------------------------

void FArcRecipeOutputModifier::ApplyToOutput(
	FArcItemSpec& OutItemSpec,
	const TArray<const FArcItemData*>& ConsumedIngredients,
	const TArray<float>& IngredientQualityMults,
	float AverageQuality) const
{
	// Base does nothing
}

// -------------------------------------------------------------------
// FArcRecipeOutputModifier_Stats
// -------------------------------------------------------------------

void FArcRecipeOutputModifier_Stats::ApplyToOutput(
	FArcItemSpec& OutItemSpec,
	const TArray<const FArcItemData*>& ConsumedIngredients,
	const TArray<float>& IngredientQualityMults,
	float AverageQuality) const
{
	if (BaseStats.Num() == 0)
	{
		return;
	}

	FArcItemFragment_ItemStats* StatsFragment = OutItemSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
	const bool bCreatedNew = (StatsFragment == nullptr);

	if (bCreatedNew)
	{
		StatsFragment = new FArcItemFragment_ItemStats();
	}

	const float QualityScale = 1.0f + (AverageQuality - 1.0f) * QualityScalingFactor;

	for (const FArcItemAttributeStat& BaseStat : BaseStats)
	{
		FArcItemAttributeStat ScaledStat = BaseStat;
		ScaledStat.SetValue(BaseStat.Value.GetValue() * QualityScale);

		StatsFragment->DefaultStats.Add(ScaledStat);
	}

	if (bCreatedNew)
	{
		OutItemSpec.AddInstanceData(StatsFragment);
	}
}

// -------------------------------------------------------------------
// FArcRecipeOutputModifier_Abilities
// -------------------------------------------------------------------

static bool HasTriggerTagsInIngredients(
	const FGameplayTagContainer& TriggerTags,
	const TArray<const FArcItemData*>& ConsumedIngredients)
{
	for (const FArcItemData* Ingredient : ConsumedIngredients)
	{
		if (Ingredient && Ingredient->GetItemAggregatedTags().HasAll(TriggerTags))
		{
			return true;
		}
	}
	return false;
}

void FArcRecipeOutputModifier_Abilities::ApplyToOutput(
	FArcItemSpec& OutItemSpec,
	const TArray<const FArcItemData*>& ConsumedIngredients,
	const TArray<float>& IngredientQualityMults,
	float AverageQuality) const
{
	if (TriggerTags.IsEmpty() || AbilitiesToGrant.Num() == 0)
	{
		return;
	}

	if (!HasTriggerTagsInIngredients(TriggerTags, ConsumedIngredients))
	{
		return;
	}

	FArcItemFragment_GrantedAbilities* AbilitiesFragment = new FArcItemFragment_GrantedAbilities();
	AbilitiesFragment->Abilities = AbilitiesToGrant;
	OutItemSpec.AddInstanceData(AbilitiesFragment);
}

// -------------------------------------------------------------------
// FArcRecipeOutputModifier_Effects
// -------------------------------------------------------------------

void FArcRecipeOutputModifier_Effects::ApplyToOutput(
	FArcItemSpec& OutItemSpec,
	const TArray<const FArcItemData*>& ConsumedIngredients,
	const TArray<float>& IngredientQualityMults,
	float AverageQuality) const
{
	if (TriggerTags.IsEmpty() || EffectsToGrant.Num() == 0)
	{
		return;
	}

	if (!HasTriggerTagsInIngredients(TriggerTags, ConsumedIngredients))
	{
		return;
	}

	if (MinQualityThreshold > 0.0f && AverageQuality < MinQualityThreshold)
	{
		return;
	}

	FArcItemFragment_GrantedGameplayEffects* EffectsFragment = new FArcItemFragment_GrantedGameplayEffects();
	EffectsFragment->Effects = EffectsToGrant;
	OutItemSpec.AddInstanceData(EffectsFragment);
}

// -------------------------------------------------------------------
// FArcRecipeOutputModifier_TransferStats
// -------------------------------------------------------------------

void FArcRecipeOutputModifier_TransferStats::ApplyToOutput(
	FArcItemSpec& OutItemSpec,
	const TArray<const FArcItemData*>& ConsumedIngredients,
	const TArray<float>& IngredientQualityMults,
	float AverageQuality) const
{
	if (!ConsumedIngredients.IsValidIndex(IngredientSlotIndex))
	{
		return;
	}

	const FArcItemData* SourceItem = ConsumedIngredients[IngredientSlotIndex];
	if (!SourceItem)
	{
		return;
	}

	// Try to get stats from the source item's instance data
	const FArcItemInstance_ItemStats* SourceStats = nullptr;
	const TMap<FName, TSharedPtr<FArcItemInstance>>& InstancedData = SourceItem->InstancedData;
	for (const auto& Pair : InstancedData)
	{
		if (Pair.Value.IsValid() && Pair.Value->GetScriptStruct()->IsChildOf(FArcItemInstance_ItemStats::StaticStruct()))
		{
			SourceStats = static_cast<const FArcItemInstance_ItemStats*>(Pair.Value.Get());
			break;
		}
	}

	if (!SourceStats)
	{
		return;
	}

	float Scale = TransferScale;
	if (bScaleByQuality && IngredientQualityMults.IsValidIndex(IngredientSlotIndex))
	{
		Scale *= IngredientQualityMults[IngredientSlotIndex];
	}

	FArcItemFragment_ItemStats* OutputStatsFragment = OutItemSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
	const bool bCreatedNew = (OutputStatsFragment == nullptr);
	if (bCreatedNew)
	{
		OutputStatsFragment = new FArcItemFragment_ItemStats();
	}

	// Transfer each stat from the source, scaled
	const TMap<FName, float>& Stats = SourceStats->GetStats();
	for (const auto& StatPair : Stats)
	{
		FArcItemAttributeStat TransferredStat;
		TransferredStat.Attribute = FGameplayAttribute();
		TransferredStat.SetValue(StatPair.Value * Scale);
		// Note: We lose the exact FGameplayAttribute reference here since the instance
		// only stores FName keys. The stat will be applied by name matching during
		// item initialization.
		OutputStatsFragment->DefaultStats.Add(TransferredStat);
	}

	// Also transfer from replicated stats if present
	for (const FArcItemStatReplicated& RepStat : SourceStats->GetReplicatedItemStats())
	{
		FArcItemAttributeStat TransferredStat;
		TransferredStat.SetValue(RepStat.FinalValue * Scale);
		OutputStatsFragment->DefaultStats.Add(TransferredStat);
	}

	if (bCreatedNew)
	{
		OutItemSpec.AddInstanceData(OutputStatsFragment);
	}
}

// -------------------------------------------------------------------
// FArcRecipeOutputModifier_Random
// -------------------------------------------------------------------

void FArcRecipeOutputModifier_Random::ApplyToOutput(
	FArcItemSpec& OutItemSpec,
	const TArray<const FArcItemData*>& ConsumedIngredients,
	const TArray<float>& IngredientQualityMults,
	float AverageQuality) const
{
	// 1. Load the chooser table
	UChooserTable* ChooserTable = ModifierChooserTable.LoadSynchronous();
	if (!ChooserTable)
	{
		return;
	}

	// 2. Build the craft context struct
	FArcRecipeCraftContext CraftContext;
	CraftContext.AverageQuality = AverageQuality;
	CraftContext.IngredientCount = ConsumedIngredients.Num();

	// Aggregate tags from all consumed ingredients
	float TotalQuality = 0.0f;
	for (int32 Idx = 0; Idx < ConsumedIngredients.Num(); ++Idx)
	{
		if (const FArcItemData* ItemData = ConsumedIngredients[Idx])
		{
			CraftContext.IngredientTags.AppendTags(ItemData->GetItemAggregatedTags());
		}
		if (IngredientQualityMults.IsValidIndex(Idx))
		{
			TotalQuality += IngredientQualityMults[Idx];
		}
	}
	CraftContext.TotalQuality = TotalQuality;

	// 3. Determine total rolls
	int32 TotalRolls = MaxRolls;
	if (bQualityAffectsRolls && QualityBonusRollThreshold > 0.0f)
	{
		// Each full multiple of the threshold grants a bonus roll
		const int32 BonusRolls = FMath::FloorToInt32(AverageQuality / QualityBonusRollThreshold);
		TotalRolls += BonusRolls;
	}

	// 4. Roll the chooser table
	TSet<UArcRandomModifierEntry*> AlreadySelected;

	for (int32 Roll = 0; Roll < TotalRolls; ++Roll)
	{
		// Build evaluation context with the craft context struct
		FChooserEvaluationContext EvalContext;
		EvalContext.AddStructParam(CraftContext);

		// Evaluate — the chooser returns UObject* (expected UArcRandomModifierEntry*)
		UArcRandomModifierEntry* SelectedEntry = nullptr;

		UChooserTable::EvaluateChooser(EvalContext, ChooserTable,
			FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda(
				[&SelectedEntry, &AlreadySelected, bAllowDups = bAllowDuplicates]
				(UObject* Result) -> FObjectChooserBase::EIteratorStatus
				{
					UArcRandomModifierEntry* Entry = Cast<UArcRandomModifierEntry>(Result);
					if (Entry)
					{
						if (bAllowDups || !AlreadySelected.Contains(Entry))
						{
							SelectedEntry = Entry;
							return FObjectChooserBase::EIteratorStatus::Stop;
						}
					}
					return FObjectChooserBase::EIteratorStatus::Continue;
				}));

		if (!SelectedEntry)
		{
			continue;
		}

		AlreadySelected.Add(SelectedEntry);

		// 5. Apply the entry's modifiers to the output spec
		for (const FInstancedStruct& ModifierStruct : SelectedEntry->Modifiers)
		{
			const FArcRecipeOutputModifier* SubModifier = ModifierStruct.GetPtr<FArcRecipeOutputModifier>();
			if (!SubModifier)
			{
				continue;
			}

			// Compute effective quality incorporating the entry's ValueScale and ValueSkew.
			// This lets the random modifier entry bias how strongly quality affects the result.
			float EffectiveQuality = AverageQuality;
			if (SelectedEntry->bScaleByQuality)
			{
				EffectiveQuality *= SelectedEntry->ValueScale;
				EffectiveQuality += SelectedEntry->ValueSkew;
			}
			else
			{
				// Apply scale/skew as a fixed value independent of quality
				EffectiveQuality = SelectedEntry->ValueScale + SelectedEntry->ValueSkew;
			}

			SubModifier->ApplyToOutput(OutItemSpec, ConsumedIngredients, IngredientQualityMults, EffectiveQuality);
		}
	}
}

// -------------------------------------------------------------------
// FArcRecipeOutputModifier_RandomPool
// -------------------------------------------------------------------

static FGameplayTagContainer AggregateIngredientTags(
	const TArray<const FArcItemData*>& ConsumedIngredients)
{
	FGameplayTagContainer AggregatedTags;
	for (const FArcItemData* Ingredient : ConsumedIngredients)
	{
		if (Ingredient)
		{
			AggregatedTags.AppendTags(Ingredient->GetItemAggregatedTags());
		}
	}
	return AggregatedTags;
}

static bool IsPoolEntryEligible(
	const FArcRandomPoolEntry& Entry,
	const FGameplayTagContainer& IngredientTags,
	float AverageQuality)
{
	// Quality threshold
	if (Entry.MinQualityThreshold > 0.0f && AverageQuality < Entry.MinQualityThreshold)
	{
		return false;
	}

	// Required ingredient tags — aggregated tags must have ALL of them
	if (!Entry.RequiredIngredientTags.IsEmpty())
	{
		if (!IngredientTags.HasAll(Entry.RequiredIngredientTags))
		{
			return false;
		}
	}

	// Deny tags — must NOT have ANY
	if (!Entry.DenyIngredientTags.IsEmpty())
	{
		if (IngredientTags.HasAny(Entry.DenyIngredientTags))
		{
			return false;
		}
	}

	return true;
}

static float CalculateEffectiveWeight(
	const FArcRandomPoolEntry& Entry,
	const FGameplayTagContainer& IngredientTags,
	float AverageQuality)
{
	float Weight = Entry.BaseWeight;
	float Multiplier = 1.0f;

	// Apply tag-conditional weight modifiers
	for (const FArcRandomPoolWeightModifier& WeightMod : Entry.WeightModifiers)
	{
		if (!WeightMod.RequiredTags.IsEmpty() && IngredientTags.HasAll(WeightMod.RequiredTags))
		{
			Weight += WeightMod.BonusWeight;
			Multiplier *= WeightMod.WeightMultiplier;
		}
	}

	Weight *= Multiplier;

	// Quality scaling
	Weight *= (1.0f + (AverageQuality - 1.0f) * Entry.QualityWeightScaling);

	return FMath::Max(Weight, 0.0f);
}

static float CalculateEffectiveQuality(
	const FArcRandomPoolEntry& Entry,
	const FGameplayTagContainer& IngredientTags,
	float AverageQuality)
{
	float EffQ;
	if (Entry.bScaleByQuality)
	{
		EffQ = AverageQuality * Entry.ValueScale + Entry.ValueSkew;
	}
	else
	{
		EffQ = Entry.ValueScale + Entry.ValueSkew;
	}

	// Apply tag-conditional value skew rules (stacks sequentially)
	for (const FArcRandomPoolValueSkew& SkewRule : Entry.ValueSkewRules)
	{
		if (!SkewRule.RequiredTags.IsEmpty() && IngredientTags.HasAll(SkewRule.RequiredTags))
		{
			EffQ = EffQ * SkewRule.ValueScale + SkewRule.ValueOffset;
		}
	}

	return EffQ;
}

void FArcRecipeOutputModifier_RandomPool::ApplyToOutput(
	FArcItemSpec& OutItemSpec,
	const TArray<const FArcItemData*>& ConsumedIngredients,
	const TArray<float>& IngredientQualityMults,
	float AverageQuality) const
{
	// 1. Load pool definition
	UArcRandomPoolDefinition* PoolDef = PoolDefinition.LoadSynchronous();
	if (!PoolDef || PoolDef->Entries.Num() == 0)
	{
		return;
	}

	// 2. Get the selection mode
	const FArcRandomPoolSelectionMode* Mode = SelectionMode.GetPtr<FArcRandomPoolSelectionMode>();
	if (!Mode)
	{
		return;
	}

	// 3. Aggregate ingredient tags
	const FGameplayTagContainer IngredientTags = AggregateIngredientTags(ConsumedIngredients);

	// 4. Filter eligible entries and compute weights
	TArray<int32> EligibleIndices;
	TArray<float> EffectiveWeights;
	EligibleIndices.Reserve(PoolDef->Entries.Num());
	EffectiveWeights.Reserve(PoolDef->Entries.Num());

	for (int32 Idx = 0; Idx < PoolDef->Entries.Num(); ++Idx)
	{
		const FArcRandomPoolEntry& Entry = PoolDef->Entries[Idx];
		if (IsPoolEntryEligible(Entry, IngredientTags, AverageQuality))
		{
			EligibleIndices.Add(Idx);
			EffectiveWeights.Add(CalculateEffectiveWeight(Entry, IngredientTags, AverageQuality));
		}
	}

	if (EligibleIndices.Num() == 0)
	{
		return;
	}

	// 5. Delegate to selection mode
	FArcRandomPoolSelectionContext SelectionContext
	{
		PoolDef->Entries,
		EligibleIndices,
		EffectiveWeights,
		AverageQuality
	};

	TArray<int32> SelectedIndices;
	Mode->Select(SelectionContext, SelectedIndices);

	// 6. Apply selected entries
	for (const int32 SelectedIdx : SelectedIndices)
	{
		if (!PoolDef->Entries.IsValidIndex(SelectedIdx))
		{
			continue;
		}

		const FArcRandomPoolEntry& Entry = PoolDef->Entries[SelectedIdx];
		const float EffectiveQual = CalculateEffectiveQuality(Entry, IngredientTags, AverageQuality);

		for (const FInstancedStruct& ModifierStruct : Entry.Modifiers)
		{
			const FArcRecipeOutputModifier* SubModifier = ModifierStruct.GetPtr<FArcRecipeOutputModifier>();
			if (SubModifier)
			{
				SubModifier->ApplyToOutput(OutItemSpec, ConsumedIngredients, IngredientQualityMults, EffectiveQual);
			}
		}
	}
}
