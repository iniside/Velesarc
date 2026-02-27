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

class UArcRecipeDefinition;
struct FGameplayTagContainer;

/**
 * Static utility for building craft output from recipe + ingredients.
 * Shared between UArcCraftStationComponent (actor path) and UArcCraftTickProcessor (entity path).
 */
struct ARCCRAFT_API FArcCraftOutputBuilder
{
	/**
	 * Build an output FArcItemSpec from a recipe and matched ingredient specs.
	 * Handles quality calculation, tier evaluation, modifier application (with slot resolution).
	 *
	 * @param Recipe               The recipe definition.
	 * @param MatchedIngredients   Matched ingredient specs per slot. Invalid specs are skipped.
	 * @param QualityMultipliers   Quality multiplier per ingredient slot.
	 * @param AverageQuality       Average quality across ingredients.
	 * @return The constructed output spec.
	 */
	static FArcItemSpec Build(
		const UArcRecipeDefinition* Recipe,
		const TArray<FArcItemSpec>& MatchedIngredients,
		const TArray<float>& QualityMultipliers,
		float AverageQuality);
};
