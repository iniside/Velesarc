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

#include "ArcCraft/Recipe/ArcRecipeJsonLoader.h"

#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Recipe/ArcRecipeQuality.h"
#include "ArcCraft/Shared/ArcCraftJsonUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcRecipeJsonLoader, Log, All);

// -------------------------------------------------------------------
// LoadRecipesFromDirectory
// -------------------------------------------------------------------

TArray<UArcRecipeDefinition*> UArcRecipeJsonLoader::LoadRecipesFromDirectory(UObject* Outer, const FString& DirectoryPath)
{
	TArray<UArcRecipeDefinition*> AllRecipes;

	TArray<FString> JsonFiles;
	IFileManager::Get().FindFiles(JsonFiles, *(DirectoryPath / TEXT("*.json")), true, false);

	for (const FString& FileName : JsonFiles)
	{
		const FString FullPath = DirectoryPath / FileName;
		TArray<UArcRecipeDefinition*> Recipes = LoadRecipesFromFile(Outer, FullPath);
		AllRecipes.Append(Recipes);
	}

	UE_LOG(LogArcRecipeJsonLoader, Log, TEXT("Loaded %d recipes from directory: %s"), AllRecipes.Num(), *DirectoryPath);
	return AllRecipes;
}

// -------------------------------------------------------------------
// LoadRecipesFromFile
// -------------------------------------------------------------------

TArray<UArcRecipeDefinition*> UArcRecipeJsonLoader::LoadRecipesFromFile(UObject* Outer, const FString& FilePath)
{
	TArray<UArcRecipeDefinition*> Result;

	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		UE_LOG(LogArcRecipeJsonLoader, Error, TEXT("Failed to read file: %s"), *FilePath);
		return Result;
	}

	nlohmann::json Parsed;
	try
	{
		Parsed = nlohmann::json::parse(TCHAR_TO_UTF8(*FileContent));
	}
	catch (const nlohmann::json::parse_error& e)
	{
		UE_LOG(LogArcRecipeJsonLoader, Error, TEXT("JSON parse error in %s: %hs"), *FilePath, e.what());
		return Result;
	}

	// Handle array of recipes or single recipe
	TArray<nlohmann::json> RecipeObjects;
	if (Parsed.is_array())
	{
		for (const auto& Item : Parsed)
		{
			RecipeObjects.Add(Item);
		}
	}
	else if (Parsed.is_object())
	{
		// Check $type
		if (Parsed.contains("$type") && Parsed["$type"].get<std::string>() == "ArcRecipeDefinition")
		{
			RecipeObjects.Add(Parsed);
		}
		else
		{
			UE_LOG(LogArcRecipeJsonLoader, Warning, TEXT("JSON object in %s does not have $type = ArcRecipeDefinition"), *FilePath);
			RecipeObjects.Add(Parsed);
		}
	}

	for (const nlohmann::json& JsonObj : RecipeObjects)
	{
		const FString RecipeName = FPaths::GetBaseFilename(FilePath);
		UArcRecipeDefinition* Recipe = NewObject<UArcRecipeDefinition>(
			Outer ? Outer : GetTransientPackage(),
			*RecipeName);

		if (ParseJson(JsonObj, Recipe))
		{
			Result.Add(Recipe);
		}
		else
		{
			UE_LOG(LogArcRecipeJsonLoader, Error, TEXT("Failed to parse recipe from %s"), *FilePath);
		}
	}

	return Result;
}

// -------------------------------------------------------------------
// ParseJson
// -------------------------------------------------------------------

