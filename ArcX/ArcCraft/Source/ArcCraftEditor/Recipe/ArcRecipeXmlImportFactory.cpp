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

#include "Recipe/ArcRecipeXmlImportFactory.h"

#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeXmlLoader.h"
#include "EditorFramework/AssetImportData.h"
#include "XmlFile.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcRecipeXmlImport, Log, All);

// -------------------------------------------------------------------
// Constructor
// -------------------------------------------------------------------

UArcRecipeXmlImportFactory::UArcRecipeXmlImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	bEditorImport = true;
	bText = false;
	SupportedClass = UArcRecipeDefinition::StaticClass();
	Formats.Add(TEXT("xml;XML Recipe File"));
}

// -------------------------------------------------------------------
// UFactory: FactoryCanImport
// -------------------------------------------------------------------

bool UArcRecipeXmlImportFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename);
	if (!Extension.Equals(TEXT("xml"), ESearchCase::IgnoreCase))
	{
		return false;
	}

	// Quick-parse the XML to check root tag
	FXmlFile XmlFile(Filename);
	if (!XmlFile.IsValid())
	{
		return false;
	}

	const FXmlNode* RootNode = XmlFile.GetRootNode();
	if (!RootNode)
	{
		return false;
	}

	const FString& RootTag = RootNode->GetTag();
	return RootTag == TEXT("Recipes") || RootTag == TEXT("Recipe");
}

// -------------------------------------------------------------------
// UFactory: FactoryCreateFile
// -------------------------------------------------------------------

