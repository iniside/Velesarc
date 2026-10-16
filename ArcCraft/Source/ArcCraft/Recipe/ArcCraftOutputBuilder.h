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

#pragma once

#include "Items/ArcItemSpec.h"
#include "UObject/PrimaryAssetId.h"

struct FArcCraftContext;
struct FArcRecipeDefinitionData;

/**
 * Shared output-spec builder used by the Mass craft tick processor.
 */
struct ARCCRAFT_API FArcCraftOutputBuilder
{
	/**
	 * Build one FArcItemSpec per recipe output, in recipe order; an output with an invalid item id is skipped.
	 * Rarity and modifiers (with slot resolution) apply to the main output (Outputs[0]) only; an output's InstanceTags go on its own spec.
	 *
	 * @param RecipeId             Id of the recipe record; names the family base when Recipe has no BaseRecipe.
	 * @param Recipe               The recipe record. An upgraded output takes the chairs and selector of the family
	 *                             member whose start rarity is the output rarity; with none, no slotted result applies.
	 * @param MatchedIngredients   Matched ingredient specs per slot. Invalid specs are skipped.
	 * @param Context              Craft inputs; OutputRarity is decided and written here, and every roll draws from Stream.
	 * @return The constructed output specs.
	 */
	static TArray<FArcItemSpec> Build(
		const FPrimaryAssetId& RecipeId,
		const FArcRecipeDefinitionData& Recipe,
		const TArray<FArcItemSpec>& MatchedIngredients,
		FArcCraftContext& Context);
};
