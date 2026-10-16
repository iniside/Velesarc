// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EArcTraceTargetMode : uint8
{
	File,
	Network,
	None,
};

struct FArcTraceChannelEntry
{
	FString Name;
	FString Description;
	bool bEnabled = false;
	bool bReadOnly = false;
};

struct FArcTracePresetEntry
{
	FString Name;
	FString ChannelList;
	bool bReadOnly = false;
	bool bFromSettings = false;
};

class FArcTraceDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void DrawStatus();
	void DrawControls();
	void DrawTarget();
	void DrawPresets();
	void DrawChannels();

	static bool MatchesFilter(const FArcTraceChannelEntry& Entry, const FString& Filter);

	void RefreshChannels();
	void RefreshPresets();

	FString GetTargetDirectory() const;
	FString BuildFullPath(const FString& FileName) const;

	void StartTrace();
	void WriteSnapshotToFile();
	void SendSnapshotToHost();

	TArray<FArcTraceChannelEntry> Channels;
	TArray<FArcTracePresetEntry> Presets;

	EArcTraceTargetMode TargetMode = EArcTraceTargetMode::File;

	char DirectoryBuf[512] = {};
	char HostBuf[128] = {};
	char ChannelFilterBuf[128] = {};

	bool bTruncateFile = false;
	bool bExcludeTail = false;

	FString PendingFileName;
	FString LastStatus;

	bool bWasConnected = false;

	float ChannelRefreshTimer = 0.0f;
};
