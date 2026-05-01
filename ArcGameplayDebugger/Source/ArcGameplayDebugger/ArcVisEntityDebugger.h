// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityHandle.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class UArcVisEntityDebuggerDrawComponent;
class UWorld;
struct FMassEntityManager;
struct FArcVisualizationGrid;
struct FArcVisISMHolderKey;
struct FArcVisEntityDebugDrawData;

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
	void DrawSelectedEntityInWorld(FMassEntityManager& Manager, FArcVisEntityDebugDrawData& OutDrawData);
	void DrawGridVisualization(FMassEntityManager& Manager, UWorld& World, FArcVisEntityDebugDrawData& OutDrawData);
	void DrawHolderListPanel();
	void DrawHolderDetailPanel();
	void DrawSelectedHolderInWorld(FMassEntityManager& Manager, UWorld& World, FArcVisEntityDebugDrawData& OutDrawData);
	void RefreshHolderList();
	void DrawSingleGrid(UWorld* World, const FArcVisualizationGrid& Grid, const FIntVector& PlayerCell,
		int32 ActivationCells, int32 DeactivationCells, FColor InActiveColor, FColor BoundaryColor,
		FMassEntityManager* Manager, bool bIsMeshGrid, FArcVisEntityDebugDrawData& OutDrawData);

	void RefreshEntityList();

	void EnsureDrawActor(UWorld* World);
	void DestroyDrawActor();

	struct FVisEntityEntry
	{
		FMassEntityHandle Entity;
		FString Label;
		FVector Location = FVector::ZeroVector;
	};

	struct FHolderEntityEntry
	{
		FMassEntityHandle Entity;
		FString MeshName;
		int32 InstanceCount = 0;
		FIntVector Cell = FIntVector::ZeroValue;
		bool bCastShadow = false;
		TArray<int32> ReferencingVisEntityIndices;
	};

	TArray<FVisEntityEntry> CachedEntities;
	int32 SelectedEntityIndex = INDEX_NONE;
	char EntityFilterBuf[256] = {};
	float LastRefreshTime = 0.f;
	bool bShowGrid = false;
	TArray<FHolderEntityEntry> CachedHolders;
	int32 SelectedHolderIndex = INDEX_NONE;
	char HolderFilterBuf[256] = {};
	int32 PendingVisEntityNavIndex = INDEX_NONE;
	int32 PendingHolderNavIndex = INDEX_NONE;

	TWeakObjectPtr<AActor> DrawActor;
	TWeakObjectPtr<UArcVisEntityDebuggerDrawComponent> DrawComponent;
};
