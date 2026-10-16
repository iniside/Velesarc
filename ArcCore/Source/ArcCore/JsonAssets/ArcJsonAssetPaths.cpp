// Copyright Lukasz Baran. All Rights Reserved.

#include "JsonAssets/ArcJsonAssetPaths.h"

#include "Algo/BinarySearch.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace ArcJsonAssetPathsPrivate
{
	const TCHAR* const ProjectMountName = TEXT("Game");

	FString MakeContentDir(const FString& Directory)
	{
		FString Result = FPaths::ConvertRelativePathToFull(Directory);
		FPaths::NormalizeDirectoryName(Result);
		return Result;
	}

	FArcJsonAssetContentRoots& GetMutableContentRoots()
	{
		static FArcJsonAssetContentRoots ContentRoots;
		return ContentRoots;
	}
}

const FArcJsonAssetContentRoot* FArcJsonAssetContentRoots::Find(FStringView MountName) const
{
	for (const FArcJsonAssetContentRoot& Root : Roots)
	{
		if (FStringView(Root.MountName).Equals(MountName, ESearchCase::IgnoreCase))
		{
			return &Root;
		}
	}
	return nullptr;
}

bool FArcJsonAssetContentRoots::Add(const FArcJsonAssetContentRoot& Root)
{
	if (Find(Root.MountName))
	{
		return false;
	}
	const int32 Index = Algo::LowerBoundBy(Roots, Root.MountName, &FArcJsonAssetContentRoot::MountName);
	Roots.Insert(Root, Index);
	return true;
}

bool FArcJsonAssetContentRoots::Remove(FStringView MountName)
{
	const int32 Index = Roots.IndexOfByPredicate([MountName](const FArcJsonAssetContentRoot& Root) -> bool
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

FArcJsonAssetContentRoots ArcJsonAssetPaths::BuildContentRoots()
{
	check(IsInGameThread());
	TArray<FString> RootPaths;
	FPackageName::QueryRootContentPaths(RootPaths);

	FArcJsonAssetContentRoots Result;
	for (const FString& RootPath : RootPaths)
	{
		FArcJsonAssetContentRoot Root;
		if (MakeContentRoot(RootPath, Root))
		{
			Result.Add(Root);
		}
	}
	return Result;
}

bool ArcJsonAssetPaths::MakeContentRoot(const FString& RootPath, FArcJsonAssetContentRoot& OutRoot)
{
	check(IsInGameThread());
	FString MountName = RootPath;
	MountName.TrimCharInline(TEXT('/'), nullptr);
	if (MountName.IsEmpty() || MountName.Contains(TEXT("/")))
	{
		return false;
	}

	if (MountName.Equals(ArcJsonAssetPathsPrivate::ProjectMountName, ESearchCase::IgnoreCase))
	{
		OutRoot.MountName = ArcJsonAssetPathsPrivate::ProjectMountName;
		OutRoot.ContentDir = ArcJsonAssetPathsPrivate::MakeContentDir(FPaths::ProjectContentDir());
		return true;
	}

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(MountName);
	if (!Plugin.IsValid() || Plugin->GetLoadedFrom() != EPluginLoadedFrom::Project || !Plugin->CanContainContent()
		|| !Plugin->GetMountedAssetPath().Equals(TEXT("/") + MountName + TEXT("/"), ESearchCase::IgnoreCase))
	{
		return false;
	}

	OutRoot.MountName = MountName;
	OutRoot.ContentDir = ArcJsonAssetPathsPrivate::MakeContentDir(Plugin->GetContentDir());
	return true;
}

const FArcJsonAssetContentRoots& ArcJsonAssetPaths::GetContentRoots()
{
	check(IsInGameThread());
	return ArcJsonAssetPathsPrivate::GetMutableContentRoots();
}

FArcJsonAssetContentRootChanged& ArcJsonAssetPaths::OnContentRootChanged()
{
	static FArcJsonAssetContentRootChanged ContentRootChanged;
	return ContentRootChanged;
}

void ArcJsonAssetPaths::InitializeContentRoots()
{
	ArcJsonAssetPathsPrivate::GetMutableContentRoots() = BuildContentRoots();
}

void ArcJsonAssetPaths::HandleContentPathMounted(const FString& AssetPath, const FString& ContentPath)
{
	check(IsInGameThread());
	FArcJsonAssetContentRoot Root;
	if (!MakeContentRoot(AssetPath, Root) || !ArcJsonAssetPathsPrivate::GetMutableContentRoots().Add(Root))
	{
		return;
	}
	OnContentRootChanged().Broadcast(Root, true);
}

void ArcJsonAssetPaths::HandleContentPathDismounted(const FString& AssetPath, const FString& ContentPath)
{
	check(IsInGameThread());
	FString MountName = AssetPath;
	MountName.TrimCharInline(TEXT('/'), nullptr);
	FArcJsonAssetContentRoots& ContentRoots = ArcJsonAssetPathsPrivate::GetMutableContentRoots();
	const FArcJsonAssetContentRoot* Found = ContentRoots.Find(MountName);
	if (!Found)
	{
		return;
	}
	const FArcJsonAssetContentRoot Root = *Found;
	ContentRoots.Remove(MountName);
	OnContentRootChanged().Broadcast(Root, false);
}

FString ArcJsonAssetPaths::NormalizeFilename(const FString& Filename)
{
	FString Result = FPaths::ConvertRelativePathToFull(Filename);
	FPaths::NormalizeFilename(Result);
	FPaths::RemoveDuplicateSlashes(Result);
	FPaths::CollapseRelativeDirectories(Result);
	return Result;
}

bool ArcJsonAssetPaths::SplitPackagePath(const FString& PackagePath, FString& OutMountName, FString& OutRelativePath)
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

FString ArcJsonAssetPaths::MakePackagePath(const FArcJsonAssetContentRoot& Root, const FString& RelativePath)
{
	return TEXT("/") + Root.MountName + TEXT("/") + RelativePath;
}

bool ArcJsonAssetPaths::IsUnderRoot(const FArcJsonAssetContentRoot& Root, const FString& Filename)
{
	return NormalizeFilename(Filename).StartsWith(Root.ContentDir + TEXT("/"));
}

bool ArcJsonAssetPaths::ToFilename(const FArcJsonAssetContentRoots& Roots, const FString& PackagePath, FString& OutFilename)
{
	FString MountName;
	FString RelativePath;
	if (!SplitPackagePath(PackagePath, MountName, RelativePath))
	{
		return false;
	}
	const FArcJsonAssetContentRoot* Root = Roots.Find(MountName);
	if (!Root)
	{
		return false;
	}
	OutFilename = Root->ContentDir / RelativePath;
	return true;
}

bool ArcJsonAssetPaths::ToPackagePath(const FArcJsonAssetContentRoots& Roots, const FString& Filename, FString& OutPackagePath)
{
	const FString FullPath = NormalizeFilename(Filename);
	for (const FArcJsonAssetContentRoot& Root : Roots.Roots)
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
