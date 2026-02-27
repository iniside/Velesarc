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

#include "ArcCraft/Recipe/ArcCraftOutputBuilder.h"

#include "Abilities/GameplayAbility.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Items/Fragments/ArcItemFragment_GrantedGameplayEffects.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "ArcCraft/Recipe/ArcCraftModifierSlot.h"
#include "ArcCraft/Recipe/ArcCraftSlotResolver.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Recipe/ArcRecipeOutput.h"
#include "ArcCraft/Recipe/ArcRecipeQuality.h"

static void ApplyResult(FArcItemSpec& OutSpec, const FArcCraftModifierResult& Result)
{
	switch (Result.Type)
	{
	case EArcCraftModifierResultType::Stat:
		{
			FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			if (!StatsFragment)
			{
				StatsFragment = new FArcItemFragment_ItemStats();
				StatsFragment->DefaultStats.Add(Result.Stat);
				OutSpec.AddInstanceData(StatsFragment);
			}
			else
			{
				StatsFragment->DefaultStats.Add(Result.Stat);
			}
		}
		break;

	case EArcCraftModifierResultType::Ability:
		{
			if (Result.Ability.GrantedAbility)
			{
				FArcItemFragment_GrantedAbilities* AbilitiesFragment = new FArcItemFragment_GrantedAbilities();
				AbilitiesFragment->Abilities.Add(Result.Ability);
				OutSpec.AddInstanceData(AbilitiesFragment);
			}
		}
		break;

	case EArcCraftModifierResultType::Effect:
		{
			if (Result.Effect)
			{
				FArcItemFragment_GrantedGameplayEffects* EffectsFragment = new FArcItemFragment_GrantedGameplayEffects();
				EffectsFragment->Effects.Add(Result.Effect);
				OutSpec.AddInstanceData(EffectsFragment);
			}
		}
		break;

	default:
		break;
	}
}

FArcItemSpec FArcCraftOutputBuilder::Build(
	const UArcRecipeDefinition* Recipe,
	const TArray<FArcItemSpec>& MatchedIngredients,
	const TArray<float>& QualityMultipliers,
	float AverageQuality)
{
	FArcItemSpec OutputSpec;
	if (!Recipe || !Recipe->OutputItemDefinition.IsValid())
	{
		return OutputSpec;
	}

	// Compute output level
	uint8 OutputLevel = Recipe->OutputLevel;
	if (Recipe->bQualityAffectsLevel && !Recipe->QualityTierTable.IsNull())
	{
		UArcQualityTierTable* TierTable = Recipe->QualityTierTable.LoadSynchronous();
		if (TierTable)
		{
			float TotalTierValue = 0.0f;
			int32 TierCount = 0;
			for (int32 Idx = 0; Idx < MatchedIngredients.Num(); ++Idx)
			{
				if (MatchedIngredients[Idx].GetItemDefinitionId().IsValid())
				{
					const UArcItemDefinition* IngDef = MatchedIngredients[Idx].GetItemDefinition();
					if (IngDef)
					{
						const FArcItemFragment_Tags* IngTags = IngDef->FindFragment<FArcItemFragment_Tags>();
						if (IngTags)
						{
							const FGameplayTag BestTag = TierTable->FindBestTierTag(IngTags->ItemTags);
							if (BestTag.IsValid())
							{
								TotalTierValue += TierTable->GetTierValue(BestTag);
								++TierCount;
							}
						}
					}
				}
			}
			if (TierCount > 0)
			{
				OutputLevel = static_cast<uint8>(FMath::Clamp(
					FMath::RoundToInt(TotalTierValue / TierCount), 1, 255));
			}
		}
	}

	// Create base output spec
	OutputSpec = FArcItemSpec::NewItem(Recipe->OutputItemDefinition, OutputLevel, Recipe->OutputAmount);

	// Phase 1: Evaluate all modifiers into pending results (eligibility checked per-modifier)
	TArray<FArcCraftPendingModifier> PendingModifiers;
	PendingModifiers.Reserve(Recipe->OutputModifiers.Num());
	for (int32 Idx = 0; Idx < Recipe->OutputModifiers.Num(); ++Idx)
	{
		const FArcRecipeOutputModifier* Modifier =
			Recipe->OutputModifiers[Idx].GetPtr<FArcRecipeOutputModifier>();
		if (Modifier)
		{
			TArray<FArcCraftPendingModifier> Results = Modifier->Evaluate(
				MatchedIngredients, QualityMultipliers, AverageQuality);
			for (FArcCraftPendingModifier& Pending : Results)
			{
				Pending.ModifierIndex = Idx;
			}
			PendingModifiers.Append(MoveTemp(Results));
		}
	}

	// Phase 2: Resolve through slot configuration (if slots exist)
	TArray<int32> ResolvedIndices;
	if (Recipe->ModifierSlots.Num() > 0)
	{
		ResolvedIndices = FArcCraftSlotResolver::Resolve(
			PendingModifiers, Recipe->ModifierSlots,
			Recipe->MaxUsableSlots, Recipe->SlotSelectionMode);
	}
	else
	{
		// No slots — all pending modifiers apply
		ResolvedIndices.SetNum(PendingModifiers.Num());
		for (int32 i = 0; i < PendingModifiers.Num(); ++i)
		{
			ResolvedIndices[i] = i;
		}
	}

	// Phase 3: Apply resolved modifiers
	for (int32 Idx : ResolvedIndices)
	{
		if (PendingModifiers[Idx].Result.Type != EArcCraftModifierResultType::None)
		{
			ApplyResult(OutputSpec, PendingModifiers[Idx].Result);
		}
	}

	return OutputSpec;
}

