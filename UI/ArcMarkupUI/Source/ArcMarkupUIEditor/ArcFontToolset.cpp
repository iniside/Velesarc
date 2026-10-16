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

#include "ArcFontToolset.h"

#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Factories/FontFileImportFactory.h"
#include "Fonts/CompositeFont.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcFontToolset)

namespace ArcFontToolset_Private
{
	/** Builds a {"error":"..."} JSON payload, sanitising characters that would break the string. */
	static FString ErrorJson(const FString& Message)
	{
		FString Escaped = Message;
		Escaped.ReplaceInline(TEXT("\\"), TEXT("/"));
		Escaped.ReplaceInline(TEXT("\""), TEXT("'"));
		return FString::Printf(TEXT("{\"error\":\"%s\"}"), *Escaped);
	}

	/** Imports a single font file into a UFontFace; returns the created object (or nullptr). */
	static UObject* ImportFaceObject(const FString& SourceFile, const FString& PackagePath, const FString& AssetName)
	{
		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = SourceFile;
		Task->DestinationPath = PackagePath;
		Task->DestinationName = AssetName;
		Task->bAutomated = true;
		Task->bReplaceExisting = true;
		Task->bReplaceExistingSettings = true;
		Task->bSave = true;
		Task->bAsync = false;
		// Force the font-face factory: this guarantees a UFontFace (not a legacy offline UFont).
		Task->Factory = NewObject<UFontFileImportFactory>();

		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		AssetToolsModule.Get().ImportAssetTasks({ Task });

		const TArray<UObject*>& Imported = Task->GetObjects();
		return Imported.Num() > 0 ? Imported[0] : nullptr;
	}

	/** Assembles a runtime UFont whose default typeface maps TypefaceNames -> Faces, saves it, returns its path. */
	static FString BuildRuntimeFont(const FString& PackagePath, const FString& AssetName,
		const TArray<UFontFace*>& Faces, const TArray<FString>& TypefaceNames, FString& OutError)
	{
		const FString PackageName = PackagePath / AssetName;
		UPackage* Package = CreatePackage(*PackageName);
		if (Package == nullptr)
		{
			OutError = FString::Printf(TEXT("Could not create package: %s"), *PackageName);
			return FString();
		}
		Package->FullyLoad();

		UFont* Font = FindObject<UFont>(Package, *AssetName);
		const bool bNew = (Font == nullptr);
		if (bNew)
		{
			Font = NewObject<UFont>(Package, FName(*AssetName), RF_Public | RF_Standalone);
		}

		Font->FontCacheType = EFontCacheType::Runtime;

		FTypeface& Typeface = Font->GetMutableInternalCompositeFont().DefaultTypeface;
		Typeface.Fonts.Reset();
		for (int32 Index = 0; Index < Faces.Num(); ++Index)
		{
			const FName EntryName(*TypefaceNames[Index]);
			FTypefaceEntry Entry(EntryName);
			Entry.Font = FFontData(Faces[Index]);
			Typeface.Fonts.Add(Entry);
		}

		if (bNew)
		{
			FAssetRegistryModule::AssetCreated(Font);
		}
		Font->MarkPackageDirty();

		const FString FileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(Package, Font, *FileName, SaveArgs))
		{
			OutError = FString::Printf(TEXT("Failed to save font package: %s"), *FileName);
			return FString();
		}

		return Font->GetPathName();
	}
}

FString UArcFontToolset::ImportFontFace(const FString& SourceFile, const FString& PackagePath, const FString& AssetName)
{
	if (SourceFile.IsEmpty() || PackagePath.IsEmpty() || AssetName.IsEmpty())
	{
		return ArcFontToolset_Private::ErrorJson(TEXT("SourceFile, PackagePath and AssetName are all required."));
	}
	if (!FPaths::FileExists(SourceFile))
	{
		return ArcFontToolset_Private::ErrorJson(FString::Printf(TEXT("Source file not found: %s"), *SourceFile));
	}

	UObject* Imported = ArcFontToolset_Private::ImportFaceObject(SourceFile, PackagePath, AssetName);
	if (Imported == nullptr)
	{
		return ArcFontToolset_Private::ErrorJson(FString::Printf(TEXT("Import produced no asset from '%s' (is it a valid font file?)."), *SourceFile));
	}

	return FString::Printf(TEXT("{\"success\":true,\"asset\":\"%s\"}"), *Imported->GetPathName());
}

