// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityHandle.h"
#include "MassEntityTypes.h"

class FArcAIDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void DrawEntityListPanel();
	void DrawEntityDetailPanel();
	void DrawPlanInWorld();

	void RefreshEntityList();

	struct FEntityEntry
	{
		FMassEntityHandle Entity;
		FString Label;
		FVector Location = FVector::ZeroVector;
	};

	TArray<FEntityEntry> CachedEntities;
	int32 SelectedEntityIndex = INDEX_NONE;
	char EntityFilterBuf[256] = {};
	float LastRefreshTime = 0.f;
};
