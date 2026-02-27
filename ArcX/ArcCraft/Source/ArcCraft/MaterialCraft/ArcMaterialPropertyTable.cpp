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

#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTable.h"

#include "ArcJsonIncludes.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"
#include "ArcCraft/MaterialCraft/ArcQualityBandPreset.h"
#include "ArcCraft/Recipe/ArcRecipeOutput.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"
#include "ArcCraft/MaterialCraft/ArcMaterialOutputModifier.h"
#include "ArcCraft/Shared/ArcCraftJsonUtils.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"
#include "UObject/Package.h"

#include "Misc/DataValidation.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMaterialPropertyTableJson, Log, All);

// (No XML helpers needed -- JSON export uses ArcCraftJsonUtils)

// -------------------------------------------------------------------
// PostInitProperties
// -------------------------------------------------------------------

void UArcMaterialPropertyTable::PostInitProperties()
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

UAssetImportData* UArcMaterialPropertyTable::GetAssetImportData() const
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

void UArcMaterialPropertyTable::ExportToJson()
{
#if WITH_EDITOR
	const FString PackagePath = GetOutermost()->GetName();
	FString FilePath;
	if (!FPackageName::TryConvertLongPackageNameToFilename(PackagePath, FilePath, TEXT(".json")))
	{
		UE_LOG(LogArcMaterialPropertyTableJson, Error, TEXT("ExportToJson: Could not resolve file path for %s"), *PackagePath);
		return;
	}

	nlohmann::json JsonObj;

	JsonObj["$schema"] = TCHAR_TO_UTF8(*ArcCraftJsonUtils::GetSchemaFilePath(TEXT("material-property-table.schema.json")));
	JsonObj["$type"] = "ArcMaterialPropertyTable";
	JsonObj["name"] = TCHAR_TO_UTF8(*TableName.ToString());
	JsonObj["maxActiveRules"] = MaxActiveRules;

	if (!DefaultTierTable.IsNull())
	{
		JsonObj["defaultTierTable"] = TCHAR_TO_UTF8(*DefaultTierTable.ToSoftObjectPath().ToString());
	}

	JsonObj["extraIngredientWeightBonus"] = ExtraIngredientWeightBonus;
	JsonObj["extraTimeWeightBonusCap"] = ExtraTimeWeightBonusCap;
	JsonObj["baseBandBudget"] = BaseBandBudget;
	JsonObj["budgetPerQuality"] = BudgetPerQuality;

	// Rules
	nlohmann::json RulesArr = nlohmann::json::array();
	for (const FArcMaterialPropertyRule& Rule : Rules)
	{
		nlohmann::json RuleObj;

		RuleObj["name"] = TCHAR_TO_UTF8(*Rule.RuleName.ToString());
		RuleObj["priority"] = Rule.Priority;

		if (!Rule.QualityBandPreset.IsNull())
		{
			RuleObj["qualityBandPreset"] = TCHAR_TO_UTF8(*Rule.QualityBandPreset.ToSoftObjectPath().ToString());
		}

		// TagQuery
		if (!Rule.TagQuery.IsEmpty())
		{
			RuleObj["tagQuery"] = ArcCraftJsonUtils::SerializeTagQuery(Rule.TagQuery);
		}

		// RequiredRecipeTags
		if (Rule.RequiredRecipeTags.Num() > 0)
		{
			RuleObj["requiredRecipeTags"] = ArcCraftJsonUtils::SerializeGameplayTags(Rule.RequiredRecipeTags);
		}

		// OutputTags
		if (Rule.OutputTags.Num() > 0)
		{
			RuleObj["outputTags"] = ArcCraftJsonUtils::SerializeGameplayTags(Rule.OutputTags);
		}

		// QualityBands (inline)
		if (Rule.QualityBands.Num() > 0)
		{
			nlohmann::json BandsArr = nlohmann::json::array();
			for (const FArcMaterialQualityBand& Band : Rule.QualityBands)
			{
				BandsArr.push_back(ArcCraftJsonUtils::SerializeQualityBand(Band));
			}
			RuleObj["qualityBands"] = BandsArr;
		}

		RulesArr.push_back(RuleObj);
	}
	JsonObj["rules"] = RulesArr;

	const FString JsonStr = UTF8_TO_TCHAR(JsonObj.dump(1, '\t').c_str());
	if (FFileHelper::SaveStringToFile(JsonStr, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogArcMaterialPropertyTableJson, Log, TEXT("ExportToJson: Exported material property table to %s"), *FilePath);
	}
	else
	{
		UE_LOG(LogArcMaterialPropertyTableJson, Error, TEXT("ExportToJson: Failed to write %s"), *FilePath);
	}
#endif // WITH_EDITOR
}

