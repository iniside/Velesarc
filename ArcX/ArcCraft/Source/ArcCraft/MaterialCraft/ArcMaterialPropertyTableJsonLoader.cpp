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

#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTableJsonLoader.h"

#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTable.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyRule.h"
#include "ArcCraft/MaterialCraft/ArcQualityBandPreset.h"
#include "ArcCraft/Recipe/ArcRecipeQuality.h"
#include "ArcCraft/Shared/ArcCraftJsonUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMaterialPropertyTableJsonLoader, Log, All);

// -------------------------------------------------------------------
// LoadFromFile (Table)
// -------------------------------------------------------------------

UArcMaterialPropertyTable* UArcMaterialPropertyTableJsonLoader::LoadFromFile(UObject* Outer, const FString& FilePath)
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		UE_LOG(LogArcMaterialPropertyTableJsonLoader, Error, TEXT("Failed to read file: %s"), *FilePath);
		return nullptr;
	}

	nlohmann::json Parsed;
	try
	{
		Parsed = nlohmann::json::parse(TCHAR_TO_UTF8(*FileContent));
	}
	catch (const nlohmann::json::parse_error& e)
	{
		UE_LOG(LogArcMaterialPropertyTableJsonLoader, Error, TEXT("JSON parse error in %s: %hs"), *FilePath, e.what());
		return nullptr;
	}

	const FString AssetName = FPaths::GetBaseFilename(FilePath);
	UArcMaterialPropertyTable* Table = NewObject<UArcMaterialPropertyTable>(
		Outer ? Outer : GetTransientPackage(),
		*AssetName);

	if (ParseJson(Parsed, Table))
	{
		return Table;
	}

	UE_LOG(LogArcMaterialPropertyTableJsonLoader, Error, TEXT("Failed to parse material property table from %s"), *FilePath);
	return nullptr;
}

// -------------------------------------------------------------------
// ParseJson (Table)
// -------------------------------------------------------------------

bool UArcMaterialPropertyTableJsonLoader::ParseJson(const nlohmann::json& JsonObj, UArcMaterialPropertyTable* OutTable)
{
	if (!OutTable)
	{
		return false;
	}

	// name
	if (JsonObj.contains("name"))
	{
		OutTable->TableName = FText::FromString(UTF8_TO_TCHAR(JsonObj["name"].get<std::string>().c_str()));
	}

	// maxActiveRules
	if (JsonObj.contains("maxActiveRules"))
	{
		OutTable->MaxActiveRules = JsonObj["maxActiveRules"].get<int32>();
	}

	// defaultTierTable
	if (JsonObj.contains("defaultTierTable"))
	{
		const FString Path = UTF8_TO_TCHAR(JsonObj["defaultTierTable"].get<std::string>().c_str());
		OutTable->DefaultTierTable = TSoftObjectPtr<UArcQualityTierTable>(FSoftObjectPath(Path));
	}

	// extraIngredientWeightBonus
	if (JsonObj.contains("extraIngredientWeightBonus"))
	{
		OutTable->ExtraIngredientWeightBonus = JsonObj["extraIngredientWeightBonus"].get<float>();
	}

	// extraTimeWeightBonusCap
	if (JsonObj.contains("extraTimeWeightBonusCap"))
	{
		OutTable->ExtraTimeWeightBonusCap = JsonObj["extraTimeWeightBonusCap"].get<float>();
	}

	// baseBandBudget
	if (JsonObj.contains("baseBandBudget"))
	{
		OutTable->BaseBandBudget = JsonObj["baseBandBudget"].get<float>();
	}

	// budgetPerQuality
	if (JsonObj.contains("budgetPerQuality"))
	{
		OutTable->BudgetPerQuality = JsonObj["budgetPerQuality"].get<float>();
	}

	// rules
	if (JsonObj.contains("rules") && JsonObj["rules"].is_array())
	{
		OutTable->Rules.Empty();
		for (const auto& RuleObj : JsonObj["rules"])
		{
			FArcMaterialPropertyRule Rule;

			if (RuleObj.contains("name"))
			{
				Rule.RuleName = FText::FromString(UTF8_TO_TCHAR(RuleObj["name"].get<std::string>().c_str()));
			}

			if (RuleObj.contains("priority"))
			{
				Rule.Priority = RuleObj["priority"].get<int32>();
			}

			if (RuleObj.contains("maxContributions"))
			{
				Rule.MaxContributions = RuleObj["maxContributions"].get<int32>();
			}

			// qualityBandPreset
			if (RuleObj.contains("qualityBandPreset"))
			{
				const FString Path = UTF8_TO_TCHAR(RuleObj["qualityBandPreset"].get<std::string>().c_str());
				Rule.QualityBandPreset = TSoftObjectPtr<UArcQualityBandPreset>(FSoftObjectPath(Path));
			}

			// tagQuery
			if (RuleObj.contains("tagQuery"))
			{
				Rule.TagQuery = ArcCraftJsonUtils::ParseTagQuery(RuleObj["tagQuery"]);
			}

			// requiredRecipeTags
			if (RuleObj.contains("requiredRecipeTags"))
			{
				Rule.RequiredRecipeTags = ArcCraftJsonUtils::ParseGameplayTags(RuleObj["requiredRecipeTags"]);
			}

			// outputTags
			if (RuleObj.contains("outputTags"))
			{
				Rule.OutputTags = ArcCraftJsonUtils::ParseGameplayTags(RuleObj["outputTags"]);
			}

			// qualityBands
			if (RuleObj.contains("qualityBands") && RuleObj["qualityBands"].is_array())
			{
				for (const auto& BandObj : RuleObj["qualityBands"])
				{
					FArcMaterialQualityBand Band;
					if (ArcCraftJsonUtils::ParseQualityBand(BandObj, Band))
					{
						Rule.QualityBands.Add(MoveTemp(Band));
					}
				}
			}

			OutTable->Rules.Add(MoveTemp(Rule));
		}
	}

	return true;
}

// -------------------------------------------------------------------
// LoadPresetFromFile
// -------------------------------------------------------------------

UArcQualityBandPreset* UArcMaterialPropertyTableJsonLoader::LoadPresetFromFile(UObject* Outer, const FString& FilePath)
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		UE_LOG(LogArcMaterialPropertyTableJsonLoader, Error, TEXT("Failed to read file: %s"), *FilePath);
		return nullptr;
	}

	nlohmann::json Parsed;
	try
	{
		Parsed = nlohmann::json::parse(TCHAR_TO_UTF8(*FileContent));
	}
	catch (const nlohmann::json::parse_error& e)
	{
		UE_LOG(LogArcMaterialPropertyTableJsonLoader, Error, TEXT("JSON parse error in %s: %hs"), *FilePath, e.what());
		return nullptr;
	}

	const FString AssetName = FPaths::GetBaseFilename(FilePath);
	UArcQualityBandPreset* Preset = NewObject<UArcQualityBandPreset>(
		Outer ? Outer : GetTransientPackage(),
		*AssetName);

	if (ParsePresetJson(Parsed, Preset))
	{
		return Preset;
	}

	UE_LOG(LogArcMaterialPropertyTableJsonLoader, Error, TEXT("Failed to parse quality band preset from %s"), *FilePath);
	return nullptr;
}

// -------------------------------------------------------------------
// ParsePresetJson
// -------------------------------------------------------------------

bool UArcMaterialPropertyTableJsonLoader::ParsePresetJson(const nlohmann::json& JsonObj, UArcQualityBandPreset* OutPreset)
{
	if (!OutPreset)
	{
		return false;
	}

	// name
	if (JsonObj.contains("name"))
	{
		OutPreset->PresetName = FText::FromString(UTF8_TO_TCHAR(JsonObj["name"].get<std::string>().c_str()));
	}

	// qualityBands
	if (JsonObj.contains("qualityBands") && JsonObj["qualityBands"].is_array())
	{
		OutPreset->QualityBands.Empty();
		for (const auto& BandObj : JsonObj["qualityBands"])
		{
			FArcMaterialQualityBand Band;
			if (ArcCraftJsonUtils::ParseQualityBand(BandObj, Band))
			{
				OutPreset->QualityBands.Add(MoveTemp(Band));
			}
		}
	}

	return true;
}
