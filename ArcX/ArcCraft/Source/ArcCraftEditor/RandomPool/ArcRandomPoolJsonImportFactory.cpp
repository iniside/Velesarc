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

#include "RandomPool/ArcRandomPoolJsonImportFactory.h"

#include "ArcCraft/Recipe/ArcRandomPoolDefinition.h"
#include "ArcCraft/Recipe/ArcRandomPoolJsonLoader.h"
#include "ArcJsonIncludes.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcRandomPoolJsonImport, Log, All);

// -------------------------------------------------------------------
// Constructor
// -------------------------------------------------------------------

UArcRandomPoolJsonImportFactory::UArcRandomPoolJsonImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	bEditorImport = true;
	bText = false;
	SupportedClass = UArcRandomPoolDefinition::StaticClass();
	Formats.Add(TEXT("json;JSON Random Pool File"));
}

// -------------------------------------------------------------------
// UFactory: FactoryCanImport
// -------------------------------------------------------------------

bool UArcRandomPoolJsonImportFactory::FactoryCanImport(const FString& Filename)
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
		return JsonObj.contains("$type") && JsonObj["$type"] == "ArcRandomPoolDefinition";
	}
	catch (...)
	{
		return false;
	}
}

// -------------------------------------------------------------------
// UFactory: FactoryCreateFile
// -------------------------------------------------------------------

UObject* UArcRandomPoolJsonImportFactory::FactoryCreateFile(
	UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn,
	bool& bOutOperationCanceled)
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *Filename))
	{
		UE_LOG(LogArcRandomPoolJsonImport, Error, TEXT("Failed to read JSON file: %s"), *Filename);
		return nullptr;
	}

	nlohmann::json JsonRoot;
	try
	{
		JsonRoot = nlohmann::json::parse(TCHAR_TO_UTF8(*FileContent));
	}
	catch (const nlohmann::json::parse_error& e)
	{
		UE_LOG(LogArcRandomPoolJsonImport, Error, TEXT("Failed to parse JSON file: %s - %hs"),
			*Filename, e.what());
		return nullptr;
	}

	if (!JsonRoot.contains("$type") || JsonRoot["$type"] != "ArcRandomPoolDefinition")
	{
		UE_LOG(LogArcRandomPoolJsonImport, Error, TEXT("Expected $type \"ArcRandomPoolDefinition\" in: %s"), *Filename);
		return nullptr;
	}

	UArcRandomPoolDefinition* NewPool = NewObject<UArcRandomPoolDefinition>(
		InParent, InClass, InName, Flags);

	if (UArcRandomPoolJsonLoader::ParseJson(JsonRoot, NewPool))
	{
		if (UAssetImportData* ImportData = NewPool->GetAssetImportData())
		{
			ImportData->Update(Filename);
		}

		UE_LOG(LogArcRandomPoolJsonImport, Log, TEXT("Imported random pool '%s' from: %s"),
			*NewPool->PoolName.ToString(), *Filename);
		return NewPool;
	}

	UE_LOG(LogArcRandomPoolJsonImport, Warning, TEXT("Failed to parse random pool from: %s"), *Filename);
	NewPool->MarkAsGarbage();
	return nullptr;
}

// -------------------------------------------------------------------
// FReimportHandler: CanReimport
// -------------------------------------------------------------------

bool UArcRandomPoolJsonImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UArcRandomPoolDefinition* Pool = Cast<UArcRandomPoolDefinition>(Obj);
	if (!Pool)
	{
		return false;
	}

	const UAssetImportData* ImportData = Pool->GetAssetImportData();
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

void UArcRandomPoolJsonImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UArcRandomPoolDefinition* Pool = Cast<UArcRandomPoolDefinition>(Obj);
	if (Pool && NewReimportPaths.Num() > 0)
	{
		if (UAssetImportData* ImportData = Pool->GetAssetImportData())
		{
			ImportData->UpdateFilenameOnly(NewReimportPaths[0]);
		}
	}
}

// -------------------------------------------------------------------
// FReimportHandler: Reimport
// -------------------------------------------------------------------

EReimportResult::Type UArcRandomPoolJsonImportFactory::Reimport(UObject* Obj)
{
	UArcRandomPoolDefinition* Pool = Cast<UArcRandomPoolDefinition>(Obj);
	if (!Pool)
	{
		return EReimportResult::Failed;
	}

	const UAssetImportData* ImportData = Pool->GetAssetImportData();
	if (!ImportData)
	{
		UE_LOG(LogArcRandomPoolJsonImport, Error, TEXT("Pool has no import data for reimport."));
		return EReimportResult::Failed;
	}

	const FString SourceFile = ImportData->GetFirstFilename();
	if (SourceFile.IsEmpty())
	{
		UE_LOG(LogArcRandomPoolJsonImport, Error, TEXT("Pool has no source file path stored."));
		return EReimportResult::Failed;
	}

	if (!FPaths::FileExists(SourceFile))
	{
		UE_LOG(LogArcRandomPoolJsonImport, Error, TEXT("Source JSON file not found: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *SourceFile))
	{
		UE_LOG(LogArcRandomPoolJsonImport, Error, TEXT("Failed to read JSON for reimport: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	nlohmann::json JsonRoot;
	try
	{
		JsonRoot = nlohmann::json::parse(TCHAR_TO_UTF8(*FileContent));
	}
	catch (const nlohmann::json::parse_error& e)
	{
		UE_LOG(LogArcRandomPoolJsonImport, Error, TEXT("Failed to parse JSON for reimport: %s - %hs"),
			*SourceFile, e.what());
		return EReimportResult::Failed;
	}

	if (!JsonRoot.contains("$type") || JsonRoot["$type"] != "ArcRandomPoolDefinition")
	{
		UE_LOG(LogArcRandomPoolJsonImport, Error, TEXT("Expected $type \"ArcRandomPoolDefinition\" in: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	ResetToDefaults(Pool);

	if (!UArcRandomPoolJsonLoader::ParseJson(JsonRoot, Pool))
	{
		UE_LOG(LogArcRandomPoolJsonImport, Error, TEXT("Failed to parse during reimport from: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	if (UAssetImportData* MutableImportData = Pool->GetAssetImportData())
	{
		MutableImportData->Update(SourceFile);
	}

	Pool->MarkPackageDirty();

	UE_LOG(LogArcRandomPoolJsonImport, Log, TEXT("Successfully reimported random pool '%s' from: %s"),
		*Pool->PoolName.ToString(), *SourceFile);

	return EReimportResult::Succeeded;
}

// -------------------------------------------------------------------
// FReimportHandler: GetPriority
// -------------------------------------------------------------------

int32 UArcRandomPoolJsonImportFactory::GetPriority() const
{
	return ImportPriority;
}

// -------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------

void UArcRandomPoolJsonImportFactory::ResetToDefaults(UArcRandomPoolDefinition* Pool)
{
	Pool->PoolName = FText::GetEmpty();
	Pool->Entries.Reset();
}