// -------------------------------------------------------------------
// Validation
// -------------------------------------------------------------------

#if WITH_EDITOR

EDataValidationResult UArcMaterialPropertyTable::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Rules.Num() == 0)
	{
		Context.AddWarning(FText::FromString(
			FString::Printf(TEXT("Material property table '%s' has no rules."),
				*TableName.ToString())));
	}

	for (int32 RuleIdx = 0; RuleIdx < Rules.Num(); ++RuleIdx)
	{
		const FArcMaterialPropertyRule& Rule = Rules[RuleIdx];
		const FString RuleName = Rule.RuleName.IsEmpty()
			? FString::Printf(TEXT("Rule[%d]"), RuleIdx)
			: Rule.RuleName.ToString();

		if (Rule.TagQuery.IsEmpty())
		{
			Context.AddWarning(FText::FromString(
				FString::Printf(TEXT("%s: TagQuery is empty — rule will always match (no tag filter)."), *RuleName)));
		}

		// Validate effective quality bands (inline or from preset)
		const TArray<FArcMaterialQualityBand>& Bands = Rule.GetEffectiveQualityBands();

		if (!Rule.QualityBandPreset.IsNull() && Rule.QualityBandPreset.LoadSynchronous() == nullptr)
		{
			Context.AddError(FText::FromString(
				FString::Printf(TEXT("%s: QualityBandPreset is set but cannot be loaded."), *RuleName)));
			Result = EDataValidationResult::Invalid;
		}

		if (Bands.Num() == 0)
		{
			Context.AddWarning(FText::FromString(
				FString::Printf(TEXT("%s: No quality bands defined (inline or preset)."), *RuleName)));
		}

		for (int32 BandIdx = 0; BandIdx < Bands.Num(); ++BandIdx)
		{
			const FArcMaterialQualityBand& Band = Bands[BandIdx];
			const FString BandName = Band.BandName.IsEmpty()
				? FString::Printf(TEXT("Band[%d]"), BandIdx)
				: Band.BandName.ToString();

			if (Band.BaseWeight <= 0.0f)
			{
				Context.AddError(FText::FromString(
					FString::Printf(TEXT("%s / %s: BaseWeight must be > 0 (got %.3f)."),
						*RuleName, *BandName, Band.BaseWeight)));
				Result = EDataValidationResult::Invalid;
			}

			if (Band.Modifiers.Num() == 0)
			{
				Context.AddWarning(FText::FromString(
					FString::Printf(TEXT("%s / %s: Band has no modifiers."),
						*RuleName, *BandName)));
			}

			if (Band.MinQuality < 0.0f)
			{
				Context.AddError(FText::FromString(
					FString::Printf(TEXT("%s / %s: MinQuality must be >= 0 (got %.3f)."),
						*RuleName, *BandName, Band.MinQuality)));
				Result = EDataValidationResult::Invalid;
			}
		}

	}

	if (BaseBandBudget < 0.0f)
	{
		Context.AddError(FText::FromString(
			FString::Printf(TEXT("BaseBandBudget must be >= 0 (got %.3f)."), BaseBandBudget)));
		Result = EDataValidationResult::Invalid;
	}

	if (BudgetPerQuality < 0.0f)
	{
		Context.AddError(FText::FromString(
			FString::Printf(TEXT("BudgetPerQuality must be >= 0 (got %.3f)."), BudgetPerQuality)));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

#endif
