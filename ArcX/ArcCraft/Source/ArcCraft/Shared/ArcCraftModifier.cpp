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

#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Items/Fragments/ArcItemFragment_GrantedGameplayEffects.h"

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
	if (BaseStats.Num() == 0)
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

	FArcItemFragment_ItemStats* StatsFragment = OutItemSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
	const bool bCreatedNew = (StatsFragment == nullptr);

	if (bCreatedNew)
	{
		StatsFragment = new FArcItemFragment_ItemStats();
	}

	const float QualityScale = 1.0f + (EffectiveQuality - 1.0f) * QualityScalingFactor;

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
// FArcCraftModifier_Abilities
// -------------------------------------------------------------------

void FArcCraftModifier_Abilities::Apply(
	FArcItemSpec& OutItemSpec,
	const FGameplayTagContainer& AggregatedIngredientTags,
	float EffectiveQuality) const
{
	if (AbilitiesToGrant.Num() == 0)
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
	AbilitiesFragment->Abilities = AbilitiesToGrant;
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
	if (EffectsToGrant.Num() == 0)
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
	EffectsFragment->Effects = EffectsToGrant;
	OutItemSpec.AddInstanceData(EffectsFragment);
}
