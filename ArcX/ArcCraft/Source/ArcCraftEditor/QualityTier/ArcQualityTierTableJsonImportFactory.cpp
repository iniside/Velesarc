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

#include "QualityTier/ArcQualityTierTableJsonImportFactory.h"

#include "ArcCraft/Recipe/ArcRecipeQuality.h"
#include "ArcCraft/Recipe/ArcQualityTierTableJsonLoader.h"
#include "ArcJsonIncludes.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcQualityTierJsonImport, Log, All);

// -------------------------------------------------------------------
// Constructor
// -------------------------------------------------------------------

UArcQualityTierTableJsonImportFactory::UArcQualityTierTableJsonImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	bEditorImport = true;
	bText = false;
	SupportedClass = UArcQualityTierTable::StaticClass();
	Formats.Add(TEXT("json;JSON Quality Tier Table"));
}

// -------------------------------------------------------------------
// UFactory: FactoryCanImport
// -------------------------------------------------------------------

bool UArcQualityTierTableJsonImportFactory::FactoryCanImport(const FString& Filename)
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
		return JsonObj.contains("$type") && JsonObj["$type"] == "ArcQualityTierTable";
	}
	catch (...)
	{
		return false;
	}
}

// -------------------------------------------------------------------
// UFactory: FactoryCreateFile
// -------------------------------------------------------------------

UObject* UArcQualityTierTableJsonImportFactory::FactoryCreateFile(
	UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn,
	bool& bOutOperationCanceled)
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *Filename))
	{
		UE_LOG(LogArcQualityTierJsonImport, Error, TEXT("Failed to read JSON file: %s"), *Filename);
		return nullptr;
	}

	nlohmann::json JsonRoot;
	try
	{
		JsonRoot = nlohmann::json::parse(TCHAR_TO_UTF8(*FileContent));
	}
	catch (const nlohmann::json::parse_error& e)
	{
		UE_LOG(LogArcQualityTierJsonImport, Error, TEXT("Failed to parse JSON file: %s - %hs"),
			*Filename, e.what());
		return nullptr;
	}

	if (!JsonRoot.contains("$type") || JsonRoot["$type"] != "ArcQualityTierTable")
	{
		UE_LOG(LogArcQualityTierJsonImport, Error, TEXT("Expected $type \"ArcQualityTierTable\" in: %s"), *Filename);
		return nullptr;
	}

	UArcQualityTierTable* NewTable = NewObject<UArcQualityTierTable>(
		InParent, InClass, InName, Flags);

	if (UArcQualityTierTableJsonLoader::ParseJson(JsonRoot, NewTable))
	{
		if (UAssetImportData* ImportData = NewTable->GetAssetImportData())
		{
			ImportData->Update(Filename);
		}

		UE_LOG(LogArcQualityTierJsonImport, Log, TEXT("Imported quality tier table from: %s"), *Filename);
		return NewTable;
	}

	UE_LOG(LogArcQualityTierJsonImport, Warning, TEXT("Failed to parse quality tier table from: %s"), *Filename);
	NewTable->MarkAsGarbage();
	return nullptr;
}

// -------------------------------------------------------------------
// FReimportHandler: CanReimport
// -------------------------------------------------------------------

bool UArcQualityTierTableJsonImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UArcQualityTierTable* Table = Cast<UArcQualityTierTable>(Obj);
	if (!Table)
	{
		return false;
	}

	const UAssetImportData* ImportData = Table->GetAssetImportData();
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

void UArcQualityTierTableJsonImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UArcQualityTierTable* Table = Cast<UArcQualityTierTable>(Obj);
	if (Table && NewReimportPaths.Num() > 0)
	{
		if (UAssetImportData* ImportData = Table->GetAssetImportData())
		{
			ImportData->UpdateFilenameOnly(NewReimportPaths[0]);
		}
	}
}

// -------------------------------------------------------------------
// FReimportHandler: Reimport
// -------------------------------------------------------------------

EReimportResult::Type UArcQualityTierTableJsonImportFactory::Reimport(UObject* Obj)
{
	UArcQualityTierTable* Table = Cast<UArcQualityTierTable>(Obj);
	if (!Table)
	{
		return EReimportResult::Failed;
	}

	const UAssetImportData* ImportData = Table->GetAssetImportData();
	if (!ImportData)
	{
		UE_LOG(LogArcQualityTierJsonImport, Error, TEXT("Table has no import data for reimport."));
		return EReimportResult::Failed;
	}

	const FString SourceFile = ImportData->GetFirstFilename();
	if (SourceFile.IsEmpty())
	{
		UE_LOG(LogArcQualityTierJsonImport, Error, TEXT("Table has no source file path stored."));
		return EReimportResult::Failed;
	}

	if (!FPaths::FileExists(SourceFile))
	{
		UE_LOG(LogArcQualityTierJsonImport, Error, TEXT("Source JSON file not found: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *SourceFile))
	{
		UE_LOG(LogArcQualityTierJsonImport, Error, TEXT("Failed to read JSON for reimport: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	nlohmann::json JsonRoot;
	try
	{
		JsonRoot = nlohmann::json::parse(TCHAR_TO_UTF8(*FileContent));
	}
	catch (const nlohmann::json::parse_error& e)
	{
		UE_LOG(LogArcQualityTierJsonImport, Error, TEXT("Failed to parse JSON for reimport: %s - %hs"),
			*SourceFile, e.what());
		return EReimportResult::Failed;
	}

	if (!JsonRoot.contains("$type") || JsonRoot["$type"] != "ArcQualityTierTable")
	{
		UE_LOG(LogArcQualityTierJsonImport, Error, TEXT("Expected $type \"ArcQualityTierTable\" in: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	ResetToDefaults(Table);

	if (!UArcQualityTierTableJsonLoader::ParseJson(JsonRoot, Table))
	{
		UE_LOG(LogArcQualityTierJsonImport, Error, TEXT("Failed to parse during reimport from: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	if (UAssetImportData* MutableImportData = Table->GetAssetImportData())
	{
		MutableImportData->Update(SourceFile);
	}

	Table->MarkPackageDirty();

	UE_LOG(LogArcQualityTierJsonImport, Log, TEXT("Successfully reimported quality tier table from: %s"), *SourceFile);
	return EReimportResult::Succeeded;
}

// -------------------------------------------------------------------
// FReimportHandler: GetPriority
// -------------------------------------------------------------------

int32 UArcQualityTierTableJsonImportFactory::GetPriority() const
{
	return ImportPriority;
}

// -------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------

void UArcQualityTierTableJsonImportFactory::ResetToDefaults(UArcQualityTierTable* Table)
{
	Table->Tiers.Reset();
}
