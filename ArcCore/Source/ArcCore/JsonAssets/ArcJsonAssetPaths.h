// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FArcJsonAssetContentRoot
{
	FString MountName;
	FString ContentDir;
};

struct FArcJsonAssetContentRoots
{
	TArray<FArcJsonAssetContentRoot> Roots;

	ARCCORE_API const FArcJsonAssetContentRoot* Find(FStringView MountName) const;
	ARCCORE_API bool Add(const FArcJsonAssetContentRoot& Root);
	ARCCORE_API bool Remove(FStringView MountName);
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FArcJsonAssetContentRootChanged, const FArcJsonAssetContentRoot& /*Root*/, bool /*bMounted*/);

namespace ArcJsonAssetPaths
{
	ARCCORE_API FArcJsonAssetContentRoots BuildContentRoots();
	ARCCORE_API bool MakeContentRoot(const FString& RootPath, FArcJsonAssetContentRoot& OutRoot);

	ARCCORE_API const FArcJsonAssetContentRoots& GetContentRoots();
	ARCCORE_API FArcJsonAssetContentRootChanged& OnContentRootChanged();
	void InitializeContentRoots();
	void HandleContentPathMounted(const FString& AssetPath, const FString& ContentPath);
	void HandleContentPathDismounted(const FString& AssetPath, const FString& ContentPath);

	ARCCORE_API FString NormalizeFilename(const FString& Filename);
	ARCCORE_API bool SplitPackagePath(const FString& PackagePath, FString& OutMountName, FString& OutRelativePath);
	ARCCORE_API FString MakePackagePath(const FArcJsonAssetContentRoot& Root, const FString& RelativePath);
	ARCCORE_API bool IsUnderRoot(const FArcJsonAssetContentRoot& Root, const FString& Filename);

	ARCCORE_API bool ToFilename(const FArcJsonAssetContentRoots& Roots, const FString& PackagePath, FString& OutFilename);
	ARCCORE_API bool ToPackagePath(const FArcJsonAssetContentRoots& Roots, const FString& Filename, FString& OutPackagePath);
}
