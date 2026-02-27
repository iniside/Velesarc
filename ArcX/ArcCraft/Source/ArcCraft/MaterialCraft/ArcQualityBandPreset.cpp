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

#include "ArcCraft/MaterialCraft/ArcQualityBandPreset.h"

#include "ArcJsonIncludes.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"
#include "ArcCraft/Recipe/ArcRecipeOutput.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"
#include "ArcCraft/MaterialCraft/ArcMaterialOutputModifier.h"
#include "ArcCraft/Shared/ArcCraftJsonUtils.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"
#include "UObject/Package.h"

#include "Misc/DataValidation.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcQualityBandPresetJson, Log, All);

// (No XML helpers needed -- JSON export uses ArcCraftJsonUtils)

// -------------------------------------------------------------------
// PostInitProperties
// -------------------------------------------------------------------

void UArcQualityBandPreset::PostInitProperties()
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

UAssetImportData* UArcQualityBandPreset::GetAssetImportData() const
{
#if WITH_EDITORONLY_DATA
	return AssetImportData;
#else
	return nullptr;
#endif
}

// -------------------------------------------------------------------
// ExportToJson
// -------------------------------------------------------------------

void UArcQualityBandPreset::ExportToJson()
{
#if WITH_EDITOR
	const FString PackagePath = GetOutermost()->GetName();
	FString FilePath;
	if (!FPackageName::TryConvertLongPackageNameToFilename(PackagePath, FilePath, TEXT(".json")))
	{
		UE_LOG(LogArcQualityBandPresetJson, Error, TEXT("ExportToJson: Could not resolve file path for %s"), *PackagePath);
		return;
	}

	nlohmann::json JsonObj;

	JsonObj["$schema"] = TCHAR_TO_UTF8(*ArcCraftJsonUtils::GetSchemaFilePath(TEXT("quality-band-preset.schema.json")));
	JsonObj["$type"] = "ArcQualityBandPreset";
	JsonObj["name"] = TCHAR_TO_UTF8(*PresetName.ToString());

	nlohmann::json BandsArr = nlohmann::json::array();
	for (const FArcMaterialQualityBand& Band : QualityBands)
	{
		BandsArr.push_back(ArcCraftJsonUtils::SerializeQualityBand(Band));
	}
	JsonObj["qualityBands"] = BandsArr;

	const FString JsonStr = UTF8_TO_TCHAR(JsonObj.dump(1, '\t').c_str());
	if (FFileHelper::SaveStringToFile(JsonStr, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogArcQualityBandPresetJson, Log, TEXT("ExportToJson: Exported quality band preset to %s"), *FilePath);
	}
	else
	{
		UE_LOG(LogArcQualityBandPresetJson, Error, TEXT("ExportToJson: Failed to write %s"), *FilePath);
	}
#endif // WITH_EDITOR
}

// -------------------------------------------------------------------
// Validation
// -------------------------------------------------------------------

#if WITH_EDITOR

EDataValidationResult UArcQualityBandPreset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (QualityBands.Num() == 0)
	{
		Context.AddWarning(FText::FromString(
			FString::Printf(TEXT("Quality band preset '%s' has no bands."),
				*PresetName.ToString())));
	}

	for (int32 BandIdx = 0; BandIdx < QualityBands.Num(); ++BandIdx)
	{
		const FArcMaterialQualityBand& Band = QualityBands[BandIdx];
		const FString BandName = Band.BandName.IsEmpty()
			? FString::Printf(TEXT("Band[%d]"), BandIdx)
			: Band.BandName.ToString();

		if (Band.BaseWeight <= 0.0f)
		{
			Context.AddError(FText::FromString(
				FString::Printf(TEXT("%s: BaseWeight must be > 0 (got %.3f)."),
					*BandName, Band.BaseWeight)));
			Result = EDataValidationResult::Invalid;
		}

		if (Band.Modifiers.Num() == 0)
		{
			Context.AddWarning(FText::FromString(
				FString::Printf(TEXT("%s: Band has no modifiers."), *BandName)));
		}

		if (Band.MinQuality < 0.0f)
		{
			Context.AddError(FText::FromString(
				FString::Printf(TEXT("%s: MinQuality must be >= 0 (got %.3f)."),
					*BandName, Band.MinQuality)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}

#endif
