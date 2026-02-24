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

#include "ArcCraft/Recipe/ArcRandomPoolJsonLoader.h"

#include "ArcCraft/Recipe/ArcRandomPoolDefinition.h"
#include "ArcCraft/Shared/ArcCraftJsonUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcRandomPoolJsonLoader, Log, All);

// -------------------------------------------------------------------
// LoadFromFile
// -------------------------------------------------------------------

UArcRandomPoolDefinition* UArcRandomPoolJsonLoader::LoadFromFile(UObject* Outer, const FString& FilePath)
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		UE_LOG(LogArcRandomPoolJsonLoader, Error, TEXT("Failed to read file: %s"), *FilePath);
		return nullptr;
	}

	nlohmann::json Parsed;
	try
	{
		Parsed = nlohmann::json::parse(TCHAR_TO_UTF8(*FileContent));
	}
	catch (const nlohmann::json::parse_error& e)
	{
		UE_LOG(LogArcRandomPoolJsonLoader, Error, TEXT("JSON parse error in %s: %hs"), *FilePath, e.what());
		return nullptr;
	}

	const FString AssetName = FPaths::GetBaseFilename(FilePath);
	UArcRandomPoolDefinition* Pool = NewObject<UArcRandomPoolDefinition>(
		Outer ? Outer : GetTransientPackage(),
		*AssetName);

	if (ParseJson(Parsed, Pool))
	{
		return Pool;
	}

	UE_LOG(LogArcRandomPoolJsonLoader, Error, TEXT("Failed to parse random pool from %s"), *FilePath);
	return nullptr;
}

// -------------------------------------------------------------------
// ParseJson
// -------------------------------------------------------------------

bool UArcRandomPoolJsonLoader::ParseJson(const nlohmann::json& JsonObj, UArcRandomPoolDefinition* OutPool)
{
	if (!OutPool)
	{
		return false;
	}

	// name
	if (JsonObj.contains("name"))
	{
		OutPool->PoolName = FText::FromString(UTF8_TO_TCHAR(JsonObj["name"].get<std::string>().c_str()));
	}

	// entries
	if (JsonObj.contains("entries") && JsonObj["entries"].is_array())
	{
		OutPool->Entries.Empty();
		for (const auto& EntryObj : JsonObj["entries"])
		{
			FArcRandomPoolEntry Entry;

			if (EntryObj.contains("name"))
			{
				Entry.DisplayName = FText::FromString(UTF8_TO_TCHAR(EntryObj["name"].get<std::string>().c_str()));
			}

			if (EntryObj.contains("minQuality"))
			{
				Entry.MinQualityThreshold = EntryObj["minQuality"].get<float>();
			}

			if (EntryObj.contains("baseWeight"))
			{
				Entry.BaseWeight = EntryObj["baseWeight"].get<float>();
			}

			if (EntryObj.contains("qualityWeightScaling"))
			{
				Entry.QualityWeightScaling = EntryObj["qualityWeightScaling"].get<float>();
			}

			if (EntryObj.contains("cost"))
			{
				Entry.Cost = EntryObj["cost"].get<float>();
			}

			if (EntryObj.contains("valueScale"))
			{
				Entry.ValueScale = EntryObj["valueScale"].get<float>();
			}

			if (EntryObj.contains("valueSkew"))
			{
				Entry.ValueSkew = EntryObj["valueSkew"].get<float>();
			}

			if (EntryObj.contains("scaleByQuality"))
			{
				Entry.bScaleByQuality = EntryObj["scaleByQuality"].get<bool>();
			}

			// requiredIngredientTags
			if (EntryObj.contains("requiredIngredientTags"))
			{
				Entry.RequiredIngredientTags = ArcCraftJsonUtils::ParseGameplayTags(EntryObj["requiredIngredientTags"]);
			}

			// denyIngredientTags
			if (EntryObj.contains("denyIngredientTags"))
			{
				Entry.DenyIngredientTags = ArcCraftJsonUtils::ParseGameplayTags(EntryObj["denyIngredientTags"]);
			}

			// weightModifiers
			if (EntryObj.contains("weightModifiers") && EntryObj["weightModifiers"].is_array())
			{
				for (const auto& WModObj : EntryObj["weightModifiers"])
				{
					FArcRandomPoolWeightModifier WMod;

					if (WModObj.contains("bonusWeight"))
					{
						WMod.BonusWeight = WModObj["bonusWeight"].get<float>();
					}

					if (WModObj.contains("weightMultiplier"))
					{
						WMod.WeightMultiplier = WModObj["weightMultiplier"].get<float>();
					}

					if (WModObj.contains("requiredTags"))
					{
						WMod.RequiredTags = ArcCraftJsonUtils::ParseGameplayTags(WModObj["requiredTags"]);
					}

					Entry.WeightModifiers.Add(WMod);
				}
			}

			// valueSkewRules
			if (EntryObj.contains("valueSkewRules") && EntryObj["valueSkewRules"].is_array())
			{
				for (const auto& SkewObj : EntryObj["valueSkewRules"])
				{
					FArcRandomPoolValueSkew Skew;

					if (SkewObj.contains("valueScale"))
					{
						Skew.ValueScale = SkewObj["valueScale"].get<float>();
					}

					if (SkewObj.contains("valueOffset"))
					{
						Skew.ValueOffset = SkewObj["valueOffset"].get<float>();
					}

					if (SkewObj.contains("requiredTags"))
					{
						Skew.RequiredTags = ArcCraftJsonUtils::ParseGameplayTags(SkewObj["requiredTags"]);
					}

					Entry.ValueSkewRules.Add(Skew);
				}
			}

			// modifiers
			if (EntryObj.contains("modifiers") && EntryObj["modifiers"].is_array())
			{
				for (const auto& ModObj : EntryObj["modifiers"])
				{
					FInstancedStruct ModStruct;
					if (ArcCraftJsonUtils::ParseOutputModifier(ModObj, ModStruct))
					{
						Entry.Modifiers.Add(MoveTemp(ModStruct));
					}
				}
			}

			OutPool->Entries.Add(MoveTemp(Entry));
		}
	}

	return true;
}
