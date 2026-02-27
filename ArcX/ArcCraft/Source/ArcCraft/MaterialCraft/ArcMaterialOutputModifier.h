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

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ArcCraft/Recipe/ArcRecipeOutput.h"

#include "ArcMaterialOutputModifier.generated.h"

class UArcMaterialPropertyTable;

/**
 * Recipe output modifier that evaluates a material property table against
 * the consumed ingredients to produce quality-banded, tag-driven modifiers.
 *
 * Add this to a UArcRecipeDefinition::OutputModifiers array. When the craft
 * completes, ingredient tags are evaluated per-slot, matched against the property
 * table's rules, and each matching rule selects a quality band via weighted random.
 * The selected band's modifiers (stats, abilities, effects, etc.) are applied
 * to the output item.
 *
 * Supports:
 *  - Tag combos via FGameplayTagQuery (AND/OR/NOT), matched across ingredient slots
 *  - Quality bands with weighted random selection biased by quality
 *  - Extra-ingredient remedy: more materials = higher weight bias (not tier jump)
 *  - Extra-time remedy: designer-configured weight bonus
 *  - Recipe-level slot configs for limiting modifier counts
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Material Properties"))
struct ARCCRAFT_API FArcRecipeOutputModifier_MaterialProperties : public FArcRecipeOutputModifier
{
	GENERATED_BODY()

public:
	/** The material property table to evaluate. */
	UPROPERTY(EditAnywhere, Category = "MaterialProperties")
	TSoftObjectPtr<UArcMaterialPropertyTable> PropertyTable;

	/** If true, the recipe's quality tier table overrides the table's default.
	 *  The tier table is used for ingredient quality evaluation upstream. */
	UPROPERTY(EditAnywhere, Category = "MaterialProperties")
	bool bUseRecipeTierTable = true;

	/** Base ingredient count for "extra materials" remedy calculation.
	 *  Extra ingredients beyond this count add weight bonus (not quality).
	 *  0 = use the actual ingredient count (no extra-material bonus). */
	UPROPERTY(EditAnywhere, Category = "MaterialProperties|Remedies", meta = (ClampMin = "0"))
	int32 BaseIngredientCount = 0;

	/** Designer-configured weight bonus representing extra time spent crafting.
	 *  Capped by the table's ExtraTimeWeightBonusCap. */
	UPROPERTY(EditAnywhere, Category = "MaterialProperties|Remedies")
	float ExtraCraftTimeBonus = 0.0f;

	/** Recipe tags to pass to the evaluation context.
	 *  Used for rules with RequiredRecipeTags filtering.
	 *  If empty, rules with RequiredRecipeTags will not match. */
	UPROPERTY(EditAnywhere, Category = "MaterialProperties")
	FGameplayTagContainer RecipeTags;

	virtual TArray<FArcCraftPendingModifier> Evaluate(
		const TArray<FArcItemSpec>& ConsumedIngredients,
		const TArray<float>& IngredientQualityMults,
		float AverageQuality) const override;
};
