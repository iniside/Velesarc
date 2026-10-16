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

#include "ArcSvgAssetFactory.h"

#include "Svg/ArcSvgAsset.h"

#include "Editor.h"
#include "Subsystems/ImportSubsystem.h"
#include "Misc/FileHelper.h"

#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSvgAssetFactory)

namespace ArcSvgAssetFactory_Private
{
	DEFINE_LOG_CATEGORY_STATIC(LogArcSvgAssetFactory, Log, All);
}

UArcSvgAssetFactory::UArcSvgAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UArcSvgAsset::StaticClass();
	Formats.Add(TEXT("svg;Scalable Vector Graphics"));
	bEditorImport = true;
	bText = false;
	bCreateNew = false;
}

UObject* UArcSvgAssetFactory::FactoryCreateBinary(UClass* InClass
												  , UObject* InParent
												  , FName InName
												  , EObjectFlags Flags
												  , UObject* Context
												  , const TCHAR* Type
												  , const uint8*& Buffer
												  , const uint8* BufferEnd
												  , FFeedbackContext* Warn)
{
	UArcSvgAsset* Asset = NewObject<UArcSvgAsset>(InParent, InClass, InName, Flags);

	const int32 NumBytes = static_cast<int32>(BufferEnd - Buffer);
	Asset->SvgData.Empty(NumBytes);
	Asset->SvgData.Append(Buffer, NumBytes);

#if WITH_EDITORONLY_DATA
	Asset->AssetImportData->Update(GetCurrentFilename());
#endif

	// The import pipeline (UImportSubsystem) fires OnAssetPostImport for us — no manual
	// GEditor broadcast here.
	return Asset;
}

bool UArcSvgAssetFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UArcSvgAsset* Asset = Cast<UArcSvgAsset>(Obj);
	if (Asset == nullptr)
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	if (Asset->AssetImportData != nullptr)
	{
		Asset->AssetImportData->ExtractFilenames(OutFilenames);
	}
#endif

	return true;
}

void UArcSvgAssetFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UArcSvgAsset* Asset = Cast<UArcSvgAsset>(Obj);
	if (Asset == nullptr)
	{
		return;
	}

#if WITH_EDITORONLY_DATA
	if (Asset->AssetImportData != nullptr && NewReimportPaths.Num() > 0)
	{
		Asset->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
#endif
}

EReimportResult::Type UArcSvgAssetFactory::Reimport(UObject* Obj)
{
	UArcSvgAsset* Asset = Cast<UArcSvgAsset>(Obj);
	if (Asset == nullptr)
	{
		return EReimportResult::Failed;
	}

#if WITH_EDITORONLY_DATA
	if (Asset->AssetImportData == nullptr)
	{
		return EReimportResult::Failed;
	}

	const FString SourcePath = Asset->AssetImportData->GetFirstFilename();
	if (SourcePath.IsEmpty())
	{
		UE_LOGF(ArcSvgAssetFactory_Private::LogArcSvgAssetFactory, Warning,"Reimport of '%ls' failed: no source file path stored.", *Asset->GetPathName());
		return EReimportResult::Failed;
	}

	TArray<uint8> ReloadedBytes;
	if (!FFileHelper::LoadFileToArray(ReloadedBytes, *SourcePath))
	{
		UE_LOGF(ArcSvgAssetFactory_Private::LogArcSvgAssetFactory, Warning,"Reimport of '%ls' failed: could not read source file '%ls'.",
			*Asset->GetPathName(), *SourcePath);
		return EReimportResult::Failed;
	}

	Asset->SvgData = MoveTemp(ReloadedBytes);
	Asset->AssetImportData->Update(SourcePath);

	// Bust the extracted-file cache and re-extract immediately so the new content-hashed
	// path exists, defeating Slate's {path, size} vector graphics cache.
	Asset->ClearExtractedCache();
	Asset->GetOrExtractSvgFilePath();

	Asset->MarkPackageDirty();

	// Live UArcSvgImage widgets hold a brush bound to the old path; broadcasting lets
	// subscribers refresh (open WBPs otherwise pick up the change on reopen).
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetReimport(Asset);

	return EReimportResult::Succeeded;
#else
	return EReimportResult::Failed;
#endif
}
