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

#include "ArcCraft/Shared/ArcCraftModifier.h"

#include "GameplayEffect.h"
#include "Misc/DataValidation.h"
#include "Abilities/GameplayAbility.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Items/Fragments/ArcItemFragment_AttributeModifier.h"
#include "Items/Fragments/ArcItemFragment_GrantedGameplayEffects.h"
#include "ArcCraft/Recipe/ArcRandomPoolDefinition.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"

// -------------------------------------------------------------------
// FArcCraftModifier (base)
// -------------------------------------------------------------------

bool FArcCraftModifier::MatchesTriggerTags(const FGameplayTagContainer& IngredientTags) const
{
	if (TriggerTags.IsEmpty())
	{
		return true; // Empty = always match
	}
	return IngredientTags.HasAll(TriggerTags);
}

void FArcCraftModifier::Apply(
	FArcItemSpec& OutItemSpec,
	const FGameplayTagContainer& AggregatedIngredientTags,
	float EffectiveQuality) const
{
	// Base does nothing
}

// -------------------------------------------------------------------
// FArcCraftModifier_Stats
// -------------------------------------------------------------------

void FArcCraftModifier_Stats::Apply(
	FArcItemSpec& OutItemSpec,
	const FGameplayTagContainer& AggregatedIngredientTags,
	float EffectiveQuality) const
{
	// Quality gate
	if (MinQualityThreshold > 0.0f && EffectiveQuality < MinQualityThreshold)
	{
		return;
	}

	// Trigger tag check
	if (!MatchesTriggerTags(AggregatedIngredientTags))
	{
		return;
	}

	FArcItemFragment_ItemStats* StatsFragment = OutItemSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
	const bool bCreatedNew = (StatsFragment == nullptr);

	if (bCreatedNew)
	{
		StatsFragment = new FArcItemFragment_ItemStats();
	}

	const float QualityScale = 1.0f + (EffectiveQuality - 1.0f) * QualityScalingFactor;

	FArcItemAttributeStat ScaledStat = BaseStat;
	ScaledStat.SetValue(BaseStat.Value.GetValue() * QualityScale);
	StatsFragment->DefaultStats.Add(ScaledStat);

	if (bCreatedNew)
	{
		OutItemSpec.AddInstanceData(StatsFragment);
	}
}

// -------------------------------------------------------------------
// FArcCraftModifier_Abilities
// -------------------------------------------------------------------

void FArcCraftModifier_Abilities::Apply(
	FArcItemSpec& OutItemSpec,
	const FGameplayTagContainer& AggregatedIngredientTags,
	float EffectiveQuality) const
{
	if (!AbilityToGrant.GrantedAbility)
	{
		return;
	}

	// Quality gate
	if (MinQualityThreshold > 0.0f && EffectiveQuality < MinQualityThreshold)
	{
		return;
	}

	// Trigger tag check
	if (!MatchesTriggerTags(AggregatedIngredientTags))
	{
		return;
	}

	FArcItemFragment_GrantedAbilities* AbilitiesFragment = new FArcItemFragment_GrantedAbilities();
	AbilitiesFragment->Abilities.Add(AbilityToGrant);
	OutItemSpec.AddInstanceData(AbilitiesFragment);
}

// -------------------------------------------------------------------
// FArcCraftModifier_Effects
// -------------------------------------------------------------------

void FArcCraftModifier_Effects::Apply(
	FArcItemSpec& OutItemSpec,
	const FGameplayTagContainer& AggregatedIngredientTags,
	float EffectiveQuality) const
{
	if (!EffectToGrant)
	{
		return;
	}

	// Quality gate
	if (MinQualityThreshold > 0.0f && EffectiveQuality < MinQualityThreshold)
	{
		return;
	}

	// Trigger tag check
	if (!MatchesTriggerTags(AggregatedIngredientTags))
	{
		return;
	}

	FArcItemFragment_GrantedGameplayEffects* EffectsFragment = new FArcItemFragment_GrantedGameplayEffects();
	EffectsFragment->Effects.Add(EffectToGrant);
	OutItemSpec.AddInstanceData(EffectsFragment);
}

