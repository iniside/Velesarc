// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"

class FArcGitSourceControlSettings
{
public:
	const FString GetBinaryPath() const;
	bool SetBinaryPath(const FString& InString);

	bool GetAutoPushOnCheckIn() const;
	void SetAutoPushOnCheckIn(bool bInValue);

	const FString GetShelveRefPrefix() const;
	void SetShelveRefPrefix(const FString& InString);

	float GetRemoteStatusIntervalSeconds() const;
	void SetRemoteStatusIntervalSeconds(float InValue);

	void LoadSettings();
	void SaveSettings() const;

private:
	mutable FCriticalSection CriticalSection;

	FString BinaryPath;
	bool bAutoPushOnCheckIn = true;
	FString ShelveRefPrefix = TEXT("refs/shelves");
	float RemoteStatusIntervalSeconds = 60.f;
};
