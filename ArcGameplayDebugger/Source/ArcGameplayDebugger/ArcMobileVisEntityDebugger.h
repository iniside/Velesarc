// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityHandle.h"
#include "ArcMass/MobileVisualization/ArcMobileVisualization.h"

struct FMassEntityManager;

class FArcMobileVisEntityDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void DrawEntityListPanel();
	void DrawEntityDetailPanel();
	void RefreshEntityList();

	struct FMobileVisEntityEntry
	{
		FMassEntityHandle Entity;
		FString Label;
		FVector Location = FVector::ZeroVector;
		EArcMobileVisLOD LOD = EArcMobileVisLOD::None;
	};

	TArray<FMobileVisEntityEntry> CachedEntities;
	int32 SelectedEntityIndex = INDEX_NONE;
	char EntityFilterBuf[256] = {};
	float LastRefreshTime = 0.f;
};
