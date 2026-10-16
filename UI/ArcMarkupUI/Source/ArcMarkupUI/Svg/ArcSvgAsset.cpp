// Copyright Lukasz Baran. All Rights Reserved.

#include "Svg/ArcSvgAsset.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"

#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSvgAsset)

DEFINE_LOG_CATEGORY_STATIC(LogArcSvgAsset, Log, All);

void UArcSvgAsset::PostInitProperties()
{
#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif

	Super::PostInitProperties();
}

void UArcSvgAsset::PostLoad()
{
	Super::PostLoad();

	// Primary extraction trigger: runs once per asset load (cooked builds included),
	// keeping the disk IO off the first widget-construct frame.
	GetOrExtractSvgFilePath();
}

const FString& UArcSvgAsset::GetOrExtractSvgFilePath()
{
	IFileManager& FileManager = IFileManager::Get();

	if (!CachedExtractedPath.IsEmpty() && FileManager.FileExists(*CachedExtractedPath))
	{
		return CachedExtractedPath;
	}

	if (SvgData.IsEmpty())
	{
		CachedExtractedPath.Reset();
		return CachedExtractedPath;
	}

	const FString Hash = FMD5::HashBytes(SvgData.GetData(), static_cast<uint64>(SvgData.Num()));
	const FString PackageSafe = GetOutermost()->GetName().Replace(TEXT("/"), TEXT("_"));
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("SVGCache");
	const FString Path = Dir / FString::Printf(TEXT("%s_%s.svg"), *PackageSafe, *Hash);

	if (!FileManager.FileExists(*Path))
	{
		FileManager.MakeDirectory(*Dir, /*Tree=*/true);

		if (!FFileHelper::SaveArrayToFile(SvgData, *Path))
		{
			UE_LOGF(LogArcSvgAsset, Warning,"Failed to extract SVG bytes to '%ls' for asset '%ls'.",
				*Path, *GetPathName());
			CachedExtractedPath.Reset();
			return CachedExtractedPath;
		}
	}

	CachedExtractedPath = Path;
	return CachedExtractedPath;
}

void UArcSvgAsset::ClearExtractedCache()
{
	CachedExtractedPath.Reset();
}
