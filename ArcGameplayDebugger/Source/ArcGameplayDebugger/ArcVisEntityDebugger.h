// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityHandle.h"

class UWorld;
struct FMassEntityManager;
struct FArcVisualizationGrid;

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
	void DrawSingleGrid(UWorld* World, const FArcVisualizationGrid& Grid, const FIntVector& PlayerCell,
		int32 ActivationCells, int32 DeactivationCells, FColor InActiveColor, FColor BoundaryColor,
		FMassEntityManager* Manager, bool bIsMeshGrid);

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
