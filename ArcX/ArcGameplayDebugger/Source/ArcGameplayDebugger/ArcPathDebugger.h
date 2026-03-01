// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityHandle.h"
#include "MassEntityTypes.h"

class FArcPathDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void DrawEntityListPanel();
	void DrawEntityDetailPanel();
	void DrawPathInWorld();

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

	bool bDrawMoveTarget = true;
	bool bDrawVelocity = true;
	bool bDrawSteering = true;
	bool bDrawForces = true;
	bool bDrawShortPath = true;
	bool bDrawAvoidance = true;
};
