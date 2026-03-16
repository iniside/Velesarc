// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

class UArcLootTable;

/** Per-entry statistics from a Monte Carlo loot simulation. */
struct FArcLootSimEntryStats
{
	FString ItemName;
	int32 TimesDropped = 0;
	int64 TotalAmount = 0;
};

/**
 * ImGui debug window for inspecting ArcLootTable assets.
 *
 * Features:
 * - Browse all UArcLootTable assets via Asset Registry
 * - Inspect entries: item, weight, probability, amounts
 * - Monte Carlo probability simulation
 * - Drop loot near the player via UArcLootDropSubsystem
 */
class FArcLootTableDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void DrawTableList();
	void DrawTableDetail(UArcLootTable* Table);
	void DrawEntriesTable(const UArcLootTable* Table);
	void DrawStrategyInfo(const UArcLootTable* Table);
	void DrawSimulation(const UArcLootTable* Table);
	void DrawSpawnControls(const UArcLootTable* Table);

	void RunSimulation(const UArcLootTable* Table);
	void RefreshLootTables();

	// Asset list
	TArray<FAssetData> CachedLootTables;
	TArray<FString> CachedLootTableNames;
	int32 SelectedTableIndex = -1;
	char FilterBuf[256] = {};

	// Loaded table (kept alive during inspection)
	TWeakObjectPtr<UArcLootTable> LoadedTable;

	// Simulation state
	int32 SimulationRolls = 1000;
	TArray<FArcLootSimEntryStats> SimulationResults;
	bool bSimulationDirty = true;
};
