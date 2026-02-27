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

#include "ArcCraft/Recipe/ArcRecipeDefinition.h"

#include "ArcJsonIncludes.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Recipe/ArcRecipeOutput.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"
#include "ArcCraft/MaterialCraft/ArcMaterialOutputModifier.h"
#include "ArcCraft/Shared/ArcCraftJsonUtils.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

// -------------------------------------------------------------------
// Asset registry tag key names
// -------------------------------------------------------------------

const FName UArcRecipeDefinition::RecipeTagsName = TEXT("RecipeTags");
const FName UArcRecipeDefinition::RequiredStationTagsName = TEXT("RequiredStationTags");
const FName UArcRecipeDefinition::IngredientTagsName = TEXT("IngredientTags");
const FName UArcRecipeDefinition::IngredientItemDefsName = TEXT("IngredientItemDefs");
const FName UArcRecipeDefinition::OutputItemDefName = TEXT("OutputItemDef");
const FName UArcRecipeDefinition::IngredientCountName = TEXT("IngredientCount");

// -------------------------------------------------------------------
// Helper: serialize FGameplayTagContainer → comma-separated string
// -------------------------------------------------------------------
namespace Arc::RecipeDefinition
{
	static FString SerializeTagContainer(const FGameplayTagContainer& Tags)
	{
		FString Result;
		int32 Num = Tags.Num();
		int32 Idx = 0;
		for (const FGameplayTag& Tag : Tags)
		{
			Result += Tag.ToString();
			Idx++;
			if (Num > Idx)
			{
				Result += TEXT(",");
			}
		}
		return Result;
	}

	static FGameplayTagContainer DeserializeTagContainer(const FString& Str)
	{
		FGameplayTagContainer Container;
		if (Str.IsEmpty())
		{
			return Container;
		}

		TArray<FString> TagStrings;
		Str.ParseIntoArray(TagStrings, TEXT(","), true);

		for (const FString& TagStr : TagStrings)
		{
			FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
			if (Tag.IsValid())
			{
				Container.AddTag(Tag);
			}
		}

		return Container;
	}
}
// -------------------------------------------------------------------
// PostInitProperties — create AssetImportData subobject
// -------------------------------------------------------------------

void UArcRecipeDefinition::PostInitProperties()
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

UAssetImportData* UArcRecipeDefinition::GetAssetImportData() const
{
#if WITH_EDITORONLY_DATA
	return AssetImportData;
#else
	return nullptr;
#endif
}

// -------------------------------------------------------------------
// Asset registry tags
// -------------------------------------------------------------------

void UArcRecipeDefinition::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	// Recipe tags
	Context.AddTag(UObject::FAssetRegistryTag(
		RecipeTagsName,
		Arc::RecipeDefinition::SerializeTagContainer(RecipeTags),
		UObject::FAssetRegistryTag::TT_Hidden));

	// Required station tags
	Context.AddTag(UObject::FAssetRegistryTag(
		RequiredStationTagsName,
		Arc::RecipeDefinition::SerializeTagContainer(RequiredStationTags),
		UObject::FAssetRegistryTag::TT_Hidden));

	// Ingredient tags — union of all tag-based ingredient RequiredTags
	FGameplayTagContainer AllIngredientTags;
	FString AllItemDefs;
	for (int32 Idx = 0; Idx < Ingredients.Num(); ++Idx)
	{
		const FArcRecipeIngredient_Tags* TagIngredient = Ingredients[Idx].GetPtr<FArcRecipeIngredient_Tags>();
		if (TagIngredient)
		{
			AllIngredientTags.AppendTags(TagIngredient->RequiredTags);
		}

		const FArcRecipeIngredient_ItemDef* ItemDefIngredient = Ingredients[Idx].GetPtr<FArcRecipeIngredient_ItemDef>();
		if (ItemDefIngredient && ItemDefIngredient->ItemDefinitionId.IsValid())
		{
			if (!AllItemDefs.IsEmpty())
			{
				AllItemDefs += TEXT(",");
			}
			AllItemDefs += ItemDefIngredient->ItemDefinitionId.ToString();
		}
	}

	Context.AddTag(UObject::FAssetRegistryTag(
		IngredientTagsName,
		Arc::RecipeDefinition::SerializeTagContainer(AllIngredientTags),
		UObject::FAssetRegistryTag::TT_Hidden));

	// Ingredient item definitions
	Context.AddTag(UObject::FAssetRegistryTag(
		IngredientItemDefsName,
		AllItemDefs,
		UObject::FAssetRegistryTag::TT_Hidden));

	// Output item definition
	Context.AddTag(UObject::FAssetRegistryTag(
		OutputItemDefName,
		OutputItemDefinition.IsValid() ? OutputItemDefinition.ToString() : FString(),
		UObject::FAssetRegistryTag::TT_Hidden));

	// Ingredient count
	Context.AddTag(UObject::FAssetRegistryTag(
		IngredientCountName,
		FString::FromInt(Ingredients.Num()),
		UObject::FAssetRegistryTag::TT_Hidden));

