// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ArcTerrainSettings.generated.h"

/**
 * Project Settings surface (Project Settings -> Plugins -> Arc Terrain) for ArcTerrain's EDITOR
 * SCHEDULER budgets — how much time and memory the bake/composite rails may spend, not what the
 * terrain looks like. Every shape knob (residency radii and hysteresis, sector/weight shifts, the CBT
 * metric, collision density) is authored per terrain on AArcTerrain; there is one terrain per level
 * and one place per knob.
 *
 * The property here is the ONLY source: there is no cvar override channel. Every knob is read fresh
 * per-tick, so an edit auto-applies next frame with no delegate. The cache budget only trims on
 * insert, so UArcTerrainSubsystem subscribes OnSettingChanged (editor-only) to force an immediate
 * re-trim when it is lowered.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Arc Terrain"))
class ARCTERRAIN_API UArcTerrainSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Shows under Project Settings -> Arc Game (the project-wide ArcX settings bucket). */
	virtual FName GetCategoryName() const override { return FName("Arc Game"); }

	// ---- Scheduler ----

	/** Per-frame game-thread deadline (ms) for cell recomposites (at least one always runs). */
	UPROPERTY(config, EditAnywhere, Category = "Scheduler", meta = (ClampMin = "0.0"))
	float CompositeBudgetMs = 2.0f;

	/** Hard cap on cell recomposites per frame, independent of the ms budget. */
	UPROPERTY(config, EditAnywhere, Category = "Scheduler", meta = (ClampMin = "1"))
	int32 MaxCompositesPerFrame = 8;

	// ---- Baked-section bake rail ----

	/** Ticks a cell must go without a composite before it counts as settled (bake candidate). */
	UPROPERTY(config, EditAnywhere, Category = "Bake Rail", meta = (ClampMin = "1"))
	int32 SettleTicks = 30;

	/** Per-frame game-thread deadline (ms) for the heavy per-cell bake steps (at least one always runs). */
	UPROPERTY(config, EditAnywhere, Category = "Bake Rail", meta = (ClampMin = "0.0"))
	float BakeBudgetMs = 2.0f;

	UPROPERTY(config, EditAnywhere, Category = "Bake Rail", meta = (ClampMin = "1"))
	int32 MaxTileBakeDDCProbes = 32;

	UPROPERTY(config, EditAnywhere, Category = "Bake Rail", meta = (ClampMin = "1"))
	int32 MaxTileBakeWorkerEvaluations = 16;

	UPROPERTY(config, EditAnywhere, Category = "Bake Rail", meta = (ClampMin = "1"))
	int32 MaxTileBakeBackgroundWrites = 8;

	UPROPERTY(config, EditAnywhere, Category = "Bake Rail", meta = (ClampMin = "1"))
	int32 MaxTileBakeInFlightMB = 512;

	UPROPERTY(config, EditAnywhere, Category = "Bake Rail", meta = (ClampMin = "1"))
	int32 MaxTileBakeCompletionsPerFrame = 16;

	// ---- Runtime workers ----

	UPROPERTY(config, EditAnywhere, Category = "Runtime Workers", meta = (ClampMin = "1"))
	int32 RuntimeWorkerTasks = 16;

	UPROPERTY(config, EditAnywhere, Category = "Runtime Workers", meta = (ClampMin = "1"))
	int32 RuntimeOwnedMiB = 256;

	UPROPERTY(config, EditAnywhere, Category = "Runtime Workers", meta = (ClampMin = "1"))
	int32 RuntimeCompletionsPerFrame = 16;

	UPROPERTY(config, EditAnywhere, Category = "Runtime Workers", meta = (ClampMin = "0"))
	int32 RuntimeAtlasLaunchesPerFrame = 4;

	UPROPERTY(config, EditAnywhere, Category = "Runtime Workers", meta = (ClampMin = "0"))
	int32 RuntimeLightingLaunchesPerFrame = 8;

	UPROPERTY(config, EditAnywhere, Category = "Runtime Workers", meta = (ClampMin = "0"))
	int32 RuntimeFarLaunchesPerFrame = 4;

	// ---- Cache ----

	/** Budget (MiB) of the GPU/CPU stamp-source cache; LRU entries not in-flight evict past it. */
	UPROPERTY(config, EditAnywhere, Category = "Cache", meta = (ClampMin = "0"))
	int32 SourceCacheMB = 1536;

	/** Ceiling (MiB, clamped to SourceCacheMB) on the source bytes one data-mask bake pass may hold
	 *  unevictable; sources past it decode per tile as before. */
	UPROPERTY(config, EditAnywhere, Category = "Cache", meta = (ClampMin = "0"))
	int32 SourceCachePinMB = 1024;

	/** 1 = flush queued section-package saves through the experimental SaveConcurrent batch API. */
	UPROPERTY(config, EditAnywhere, Category = "Cache", meta = (ClampMin = "0", ClampMax = "1"))
	int32 BatchSaveConcurrent = 1;
};