bool UArcRecipeJsonLoader::ParseJson(const nlohmann::json& JsonObj, UArcRecipeDefinition* OutRecipe)
{
	if (!OutRecipe)
	{
		return false;
	}

	// id
	if (JsonObj.contains("id"))
	{
		const FString IdStr = UTF8_TO_TCHAR(JsonObj["id"].get<std::string>().c_str());
		if (!FGuid::Parse(IdStr, OutRecipe->RecipeId))
		{
			// Deterministic GUID from string
			OutRecipe->RecipeId = FGuid::NewDeterministicGuid(IdStr);
		}
	}

	// name
	if (JsonObj.contains("name"))
	{
		OutRecipe->RecipeName = FText::FromString(UTF8_TO_TCHAR(JsonObj["name"].get<std::string>().c_str()));
	}

	// craftTime
	if (JsonObj.contains("craftTime"))
	{
		OutRecipe->CraftTime = JsonObj["craftTime"].get<float>();
	}

	// tags
	if (JsonObj.contains("tags"))
	{
		OutRecipe->RecipeTags = ArcCraftJsonUtils::ParseGameplayTags(JsonObj["tags"]);
	}

	// requiredStationTags
	if (JsonObj.contains("requiredStationTags"))
	{
		OutRecipe->RequiredStationTags = ArcCraftJsonUtils::ParseGameplayTags(JsonObj["requiredStationTags"]);
	}

	// requiredInstigatorTags
	if (JsonObj.contains("requiredInstigatorTags"))
	{
		OutRecipe->RequiredInstigatorTags = ArcCraftJsonUtils::ParseGameplayTags(JsonObj["requiredInstigatorTags"]);
	}

	// qualityTierTable
	if (JsonObj.contains("qualityTierTable"))
	{
		const FString Path = UTF8_TO_TCHAR(JsonObj["qualityTierTable"].get<std::string>().c_str());
		OutRecipe->QualityTierTable = TSoftObjectPtr<UArcQualityTierTable>(FSoftObjectPath(Path));
	}

	// qualityAffectsLevel
	if (JsonObj.contains("qualityAffectsLevel"))
	{
		OutRecipe->bQualityAffectsLevel = JsonObj["qualityAffectsLevel"].get<bool>();
	}

	// ingredients
	if (JsonObj.contains("ingredients") && JsonObj["ingredients"].is_array())
	{
		OutRecipe->Ingredients.Empty();
		for (const auto& IngObj : JsonObj["ingredients"])
		{
			FInstancedStruct IngredientStruct;
			if (ParseIngredient(IngObj, IngredientStruct))
			{
				OutRecipe->Ingredients.Add(MoveTemp(IngredientStruct));
			}
			else
			{
				UE_LOG(LogArcRecipeJsonLoader, Warning, TEXT("Failed to parse ingredient in recipe '%s'"),
					*OutRecipe->RecipeName.ToString());
			}
		}
	}

	// output
	if (JsonObj.contains("output") && JsonObj["output"].is_object())
	{
		const auto& OutputObj = JsonObj["output"];

		if (OutputObj.contains("itemDefinition"))
		{
			const FString ItemDefStr = UTF8_TO_TCHAR(OutputObj["itemDefinition"].get<std::string>().c_str());
			OutRecipe->OutputItemDefinition = FArcNamedPrimaryAssetId(FPrimaryAssetId(ItemDefStr));
		}

		if (OutputObj.contains("amount"))
		{
			OutRecipe->OutputAmount = OutputObj["amount"].get<int32>();
		}

		if (OutputObj.contains("level"))
		{
			OutRecipe->OutputLevel = static_cast<uint8>(OutputObj["level"].get<int32>());
		}

		if (OutputObj.contains("modifiers") && OutputObj["modifiers"].is_array())
		{
			OutRecipe->OutputModifiers.Empty();
			for (const auto& ModObj : OutputObj["modifiers"])
			{
				FInstancedStruct ModStruct;
				if (ArcCraftJsonUtils::ParseOutputModifier(ModObj, ModStruct))
				{
					OutRecipe->OutputModifiers.Add(MoveTemp(ModStruct));
				}
			}
		}
	}

	return true;
}

// -------------------------------------------------------------------
// ParseIngredient
// -------------------------------------------------------------------

bool UArcRecipeJsonLoader::ParseIngredient(const nlohmann::json& IngObj, FInstancedStruct& OutIngredient)
{
	if (!IngObj.contains("type"))
	{
		UE_LOG(LogArcRecipeJsonLoader, Warning, TEXT("Ingredient missing 'type' field"));
		return false;
	}

	const std::string TypeStr = IngObj["type"].get<std::string>();

	if (TypeStr == "ItemDef")
	{
		FArcRecipeIngredient_ItemDef ItemDefIngredient;

		if (IngObj.contains("amount"))
		{
			ItemDefIngredient.Amount = IngObj["amount"].get<int32>();
		}

		if (IngObj.contains("consume"))
		{
			ItemDefIngredient.bConsumeOnCraft = IngObj["consume"].get<bool>();
		}

		if (IngObj.contains("slotName"))
		{
			ItemDefIngredient.SlotName = FText::FromString(UTF8_TO_TCHAR(IngObj["slotName"].get<std::string>().c_str()));
		}

		if (IngObj.contains("itemDefinition"))
		{
			const FString ItemDefStr = UTF8_TO_TCHAR(IngObj["itemDefinition"].get<std::string>().c_str());
			ItemDefIngredient.ItemDefinitionId = FArcNamedPrimaryAssetId(FPrimaryAssetId(ItemDefStr));
		}

		OutIngredient.InitializeAs<FArcRecipeIngredient_ItemDef>(ItemDefIngredient);
		return true;
	}
	else if (TypeStr == "Tags")
	{
		FArcRecipeIngredient_Tags TagsIngredient;

		if (IngObj.contains("amount"))
		{
			TagsIngredient.Amount = IngObj["amount"].get<int32>();
		}

		if (IngObj.contains("consume"))
		{
			TagsIngredient.bConsumeOnCraft = IngObj["consume"].get<bool>();
		}

		if (IngObj.contains("slotName"))
		{
			TagsIngredient.SlotName = FText::FromString(UTF8_TO_TCHAR(IngObj["slotName"].get<std::string>().c_str()));
		}

		if (IngObj.contains("requiredTags"))
		{
			TagsIngredient.RequiredTags = ArcCraftJsonUtils::ParseGameplayTags(IngObj["requiredTags"]);
		}

		if (IngObj.contains("denyTags"))
		{
			TagsIngredient.DenyTags = ArcCraftJsonUtils::ParseGameplayTags(IngObj["denyTags"]);
		}

		if (IngObj.contains("minimumTier"))
		{
			const FString TierStr = UTF8_TO_TCHAR(IngObj["minimumTier"].get<std::string>().c_str());
			TagsIngredient.MinimumTierTag = FGameplayTag::RequestGameplayTag(FName(*TierStr), false);
		}

		OutIngredient.InitializeAs<FArcRecipeIngredient_Tags>(TagsIngredient);
		return true;
	}

	UE_LOG(LogArcRecipeJsonLoader, Warning, TEXT("Unknown ingredient type: %hs"), TypeStr.c_str());
	return false;
}
