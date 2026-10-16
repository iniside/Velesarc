// Copyright Lukasz Baran. All Rights Reserved.

#include "Cook/ArcRmlCookFiles.h"

#if WITH_EDITOR
#include "ArcRmlUILog.h"
#include "ArcRmlUIModule.h"
#include "Compile/ArcRmlCompiledFile.h"
#include "Cooker/CookDependency.h"
#include "Cooker/CookDependencyContext.h"
#include "Cooker/CookEvents.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

namespace ArcRmlCookFilesPrivate
{
	struct FFoundFile
	{
		FString PackagePath;
		FString Filename;
	};

	const TCHAR* const CookedExtensions[] = { TEXT("rmlc"), TEXT("rcssc"), TEXT("svg"), TEXT("png"), TEXT("ttf"), TEXT("otf") };
	const TCHAR* const SourceExtensions[] = { TEXT("rml"), TEXT("rcss"), TEXT("svg"), TEXT("png"), TEXT("ttf"), TEXT("otf") };

	bool HasExtension(const FString& Extension, TConstArrayView<const TCHAR*> Extensions)
	{
		for (const TCHAR* Candidate : Extensions)
		{
			if (Extension.Equals(Candidate, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	void FindFiles(const FArcRmlContentRoots& Roots, TConstArrayView<const TCHAR*> Extensions, TArray<FFoundFile>& OutFiles)
	{
		for (const FArcRmlContentRoot& Root : Roots.Roots)
		{
			const FString Prefix = Root.ContentDir + TEXT("/");
			IFileManager::Get().IterateDirectoryRecursively(*Root.ContentDir, [&Root, &Prefix, Extensions, &OutFiles](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
			{
				if (bIsDirectory || !HasExtension(FPaths::GetExtension(FilenameOrDirectory), Extensions))
				{
					return true;
				}

				FString File = FPaths::ConvertRelativePathToFull(FilenameOrDirectory);
				FPaths::NormalizeFilename(File);
				if (!File.StartsWith(Prefix))
				{
					return true;
				}
				FString PackagePath = ArcRmlPaths::MakePackagePath(Root, File.RightChop(Prefix.Len()));
				if (PackagePath.StartsWith(ArcRmlPaths::CookExcludedPrefix))
				{
					return true;
				}
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

	void HashSources(FCbFieldViewIterator Args, UE::Cook::FCookDependencyContext& Context)
	{
		if (!IsInGameThread() && !IsInParallelGameThread())
		{
			Context.LogError(TEXT("ArcRmlUI: the cook file hash reads the content root table and must run on the game thread"));
			return;
		}

		const FArcRmlUIModule* Module = FModuleManager::GetModulePtr<FArcRmlUIModule>(TEXT("ArcRmlUI"));
		if (!Module)
		{
			Context.LogError(TEXT("ArcRmlUI: the ArcRmlUI module is not loaded"));
			return;
		}

		uint64 RegistryHash = 0;
		if (!Module->GetCookRegistryHash(RegistryHash))
		{
			Context.LogError(TEXT("ArcRmlUI: no RmlUi registry hash was captured at cook start"));
			return;
		}

		const uint32 FormatVersion = ArcRmlCompiledFormatVersion;
		Context.Update(&FormatVersion, sizeof(FormatVersion));
		Context.Update(&RegistryHash, sizeof(RegistryHash));

		TArray<FFoundFile> Files;
		FindFiles(Module->GetContentRoots(), SourceExtensions, Files);
		const int32 FileCount = Files.Num();
		Context.Update(&FileCount, sizeof(FileCount));

		const FString ProjectDir = GetFullProjectDir();
		TArray<uint8> Bytes;
		for (const FFoundFile& File : Files)
		{
			const FString DiskPath = File.Filename.StartsWith(ProjectDir) ? File.Filename.RightChop(ProjectDir.Len()) : File.Filename;
			for (const FString* Path : { &File.PackagePath, &DiskPath })
			{
				const FTCHARToUTF8 Utf8(**Path);
				const int32 PathLength = Utf8.Length();
				Context.Update(&PathLength, sizeof(PathLength));
				Context.Update(Utf8.Get(), PathLength);
			}

			Bytes.Reset();
			if (!FFileHelper::LoadFileToArray(Bytes, *File.Filename))
			{
				Context.LogError(FString::Printf(TEXT("ArcRmlUI: cannot read '%s' for the cook file hash"), *File.Filename));
				return;
			}
			const int64 Size = Bytes.Num();
			Context.Update(&Size, sizeof(Size));
			Context.Update(Bytes.GetData(), Size);
		}
	}
}

UE_COOK_DEPENDENCY_FUNCTION(ArcRmlCookFilesSources, ArcRmlCookFilesPrivate::HashSources);
#endif

bool UArcRmlCookFiles::NeedsLoadForServer() const
{
	return false;
}

#if WITH_EDITOR
void UArcRmlCookFiles::OnCookEvent(UE::Cook::ECookEvent CookEvent, UE::Cook::FCookEventContext& Context)
{
	Super::OnCookEvent(CookEvent, Context);

	if (CookEvent != UE::Cook::ECookEvent::PlatformCookDependencies || !Context.IsCooking())
	{
		return;
	}

	Context.AddSaveBuildDependency(UE::Cook::FCookDependency::Function(UE_COOK_DEPENDENCY_FUNCTION_CALL(ArcRmlCookFilesSources), FCbFieldIterator()));

	CookRoots = FArcRmlUIModule::Get().GetContentRoots();
	CookPackageSuffix.Reset();

	FString SourceBase;
	if (!FPackageName::TryConvertLongPackageNameToFilename(GetPackage()->GetName(), SourceBase))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: '%ls' has no source filename; no RML files are cooked", *GetPackage()->GetName());
		return;
	}
	SourceBase = FPaths::ConvertRelativePathToFull(SourceBase);
	FPaths::NormalizeFilename(SourceBase);

	const FString ProjectDir = ArcRmlCookFilesPrivate::GetFullProjectDir();
	if (!SourceBase.StartsWith(ProjectDir))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: '%ls' is outside the project directory; no RML files are cooked", *SourceBase);
		return;
	}
	CookPackageSuffix = FString(TEXT("/")) + FApp::GetProjectName() + TEXT("/") + SourceBase.RightChop(ProjectDir.Len());
}

void UArcRmlCookFiles::CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform,
	TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile)
{
	using namespace ArcRmlCookFilesPrivate;

	if (!TargetPlatform)
	{
		return;
	}

	// Zen stages an additional file only when it lies under <CookDir>/<ProjectName>/ (ZenFileSystemManifest.cpp:529-573).
	FString CookedBase = FPaths::ConvertRelativePathToFull(FPaths::GetBaseFilename(PackageFilename, false));
	FPaths::NormalizeFilename(CookedBase);
	if (CookPackageSuffix.IsEmpty() || !CookedBase.EndsWith(CookPackageSuffix))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot derive the cook directory from '%ls'; no RML files are cooked", PackageFilename);
		return;
	}
	const FString CookedProjectDir = CookedBase.LeftChop(CookPackageSuffix.Len()) / FApp::GetProjectName();
	const FString ProjectDir = GetFullProjectDir();

	TArray<FFoundFile> Files;
	FindFiles(CookRoots, CookedExtensions, Files);

	int32 Written = 0;
	TArray<uint8> Bytes;
	for (const FFoundFile& File : Files)
	{
		if (!File.Filename.StartsWith(ProjectDir))
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: '%ls' is outside the project directory and is not cooked", *File.Filename);
			continue;
		}

		Bytes.Reset();
		if (!FFileHelper::LoadFileToArray(Bytes, *File.Filename))
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot read '%ls'; it is not cooked", *File.Filename);
			continue;
		}

		const FString CookedFilename = CookedProjectDir / File.Filename.RightChop(ProjectDir.Len());
		WriteAdditionalFile(*CookedFilename, Bytes.GetData(), Bytes.Num());
		++Written;
	}

	UE_LOGF(LogArcRmlUI, Display, "ArcRmlUI: cooked %d RML files into '%ls'", Written, *CookedProjectDir);
}
#endif
