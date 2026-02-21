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
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTable.h"
#include "Items/ArcItemSpec.h"

void FArcRecipeOutputModifier_MaterialProperties::ApplyToOutput(
	FArcItemSpec& OutItemSpec,
	const TArray<const FArcItemData*>& ConsumedIngredients,
	const TArray<float>& IngredientQualityMults,
	float AverageQuality) const
{
	// 1. Load property table
	UArcMaterialPropertyTable* Table = PropertyTable.LoadSynchronous();
	if (!Table || Table->Rules.Num() == 0)
	{
		return;
	}

	// 2. Build material craft context (per-slot tags, not aggregated)
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
		return;
	}

	// 5. Filter through slot configs (if configured)
	if (ModifierSlotConfigs.Num() > 0)
	{
		Evaluations = FArcMaterialCraftEvaluator::FilterBySlotConfig(Evaluations, ModifierSlotConfigs);
	}

	// 6. Apply evaluated band modifiers to the output item
	FArcMaterialCraftEvaluator::ApplyEvaluations(
		Evaluations,
		OutItemSpec,
		ConsumedIngredients,
		IngredientQualityMults,
		Context.BandEligibilityQuality);
}
