// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IPlatformFile;

struct FArcRmlContentRoot
{
	FString MountName;
	FString ContentDir;
};

struct FArcRmlContentRoots
{
	TArray<FArcRmlContentRoot> Roots;

	ARCRMLUI_API const FArcRmlContentRoot* Find(FStringView MountName) const;
	ARCRMLUI_API bool Add(const FArcRmlContentRoot& Root);
	ARCRMLUI_API bool Remove(FStringView MountName);
};

namespace ArcRmlPaths
{
	inline const TCHAR* const ComponentDirectory = TEXT("UI/Rml/Components/");
	inline const TCHAR* const FontDirectory = TEXT("UI/Rml/Fonts/");
	inline const TCHAR* const CookExcludedPrefix = TEXT("/Game/UI/Rml/Test/");

	ARCRMLUI_API FArcRmlContentRoots BuildContentRoots();
	ARCRMLUI_API bool MakeContentRoot(const FString& RootPath, FArcRmlContentRoot& OutRoot);

	ARCRMLUI_API FString NormalizePackagePath(const FString& Path);
	ARCRMLUI_API FString JoinPackagePath(const FString& DocumentPath, const FString& Link);
	ARCRMLUI_API bool SplitPackagePath(const FString& PackagePath, FString& OutMountName, FString& OutRelativePath);
	ARCRMLUI_API FString MakePackagePath(const FArcRmlContentRoot& Root, const FString& RelativePath);
	ARCRMLUI_API bool IsComponentPath(const FString& PackagePath);

	ARCRMLUI_API bool ToFilename(const FArcRmlContentRoots& Roots, const FString& PackagePath, FString& OutFilename);
	ARCRMLUI_API bool ToPackagePath(const FArcRmlContentRoots& Roots, const FString& Filename, FString& OutPackagePath);

	ARCRMLUI_API void FindFontFiles(IPlatformFile& PlatformFile, const FArcRmlContentRoot& Root, TArray<FString>& OutPackagePaths);
}
