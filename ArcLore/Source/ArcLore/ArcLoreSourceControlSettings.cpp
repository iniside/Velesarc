// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcLoreSourceControlSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "SourceControlHelpers.h"

namespace ArcLoreSettingsConstants
{
	static const FString SettingsSection = TEXT("ArcLoreSourceControl.ArcLoreSourceControlSettings");
}

const FString FArcLoreSourceControlSettings::GetBinaryPath() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return BinaryPath;
}

bool FArcLoreSourceControlSettings::SetBinaryPath(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	const bool bChanged = (BinaryPath != InString);
	if (bChanged)
	{
		BinaryPath = InString;
	}
	return bChanged;
}

float FArcLoreSourceControlSettings::GetRemoteStatusIntervalSeconds() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return RemoteStatusIntervalSeconds;
}

void FArcLoreSourceControlSettings::SetRemoteStatusIntervalSeconds(float InValue)
{
	FScopeLock ScopeLock(&CriticalSection);
	RemoteStatusIntervalSeconds = InValue;
}

void FArcLoreSourceControlSettings::LoadSettings()
{
	FScopeLock ScopeLock(&CriticalSection);
	const FString& IniFile = SourceControlHelpers::GetSettingsIni();
	GConfig->GetString(*ArcLoreSettingsConstants::SettingsSection, TEXT("BinaryPath"), BinaryPath, IniFile);
	GConfig->GetFloat(*ArcLoreSettingsConstants::SettingsSection, TEXT("RemoteStatusIntervalSeconds"), RemoteStatusIntervalSeconds, IniFile);
}

void FArcLoreSourceControlSettings::SaveSettings() const
{
	FScopeLock ScopeLock(&CriticalSection);
	const FString& IniFile = SourceControlHelpers::GetSettingsIni();
	GConfig->SetString(*ArcLoreSettingsConstants::SettingsSection, TEXT("BinaryPath"), *BinaryPath, IniFile);
	GConfig->SetFloat(*ArcLoreSettingsConstants::SettingsSection, TEXT("RemoteStatusIntervalSeconds"), RemoteStatusIntervalSeconds, IniFile);
}
