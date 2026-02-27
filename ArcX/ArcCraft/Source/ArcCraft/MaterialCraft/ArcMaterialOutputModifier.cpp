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

#include "ArcCraft/MaterialCraft/ArcMaterialOutputModifier.h"

#include "ArcCraft/MaterialCraft/ArcMaterialCraftContext.h"
#include "ArcCraft/MaterialCraft/ArcMaterialCraftEvaluator.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyRule.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTable.h"
#include "ArcCraft/Recipe/ArcCraftSlotResolver.h"
#include "ArcCraft/Recipe/ArcRecipeOutput.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

TArray<FArcCraftPendingModifier> FArcRecipeOutputModifier_MaterialProperties::Evaluate(
	const TArray<FArcItemSpec>& ConsumedIngredients,
	const TArray<float>& IngredientQualityMults,
	float AverageQuality) const
{
	TArray<FArcCraftPendingModifier> Result;

	// 1. Load property table
	UArcMaterialPropertyTable* Table = PropertyTable.LoadSynchronous();
	if (!Table || Table->Rules.Num() == 0)
	{
		return Result;
	}

	// 2. Build material craft context
	FArcMaterialCraftContext Context = FArcMaterialCraftContext::Build(
		ConsumedIngredients,
		IngredientQualityMults,
		AverageQuality,
		RecipeTags,
		BaseIngredientCount,
		ExtraCraftTimeBonus);

	// 3. Compute separated quality and weight bonus
	FArcMaterialCraftEvaluator::ComputeQualityAndWeightBonus(Table, Context);

	// 4. Evaluate rules and select bands
	TArray<FArcMaterialRuleEvaluation> Evaluations = FArcMaterialCraftEvaluator::EvaluateRules(Table, Context);

	if (Evaluations.Num() == 0)
	{
		return Result;
	}

	// 5. Aggregate ingredient AssetTags for terminal modifier application
	FGameplayTagContainer AggregatedTags;
	for (const FArcItemSpec& Ingredient : ConsumedIngredients)
	{
		if (Ingredient.GetItemDefinition())
		{
			if (const FArcItemFragment_Tags* TagsFrag = ArcItemsHelper::GetFragment<FArcItemFragment_Tags>(const_cast<UArcItemDefinition*>(Ingredient.GetItemDefinition())))
			{
				AggregatedTags.AppendTags(TagsFrag->AssetTags);
			}
		}
	}

	// 6. Create pending modifiers per rule evaluation — one per atomic result
	const float BandEligibilityQuality = Context.BandEligibilityQuality;

	for (const FArcMaterialRuleEvaluation& Eval : Evaluations)
	{
		if (!Eval.Band)
		{
			continue;
		}

		// Determine slot tag: use the rule's first OutputTag if available,
		// otherwise fall back to this modifier's own SlotTag.
		FGameplayTag EvalSlotTag = SlotTag;
		if (Eval.Rule && !Eval.Rule->OutputTags.IsEmpty())
		{
			auto TagIt = Eval.Rule->OutputTags.CreateConstIterator();
			EvalSlotTag = *TagIt;
		}

		// Convert each band modifier to an atomic result
		for (const FInstancedStruct& ModStruct : Eval.Band->Modifiers)
		{
			const FArcCraftModifier* BaseMod = ModStruct.GetPtr<FArcCraftModifier>();
			if (!BaseMod)
			{
				continue;
			}

			// Eligibility checks on terminal modifier
			if (BaseMod->MinQualityThreshold > 0.0f && BandEligibilityQuality < BaseMod->MinQualityThreshold)
			{
				continue;
			}
			if (!BaseMod->MatchesTriggerTags(AggregatedTags))
			{
				continue;
			}

			FArcCraftModifierResult ModResult;

			if (const FArcCraftModifier_Stats* StatMod = ModStruct.GetPtr<FArcCraftModifier_Stats>())
			{
				const float QScale = 1.0f + (BandEligibilityQuality - 1.0f) * StatMod->QualityScalingFactor;
				ModResult.Type = EArcCraftModifierResultType::Stat;
				ModResult.Stat = StatMod->BaseStat;
				ModResult.Stat.SetValue(StatMod->BaseStat.Value.GetValue() * QScale);
			}
			else if (const FArcCraftModifier_Abilities* AbilMod = ModStruct.GetPtr<FArcCraftModifier_Abilities>())
			{
				ModResult.Type = EArcCraftModifierResultType::Ability;
				ModResult.Ability = AbilMod->AbilityToGrant;
			}
			else if (const FArcCraftModifier_Effects* EffMod = ModStruct.GetPtr<FArcCraftModifier_Effects>())
			{
				ModResult.Type = EArcCraftModifierResultType::Effect;
				ModResult.Effect = EffMod->EffectToGrant;
			}
			else
			{
				continue; // Unknown modifier type — skip
			}

			FArcCraftPendingModifier Pending;
			Pending.SlotTag = EvalSlotTag;
			Pending.EffectiveWeight = Eval.EffectiveWeight;
			Pending.Result = MoveTemp(ModResult);
			Result.Add(MoveTemp(Pending));
		}
	}

	return Result;
}