#if WITH_EDITORONLY_DATA
	if (AssetImportData)
	{
		Context.AddTag(UObject::FAssetRegistryTag(
			UObject::SourceFileTagName(),
			AssetImportData->GetSourceData().ToJson(),
			UObject::FAssetRegistryTag::TT_Hidden));
	}
#endif
}

// -------------------------------------------------------------------
// Static query helpers
// -------------------------------------------------------------------

FGameplayTagContainer UArcRecipeDefinition::GetTagsFromAssetData(const FAssetData& AssetData, FName TagKey)
{
	FString TagString;
	if (AssetData.GetTagValue(TagKey, TagString))
	{
		return Arc::RecipeDefinition::DeserializeTagContainer(TagString);
	}
	return FGameplayTagContainer();
}

FGameplayTagContainer UArcRecipeDefinition::GetRecipeTagsFromAssetData(const FAssetData& AssetData)
{
	return GetTagsFromAssetData(AssetData, RecipeTagsName);
}

FGameplayTagContainer UArcRecipeDefinition::GetRequiredStationTagsFromAssetData(const FAssetData& AssetData)
{
	return GetTagsFromAssetData(AssetData, RequiredStationTagsName);
}

FGameplayTagContainer UArcRecipeDefinition::GetIngredientTagsFromAssetData(const FAssetData& AssetData)
{
	return GetTagsFromAssetData(AssetData, IngredientTagsName);
}

int32 UArcRecipeDefinition::GetIngredientCountFromAssetData(const FAssetData& AssetData)
{
	FString CountStr;
	if (AssetData.GetTagValue(IngredientCountName, CountStr))
	{
		return FCString::Atoi(*CountStr);
	}
	return 0;
}

TArray<FAssetData> UArcRecipeDefinition::FilterByStationTags(
	const TArray<FAssetData>& Recipes,
	const FGameplayTagContainer& StationTags)
{
	TArray<FAssetData> Filtered;
	for (const FAssetData& Data : Recipes)
	{
		FGameplayTagContainer RequiredTags = GetRequiredStationTagsFromAssetData(Data);
		// Recipe with no station requirements matches any station
		if (RequiredTags.Num() == 0 || StationTags.HasAll(RequiredTags))
		{
			Filtered.Add(Data);
		}
	}
	return Filtered;
}

// -------------------------------------------------------------------
// Primary asset ID
// -------------------------------------------------------------------

FPrimaryAssetId UArcRecipeDefinition::GetPrimaryAssetId() const
{
	if (RecipeType.IsValid())
	{
		return FPrimaryAssetId(RecipeType, *RecipeId.ToString());
	}
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("ArcRecipe")), *RecipeId.ToString());
}

void UArcRecipeDefinition::PreSave(FObjectPreSaveContext SaveContext)
{
	if (SaveContext.IsCooking())
	{
		Super::PreSave(SaveContext);
		return;
	}

	if (!RecipeId.IsValid())
	{
		RecipeId = FGuid::NewGuid();
	}

	Super::PreSave(SaveContext);
}

void UArcRecipeDefinition::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		RecipeId = FGuid::NewGuid();
	}
}

void UArcRecipeDefinition::RegenerateRecipeId()
{
	RecipeId = FGuid::NewGuid();
}

// -------------------------------------------------------------------
// JSON Export
// -------------------------------------------------------------------

