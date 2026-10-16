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

#include "ArcSvgToolset.h"

#include "ArcSvgAssetFactory.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSvgToolset)

namespace ArcSvgToolset_Private
{
	/** Builds a {"error":"..."} JSON payload, sanitising characters that would break the string. */
	static FString ErrorJson(const FString& Message)
	{
		FString Escaped = Message;
		Escaped.ReplaceInline(TEXT("\\"), TEXT("/"));
		Escaped.ReplaceInline(TEXT("\""), TEXT("'"));
		return FString::Printf(TEXT("{\"error\":\"%s\"}"), *Escaped);
	}
}

FString UArcSvgToolset::ImportSvgAsset(const FString& SourceFile, const FString& PackagePath, const FString& AssetName)
{
	if (SourceFile.IsEmpty() || PackagePath.IsEmpty() || AssetName.IsEmpty())
	{
		return ArcSvgToolset_Private::ErrorJson(TEXT("SourceFile, PackagePath and AssetName are all required."));
	}

	if (!FPaths::FileExists(SourceFile))
	{
		return ArcSvgToolset_Private::ErrorJson(FString::Printf(TEXT("Source file not found: %s"), *SourceFile));
	}

	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = SourceFile;
	Task->DestinationPath = PackagePath;
	Task->DestinationName = AssetName;
	Task->bAutomated = true;
	Task->bReplaceExisting = true;
	Task->bReplaceExistingSettings = true;
	Task->bSave = true;
	Task->bAsync = false;
	// Force our factory: the .svg extension is otherwise unclaimed by automated import.
	Task->Factory = NewObject<UArcSvgAssetFactory>();

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	AssetToolsModule.Get().ImportAssetTasks({ Task });

	const TArray<UObject*>& Imported = Task->GetObjects();
	if (Imported.Num() == 0 || Imported[0] == nullptr)
	{
		return ArcSvgToolset_Private::ErrorJson(FString::Printf(TEXT("Import produced no asset from '%s' (is it a valid SVG?)."), *SourceFile));
	}

	return FString::Printf(TEXT("{\"success\":true,\"asset\":\"%s\"}"), *Imported[0]->GetPathName());
}
