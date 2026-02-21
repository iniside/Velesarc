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
#include "UObject/Object.h"

#include "ArcRecipeXmlLoader.generated.h"

class FXmlNode;
class UArcRecipeDefinition;
struct FInstancedStruct;

/**
 * Utility class for loading recipe definitions from XML files.
 * Supports loading individual files or all XML files from a directory.
 */
UCLASS()
class ARCCRAFT_API UArcRecipeXmlLoader : public UObject
{
	GENERATED_BODY()

public:
	/** Load all recipes from XML files in a directory. */
	UFUNCTION(BlueprintCallable, Category = "Arc Craft|Recipes")
	static TArray<UArcRecipeDefinition*> LoadRecipesFromDirectory(
		UObject* Outer,
		const FString& DirectoryPath);

	/** Load recipes from a single XML file (can contain multiple <Recipe> nodes). */
	UFUNCTION(BlueprintCallable, Category = "Arc Craft|Recipes")
	static TArray<UArcRecipeDefinition*> LoadRecipesFromFile(
		UObject* Outer,
		const FString& FilePath);

	/** Parse a single <Recipe> XML node into a recipe definition. */
	static bool ParseRecipeNode(
		const FXmlNode* RecipeNode,
		UArcRecipeDefinition* OutRecipe);

private:
	static bool ParseIngredientNode(
		const FXmlNode* IngredientNode,
		FInstancedStruct& OutIngredient);

	static bool ParseOutputModifierNode(
		const FXmlNode* ModifierNode,
		FInstancedStruct& OutModifier);

	static FGameplayTag ParseGameplayTag(const FString& TagString);
	static FGameplayTagContainer ParseGameplayTags(const FString& TagsString);
};