void UArcRecipeDefinition::ExportToJson()
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

	JsonObj["$schema"] = TCHAR_TO_UTF8(*ArcCraftJsonUtils::GetSchemaFilePath(TEXT("recipe-definition.schema.json")));
	JsonObj["$type"] = "ArcRecipeDefinition";

	JsonObj["id"] = TCHAR_TO_UTF8(*RecipeId.ToString());
	JsonObj["name"] = TCHAR_TO_UTF8(*RecipeName.ToString());
	JsonObj["craftTime"] = CraftTime;

	// Tags
	if (RecipeTags.Num() > 0)
	{
		JsonObj["tags"] = ArcCraftJsonUtils::SerializeGameplayTags(RecipeTags);
	}
	if (RequiredStationTags.Num() > 0)
	{
		JsonObj["requiredStationTags"] = ArcCraftJsonUtils::SerializeGameplayTags(RequiredStationTags);
	}
	if (RequiredInstigatorTags.Num() > 0)
	{
		JsonObj["requiredInstigatorTags"] = ArcCraftJsonUtils::SerializeGameplayTags(RequiredInstigatorTags);
	}

	// Quality tier table
	if (!QualityTierTable.IsNull())
	{
		JsonObj["qualityTierTable"] = TCHAR_TO_UTF8(*QualityTierTable.ToSoftObjectPath().ToString());
	}
	JsonObj["qualityAffectsLevel"] = bQualityAffectsLevel;

	// Ingredients
	if (Ingredients.Num() > 0)
	{
		nlohmann::json IngredientsArr = nlohmann::json::array();
		for (const FInstancedStruct& IngredientStruct : Ingredients)
		{
			const FArcRecipeIngredient_ItemDef* ItemDefIngredient = IngredientStruct.GetPtr<FArcRecipeIngredient_ItemDef>();
			const FArcRecipeIngredient_Tags* TagsIngredient = IngredientStruct.GetPtr<FArcRecipeIngredient_Tags>();

			if (ItemDefIngredient)
			{
				nlohmann::json IngObj;
				IngObj["type"] = "ItemDef";
				IngObj["amount"] = ItemDefIngredient->Amount;
				IngObj["consume"] = ItemDefIngredient->bConsumeOnCraft;
				if (!ItemDefIngredient->SlotName.IsEmpty())
				{
					IngObj["slotName"] = TCHAR_TO_UTF8(*ItemDefIngredient->SlotName.ToString());
				}
				if (ItemDefIngredient->ItemDefinitionId.IsValid())
				{
					IngObj["itemDefinition"] = TCHAR_TO_UTF8(*ItemDefIngredient->ItemDefinitionId.AssetId.ToString());
				}
				IngredientsArr.push_back(IngObj);
			}
			else if (TagsIngredient)
			{
				nlohmann::json IngObj;
				IngObj["type"] = "Tags";
				IngObj["amount"] = TagsIngredient->Amount;
				IngObj["consume"] = TagsIngredient->bConsumeOnCraft;
				if (!TagsIngredient->SlotName.IsEmpty())
				{
					IngObj["slotName"] = TCHAR_TO_UTF8(*TagsIngredient->SlotName.ToString());
				}
				if (TagsIngredient->RequiredTags.Num() > 0)
				{
					IngObj["requiredTags"] = ArcCraftJsonUtils::SerializeGameplayTags(TagsIngredient->RequiredTags);
				}
				if (TagsIngredient->DenyTags.Num() > 0)
				{
					IngObj["denyTags"] = ArcCraftJsonUtils::SerializeGameplayTags(TagsIngredient->DenyTags);
				}
				if (TagsIngredient->MinimumTierTag.IsValid())
				{
					IngObj["minimumTier"] = TCHAR_TO_UTF8(*TagsIngredient->MinimumTierTag.ToString());
				}
				IngredientsArr.push_back(IngObj);
			}
		}
		JsonObj["ingredients"] = IngredientsArr;
	}

	// Output
	{
		nlohmann::json OutputObj;
		if (OutputItemDefinition.IsValid())
		{
			OutputObj["itemDefinition"] = TCHAR_TO_UTF8(*OutputItemDefinition.AssetId.ToString());
		}
		OutputObj["amount"] = OutputAmount;
		OutputObj["level"] = static_cast<int32>(OutputLevel);

		// Output modifiers
		if (OutputModifiers.Num() > 0)
		{
			nlohmann::json ModifiersArr = nlohmann::json::array();
			for (const FInstancedStruct& ModStruct : OutputModifiers)
			{
				nlohmann::json ModJson = ArcCraftJsonUtils::SerializeOutputModifier(ModStruct);
				if (!ModJson.is_null())
				{
					ModifiersArr.push_back(ModJson);
				}
			}
			OutputObj["modifiers"] = ModifiersArr;
		}
		JsonObj["output"] = OutputObj;
	}

	const FString JsonStr = UTF8_TO_TCHAR(JsonObj.dump(1, '\t').c_str());
	if (FFileHelper::SaveStringToFile(JsonStr, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Log, TEXT("ExportToJson: Exported recipe to %s"), *FilePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ExportToJson: Failed to write %s"), *FilePath);
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
EDataValidationResult UArcRecipeDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!RecipeId.IsValid())
	{
		Context.AddWarning(FText::FromString(TEXT("Recipe has no valid RecipeId GUID. It will be generated on save.")));
	}

	if (!OutputItemDefinition.IsValid())
	{
		Context.AddError(FText::FromString(TEXT("Recipe must have a valid output item definition.")));
		Result = EDataValidationResult::Invalid;
	}

	if (Ingredients.Num() == 0)
	{
		Context.AddWarning(FText::FromString(TEXT("Recipe has no ingredients.")));
	}

	if (CraftTime <= 0.0f)
	{
		Context.AddWarning(FText::FromString(TEXT("Recipe craft time is zero or negative.")));
	}

	for (int32 Idx = 0; Idx < Ingredients.Num(); ++Idx)
	{
		const FArcRecipeIngredient* Ingredient = Ingredients[Idx].GetPtr<FArcRecipeIngredient>();
		if (!Ingredient)
		{
			Context.AddError(FText::Format(
				FText::FromString(TEXT("Ingredient slot {0} has invalid type.")), FText::AsNumber(Idx)));
			Result = EDataValidationResult::Invalid;
		}
		else if (Ingredient->Amount <= 0)
		{
			Context.AddError(FText::Format(
				FText::FromString(TEXT("Ingredient slot {0} has zero or negative amount.")), FText::AsNumber(Idx)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif
