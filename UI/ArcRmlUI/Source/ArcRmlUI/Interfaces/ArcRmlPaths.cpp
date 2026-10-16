// Copyright Lukasz Baran. All Rights Reserved.

#include "Interfaces/ArcRmlPaths.h"

#include "Algo/BinarySearch.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace ArcRmlPathsPrivate
{
	const TCHAR* const ProjectMountName = TEXT("Game");

	FString MakeContentDir(const FString& Directory)
	{
		FString Result = FPaths::ConvertRelativePathToFull(Directory);
		FPaths::NormalizeDirectoryName(Result);
		return Result;
	}
}

const FArcRmlContentRoot* FArcRmlContentRoots::Find(FStringView MountName) const
{
	for (const FArcRmlContentRoot& Root : Roots)
	{
		if (FStringView(Root.MountName).Equals(MountName, ESearchCase::IgnoreCase))
		{
			return &Root;
		}
	}
	return nullptr;
}

bool FArcRmlContentRoots::Add(const FArcRmlContentRoot& Root)
{
	if (Find(Root.MountName))
	{
		return false;
	}
	const int32 Index = Algo::LowerBoundBy(Roots, Root.MountName, &FArcRmlContentRoot::MountName);
	Roots.Insert(Root, Index);
	return true;
}

bool FArcRmlContentRoots::Remove(FStringView MountName)
{
	const int32 Index = Roots.IndexOfByPredicate([MountName](const FArcRmlContentRoot& Root) -> bool
	{
		return FStringView(Root.MountName).Equals(MountName, ESearchCase::IgnoreCase);
	});
	if (Index == INDEX_NONE)
	{
		return false;
	}
	Roots.RemoveAt(Index);
	return true;
}

FArcRmlContentRoots ArcRmlPaths::BuildContentRoots()
{
	check(IsInGameThread());
	TArray<FString> RootPaths;
	FPackageName::QueryRootContentPaths(RootPaths);

	FArcRmlContentRoots Result;
	for (const FString& RootPath : RootPaths)
	{
		FArcRmlContentRoot Root;
		if (MakeContentRoot(RootPath, Root))
		{
			Result.Add(Root);
		}
	}
	return Result;
}

bool ArcRmlPaths::MakeContentRoot(const FString& RootPath, FArcRmlContentRoot& OutRoot)
{
	check(IsInGameThread());
	FString MountName = RootPath;
	MountName.TrimCharInline(TEXT('/'), nullptr);
	if (MountName.IsEmpty() || MountName.Contains(TEXT("/")))
	{
		return false;
	}

	if (MountName.Equals(ArcRmlPathsPrivate::ProjectMountName, ESearchCase::IgnoreCase))
	{
		OutRoot.MountName = ArcRmlPathsPrivate::ProjectMountName;
		OutRoot.ContentDir = ArcRmlPathsPrivate::MakeContentDir(FPaths::ProjectContentDir());
		return true;
	}

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(MountName);
	if (!Plugin.IsValid() || Plugin->GetLoadedFrom() != EPluginLoadedFrom::Project || !Plugin->CanContainContent()
		|| !Plugin->GetMountedAssetPath().Equals(TEXT("/") + MountName + TEXT("/"), ESearchCase::IgnoreCase))
	{
		return false;
	}

	OutRoot.MountName = MountName;
	OutRoot.ContentDir = ArcRmlPathsPrivate::MakeContentDir(Plugin->GetContentDir());
	return true;
}

FString ArcRmlPaths::NormalizePackagePath(const FString& Path)
{
	FString Result = Path.Replace(TEXT("\\"), TEXT("/"));
	FPaths::RemoveDuplicateSlashes(Result);
	FPaths::CollapseRelativeDirectories(Result);
	return Result;
}

FString ArcRmlPaths::JoinPackagePath(const FString& DocumentPath, const FString& Link)
{
	const FString NormalizedLink = Link.Replace(TEXT("\\"), TEXT("/"));
	if (NormalizedLink.StartsWith(TEXT("/")))
	{
		return NormalizePackagePath(NormalizedLink);
	}

	FString Folder = DocumentPath.Replace(TEXT("\\"), TEXT("/"));
	int32 Slash = INDEX_NONE;
	if (Folder.FindLastChar(TEXT('/'), Slash))
	{
		Folder.LeftInline(Slash + 1);
	}
	else
	{
		Folder.Reset();
	}
	return NormalizePackagePath(Folder + NormalizedLink);
}

bool ArcRmlPaths::SplitPackagePath(const FString& PackagePath, FString& OutMountName, FString& OutRelativePath)
{
	if (!PackagePath.StartsWith(TEXT("/")))
	{
		return false;
	}
	const int32 Slash = PackagePath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1);
	if (Slash <= 1 || Slash == PackagePath.Len() - 1)
	{
		return false;
	}
	OutMountName = PackagePath.Mid(1, Slash - 1);
	OutRelativePath = PackagePath.RightChop(Slash + 1);
	return true;
}

FString ArcRmlPaths::MakePackagePath(const FArcRmlContentRoot& Root, const FString& RelativePath)
{
	return TEXT("/") + Root.MountName + TEXT("/") + RelativePath;
}

bool ArcRmlPaths::IsComponentPath(const FString& PackagePath)
{
	FString MountName;
	FString RelativePath;
	return SplitPackagePath(PackagePath, MountName, RelativePath) && RelativePath.StartsWith(ComponentDirectory);
}

bool ArcRmlPaths::ToFilename(const FArcRmlContentRoots& Roots, const FString& PackagePath, FString& OutFilename)
{
	FString MountName;
	FString RelativePath;
	if (!SplitPackagePath(PackagePath, MountName, RelativePath))
	{
		return false;
	}
	const FArcRmlContentRoot* Root = Roots.Find(MountName);
	if (!Root)
	{
		return false;
	}
	OutFilename = Root->ContentDir / RelativePath;
	return true;
}

bool ArcRmlPaths::ToPackagePath(const FArcRmlContentRoots& Roots, const FString& Filename, FString& OutPackagePath)
{
	FString FullPath = FPaths::ConvertRelativePathToFull(Filename);
	FPaths::NormalizeFilename(FullPath);
	for (const FArcRmlContentRoot& Root : Roots.Roots)
	{
		const FString Prefix = Root.ContentDir + TEXT("/");
		if (FullPath.StartsWith(Prefix))
		{
			OutPackagePath = MakePackagePath(Root, FullPath.RightChop(Prefix.Len()));
			return true;
		}
	}
	return false;
}

void ArcRmlPaths::FindFontFiles(IPlatformFile& PlatformFile, const FArcRmlContentRoot& Root, TArray<FString>& OutPackagePaths)
{
	const FString Directory = Root.ContentDir / FontDirectory;
	TArray<FString> Files;
	PlatformFile.FindFiles(Files, *Directory, TEXT(".ttf"));
	PlatformFile.FindFiles(Files, *Directory, TEXT(".otf"));
	Files.Sort();
	for (const FString& File : Files)
	{
		OutPackagePaths.Add(MakePackagePath(Root, FString(FontDirectory) + FPaths::GetCleanFilename(File)));
	}
}
