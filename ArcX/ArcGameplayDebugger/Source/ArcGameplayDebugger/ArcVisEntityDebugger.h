// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityHandle.h"

class FArcVisEntityDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void DrawEntityListPanel();
	void DrawEntityDetailPanel();
	void DrawSelectedEntityInWorld();
	void DrawGridVisualization();

	void RefreshEntityList();

	struct FVisEntityEntry
	{
		FMassEntityHandle Entity;
		FString Label;
		FVector Location = FVector::ZeroVector;
		bool bIsActorRepresentation = false;
	};

	TArray<FVisEntityEntry> CachedEntities;
	int32 SelectedEntityIndex = INDEX_NONE;
	char EntityFilterBuf[256] = {};
	float LastRefreshTime = 0.f;
	bool bShowGrid = false;
};
