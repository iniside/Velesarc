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

#include "ArcCraft/Recipe/ArcRandomPoolDefinition.h"

#include "ArcJsonIncludes.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"
#include "ArcCraft/Recipe/ArcRecipeOutput.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"
#include "ArcCraft/MaterialCraft/ArcMaterialOutputModifier.h"
#include "ArcCraft/Shared/ArcCraftJsonUtils.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

// -------------------------------------------------------------------
// PostInitProperties — create AssetImportData subobject
// -------------------------------------------------------------------

void UArcRandomPoolDefinition::PostInitProperties()
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

UAssetImportData* UArcRandomPoolDefinition::GetAssetImportData() const
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

void UArcRandomPoolDefinition::ExportToJson()
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

	JsonObj["$schema"] = "../../Schemas/random-pool-definition.schema.json";
	JsonObj["$type"] = "ArcRandomPoolDefinition";
	JsonObj["name"] = TCHAR_TO_UTF8(*PoolName.ToString());

	nlohmann::json EntriesArr = nlohmann::json::array();
	for (const FArcRandomPoolEntry& Entry : Entries)
	{
		nlohmann::json EntryObj;

		EntryObj["name"] = TCHAR_TO_UTF8(*Entry.DisplayName.ToString());
		EntryObj["minQuality"] = Entry.MinQualityThreshold;
		EntryObj["baseWeight"] = Entry.BaseWeight;
		EntryObj["qualityWeightScaling"] = Entry.QualityWeightScaling;
		EntryObj["cost"] = Entry.Cost;
		EntryObj["valueScale"] = Entry.ValueScale;
		EntryObj["valueSkew"] = Entry.ValueSkew;
		EntryObj["scaleByQuality"] = Entry.bScaleByQuality;

		if (Entry.RequiredIngredientTags.Num() > 0)
		{
			EntryObj["requiredIngredientTags"] = ArcCraftJsonUtils::SerializeGameplayTags(Entry.RequiredIngredientTags);
		}

		if (Entry.DenyIngredientTags.Num() > 0)
		{
			EntryObj["denyIngredientTags"] = ArcCraftJsonUtils::SerializeGameplayTags(Entry.DenyIngredientTags);
		}

		// WeightModifiers
		if (Entry.WeightModifiers.Num() > 0)
		{
			nlohmann::json WModArr = nlohmann::json::array();
			for (const FArcRandomPoolWeightModifier& WMod : Entry.WeightModifiers)
			{
				nlohmann::json WModObj;
				WModObj["bonusWeight"] = WMod.BonusWeight;
				WModObj["weightMultiplier"] = WMod.WeightMultiplier;
				if (WMod.RequiredTags.Num() > 0)
				{
					WModObj["requiredTags"] = ArcCraftJsonUtils::SerializeGameplayTags(WMod.RequiredTags);
				}
				WModArr.push_back(WModObj);
			}
			EntryObj["weightModifiers"] = WModArr;
		}

		// ValueSkewRules
		if (Entry.ValueSkewRules.Num() > 0)
		{
			nlohmann::json SkewArr = nlohmann::json::array();
			for (const FArcRandomPoolValueSkew& Skew : Entry.ValueSkewRules)
			{
				nlohmann::json SkewObj;
				SkewObj["valueScale"] = Skew.ValueScale;
				SkewObj["valueOffset"] = Skew.ValueOffset;
				if (Skew.RequiredTags.Num() > 0)
				{
					SkewObj["requiredTags"] = ArcCraftJsonUtils::SerializeGameplayTags(Skew.RequiredTags);
				}
				SkewArr.push_back(SkewObj);
			}
			EntryObj["valueSkewRules"] = SkewArr;
		}

		// Modifiers
		if (Entry.Modifiers.Num() > 0)
		{
			nlohmann::json ModifiersArr = nlohmann::json::array();
			for (const FInstancedStruct& ModStruct : Entry.Modifiers)
			{
				nlohmann::json ModJson = ArcCraftJsonUtils::SerializeCraftModifier(ModStruct);
				if (!ModJson.is_null())
				{
					ModifiersArr.push_back(ModJson);
				}
			}
			EntryObj["modifiers"] = ModifiersArr;
		}

		EntriesArr.push_back(EntryObj);
	}
	JsonObj["entries"] = EntriesArr;

	const FString JsonStr = UTF8_TO_TCHAR(JsonObj.dump(1, '\t').c_str());
	if (FFileHelper::SaveStringToFile(JsonStr, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Log, TEXT("ExportToJson: Exported random pool to %s"), *FilePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ExportToJson: Failed to write %s"), *FilePath);
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
EDataValidationResult UArcRandomPoolDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Entries.Num() == 0)
	{
		Context.AddWarning(FText::FromString(TEXT("Random pool has no entries.")));
	}

	for (int32 Idx = 0; Idx < Entries.Num(); ++Idx)
	{
		const FArcRandomPoolEntry& Entry = Entries[Idx];

		if (Entry.Modifiers.Num() == 0)
		{
			Context.AddWarning(FText::Format(
				FText::FromString(TEXT("Pool entry {0} has no modifiers.")),
				FText::AsNumber(Idx)));
		}

		if (Entry.BaseWeight <= 0.0f)
		{
			Context.AddError(FText::Format(
				FText::FromString(TEXT("Pool entry {0} has zero or negative base weight.")),
				FText::AsNumber(Idx)));
			Result = EDataValidationResult::Invalid;
		}

		if (Entry.Cost < 0.0f)
		{
			Context.AddError(FText::Format(
				FText::FromString(TEXT("Pool entry {0} has negative cost.")),
				FText::AsNumber(Idx)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif
