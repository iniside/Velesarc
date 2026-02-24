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

#include "MaterialProperty/ArcQualityBandPresetJsonImportFactory.h"

#include "ArcCraft/MaterialCraft/ArcQualityBandPreset.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTableJsonLoader.h"
#include "ArcJsonIncludes.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcQualityBandPresetJsonImport, Log, All);

// -------------------------------------------------------------------
// Constructor
// -------------------------------------------------------------------

UArcQualityBandPresetJsonImportFactory::UArcQualityBandPresetJsonImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	bEditorImport = true;
	bText = false;
	SupportedClass = UArcQualityBandPreset::StaticClass();
	Formats.Add(TEXT("json;JSON Quality Band Preset"));
}

// -------------------------------------------------------------------
// UFactory: FactoryCanImport
// -------------------------------------------------------------------

bool UArcQualityBandPresetJsonImportFactory::FactoryCanImport(const FString& Filename)
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
		return JsonObj.contains("$type") && JsonObj["$type"] == "ArcQualityBandPreset";
	}
	catch (...)
	{
		return false;
	}
}

// -------------------------------------------------------------------
// UFactory: FactoryCreateFile
// -------------------------------------------------------------------

UObject* UArcQualityBandPresetJsonImportFactory::FactoryCreateFile(
	UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn,
	bool& bOutOperationCanceled)
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *Filename))
	{
		UE_LOG(LogArcQualityBandPresetJsonImport, Error, TEXT("Failed to read JSON file: %s"), *Filename);
		return nullptr;
	}

	nlohmann::json JsonRoot;
	try
	{
		JsonRoot = nlohmann::json::parse(TCHAR_TO_UTF8(*FileContent));
	}
	catch (const nlohmann::json::parse_error& e)
	{
		UE_LOG(LogArcQualityBandPresetJsonImport, Error, TEXT("Failed to parse JSON file: %s - %hs"),
			*Filename, e.what());
		return nullptr;
	}

	if (!JsonRoot.contains("$type") || JsonRoot["$type"] != "ArcQualityBandPreset")
	{
		UE_LOG(LogArcQualityBandPresetJsonImport, Error, TEXT("Expected $type \"ArcQualityBandPreset\" in: %s"), *Filename);
		return nullptr;
	}

	UArcQualityBandPreset* NewPreset = NewObject<UArcQualityBandPreset>(
		InParent, InClass, InName, Flags);

	if (UArcMaterialPropertyTableJsonLoader::ParsePresetJson(JsonRoot, NewPreset))
	{
		if (UAssetImportData* ImportData = NewPreset->GetAssetImportData())
		{
			ImportData->Update(Filename);
		}

		UE_LOG(LogArcQualityBandPresetJsonImport, Log, TEXT("Imported quality band preset '%s' from: %s"),
			*NewPreset->PresetName.ToString(), *Filename);
		return NewPreset;
	}

	UE_LOG(LogArcQualityBandPresetJsonImport, Warning, TEXT("Failed to parse quality band preset from: %s"), *Filename);
	NewPreset->MarkAsGarbage();
	return nullptr;
}

// -------------------------------------------------------------------
// FReimportHandler: CanReimport
// -------------------------------------------------------------------

bool UArcQualityBandPresetJsonImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UArcQualityBandPreset* Preset = Cast<UArcQualityBandPreset>(Obj);
	if (!Preset)
	{
		return false;
	}

	const UAssetImportData* ImportData = Preset->GetAssetImportData();
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

void UArcQualityBandPresetJsonImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UArcQualityBandPreset* Preset = Cast<UArcQualityBandPreset>(Obj);
	if (Preset && NewReimportPaths.Num() > 0)
	{
		if (UAssetImportData* ImportData = Preset->GetAssetImportData())
		{
			ImportData->UpdateFilenameOnly(NewReimportPaths[0]);
		}
	}
}

// -------------------------------------------------------------------
// FReimportHandler: Reimport
// -------------------------------------------------------------------

EReimportResult::Type UArcQualityBandPresetJsonImportFactory::Reimport(UObject* Obj)
{
	UArcQualityBandPreset* Preset = Cast<UArcQualityBandPreset>(Obj);
	if (!Preset)
	{
		return EReimportResult::Failed;
	}

	const UAssetImportData* ImportData = Preset->GetAssetImportData();
	if (!ImportData)
	{
		UE_LOG(LogArcQualityBandPresetJsonImport, Error, TEXT("Preset has no import data for reimport."));
		return EReimportResult::Failed;
	}

	const FString SourceFile = ImportData->GetFirstFilename();
	if (SourceFile.IsEmpty())
	{
		UE_LOG(LogArcQualityBandPresetJsonImport, Error, TEXT("Preset has no source file path stored."));
		return EReimportResult::Failed;
	}

	if (!FPaths::FileExists(SourceFile))
	{
		UE_LOG(LogArcQualityBandPresetJsonImport, Error, TEXT("Source JSON file not found: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *SourceFile))
	{
		UE_LOG(LogArcQualityBandPresetJsonImport, Error, TEXT("Failed to read JSON for reimport: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	nlohmann::json JsonRoot;
	try
	{
		JsonRoot = nlohmann::json::parse(TCHAR_TO_UTF8(*FileContent));
	}
	catch (const nlohmann::json::parse_error& e)
	{
		UE_LOG(LogArcQualityBandPresetJsonImport, Error, TEXT("Failed to parse JSON for reimport: %s - %hs"),
			*SourceFile, e.what());
		return EReimportResult::Failed;
	}

	if (!JsonRoot.contains("$type") || JsonRoot["$type"] != "ArcQualityBandPreset")
	{
		UE_LOG(LogArcQualityBandPresetJsonImport, Error, TEXT("Expected $type \"ArcQualityBandPreset\" in: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	ResetToDefaults(Preset);

	if (!UArcMaterialPropertyTableJsonLoader::ParsePresetJson(JsonRoot, Preset))
	{
		UE_LOG(LogArcQualityBandPresetJsonImport, Error, TEXT("Failed to parse during reimport from: %s"), *SourceFile);
		return EReimportResult::Failed;
	}

	if (UAssetImportData* MutableImportData = Preset->GetAssetImportData())
	{
		MutableImportData->Update(SourceFile);
	}

	Preset->MarkPackageDirty();

	UE_LOG(LogArcQualityBandPresetJsonImport, Log, TEXT("Successfully reimported quality band preset '%s' from: %s"),
		*Preset->PresetName.ToString(), *SourceFile);

	return EReimportResult::Succeeded;
}

// -------------------------------------------------------------------
// FReimportHandler: GetPriority
// -------------------------------------------------------------------

int32 UArcQualityBandPresetJsonImportFactory::GetPriority() const
{
	return ImportPriority;
}

// -------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------

void UArcQualityBandPresetJsonImportFactory::ResetToDefaults(UArcQualityBandPreset* Preset)
{
	Preset->PresetName = FText::GetEmpty();
	Preset->QualityBands.Reset();
}
