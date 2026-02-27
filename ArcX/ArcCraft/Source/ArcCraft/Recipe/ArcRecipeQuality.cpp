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

#include "ArcCraft/Recipe/ArcRecipeQuality.h"

#include "ArcJsonIncludes.h"
#include "ArcCraft/Shared/ArcCraftJsonUtils.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"
#include "UObject/Package.h"

// -------------------------------------------------------------------
// PostInitProperties / AssetImportData
// -------------------------------------------------------------------

void UArcQualityTierTable::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		if (!AssetImportData)
		{
			AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
		}
	}
#endif
}

UAssetImportData* UArcQualityTierTable::GetAssetImportData() const
{
#if WITH_EDITORONLY_DATA
	return AssetImportData;
#else
	return nullptr;
#endif
}

// -------------------------------------------------------------------
// JSON Export
// -------------------------------------------------------------------

void UArcQualityTierTable::ExportToJson()
{
#if WITH_EDITOR
	const FString PackagePath = GetOutermost()->GetName();
	FString FilePath;
	if (!FPackageName::TryConvertLongPackageNameToFilename(PackagePath, FilePath, TEXT(".json")))
	{
		UE_LOG(LogTemp, Error, TEXT("ExportToJson: Could not resolve file path for %s"), *PackagePath);
		return;
	}

	nlohmann::json JsonObj;

	JsonObj["$schema"] = TCHAR_TO_UTF8(*ArcCraftJsonUtils::GetSchemaFilePath(TEXT("quality-tier-table.schema.json")));
	JsonObj["$type"] = "ArcQualityTierTable";
	JsonObj["name"] = TCHAR_TO_UTF8(*GetName());

	nlohmann::json TiersArr = nlohmann::json::array();
	for (const FArcQualityTierMapping& Tier : Tiers)
	{
		nlohmann::json TierObj;
		TierObj["tag"] = Tier.TierTag.IsValid() ? TCHAR_TO_UTF8(*Tier.TierTag.ToString()) : "";
		TierObj["value"] = Tier.TierValue;
		TierObj["multiplier"] = Tier.QualityMultiplier;
		TiersArr.push_back(TierObj);
	}
	JsonObj["tiers"] = TiersArr;

	const FString JsonStr = UTF8_TO_TCHAR(JsonObj.dump(1, '\t').c_str());
	if (FFileHelper::SaveStringToFile(JsonStr, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Log, TEXT("ExportToJson: Exported quality tier table to %s"), *FilePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ExportToJson: Failed to write %s"), *FilePath);
	}
#endif
}

// -------------------------------------------------------------------
// Tier lookups
// -------------------------------------------------------------------

int32 UArcQualityTierTable::GetTierValue(const FGameplayTag& InTag) const
{
	for (const FArcQualityTierMapping& Mapping : Tiers)
	{
		if (Mapping.TierTag == InTag)
		{
			return Mapping.TierValue;
		}
	}
	return -1;
}

float UArcQualityTierTable::GetQualityMultiplier(const FGameplayTag& InTag) const
{
	for (const FArcQualityTierMapping& Mapping : Tiers)
	{
		if (Mapping.TierTag == InTag)
		{
			return Mapping.QualityMultiplier;
		}
	}
	return 1.0f;
}

FGameplayTag UArcQualityTierTable::FindBestTierTag(const FGameplayTagContainer& InTags) const
{
	FGameplayTag BestTag;
	int32 BestValue = -1;

	for (const FArcQualityTierMapping& Mapping : Tiers)
	{
		if (InTags.HasTag(Mapping.TierTag) && Mapping.TierValue > BestValue)
		{
			BestValue = Mapping.TierValue;
			BestTag = Mapping.TierTag;
		}
	}

	return BestTag;
}

float UArcQualityTierTable::EvaluateQuality(const FGameplayTagContainer& InItemTags) const
{
	const FGameplayTag BestTag = FindBestTierTag(InItemTags);
	if (BestTag.IsValid())
	{
		return GetQualityMultiplier(BestTag);
	}
	return 1.0f;
}