// -------------------------------------------------------------------
// FArcCraftModifier_AttributeModifier
// -------------------------------------------------------------------

void FArcCraftModifier_AttributeModifier::Apply(
	FArcItemSpec& OutItemSpec,
	const FGameplayTagContainer& AggregatedIngredientTags,
	float EffectiveQuality) const
{
	if (!BackingGameplayEffect)
	{
		return;
	}

	if (MinQualityThreshold > 0.0f && EffectiveQuality < MinQualityThreshold)
	{
		return;
	}

	if (!MatchesTriggerTags(AggregatedIngredientTags))
	{
		return;
	}

	FArcItemFragment_AttributeModifier* Fragment = OutItemSpec.FindFragmentMutable<FArcItemFragment_AttributeModifier>();
	const bool bCreatedNew = (Fragment == nullptr);

	if (bCreatedNew)
	{
		Fragment = new FArcItemFragment_AttributeModifier();
		Fragment->BackingGameplayEffect = BackingGameplayEffect;
	}

	const float QualityScale = 1.0f + (EffectiveQuality - 1.0f) * QualityScalingFactor;
	for (const auto& [Tag, Magnitude] : BaseAttributes)
	{
		Fragment->Attributes.Add(Tag, Magnitude * QualityScale);
	}

	if (bCreatedNew)
	{
		OutItemSpec.AddInstanceData(Fragment);
	}
}

// -------------------------------------------------------------------
// FArcCraftModifier_RandomPool
// -------------------------------------------------------------------

void FArcCraftModifier_RandomPool::Apply(
	FArcItemSpec& OutItemSpec,
	const FGameplayTagContainer& AggregatedIngredientTags,
	float EffectiveQuality) const
{
	// Quality gate
	if (MinQualityThreshold > 0.0f && EffectiveQuality < MinQualityThreshold)
	{
		return;
	}

	// Trigger tag check
	if (!MatchesTriggerTags(AggregatedIngredientTags))
	{
		return;
	}

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

	// 3. Filter eligible entries and compute weights
	TArray<int32> EligibleIndices;
	TArray<float> EffectiveWeights;
	EligibleIndices.Reserve(PoolDef->Entries.Num());
	EffectiveWeights.Reserve(PoolDef->Entries.Num());

	for (int32 Idx = 0; Idx < PoolDef->Entries.Num(); ++Idx)
	{
		const FArcRandomPoolEntry& Entry = PoolDef->Entries[Idx];

		// Eligibility: quality threshold
		if (Entry.MinQualityThreshold > 0.0f && EffectiveQuality < Entry.MinQualityThreshold)
		{
			continue;
		}

		// Eligibility: required ingredient tags
		if (!Entry.RequiredIngredientTags.IsEmpty() &&
			!AggregatedIngredientTags.HasAll(Entry.RequiredIngredientTags))
		{
			continue;
		}

		// Eligibility: deny tags
		if (!Entry.DenyIngredientTags.IsEmpty() &&
			AggregatedIngredientTags.HasAny(Entry.DenyIngredientTags))
		{
			continue;
		}

		// Compute effective weight
		float EntryWeight = Entry.BaseWeight;
		float Multiplier = 1.0f;

		for (const FArcRandomPoolWeightModifier& WeightMod : Entry.WeightModifiers)
		{
			if (!WeightMod.RequiredTags.IsEmpty() &&
				AggregatedIngredientTags.HasAll(WeightMod.RequiredTags))
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
		return;
	}

	// 4. Delegate to selection mode
	FArcRandomPoolSelectionContext SelectionContext
	{
		PoolDef->Entries,
		EligibleIndices,
		EffectiveWeights,
		EffectiveQuality
	};

	TArray<int32> SelectedIndices;
	Mode->Select(SelectionContext, SelectedIndices);

	// 5. Apply selected entries' sub-modifiers
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
			EntryEffQ = EffectiveQuality * Entry.ValueScale + Entry.ValueSkew;
		}
		else
		{
			EntryEffQ = Entry.ValueScale + Entry.ValueSkew;
		}

		// Apply tag-conditional value skew rules
		for (const FArcRandomPoolValueSkew& SkewRule : Entry.ValueSkewRules)
		{
			if (!SkewRule.RequiredTags.IsEmpty() &&
				AggregatedIngredientTags.HasAll(SkewRule.RequiredTags))
			{
				EntryEffQ = EntryEffQ * SkewRule.ValueScale + SkewRule.ValueOffset;
			}
		}

		for (const FInstancedStruct& ModifierStruct : Entry.Modifiers)
		{
			const FArcCraftModifier* SubModifier = ModifierStruct.GetPtr<FArcCraftModifier>();
			if (SubModifier)
			{
				SubModifier->Apply(OutItemSpec, AggregatedIngredientTags, EntryEffQ);
			}
		}
	}
}

