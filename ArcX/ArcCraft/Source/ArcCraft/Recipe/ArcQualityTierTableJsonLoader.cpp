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

#include "ArcCraft/Recipe/ArcQualityTierTableJsonLoader.h"

#include "ArcCraft/Recipe/ArcRecipeQuality.h"
#include "ArcCraft/Shared/ArcCraftJsonUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcQualityTierTableJsonLoader, Log, All);

// -------------------------------------------------------------------
// LoadFromFile
// -------------------------------------------------------------------

UArcQualityTierTable* UArcQualityTierTableJsonLoader::LoadFromFile(UObject* Outer, const FString& FilePath)
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		UE_LOG(LogArcQualityTierTableJsonLoader, Error, TEXT("Failed to read file: %s"), *FilePath);
		return nullptr;
	}

	nlohmann::json Parsed;
	try
	{
		Parsed = nlohmann::json::parse(TCHAR_TO_UTF8(*FileContent));
	}
	catch (const nlohmann::json::parse_error& e)
	{
		UE_LOG(LogArcQualityTierTableJsonLoader, Error, TEXT("JSON parse error in %s: %hs"), *FilePath, e.what());
		return nullptr;
	}

	const FString AssetName = FPaths::GetBaseFilename(FilePath);
	UArcQualityTierTable* Table = NewObject<UArcQualityTierTable>(
		Outer ? Outer : GetTransientPackage(),
		*AssetName);

	if (ParseJson(Parsed, Table))
	{
		return Table;
	}

	UE_LOG(LogArcQualityTierTableJsonLoader, Error, TEXT("Failed to parse quality tier table from %s"), *FilePath);
	return nullptr;
}

// -------------------------------------------------------------------
// ParseJson
// -------------------------------------------------------------------

bool UArcQualityTierTableJsonLoader::ParseJson(const nlohmann::json& JsonObj, UArcQualityTierTable* OutTable)
{
	if (!OutTable)
	{
		return false;
	}

	// tiers
	if (JsonObj.contains("tiers") && JsonObj["tiers"].is_array())
	{
		OutTable->Tiers.Empty();
		for (const auto& TierObj : JsonObj["tiers"])
		{
			FArcQualityTierMapping Mapping;

			if (TierObj.contains("tag"))
			{
				const FString TagStr = UTF8_TO_TCHAR(TierObj["tag"].get<std::string>().c_str());
				Mapping.TierTag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
			}

			if (TierObj.contains("value"))
			{
				Mapping.TierValue = TierObj["value"].get<int32>();
			}

			if (TierObj.contains("multiplier"))
			{
				Mapping.QualityMultiplier = TierObj["multiplier"].get<float>();
			}

			OutTable->Tiers.Add(Mapping);
		}
	}

	return true;
}
