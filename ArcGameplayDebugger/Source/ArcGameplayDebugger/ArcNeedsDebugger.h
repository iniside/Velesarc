// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityHandle.h"
#include "MassEntityTypes.h"

class FArcNeedsDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void RefreshEntityList();
	void DrawEntityListPanel();
	void DrawDetailPanel();

	struct FNeedEntry
	{
		bool bHasHunger = false;
		bool bHasThirst = false;
		bool bHasFatigue = false;
	};

	struct FEntityEntry
	{
		FMassEntityHandle Entity;
		FString Label;
		FNeedEntry NeedInfo;
	};

	TArray<FEntityEntry> CachedEntities;
	int32 SelectedEntityIdx = INDEX_NONE;
	char FilterBuf[256] = {};
	float LastRefreshTime = 0.f;
};
