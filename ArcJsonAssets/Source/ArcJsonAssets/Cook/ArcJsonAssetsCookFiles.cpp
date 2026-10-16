// Copyright Lukasz Baran. All Rights Reserved.

#include "Cook/ArcJsonAssetsCookFiles.h"

#if WITH_EDITOR
#include "Cooker/CookDependency.h"
#include "Cooker/CookDependencyContext.h"
#include "Cooker/CookEvents.h"
#include "HAL/FileManager.h"
#include "JsonAssets/ArcJsonAssetType.h"
#include "JsonAssets/ArcJsonAssetsLog.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace ArcJsonAssetsCookFilesPrivate
{
	struct FFoundFile
	{
		FString PackagePath;
		FString Filename;
	};

	TArray<FString> GetSortedExtensions()
	{
		TArray<FString> Result;
		for (const FArcJsonAssetTypeInfo& Type : ArcJsonAssets::GetTypes())
		{
			Result.Add(Type.Extension.ToLower());
		}
		Result.Sort([](const FString& A, const FString& B) -> bool
		{
			return A.Compare(B, ESearchCase::CaseSensitive) < 0;
		});
		return Result;
	}

	void FindFiles(const FArcJsonAssetContentRoots& Roots, const TArray<FString>& Extensions, TArray<FFoundFile>& OutFiles)
	{
		for (const FArcJsonAssetContentRoot& Root : Roots.Roots)
		{
			const FString Prefix = Root.ContentDir + TEXT("/");
			IFileManager::Get().IterateDirectoryRecursively(*Root.ContentDir, [&Root, &Prefix, &Extensions, &OutFiles](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
			{
				if (bIsDirectory || !Extensions.Contains(FPaths::GetExtension(FilenameOrDirectory).ToLower()))
				{
					return true;
				}

				FString File = ArcJsonAssetPaths::NormalizeFilename(FilenameOrDirectory);
				if (!File.StartsWith(Prefix))
				{
					return true;
				}
				FString PackagePath = ArcJsonAssetPaths::MakePackagePath(Root, File.RightChop(Prefix.Len()));
				OutFiles.Add({ MoveTemp(PackagePath), MoveTemp(File) });
				return true;
			});
		}
		OutFiles.Sort([](const FFoundFile& A, const FFoundFile& B) -> bool
		{
			return A.PackagePath.Compare(B.PackagePath, ESearchCase::CaseSensitive) < 0;
		});
	}

	FString GetFullProjectDir()
	{
		FString Result = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FPaths::NormalizeDirectoryName(Result);
		return Result + TEXT("/");
	}

	void UpdateString(UE::Cook::FCookDependencyContext& Context, const FString& Value)
	{
		const FTCHARToUTF8 Utf8(*Value);
		const int32 Length = Utf8.Length();
		Context.Update(&Length, sizeof(Length));
		Context.Update(Utf8.Get(), Length);
	}

	void HashSources(FCbFieldViewIterator Args, UE::Cook::FCookDependencyContext& Context)
	{
		if (!IsInGameThread())
		{
			Context.LogError(TEXT("ArcJsonAssets: the cook file hash reads the type registry and the content root table and must run on the game thread"));
			return;
		}

		const TArray<FString> Extensions = GetSortedExtensions();
		const int32 ExtensionCount = Extensions.Num();
		Context.Update(&ExtensionCount, sizeof(ExtensionCount));
		for (const FString& Extension : Extensions)
		{
			UpdateString(Context, Extension);
		}

		TArray<FFoundFile> Files;
		FindFiles(ArcJsonAssetPaths::GetContentRoots(), Extensions, Files);
		const int32 FileCount = Files.Num();
		Context.Update(&FileCount, sizeof(FileCount));

		const FString ProjectDir = GetFullProjectDir();
		TArray<uint8> Bytes;
		for (const FFoundFile& File : Files)
		{
			UpdateString(Context, File.PackagePath);
			UpdateString(Context, File.Filename.StartsWith(ProjectDir) ? File.Filename.RightChop(ProjectDir.Len()) : File.Filename);

			Bytes.Reset();
			if (!FFileHelper::LoadFileToArray(Bytes, *File.Filename))
			{
				Context.LogError(FString::Printf(TEXT("ArcJsonAssets: cannot read '%s' for the cook file hash"), *File.Filename));
				return;
			}
			const int64 Size = Bytes.Num();
			Context.Update(&Size, sizeof(Size));
			Context.Update(Bytes.GetData(), Size);
		}
	}
}

UE_COOK_DEPENDENCY_FUNCTION(ArcJsonAssetsCookFilesSources, ArcJsonAssetsCookFilesPrivate::HashSources);

void UArcJsonAssetsCookFiles::OnCookEvent(UE::Cook::ECookEvent CookEvent, UE::Cook::FCookEventContext& Context)
{
	Super::OnCookEvent(CookEvent, Context);

	if (CookEvent != UE::Cook::ECookEvent::PlatformCookDependencies || !Context.IsCooking())
	{
		return;
	}

	Context.AddSaveBuildDependency(UE::Cook::FCookDependency::Function(UE_COOK_DEPENDENCY_FUNCTION_CALL(ArcJsonAssetsCookFilesSources), FCbFieldIterator()));

	CookRoots = ArcJsonAssetPaths::GetContentRoots();
	CookExtensions = ArcJsonAssetsCookFilesPrivate::GetSortedExtensions();
	CookPackageSuffix.Reset();

	FString SourceBase;
	if (!FPackageName::TryConvertLongPackageNameToFilename(GetPackage()->GetName(), SourceBase))
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: '%ls' has no source filename; no JSON asset files are cooked", *GetPackage()->GetName());
		return;
	}
	SourceBase = FPaths::ConvertRelativePathToFull(SourceBase);
	FPaths::NormalizeFilename(SourceBase);

	const FString ProjectDir = ArcJsonAssetsCookFilesPrivate::GetFullProjectDir();
	if (!SourceBase.StartsWith(ProjectDir))
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: '%ls' is outside the project directory; no JSON asset files are cooked", *SourceBase);
		return;
	}
	CookPackageSuffix = FString(TEXT("/")) + FApp::GetProjectName() + TEXT("/") + SourceBase.RightChop(ProjectDir.Len());
}

