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

#include "Recipe/ArcRecipeJsonImportFactory.h"

#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeJsonLoader.h"
#include "ArcJsonIncludes.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcRecipeJsonImport, Log, All);

// -------------------------------------------------------------------
// Constructor
// -------------------------------------------------------------------

UArcRecipeJsonImportFactory::UArcRecipeJsonImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	bEditorImport = true;
	bText = false;
	SupportedClass = UArcRecipeDefinition::StaticClass();
	Formats.Add(TEXT("json;JSON Recipe File"));
}

// -------------------------------------------------------------------
// UFactory: FactoryCanImport
// -------------------------------------------------------------------

bool UArcRecipeJsonImportFactory::FactoryCanImport(const FString& Filename)
{
	if (!FPaths::GetExtension(Filename).Equals(TEXT("json"), ESearchCase::IgnoreCase))
	{
		return false;
	}

	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *Filename))
	{
		return false;
	}

	try
	{
		nlohmann::json JsonObj = nlohmann::json::parse(TCHAR_TO_UTF8(*FileContent));

		// Single recipe object
		if (JsonObj.is_object())
		{
			return JsonObj.contains("$type") && JsonObj["$type"] == "ArcRecipeDefinition";
		}

		// Array of recipes
		if (JsonObj.is_array() && !JsonObj.empty())
		{
			const auto& First = JsonObj[0];
			return First.is_object() && First.contains("$type") && First["$type"] == "ArcRecipeDefinition";
		}

		return false;
	}
	catch (...)
	{
		return false;
	}
}

// -------------------------------------------------------------------
// UFactory: FactoryCreateFile
// -------------------------------------------------------------------

UObject* UArcRecipeJsonImportFactory::FactoryCreateFile(
	UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn,
	bool& bOutOperationCanceled)
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *Filename))
	{
		UE_LOG(LogArcRecipeJsonImport, Error, TEXT("Failed to read JSON file: %s"), *Filename);
		return nullptr;
	}

	nlohmann::json JsonRoot;
	try
	{
		JsonRoot = nlohmann::json::parse(TCHAR_TO_UTF8(*FileContent));
	}
	catch (const nlohmann::json::parse_error& e)
	{
		UE_LOG(LogArcRecipeJsonImport, Error, TEXT("Failed to parse JSON file: %s - %hs"),
			*Filename, e.what());
		return nullptr;
	}

	// Collect all recipe JSON objects
	TArray<const nlohmann::json*> RecipeNodes;
	if (JsonRoot.is_array())
	{
		for (const auto& Element : JsonRoot)
		{
			if (Element.is_object() && Element.contains("$type") && Element["$type"] == "ArcRecipeDefinition")
			{
				RecipeNodes.Add(&Element);
			}
		}
	}
	else if (JsonRoot.is_object())
	{
		RecipeNodes.Add(&JsonRoot);
	}

	if (RecipeNodes.IsEmpty())
	{
		UE_LOG(LogArcRecipeJsonImport, Warning, TEXT("No recipe objects found in: %s"), *Filename);
		return nullptr;
	}

	UObject* FirstCreated = nullptr;

	for (int32 Idx = 0; Idx < RecipeNodes.Num(); ++Idx)
	{
		const nlohmann::json& RecipeJson = *RecipeNodes[Idx];

		// Determine asset name: use "name" field from JSON, or fall back to base name + index
		FString AssetName;
		if (RecipeJson.contains("name") && RecipeJson["name"].is_string())
		{
			AssetName = UTF8_TO_TCHAR(RecipeJson["name"].get<std::string>().c_str());
			AssetName = AssetName.Replace(TEXT(" "), TEXT("_"));
			AssetName = AssetName.Replace(TEXT("/"), TEXT("_"));
		}
		else if (RecipeNodes.Num() == 1)
		{
			AssetName = InName.ToString();
		}
		else
		{
			AssetName = FString::Printf(TEXT("%s_%d"), *InName.ToString(), Idx);
		}

		const FName RecipeAssetName = *AssetName;

		UArcRecipeDefinition* NewRecipe = NewObject<UArcRecipeDefinition>(
			InParent, InClass, RecipeAssetName, Flags);

		if (UArcRecipeJsonLoader::ParseJson(RecipeJson, NewRecipe))
		{
			// Store source file path for reimport
			if (UAssetImportData* ImportData = NewRecipe->GetAssetImportData())
			{
				ImportData->Update(Filename);
			}

			if (!FirstCreated)
			{
				FirstCreated = NewRecipe;
			}

			UE_LOG(LogArcRecipeJsonImport, Log, TEXT("Imported recipe '%s' from: %s"),
				*NewRecipe->RecipeName.ToString(), *Filename);
		}
		else
		{
			UE_LOG(LogArcRecipeJsonImport, Warning,
				TEXT("Failed to parse recipe node %d from: %s"), Idx, *Filename);
			// Clean up failed object
			NewRecipe->MarkAsGarbage();
		}
	}

	return FirstCreated;
}

