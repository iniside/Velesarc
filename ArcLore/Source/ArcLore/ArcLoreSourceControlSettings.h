// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"

class FArcLoreSourceControlSettings
{
public:
	// TODO: vestigial git-binary path. Still consumed by the legacy git-based
	// ArcLoreSourceControlUtils / Command / Label code paths (and the provider's
	// Tick/GetLabels). Remove once those are ported to the Lore C API. No longer
	// editable from the settings widget.
	const FString GetBinaryPath() const;
	bool SetBinaryPath(const FString& InString);

	float GetRemoteStatusIntervalSeconds() const;
	void SetRemoteStatusIntervalSeconds(float InValue);

	void LoadSettings();
	void SaveSettings() const;

private:
	mutable FCriticalSection CriticalSection;

	FString BinaryPath;
	float RemoteStatusIntervalSeconds = 60.f;
};
