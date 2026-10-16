// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcLoreSourceControlUtils.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace ArcLoreSourceControlUtils
{

// Walk parent directories starting at InStartDir until one containing a ".lore" subdirectory is found.
bool FindLoreRepositoryRoot(const FString& InStartDir, FString& OutRoot)
{
	bool bFound = false;
	FString CurrentDir = FPaths::ConvertRelativePathToFull(InStartDir);
	FPaths::NormalizeDirectoryName(CurrentDir);

	// Trim any trailing separators so FindLastChar('/') walks cleanly.
	int32 Len = CurrentDir.Len();
	while(Len > 0 && (CurrentDir[Len - 1] == TEXT('/') || CurrentDir[Len - 1] == TEXT('\\')))
	{
		CurrentDir.LeftChopInline(1);
		Len = CurrentDir.Len();
	}

	while(!bFound && !CurrentDir.IsEmpty())
	{
		const FString PathToLoreSubdirectory = CurrentDir / TEXT(".lore");
		bFound = IFileManager::Get().DirectoryExists(*PathToLoreSubdirectory);
		if(bFound)
		{
			OutRoot = CurrentDir;
		}
		else
		{
			int32 LastSlashIndex;
			if(CurrentDir.FindLastChar(TEXT('/'), LastSlashIndex))
			{
				CurrentDir.LeftInline(LastSlashIndex);
			}
			else
			{
				CurrentDir.Empty();
			}
		}
	}

	return bFound;
}

// Minimal line-scan of <root>/.lore/config.toml for "identity" and "remote_url" scalar keys.
bool ParseLoreConfig(const FString& InConfigPath, FString& OutIdentity, FString& OutRemoteUrl)
{
	TArray<FString> Lines;
	if(!FFileHelper::LoadFileToStringArray(Lines, *InConfigPath))
	{
		return false;
	}

	for(const FString& Line : Lines)
	{
		int32 EqualsIndex;
		if(!Line.FindChar(TEXT('='), EqualsIndex))
		{
			// Not a key=value line (section header, blank, comment); ignore.
			continue;
		}

		const FString Key = Line.Left(EqualsIndex).TrimStartAndEnd();
		FString Value = Line.RightChop(EqualsIndex + 1).TrimStartAndEnd();

		// Strip a single pair of surrounding double-quotes if present.
		if(Value.Len() >= 2 && Value.StartsWith(TEXT("\"")) && Value.EndsWith(TEXT("\"")))
		{
			Value = Value.Mid(1, Value.Len() - 2);
		}

		if(Key == TEXT("identity"))
		{
			OutIdentity = Value;
		}
		else if(Key == TEXT("remote_url"))
		{
			OutRemoteUrl = Value;
		}
		// Ignore every other key/section.
	}

	return true;
}

}