// -------------------------------------------------------------------
// FReimportHandler: CanReimport
// -------------------------------------------------------------------

bool UArcRecipeJsonImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UArcRecipeDefinition* Recipe = Cast<UArcRecipeDefinition>(Obj);
	if (!Recipe)
	{
		return false;
	}

	const UAssetImportData* ImportData = Recipe->GetAssetImportData();
	if (!ImportData)
	{
		return false;
	}

	const FString SourceFile = ImportData->GetFirstFilename();
	if (SourceFile.IsEmpty())
	{
		return false;
	}

	OutFilenames.Add(SourceFile);
	return true;
}

// -------------------------------------------------------------------
// FReimportHandler: SetReimportPaths
// -------------------------------------------------------------------

void UArcRecipeJsonImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UArcRecipeDefinition* Recipe = Cast<UArcRecipeDefinition>(Obj);
	if (Recipe && NewReimportPaths.Num() > 0)
	{
		if (UAssetImportData* ImportData = Recipe->GetAssetImportData())
		{
			ImportData->UpdateFilenameOnly(NewReimportPaths[0]);
		}
	}
}

// -------------------------------------------------------------------
// FReimportHandler: Reimport
// -------------------------------------------------------------------

EReimportResult::Type UArcRecipeJsonImportFactory::Reimport(UObject* Obj)
{
	UArcRecipeDefinition* Recipe = Cast<UArcRecipeDefinition>(Obj);
	if (!Recipe)
	{
		return EReimportResult::Failed;
	}

	const UAssetImportData* ImportData = Recipe->GetAssetImportData();
	if (!ImportData)
	{
		UE_LOG(LogArcRecipeJsonImport, Error, TEXT("Recipe has no import data for reimport."));
		return EReimportResult::Failed;
	}

	const FString SourceFile = ImportData->GetFirstFilename();
	if (SourceFile.IsEmpty())
	{
		UE_LOG(LogArcRecipeJsonImport, Error, TEXT("Recipe has no source file path stored."));
		return EReimportResult::Failed;
	}

	if (!FPaths::FileExists(SourceFile))
	{
		UE_LOG(LogArcRecipeJsonImport, Error, TEXT("Source JSON file not found: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	// Read and parse the JSON file
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *SourceFile))
	{
		UE_LOG(LogArcRecipeJsonImport, Error, TEXT("Failed to read JSON for reimport: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	nlohmann::json JsonRoot;
	try
	{
		JsonRoot = nlohmann::json::parse(TCHAR_TO_UTF8(*FileContent));
	}
	catch (const nlohmann::json::parse_error& e)
	{
		UE_LOG(LogArcRecipeJsonImport, Error, TEXT("Failed to parse JSON for reimport: %s - %hs"),
			*SourceFile, e.what());
		return EReimportResult::Failed;
	}

	// Find the matching recipe node (by id or fall back to first)
	const nlohmann::json* RecipeNode = FindMatchingRecipeNode(JsonRoot, Recipe->RecipeId);
	if (!RecipeNode)
	{
		UE_LOG(LogArcRecipeJsonImport, Error,
			TEXT("No matching recipe object found in JSON for reimport: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	// Full overwrite: reset to defaults, then re-parse
	ResetRecipeToDefaults(Recipe);

	if (!UArcRecipeJsonLoader::ParseJson(*RecipeNode, Recipe))
	{
		UE_LOG(LogArcRecipeJsonImport, Error,
			TEXT("Failed to parse recipe during reimport from: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	// Update import data with fresh timestamp/hash
	if (UAssetImportData* MutableImportData = Recipe->GetAssetImportData())
	{
		MutableImportData->Update(SourceFile);
	}

	Recipe->MarkPackageDirty();

	UE_LOG(LogArcRecipeJsonImport, Log, TEXT("Successfully reimported recipe '%s' from: %s"),
		*Recipe->RecipeName.ToString(), *SourceFile);

	return EReimportResult::Succeeded;
}

// -------------------------------------------------------------------
// FReimportHandler: GetPriority
// -------------------------------------------------------------------

int32 UArcRecipeJsonImportFactory::GetPriority() const
{
	return ImportPriority;
}

// -------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------

const nlohmann::json* UArcRecipeJsonImportFactory::FindMatchingRecipeNode(
	const nlohmann::json& JsonRoot, const FGuid& RecipeId)
{
	TArray<const nlohmann::json*> RecipeNodes;

	if (JsonRoot.is_array())
	{
		for (const auto& Element : JsonRoot)
		{
			if (Element.is_object() && Element.contains("$type") && Element["$type"] == "ArcRecipeDefinition")
			{
				RecipeNodes.Add(&Element);
			}
		}
	}
	else if (JsonRoot.is_object())
	{
		RecipeNodes.Add(&JsonRoot);
	}

	if (RecipeNodes.IsEmpty())
	{
		return nullptr;
	}

	// Try to match by "id" field
	if (RecipeId.IsValid())
	{
		for (const nlohmann::json* Node : RecipeNodes)
		{
			if (Node->contains("id") && (*Node)["id"].is_string())
			{
				const FString IdStr = UTF8_TO_TCHAR((*Node)["id"].get<std::string>().c_str());

				FGuid ParsedId;
				if (FGuid::Parse(IdStr, ParsedId) && ParsedId == RecipeId)
				{
					return Node;
				}

				// Also try deterministic GUID from string (matching ParseJson behavior)
				if (FGuid::NewDeterministicGuid(IdStr) == RecipeId)
				{
					return Node;
				}
			}
		}
	}

	// Fall back to first node
	return RecipeNodes[0];
}

void UArcRecipeJsonImportFactory::ResetRecipeToDefaults(UArcRecipeDefinition* Recipe)
{
	// Preserve the RecipeId (used for reimport matching) and AssetImportData
	const FGuid SavedId = Recipe->RecipeId;

	Recipe->RecipeName = FText::GetEmpty();
	Recipe->RecipeTags.Reset();
	Recipe->CraftTime = 5.0f;
	Recipe->RequiredStationTags.Reset();
	Recipe->RequiredInstigatorTags.Reset();
	Recipe->Ingredients.Reset();
	Recipe->OutputItemDefinition = FArcNamedPrimaryAssetId();
	Recipe->OutputAmount = 1;
	Recipe->OutputLevel = 1;
	Recipe->OutputModifiers.Reset();
	Recipe->QualityTierTable = nullptr;
	Recipe->bQualityAffectsLevel = false;

	// RecipeId will be overwritten by ParseJson if JSON has an "id" field.
	// If not, it gets a new GUID, so we restore the saved one afterward only as fallback.
	Recipe->RecipeId = SavedId;
}