UObject* UArcRecipeXmlImportFactory::FactoryCreateFile(
	UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn,
	bool& bOutOperationCanceled)
{
	FXmlFile XmlFile(Filename);
	if (!XmlFile.IsValid())
	{
		UE_LOG(LogArcRecipeXmlImport, Error, TEXT("Failed to parse XML file: %s - %s"),
			*Filename, *XmlFile.GetLastError());
		return nullptr;
	}

	const FXmlNode* RootNode = XmlFile.GetRootNode();
	if (!RootNode)
	{
		UE_LOG(LogArcRecipeXmlImport, Error, TEXT("No root node in XML file: %s"), *Filename);
		return nullptr;
	}

	// Collect all <Recipe> nodes
	TArray<const FXmlNode*> RecipeNodes;
	if (RootNode->GetTag() == TEXT("Recipes"))
	{
		for (const FXmlNode* Child : RootNode->GetChildrenNodes())
		{
			if (Child->GetTag() == TEXT("Recipe"))
			{
				RecipeNodes.Add(Child);
			}
		}
	}
	else if (RootNode->GetTag() == TEXT("Recipe"))
	{
		RecipeNodes.Add(RootNode);
	}

	if (RecipeNodes.IsEmpty())
	{
		UE_LOG(LogArcRecipeXmlImport, Warning, TEXT("No <Recipe> nodes found in: %s"), *Filename);
		return nullptr;
	}

	UObject* FirstCreated = nullptr;

	for (int32 Idx = 0; Idx < RecipeNodes.Num(); ++Idx)
	{
		const FXmlNode* RecipeNode = RecipeNodes[Idx];

		// Determine asset name: use Name attribute from XML, or fall back to base name + index
		FString AssetName;
		const FString NameAttr = RecipeNode->GetAttribute(TEXT("Name"));
		if (!NameAttr.IsEmpty())
		{
			// Sanitize: replace invalid chars for asset names
			AssetName = NameAttr.Replace(TEXT(" "), TEXT("_"));
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

		if (UArcRecipeXmlLoader::ParseRecipeNode(RecipeNode, NewRecipe))
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

			UE_LOG(LogArcRecipeXmlImport, Log, TEXT("Imported recipe '%s' from: %s"),
				*NewRecipe->RecipeName.ToString(), *Filename);
		}
		else
		{
			UE_LOG(LogArcRecipeXmlImport, Warning,
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

bool UArcRecipeXmlImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
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

void UArcRecipeXmlImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
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

EReimportResult::Type UArcRecipeXmlImportFactory::Reimport(UObject* Obj)
{
	UArcRecipeDefinition* Recipe = Cast<UArcRecipeDefinition>(Obj);
	if (!Recipe)
	{
		return EReimportResult::Failed;
	}

	const UAssetImportData* ImportData = Recipe->GetAssetImportData();
	if (!ImportData)
	{
		UE_LOG(LogArcRecipeXmlImport, Error, TEXT("Recipe has no import data for reimport."));
		return EReimportResult::Failed;
	}

	const FString SourceFile = ImportData->GetFirstFilename();
	if (SourceFile.IsEmpty())
	{
		UE_LOG(LogArcRecipeXmlImport, Error, TEXT("Recipe has no source file path stored."));
		return EReimportResult::Failed;
	}

	if (!FPaths::FileExists(SourceFile))
	{
		UE_LOG(LogArcRecipeXmlImport, Error, TEXT("Source XML file not found: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	// Parse the XML file
	FXmlFile XmlFile(SourceFile);
	if (!XmlFile.IsValid())
	{
		UE_LOG(LogArcRecipeXmlImport, Error, TEXT("Failed to parse XML for reimport: %s - %s"),
			*SourceFile, *XmlFile.GetLastError());
		return EReimportResult::Failed;
	}

	const FXmlNode* RootNode = XmlFile.GetRootNode();
	if (!RootNode)
	{
		UE_LOG(LogArcRecipeXmlImport, Error, TEXT("No root node in XML for reimport: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	// Find the matching <Recipe> node (by Id or fall back to first)
	const FXmlNode* RecipeNode = FindMatchingRecipeNode(RootNode, Recipe->RecipeId);
	if (!RecipeNode)
	{
		UE_LOG(LogArcRecipeXmlImport, Error,
			TEXT("No matching <Recipe> node found in XML for reimport: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	// Full overwrite: reset to defaults, then re-parse
	ResetRecipeToDefaults(Recipe);

	if (!UArcRecipeXmlLoader::ParseRecipeNode(RecipeNode, Recipe))
	{
		UE_LOG(LogArcRecipeXmlImport, Error,
			TEXT("Failed to parse recipe during reimport from: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	// Update import data with fresh timestamp/hash
	if (UAssetImportData* MutableImportData = Recipe->GetAssetImportData())
	{
		MutableImportData->Update(SourceFile);
	}

	Recipe->MarkPackageDirty();

	UE_LOG(LogArcRecipeXmlImport, Log, TEXT("Successfully reimported recipe '%s' from: %s"),
		*Recipe->RecipeName.ToString(), *SourceFile);

	return EReimportResult::Succeeded;
}

// -------------------------------------------------------------------
// FReimportHandler: GetPriority
// -------------------------------------------------------------------

int32 UArcRecipeXmlImportFactory::GetPriority() const
{
	return ImportPriority;
}

// -------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------

const FXmlNode* UArcRecipeXmlImportFactory::FindMatchingRecipeNode(
	const FXmlNode* RootNode, const FGuid& RecipeId)
{
	TArray<const FXmlNode*> RecipeNodes;

	if (RootNode->GetTag() == TEXT("Recipes"))
	{
		for (const FXmlNode* Child : RootNode->GetChildrenNodes())
		{
			if (Child->GetTag() == TEXT("Recipe"))
			{
				RecipeNodes.Add(Child);
			}
		}
	}
	else if (RootNode->GetTag() == TEXT("Recipe"))
	{
		RecipeNodes.Add(RootNode);
	}

	if (RecipeNodes.IsEmpty())
	{
		return nullptr;
	}

	// Try to match by Id attribute
	if (RecipeId.IsValid())
	{
		for (const FXmlNode* Node : RecipeNodes)
		{
			const FString IdStr = Node->GetAttribute(TEXT("Id"));
			if (!IdStr.IsEmpty())
			{
				FGuid ParsedId;
				if (FGuid::Parse(IdStr, ParsedId) && ParsedId == RecipeId)
				{
					return Node;
				}

				// Also try deterministic GUID from string (matching ParseRecipeNode behavior)
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

void UArcRecipeXmlImportFactory::ResetRecipeToDefaults(UArcRecipeDefinition* Recipe)
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

	// RecipeId will be overwritten by ParseRecipeNode if XML has an Id attribute.
	// If not, it gets a new GUID, so we restore the saved one afterward only as fallback.
	Recipe->RecipeId = SavedId;
}
