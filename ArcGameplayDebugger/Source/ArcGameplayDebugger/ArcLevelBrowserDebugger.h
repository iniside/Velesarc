// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

class FArcLevelBrowserDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void DrawLevelList();
	void DrawLevelDetail();
	void DrawGameFeatures();

	void RefreshLevels();

	TArray<FAssetData> CachedLevels;
	TArray<FString> CachedLevelNames;
	TArray<FString> CachedPackageNames;
	int32 SelectedLevelIndex = -1;
	char FilterBuf[256] = {};
	FString LastGameFeatureStatus;

	TSharedRef<uint8> AliveToken = MakeShared<uint8>();
};
