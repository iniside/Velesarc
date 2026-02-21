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

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "ArcRecipeCraftContext.generated.h"

/**
 * Context struct passed to ChooserTable evaluation during recipe crafting.
 * The Chooser's filter columns bind to properties on this struct.
 *
 * - GameplayTagQuery columns bind to IngredientTags or RecipeTags
 * - FloatRange columns bind to AverageQuality or TotalQuality
 * - Int/FloatRange columns can bind to IngredientCount
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcRecipeCraftContext
{
	GENERATED_BODY()

	/** All gameplay tags aggregated from all consumed ingredients. */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	FGameplayTagContainer IngredientTags;

	/** Average quality multiplier across all ingredient slots (weighted by amount). */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	float AverageQuality = 1.0f;

	/** Sum of all quality multipliers (before averaging). */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	float TotalQuality = 0.0f;

	/** Number of ingredient slots in the recipe. */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	int32 IngredientCount = 0;

	/** Tags from the recipe definition itself. */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	FGameplayTagContainer RecipeTags;
};
