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

#include "ArcCraftComponent.h"

#include "ArcCraftExecution_Recipe.generated.h"

class UArcRecipeDefinition;
class UArcItemsStoreComponent;
class UArcQualityTierTable;

/**
 * Craft execution that works with UArcRecipeDefinition instead of item-fragment-based recipes.
 * Handles ingredient matching, consumption, quality evaluation, and output generation.
 */
USTRUCT()
struct ARCCRAFT_API FArcCraftExecution_Recipe : public FArcCraftExecution
{
	GENERATED_BODY()

public:
	/** Which items store class to look for on the instigator. */
	UPROPERTY(EditAnywhere)
	TSubclassOf<UArcItemsStoreComponent> ItemsStoreClass;

	// Legacy overrides — for backward compatibility when used as CraftExecution on UArcCraftComponent
	virtual bool CanCraft(
		const UArcCraftComponent* InCraftComponent,
		const UObject* InInstigator) const override;

	virtual bool CheckAndConsumeItems(
		const UArcCraftComponent* InCraftComponent,
		const UArcItemDefinition* InCraftData,
		const UObject* InInstigator,
		bool bConsume) const override;

	virtual FArcItemSpec OnCraftFinished(
		UArcCraftComponent* InCraftComponent,
		const UArcItemDefinition* InCraftData,
		const UObject* InInstigator) const override;

	// ---- Recipe-specific methods ----

	/**
	 * Check whether the instigator has all required ingredients for the recipe.
	 * If bConsume is true, also consumes them.
	 *
	 * @param OutMatchedIngredients  If non-null, filled with the matched items per ingredient slot.
	 * @param OutQualityMultipliers  If non-null, filled with quality multiplier per ingredient slot.
	 */
	bool CheckAndConsumeRecipeItems(
		const UArcCraftComponent* InCraftComponent,
		const UArcRecipeDefinition* InRecipe,
		const UObject* InInstigator,
		bool bConsume,
		TArray<FArcItemSpec>* OutMatchedIngredients = nullptr,
		TArray<float>* OutQualityMultipliers = nullptr) const;

	/**
	 * Generate the output FArcItemSpec from a recipe after crafting completes.
	 * This performs ingredient matching (again, to get quality data), evaluates modifiers,
	 * and builds the output spec.
	 */
	FArcItemSpec OnRecipeCraftFinished(
		UArcCraftComponent* InCraftComponent,
		const UArcRecipeDefinition* InRecipe,
		const UObject* InInstigator) const;

	virtual ~FArcCraftExecution_Recipe() override = default;

private:
	/** Get the items store from the instigator actor. */
	UArcItemsStoreComponent* GetItemsStore(const UObject* InInstigator) const;

	/** Load and return the quality tier table from the recipe. */
	UArcQualityTierTable* GetTierTable(const UArcRecipeDefinition* InRecipe) const;
};