FString UArcFontToolset::CreateRuntimeFont(const FString& PackagePath, const FString& AssetName,
	const TArray<FString>& FontFacePaths, const TArray<FString>& TypefaceNames)
{
	if (PackagePath.IsEmpty() || AssetName.IsEmpty())
	{
		return ArcFontToolset_Private::ErrorJson(TEXT("PackagePath and AssetName are required."));
	}
	if (FontFacePaths.Num() == 0 || FontFacePaths.Num() != TypefaceNames.Num())
	{
		return ArcFontToolset_Private::ErrorJson(TEXT("FontFacePaths and TypefaceNames must be non-empty and the same length."));
	}

	TArray<UFontFace*> Faces;
	Faces.Reserve(FontFacePaths.Num());
	for (const FString& FacePath : FontFacePaths)
	{
		UFontFace* Face = LoadObject<UFontFace>(nullptr, *FacePath);
		if (Face == nullptr)
		{
			return ArcFontToolset_Private::ErrorJson(FString::Printf(TEXT("Font face not found: %s"), *FacePath));
		}
		Faces.Add(Face);
	}

	FString Error;
	const FString FontPath = ArcFontToolset_Private::BuildRuntimeFont(PackagePath, AssetName, Faces, TypefaceNames, Error);
	if (FontPath.IsEmpty())
	{
		return ArcFontToolset_Private::ErrorJson(Error);
	}
	return FString::Printf(TEXT("{\"success\":true,\"asset\":\"%s\"}"), *FontPath);
}

FString UArcFontToolset::ImportFontFamily(const FString& PackagePath, const FString& AssetName,
	const TArray<FString>& SourceFiles, const TArray<FString>& TypefaceNames)
{
	if (PackagePath.IsEmpty() || AssetName.IsEmpty())
	{
		return ArcFontToolset_Private::ErrorJson(TEXT("PackagePath and AssetName are required."));
	}
	if (SourceFiles.Num() == 0 || SourceFiles.Num() != TypefaceNames.Num())
	{
		return ArcFontToolset_Private::ErrorJson(TEXT("SourceFiles and TypefaceNames must be non-empty and the same length."));
	}

	// Derive a clean base name for the face assets (strip a leading "Font_").
	FString Base = AssetName;
	Base.RemoveFromStart(TEXT("Font_"));

	TArray<UFontFace*> Faces;
	Faces.Reserve(SourceFiles.Num());
	for (int32 Index = 0; Index < SourceFiles.Num(); ++Index)
	{
		const FString& Source = SourceFiles[Index];
		if (!FPaths::FileExists(Source))
		{
			return ArcFontToolset_Private::ErrorJson(FString::Printf(TEXT("Source file not found: %s"), *Source));
		}
		const FString FaceName = FString::Printf(TEXT("FontFace_%s_%s"), *Base, *TypefaceNames[Index]);
		UObject* Imported = ArcFontToolset_Private::ImportFaceObject(Source, PackagePath, FaceName);
		UFontFace* Face = Cast<UFontFace>(Imported);
		if (Face == nullptr)
		{
			return ArcFontToolset_Private::ErrorJson(FString::Printf(TEXT("Import produced no font face from '%s'."), *Source));
		}
		Faces.Add(Face);
	}

	FString Error;
	const FString FontPath = ArcFontToolset_Private::BuildRuntimeFont(PackagePath, AssetName, Faces, TypefaceNames, Error);
	if (FontPath.IsEmpty())
	{
		return ArcFontToolset_Private::ErrorJson(Error);
	}
	return FString::Printf(TEXT("{\"success\":true,\"asset\":\"%s\"}"), *FontPath);
}