// -------------------------------------------------------------------
// IsDataValid
// -------------------------------------------------------------------

#if WITH_EDITOR

EDataValidationResult FArcCraftModifier::IsDataValid(FDataValidationContext& Context) const
{
	return EDataValidationResult::Valid;
}

EDataValidationResult FArcCraftModifier_Stats::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = FArcCraftModifier::IsDataValid(Context);
	if (!BaseStat.StatTag.IsValid())
	{
		Context.AddError(FText::FromString(TEXT("CraftModifier_Stats: BaseStat.StatTag is not set")));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}

EDataValidationResult FArcCraftModifier_Abilities::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = FArcCraftModifier::IsDataValid(Context);
	if (!AbilityToGrant.GrantedAbility)
	{
		Context.AddError(FText::FromString(TEXT("CraftModifier_Abilities: AbilityToGrant.GrantedAbility is not set")));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}

EDataValidationResult FArcCraftModifier_Effects::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = FArcCraftModifier::IsDataValid(Context);
	if (!EffectToGrant)
	{
		Context.AddError(FText::FromString(TEXT("CraftModifier_Effects: EffectToGrant is not set")));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}

EDataValidationResult FArcCraftModifier_AttributeModifier::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = FArcCraftModifier::IsDataValid(Context);

	if (!BackingGameplayEffect)
	{
		Context.AddError(FText::FromString(TEXT("CraftModifier_AttributeModifier: BackingGameplayEffect is not set")));
		Result = EDataValidationResult::Invalid;
	}

	if (BackingGameplayEffect)
	{
		const UGameplayEffect* EffectCDO = BackingGameplayEffect.GetDefaultObject();
		TSet<FGameplayTag> EffectSetByCallerTags;
		for (const FGameplayModifierInfo& Modifier : EffectCDO->Modifiers)
		{
			if (Modifier.ModifierMagnitude.GetMagnitudeCalculationType()
				== EGameplayEffectMagnitudeCalculation::SetByCaller)
			{
				EffectSetByCallerTags.Add(Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag);
			}
		}

		for (const auto& [Tag, Magnitude] : BaseAttributes)
		{
			if (!EffectSetByCallerTags.Contains(Tag))
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("CraftModifier_AttributeModifier: Attribute tag '%s' has no matching SetByCaller modifier in %s"),
					*Tag.ToString(), *BackingGameplayEffect->GetName())));
				Result = EDataValidationResult::Invalid;
			}
		}
	}

	return Result;
}

EDataValidationResult FArcCraftModifier_RandomPool::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = FArcCraftModifier::IsDataValid(Context);
	if (PoolDefinition.IsNull())
	{
		Context.AddError(FText::FromString(TEXT("CraftModifier_RandomPool: PoolDefinition is not set")));
		Result = EDataValidationResult::Invalid;
	}
	if (!SelectionMode.IsValid())
	{
		Context.AddError(FText::FromString(TEXT("CraftModifier_RandomPool: SelectionMode is not set")));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}

#endif
