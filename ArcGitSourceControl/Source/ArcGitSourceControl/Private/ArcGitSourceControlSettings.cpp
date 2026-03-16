// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcGitSourceControlSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "SourceControlHelpers.h"

namespace ArcGitSettingsConstants
{
	static const FString SettingsSection = TEXT("ArcGitSourceControl.ArcGitSourceControlSettings");
}

const FString FArcGitSourceControlSettings::GetBinaryPath() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return BinaryPath;
}

bool FArcGitSourceControlSettings::SetBinaryPath(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	const bool bChanged = (BinaryPath != InString);
	if (bChanged)
	{
		BinaryPath = InString;
	}
	return bChanged;
}

bool FArcGitSourceControlSettings::GetAutoPushOnCheckIn() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return bAutoPushOnCheckIn;
}

void FArcGitSourceControlSettings::SetAutoPushOnCheckIn(bool bInValue)
{
	FScopeLock ScopeLock(&CriticalSection);
	bAutoPushOnCheckIn = bInValue;
}

const FString FArcGitSourceControlSettings::GetShelveRefPrefix() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return ShelveRefPrefix;
}

void FArcGitSourceControlSettings::SetShelveRefPrefix(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);
	ShelveRefPrefix = InString;
}

float FArcGitSourceControlSettings::GetRemoteStatusIntervalSeconds() const
{
	FScopeLock ScopeLock(&CriticalSection);
	return RemoteStatusIntervalSeconds;
}

void FArcGitSourceControlSettings::SetRemoteStatusIntervalSeconds(float InValue)
{
	FScopeLock ScopeLock(&CriticalSection);
	RemoteStatusIntervalSeconds = InValue;
}

void FArcGitSourceControlSettings::LoadSettings()
{
	FScopeLock ScopeLock(&CriticalSection);
	const FString& IniFile = SourceControlHelpers::GetSettingsIni();
	GConfig->GetString(*ArcGitSettingsConstants::SettingsSection, TEXT("BinaryPath"), BinaryPath, IniFile);
	GConfig->GetBool(*ArcGitSettingsConstants::SettingsSection, TEXT("AutoPushOnCheckIn"), bAutoPushOnCheckIn, IniFile);
	GConfig->GetString(*ArcGitSettingsConstants::SettingsSection, TEXT("ShelveRefPrefix"), ShelveRefPrefix, IniFile);
	GConfig->GetFloat(*ArcGitSettingsConstants::SettingsSection, TEXT("RemoteStatusIntervalSeconds"), RemoteStatusIntervalSeconds, IniFile);
}

void FArcGitSourceControlSettings::SaveSettings() const
{
	FScopeLock ScopeLock(&CriticalSection);
	const FString& IniFile = SourceControlHelpers::GetSettingsIni();
	GConfig->SetString(*ArcGitSettingsConstants::SettingsSection, TEXT("BinaryPath"), *BinaryPath, IniFile);
	GConfig->SetBool(*ArcGitSettingsConstants::SettingsSection, TEXT("AutoPushOnCheckIn"), bAutoPushOnCheckIn, IniFile);
	GConfig->SetString(*ArcGitSettingsConstants::SettingsSection, TEXT("ShelveRefPrefix"), *ShelveRefPrefix, IniFile);
	GConfig->SetFloat(*ArcGitSettingsConstants::SettingsSection, TEXT("RemoteStatusIntervalSeconds"), RemoteStatusIntervalSeconds, IniFile);
}
