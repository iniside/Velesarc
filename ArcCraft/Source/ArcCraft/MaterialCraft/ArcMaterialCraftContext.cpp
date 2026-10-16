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

#include "ArcCraft/MaterialCraft/ArcMaterialCraftContext.h"

#include "ArcCraft/Shared/ArcCraftItemTags.h"

FArcMaterialCraftContext FArcMaterialCraftContext::Build(
	const TArray<FArcItemSpec>& ConsumedIngredients,
	const TArray<float>& InSlotRaritySteps,
	float InAverageR,
	int32 InBaseIngredientCount,
	float InExtraCraftTimeBonus)
{
	FArcMaterialCraftContext Context;

	int32 FilledCount = 0;
	for (const FArcItemSpec& Ingredient : ConsumedIngredients)
	{
		if (Ingredient.GetItemDefinitionId().IsValid())
		{
			++FilledCount;
		}
	}

	Context.PerSlotTags.SetNum(ConsumedIngredients.Num());
	for (int32 Idx = 0; Idx < ConsumedIngredients.Num(); ++Idx)
	{
		Context.PerSlotTags[Idx] = ArcCraftItemTags::GetCraftTags(ConsumedIngredients[Idx]);
	}

	Context.SlotRaritySteps = InSlotRaritySteps;
	Context.AverageR = InAverageR;
	Context.BandEligibilityR = InAverageR; // Adjusted by evaluator
	Context.BandWeightBonus = 0.0f; // Computed by evaluator
	Context.IngredientCount = FilledCount;
	Context.BaseIngredientCount = InBaseIngredientCount > 0 ? InBaseIngredientCount : FilledCount;
	Context.ExtraCraftTimeBonus = InExtraCraftTimeBonus;

	return Context;
}