void UArcJsonAssetsCookFiles::CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform,
	TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile)
{
	using namespace ArcJsonAssetsCookFilesPrivate;

	if (!TargetPlatform)
	{
		return;
	}

	FString CookedBase = FPaths::ConvertRelativePathToFull(FPaths::GetBaseFilename(PackageFilename, false));
	FPaths::NormalizeFilename(CookedBase);
	if (CookPackageSuffix.IsEmpty() || !CookedBase.EndsWith(CookPackageSuffix))
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: cannot derive the cook directory from '%ls'; no JSON asset files are cooked", PackageFilename);
		return;
	}
	const FString CookedProjectDir = CookedBase.LeftChop(CookPackageSuffix.Len()) / FApp::GetProjectName();
	const FString ProjectDir = GetFullProjectDir();

	TArray<FFoundFile> Files;
	FindFiles(CookRoots, CookExtensions, Files);

	int32 Written = 0;
	TArray<uint8> Bytes;
	for (const FFoundFile& File : Files)
	{
		if (!File.Filename.StartsWith(ProjectDir))
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: '%ls' is outside the project directory and is not cooked", *File.Filename);
			continue;
		}

		Bytes.Reset();
		if (!FFileHelper::LoadFileToArray(Bytes, *File.Filename))
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: cannot read '%ls'; it is not cooked", *File.Filename);
			continue;
		}

		const FString CookedFilename = CookedProjectDir / File.Filename.RightChop(ProjectDir.Len());
		WriteAdditionalFile(*CookedFilename, Bytes.GetData(), Bytes.Num());
		++Written;
	}

	UE_LOGF(LogArcJsonAssets, Display, "ArcJsonAssets: cooked %d JSON asset files into '%ls'", Written, *CookedProjectDir);
}
#endif
