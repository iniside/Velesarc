// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainDiagnostics.h"

#include "ArcTerrainActor.h"
#include "ArcTerrainSubsystem.h"
#include "ArcTerrainTileAccess.h"
#include "ArcTerrainTileNames.h"
#include "Render/ArcTerrainTileAtlas.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ObjectKey.h"

DEFINE_LOG_CATEGORY(LogArcTerrainDiag);

DECLARE_CYCLE_STAT(TEXT("Tick total"), STAT_ArcTerrainTickTotal, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Phase: edit pins"), STAT_ArcTerrainEditPins, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Phase: residency"), STAT_ArcTerrainResidency, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Phase: tile stream"), STAT_ArcTerrainTileStream, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Phase: lighting"), STAT_ArcTerrainLighting, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Phase: far blocks"), STAT_ArcTerrainFarBlocks, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Phase: composite"), STAT_ArcTerrainComposite, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Phase: atlas"), STAT_ArcTerrainAtlas, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Phase: collision"), STAT_ArcTerrainCollision, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Phase: sync anchor"), STAT_ArcTerrainSyncAnchor, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Phase: bake"), STAT_ArcTerrainBake, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Blocking game thread"), STAT_ArcTerrainBlockingGT, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Diagnostics gauge gather"), STAT_ArcTerrainDiagGather, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: tile key gather"), STAT_ArcTerrainTileKeyGather, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: tile key recipe"), STAT_ArcTerrainTileKeyRecipe, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: tile key weight"), STAT_ArcTerrainTileKeyWeight, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: tile key data"), STAT_ArcTerrainTileKeyData, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: collision modifier sync"), STAT_ArcTerrainCollisionModifierSync,
	STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: collision height-op gather"), STAT_ArcTerrainCollisionHeightOpGather,
	STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: collision heightfield build"), STAT_ArcTerrainCollisionHeightfieldBuild,
	STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: collision apply"), STAT_ArcTerrainCollisionApply, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: SetHeightfield physics"), STAT_ArcTerrainSetHeightfieldPhysics,
	STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: SetHeightfield bounds"), STAT_ArcTerrainSetHeightfieldBounds,
	STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: SetHeightfield navigation"), STAT_ArcTerrainSetHeightfieldNavigation,
	STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: height finalize minmax"), STAT_ArcTerrainHeightFinalizeMinMax,
	STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: height finalize encode"), STAT_ArcTerrainHeightFinalizeEncode,
	STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: height finalize write"), STAT_ArcTerrainHeightFinalizeWrite,
	STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: atlas residency reconcile"), STAT_ArcTerrainAtlasResidencyReconcile,
	STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: atlas admission"), STAT_ArcTerrainAtlasAdmission, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: atlas height snapshot"), STAT_ArcTerrainAtlasHeightSnapshot,
	STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: atlas weight snapshot"), STAT_ArcTerrainAtlasWeightSnapshot,
	STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: atlas cbt topology"), STAT_ArcTerrainAtlasCbtTopology, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: atlas cbt publish"), STAT_ArcTerrainAtlasCbtPublish, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: atlas preview bind"), STAT_ArcTerrainAtlasPreviewBind, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: lighting window reconcile"), STAT_ArcTerrainLightingWindowReconcile,
	STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: lighting payload snapshot"), STAT_ArcTerrainLightingPayloadSnapshot,
	STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: lighting completion apply"), STAT_ArcTerrainLightingCompletionApply,
	STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: far window reconcile"), STAT_ArcTerrainFarWindowReconcile, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: far payload snapshot"), STAT_ArcTerrainFarPayloadSnapshot, STATGROUP_ArcTerrain);
DECLARE_CYCLE_STAT(TEXT("Leaf: far completion apply"), STAT_ArcTerrainFarCompletionApply, STATGROUP_ArcTerrain);

DECLARE_DWORD_COUNTER_STAT(TEXT("Live cells"), STAT_ArcTerrainCells, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("Collision patches"), STAT_ArcTerrainCollisionPatches, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("Height tiles loaded"), STAT_ArcTerrainHeightLoaded, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("Height tiles in flight"), STAT_ArcTerrainHeightInFlight, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("Height tiles unusable"), STAT_ArcTerrainHeightUnusable, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("Height manifest entries"), STAT_ArcTerrainHeightManifest, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("Weight tiles loaded"), STAT_ArcTerrainWeightLoaded, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("Weight tiles in flight"), STAT_ArcTerrainWeightInFlight, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("Weight tiles unusable"), STAT_ArcTerrainWeightUnusable, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("Weight manifest entries"), STAT_ArcTerrainWeightManifest, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("Data tiles loaded"), STAT_ArcTerrainDataLoaded, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("Data tiles in flight"), STAT_ArcTerrainDataInFlight, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("Data tiles unusable"), STAT_ArcTerrainDataUnusable, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("Data manifest entries"), STAT_ArcTerrainDataManifest, STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("Height slices granted this tick"), STAT_ArcTerrainHeightSlicesGranted,
	STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("Height slices evicted this tick"), STAT_ArcTerrainHeightSlicesEvicted,
	STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("Weight slices granted this tick"), STAT_ArcTerrainWeightSlicesGranted,
	STATGROUP_ArcTerrain);
DECLARE_DWORD_COUNTER_STAT(TEXT("Weight slices evicted this tick"), STAT_ArcTerrainWeightSlicesEvicted,
	STATGROUP_ArcTerrain);

namespace ArcTerrainDiagPrivate_5d71
{
	struct FWorldState
	{
		ArcTerrainDiag::FFrame Frame;

		/** Reset with the window: the per-tick slice stats are a difference against it. */
		ArcTerrainDiag::FAtlasCounters PrevTickAtlas;

		double LastEmitSeconds = 0.0;

		FString Label;

		uint64 BakeProgressCompleted = 0;
		uint64 BakeProgressStartCompleted = 0;
		uint64 BakeProgressLastCompleted = 0;
		double BakeProgressStartSeconds = 0.0;
		double BakeProgressLastSeconds = 0.0;
		bool bBakeProgressActive = false;
	};

	TMap<FObjectKey, TUniquePtr<FWorldState>> GWorldStates;

	FWorldState GOrphanState;

	FWorldState* GCurrentState = &GOrphanState;

	FWorldState& State()
	{
		return *GCurrentState;
	}

	uint64 GStatFrameNumber = MAX_uint64;
	ArcTerrainDiag::FTimings GStatTimings;
	ArcTerrainDiag::FGauges GStatGauges;
	ArcTerrainDiag::FAtlasCounters GStatAtlasDelta;
	double GStatGatherSeconds = 0.0;

	void AccumulateGauges(ArcTerrainDiag::FGauges& Out, const ArcTerrainDiag::FGauges& In)
	{
		for (int32 Rail = 0; Rail < ArcTerrain::NumTileRails; ++Rail)
		{
			for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
			{
				Out.TilesLoaded[Rail][Tier] += In.TilesLoaded[Rail][Tier];
				Out.TilesInFlight[Rail][Tier] += In.TilesInFlight[Rail][Tier];
				Out.TilesUnusable[Rail][Tier] += In.TilesUnusable[Rail][Tier];
				Out.ManifestEntries[Rail][Tier] += In.ManifestEntries[Rail][Tier];
				Out.BakeQueue[Rail][Tier] += In.BakeQueue[Rail][Tier];
				Out.BakeProbes[Rail][Tier] += In.BakeProbes[Rail][Tier];
				Out.BakeEvaluations[Rail][Tier] += In.BakeEvaluations[Rail][Tier];
				Out.BakeWrites[Rail][Tier] += In.BakeWrites[Rail][Tier];
				Out.BakeDeferred[Rail][Tier] += In.BakeDeferred[Rail][Tier];
			}
			Out.BakeInFlight[Rail] += In.BakeInFlight[Rail];
		}
		Out.Terrains += In.Terrains;
		Out.Cells += In.Cells;
		Out.CollisionPatches += In.CollisionPatches;
		Out.PatchesMissingHeightSource += In.PatchesMissingHeightSource;
	}

	int32 SumTiers(const int32 (&PerTier)[ArcTerrain::MaxTiers])
	{
		int32 Sum = 0;
		for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
		{
			Sum += PerTier[Tier];
		}
		return Sum;
	}

	void GatherBakeProgress(const ArcTerrainDiag::FGauges& Gauges, int32& OutRemaining, int32& OutReady,
		int32& OutDeferred, int32& OutProbing, int32& OutEvaluating, int32& OutWriting)
	{
		OutRemaining = 0;
		OutReady = 0;
		OutDeferred = 0;
		OutProbing = 0;
		OutEvaluating = 0;
		OutWriting = 0;
		for (int32 Rail = 0; Rail < ArcTerrain::NumTileRails; ++Rail)
		{
			for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
			{
				const int32 Deferred = Gauges.BakeDeferred[Rail][Tier];
				const int32 Probing = Gauges.BakeProbes[Rail][Tier];
				const int32 Evaluating = Gauges.BakeEvaluations[Rail][Tier];
				const int32 Writing = Gauges.BakeWrites[Rail][Tier];
				const int32 Ready = FMath::Max(
					Gauges.BakeQueue[Rail][Tier] - Deferred - Probing - Evaluating - Writing, 0);
				OutReady += Ready;
				OutDeferred += Deferred;
				OutProbing += Probing;
				OutEvaluating += Evaluating;
				OutWriting += Writing;
				OutRemaining += Ready + Deferred + Probing + Evaluating + Writing;
			}
		}
	}

	constexpr float MinDiagIntervalSeconds = 0.1f;

	const TCHAR* LoadResultName(EArcTerrainTileLoad Result)
	{
		switch (Result)
		{
		case EArcTerrainTileLoad::FileMissing:
			return TEXT("no file");
		case EArcTerrainTileLoad::ReadFailed:
			return TEXT("read failed");
		case EArcTerrainTileLoad::Unusable:
			return TEXT("bad header or foreign lattice");
		default:
			return TEXT("loaded");
		}
	}

	const TCHAR* CellRailName(ArcTerrainDiag::ECellRail Producer)
	{
		switch (Producer)
		{
		case ArcTerrainDiag::ECellRail::Pin:
			return TEXT("pin");
		case ArcTerrainDiag::ECellRail::Content:
			return TEXT("content");
		default:
			return TEXT("bounds");
		}
	}

	double Megabytes(int64 Bytes)
	{
		return static_cast<double>(Bytes) / (1024.0 * 1024.0);
	}

	FString TrimNote(const TCHAR* FieldName, int32 Authored, int32 Resolved)
	{
		return Resolved < Authored
			? FString::Printf(TEXT(" [%s TRIMMED %d -> %d]"), FieldName, Authored, Resolved)
			: FString::Printf(TEXT(" [%s %d]"), FieldName, Resolved);
	}

	void RecordWorst(ArcTerrainDiag::FTimings& Timings, const TCHAR* What, FIntPoint Key, double Seconds)
	{
		if (Seconds > Timings.WorstSeconds)
		{
			Timings.WorstSeconds = Seconds;
			Timings.WorstKey = Key;
			Timings.WorstWhat = What;
		}
	}

	double PhaseMs(const ArcTerrainDiag::FTimings& Timings, ArcTerrainDiag::EPhase Phase)
	{
		return Timings.PhaseSeconds[static_cast<int32>(Phase)] * 1000.0;
	}

	double MaxPhaseMs(const ArcTerrainDiag::FWindow& Window, ArcTerrainDiag::EPhase Phase)
	{
		return Window.MaxPhaseSeconds[static_cast<int32>(Phase)] * 1000.0;
	}

	double LeafMs(const ArcTerrainDiag::FTimings& Timings, ArcTerrainDiag::ELeafTiming Leaf)
	{
		return Timings.LeafSeconds[static_cast<int32>(Leaf)] * 1000.0;
	}

	double MaxLeafMs(const ArcTerrainDiag::FWindow& Window, ArcTerrainDiag::ELeafTiming Leaf)
	{
		return Window.MaxLeafSeconds[static_cast<int32>(Leaf)] * 1000.0;
	}

	uint64 PhaseCycles(const ArcTerrainDiag::FTimings& Timings, ArcTerrainDiag::EPhase Phase)
	{
		return FPlatformTime::SecondsToCycles64(Timings.PhaseSeconds[static_cast<int32>(Phase)]);
	}

	uint64 LeafCycles(const ArcTerrainDiag::FTimings& Timings, ArcTerrainDiag::ELeafTiming Leaf)
	{
		return FPlatformTime::SecondsToCycles64(Timings.LeafSeconds[static_cast<int32>(Leaf)]);
	}

	FString FormatWorst(const ArcTerrainDiag::FTimings& Timings)
	{
		return Timings.WorstWhat != nullptr
			? FString::Printf(TEXT(" | worst: %s (%d,%d) %.2f ms"), Timings.WorstWhat, Timings.WorstKey.X,
				Timings.WorstKey.Y, Timings.WorstSeconds * 1000.0)
			: FString();
	}

	FString FormatWindowTimings(const ArcTerrainDiag::FWindow& Window, const FString& Label)
	{
		using ArcTerrainDiag::EPhase;
		using ArcTerrainDiag::ELeafTiming;

		const double AvgFrameMs = Window.Ticks > 0
			? (Window.SumFrameDeltaSeconds / Window.Ticks) * 1000.0
			: 0.0;
		const double WorstFrameMs = Window.MaxFrameDeltaSeconds * 1000.0;
		const double SharePct = Window.SumFrameDeltaSeconds > 0.0
			? (Window.Sum.TotalSeconds / Window.SumFrameDeltaSeconds) * 100.0
			: 0.0;

		return FString::Printf(
			TEXT("[Time] world '%s' ticks %d over %.2f s | frame avg %.2f ms worst %.2f ms | ")
			TEXT("terrain sum %.2f ms (%.1f%% of frame) | diag overhead %.2f/%.2f ms (NOT in terrain sum) | ")
			TEXT("sum/max ms — total %.2f/%.2f | blockingGT %.2f/%.2f | ")
			TEXT("editpins %.2f/%.2f residency %.2f/%.2f tilestream %.2f/%.2f lighting %.2f/%.2f farblocks %.2f/%.2f composite %.2f/%.2f ")
			TEXT("atlas %.2f/%.2f collision %.2f/%.2f syncanchor %.2f/%.2f bake %.2f/%.2f | ")
			TEXT("leaf sum/max ms — tilekey gather %.2f/%.2f recipe %.2f/%.2f weight %.2f/%.2f ")
			TEXT("data %.2f/%.2f | collision modifiers %.2f/%.2f heightops %.2f/%.2f ")
			TEXT("heightfield %.2f/%.2f apply %.2f/%.2f | setheightfield physics %.2f/%.2f ")
			TEXT("bounds %.2f/%.2f navigation %.2f/%.2f | heightfinalize minmax %.2f/%.2f ")
			TEXT("encode %.2f/%.2f write %.2f/%.2f | atlas residency %.2f/%.2f admit %.2f/%.2f ")
			TEXT("height %.2f/%.2f weight %.2f/%.2f cbt topology %.2f/%.2f publish %.2f/%.2f ")
			TEXT("preview %.2f/%.2f | lighting window %.2f/%.2f payload %.2f/%.2f apply %.2f/%.2f | ")
			TEXT("far window %.2f/%.2f payload %.2f/%.2f apply %.2f/%.2f%s"),
			*Label, Window.Ticks, Window.WindowWallSeconds,
			AvgFrameMs, WorstFrameMs, Window.Sum.TotalSeconds * 1000.0, SharePct,
			Window.SumDiagGatherSeconds * 1000.0, Window.MaxDiagGatherSeconds * 1000.0,
			Window.Sum.TotalSeconds * 1000.0, Window.MaxTotalSeconds * 1000.0,
			Window.Sum.BlockingGameThreadSeconds * 1000.0, Window.MaxBlockingGameThreadSeconds * 1000.0,
			PhaseMs(Window.Sum, EPhase::EditPins), MaxPhaseMs(Window, EPhase::EditPins),
			PhaseMs(Window.Sum, EPhase::Residency), MaxPhaseMs(Window, EPhase::Residency),
			PhaseMs(Window.Sum, EPhase::TileStream), MaxPhaseMs(Window, EPhase::TileStream),
			PhaseMs(Window.Sum, EPhase::Lighting), MaxPhaseMs(Window, EPhase::Lighting),
			PhaseMs(Window.Sum, EPhase::FarBlocks), MaxPhaseMs(Window, EPhase::FarBlocks),
			PhaseMs(Window.Sum, EPhase::Composite), MaxPhaseMs(Window, EPhase::Composite),
			PhaseMs(Window.Sum, EPhase::Atlas), MaxPhaseMs(Window, EPhase::Atlas),
			PhaseMs(Window.Sum, EPhase::Collision), MaxPhaseMs(Window, EPhase::Collision),
			PhaseMs(Window.Sum, EPhase::SyncAnchor), MaxPhaseMs(Window, EPhase::SyncAnchor),
			PhaseMs(Window.Sum, EPhase::Bake), MaxPhaseMs(Window, EPhase::Bake),
			LeafMs(Window.Sum, ELeafTiming::TileKeyGather), MaxLeafMs(Window, ELeafTiming::TileKeyGather),
			LeafMs(Window.Sum, ELeafTiming::TileKeyRecipe), MaxLeafMs(Window, ELeafTiming::TileKeyRecipe),
			LeafMs(Window.Sum, ELeafTiming::TileKeyWeight), MaxLeafMs(Window, ELeafTiming::TileKeyWeight),
			LeafMs(Window.Sum, ELeafTiming::TileKeyData), MaxLeafMs(Window, ELeafTiming::TileKeyData),
			LeafMs(Window.Sum, ELeafTiming::CollisionModifierSync),
			MaxLeafMs(Window, ELeafTiming::CollisionModifierSync),
			LeafMs(Window.Sum, ELeafTiming::CollisionHeightOpGather),
			MaxLeafMs(Window, ELeafTiming::CollisionHeightOpGather),
			LeafMs(Window.Sum, ELeafTiming::CollisionHeightfieldBuild),
			MaxLeafMs(Window, ELeafTiming::CollisionHeightfieldBuild),
			LeafMs(Window.Sum, ELeafTiming::CollisionApply), MaxLeafMs(Window, ELeafTiming::CollisionApply),
			LeafMs(Window.Sum, ELeafTiming::SetHeightfieldPhysics),
			MaxLeafMs(Window, ELeafTiming::SetHeightfieldPhysics),
			LeafMs(Window.Sum, ELeafTiming::SetHeightfieldBounds),
			MaxLeafMs(Window, ELeafTiming::SetHeightfieldBounds),
			LeafMs(Window.Sum, ELeafTiming::SetHeightfieldNavigation),
			MaxLeafMs(Window, ELeafTiming::SetHeightfieldNavigation),
			LeafMs(Window.Sum, ELeafTiming::HeightFinalizeMinMax),
			MaxLeafMs(Window, ELeafTiming::HeightFinalizeMinMax),
			LeafMs(Window.Sum, ELeafTiming::HeightFinalizeEncode),
			MaxLeafMs(Window, ELeafTiming::HeightFinalizeEncode),
			LeafMs(Window.Sum, ELeafTiming::HeightFinalizeWrite),
			MaxLeafMs(Window, ELeafTiming::HeightFinalizeWrite),
			LeafMs(Window.Sum, ELeafTiming::AtlasResidencyReconcile),
			MaxLeafMs(Window, ELeafTiming::AtlasResidencyReconcile),
			LeafMs(Window.Sum, ELeafTiming::AtlasAdmission),
			MaxLeafMs(Window, ELeafTiming::AtlasAdmission),
			LeafMs(Window.Sum, ELeafTiming::AtlasHeightSnapshot),
			MaxLeafMs(Window, ELeafTiming::AtlasHeightSnapshot),
			LeafMs(Window.Sum, ELeafTiming::AtlasWeightSnapshot),
			MaxLeafMs(Window, ELeafTiming::AtlasWeightSnapshot),
			LeafMs(Window.Sum, ELeafTiming::AtlasCbtTopology),
			MaxLeafMs(Window, ELeafTiming::AtlasCbtTopology),
			LeafMs(Window.Sum, ELeafTiming::AtlasCbtPublish),
			MaxLeafMs(Window, ELeafTiming::AtlasCbtPublish),
			LeafMs(Window.Sum, ELeafTiming::AtlasPreviewBind),
			MaxLeafMs(Window, ELeafTiming::AtlasPreviewBind),
			LeafMs(Window.Sum, ELeafTiming::LightingWindowReconcile),
			MaxLeafMs(Window, ELeafTiming::LightingWindowReconcile),
			LeafMs(Window.Sum, ELeafTiming::LightingPayloadSnapshot),
			MaxLeafMs(Window, ELeafTiming::LightingPayloadSnapshot),
			LeafMs(Window.Sum, ELeafTiming::LightingCompletionApply),
			MaxLeafMs(Window, ELeafTiming::LightingCompletionApply),
			LeafMs(Window.Sum, ELeafTiming::FarWindowReconcile),
			MaxLeafMs(Window, ELeafTiming::FarWindowReconcile),
			LeafMs(Window.Sum, ELeafTiming::FarPayloadSnapshot),
			MaxLeafMs(Window, ELeafTiming::FarPayloadSnapshot),
			LeafMs(Window.Sum, ELeafTiming::FarCompletionApply),
			MaxLeafMs(Window, ELeafTiming::FarCompletionApply),
			*FormatWorst(Window.Sum));
	}

	FString FormatTimings(const ArcTerrainDiag::FTimings& Timings)
	{
		using ArcTerrainDiag::EPhase;
		using ArcTerrainDiag::ELeafTiming;

		return FString::Printf(
			TEXT("total %.2f ms | blockingGT %.2f | editpins %.2f residency %.2f ")
			TEXT("tilestream %.2f lighting %.2f farblocks %.2f composite %.2f atlas %.2f collision %.2f syncanchor %.2f bake %.2f | ")
			TEXT("leaf ms — tilekey gather %.2f recipe %.2f weight %.2f data %.2f | collision ")
			TEXT("modifiers %.2f heightops %.2f heightfield %.2f apply %.2f | setheightfield physics %.2f ")
			TEXT("bounds %.2f navigation %.2f | heightfinalize minmax %.2f encode %.2f write %.2f | ")
			TEXT("atlas residency %.2f admit %.2f height %.2f weight %.2f cbt topology %.2f publish %.2f ")
			TEXT("preview %.2f | lighting window %.2f payload %.2f apply %.2f | ")
			TEXT("far window %.2f payload %.2f apply %.2f%s"),
			Timings.TotalSeconds * 1000.0,
			Timings.BlockingGameThreadSeconds * 1000.0,
			PhaseMs(Timings, EPhase::EditPins), PhaseMs(Timings, EPhase::Residency),
			PhaseMs(Timings, EPhase::TileStream), PhaseMs(Timings, EPhase::Lighting),
			PhaseMs(Timings, EPhase::FarBlocks), PhaseMs(Timings, EPhase::Composite),
			PhaseMs(Timings, EPhase::Atlas), PhaseMs(Timings, EPhase::Collision),
			PhaseMs(Timings, EPhase::SyncAnchor), PhaseMs(Timings, EPhase::Bake),
			LeafMs(Timings, ELeafTiming::TileKeyGather), LeafMs(Timings, ELeafTiming::TileKeyRecipe),
			LeafMs(Timings, ELeafTiming::TileKeyWeight), LeafMs(Timings, ELeafTiming::TileKeyData),
			LeafMs(Timings, ELeafTiming::CollisionModifierSync),
			LeafMs(Timings, ELeafTiming::CollisionHeightOpGather),
			LeafMs(Timings, ELeafTiming::CollisionHeightfieldBuild),
			LeafMs(Timings, ELeafTiming::CollisionApply),
			LeafMs(Timings, ELeafTiming::SetHeightfieldPhysics),
			LeafMs(Timings, ELeafTiming::SetHeightfieldBounds),
			LeafMs(Timings, ELeafTiming::SetHeightfieldNavigation),
			LeafMs(Timings, ELeafTiming::HeightFinalizeMinMax),
			LeafMs(Timings, ELeafTiming::HeightFinalizeEncode),
			LeafMs(Timings, ELeafTiming::HeightFinalizeWrite),
			LeafMs(Timings, ELeafTiming::AtlasResidencyReconcile),
			LeafMs(Timings, ELeafTiming::AtlasAdmission),
			LeafMs(Timings, ELeafTiming::AtlasHeightSnapshot),
			LeafMs(Timings, ELeafTiming::AtlasWeightSnapshot),
			LeafMs(Timings, ELeafTiming::AtlasCbtTopology),
			LeafMs(Timings, ELeafTiming::AtlasCbtPublish),
			LeafMs(Timings, ELeafTiming::AtlasPreviewBind),
			LeafMs(Timings, ELeafTiming::LightingWindowReconcile),
			LeafMs(Timings, ELeafTiming::LightingPayloadSnapshot),
			LeafMs(Timings, ELeafTiming::LightingCompletionApply),
			LeafMs(Timings, ELeafTiming::FarWindowReconcile),
			LeafMs(Timings, ELeafTiming::FarPayloadSnapshot),
			LeafMs(Timings, ELeafTiming::FarCompletionApply),
			*FormatWorst(Timings));
	}

	int64 TileBodyBytes(EArcTerrainRail Rail, const FArcTerrainTileGrid& Grid)
	{
		const int64 PlaneTexels = static_cast<int64>(Grid.Resolution()) * Grid.Resolution();
		return PlaneTexels * static_cast<int64>(ArcTerrainTileFormat::BytesPerTexel(Rail));
	}
}

namespace ArcTerrainDiag
{
	int32 GLevel = 0;

	static FAutoConsoleVariableRef CVarArcTerrainDiag(
		TEXT("arc.Terrain.Diag"), GLevel,
		TEXT("Terrain diagnostics. 0 = off (nothing is formatted). 1 = summary lines for the ")
		TEXT("stream, the bake rails, the cells, the atlas and collision, once per ")
		TEXT("arc.Terrain.DiagIntervalSeconds and covering every tick since the last one. 2 = one line per ")
		TEXT("event: tile arrival, read rejection with its reason, tile write, cell create/destroy with the ")
		TEXT("rail that made it, collision patch build. Counters are summed over every registered terrain; ")
		TEXT("arc.Terrain.DiagAt X Y Z answers for one world position."),
		ECVF_Default);

	float GHitchMs = 20.0f;

	static FAutoConsoleVariableRef CVarArcTerrainHitchMs(
		TEXT("arc.Terrain.HitchMs"), GHitchMs,
		TEXT("Milliseconds above which one breakdown line is logged, independent of arc.Terrain.Diag: phase ")
		TEXT("times, blocking game-thread total and the worst single operation with its key. Read against ")
		TEXT("the larger of the tick total and the blocking total, so a synchronous read made off-tick by an ")
		TEXT("editor bake reports too. 0 stops the accumulation and the emit. Tested on EVERY tick; only ")
		TEXT("the emit is throttled, to arc.Terrain.DiagIntervalSeconds."),
		ECVF_Default);

	float GDiagIntervalSeconds = 1.0f;

	static FAutoConsoleVariableRef CVarArcTerrainDiagInterval(
		TEXT("arc.Terrain.DiagIntervalSeconds"), GDiagIntervalSeconds,
		TEXT("Seconds between terrain diagnostic emits — it governs BOTH the arc.Terrain.Diag summary and ")
		TEXT("the arc.Terrain.HitchMs breakdown. Every counter, phase sum and phase maximum accumulates ")
		TEXT("across the whole interval and is reset only when it is emitted, so one line describes the ")
		TEXT("window and not one arbitrary tick. arc.Terrain.HitchMs is still tested per tick; the emitted ")
		TEXT("breakdown is the worst tick that breached it, with a count of how many did. Clamped to a ")
		TEXT("minimum of 0.1. Independent of stat arcterrain, which is never throttled by this."),
		ECVF_Default);

	int32 GBakeProgress = 1;

	static FAutoConsoleVariableRef CVarArcTerrainBakeProgress(
		TEXT("arc.Terrain.BakeProgress"), GBakeProgress,
		TEXT("Logs active ArcTerrain bake work once per arc.Terrain.BakeProgressIntervalSeconds: remaining ")
		TEXT("tiles by scheduler stage, completed tiles per second and ETA. 0 disables it."),
		ECVF_Default);

	float GBakeProgressIntervalSeconds = 1.0f;

	static FAutoConsoleVariableRef CVarArcTerrainBakeProgressInterval(
		TEXT("arc.Terrain.BakeProgressIntervalSeconds"), GBakeProgressIntervalSeconds,
		TEXT("Seconds between active ArcTerrain bake progress lines while work remains. Clamped to 0.1."),
		ECVF_Default);

	const TCHAR* RailName(EArcTerrainRail Rail)
	{
		switch (Rail)
		{
		case EArcTerrainRail::Weight:
			return TEXT("weight");
		case EArcTerrainRail::Data:
			return TEXT("data");
		default:
			return TEXT("height");
		}
	}

	void SetTickingWorld(const UWorld* World)
	{
		using namespace ArcTerrainDiagPrivate_5d71;

		if (World == nullptr)
		{
			return;
		}
		TUniquePtr<FWorldState>& Slot = GWorldStates.FindOrAdd(FObjectKey(World));
		if (!Slot.IsValid())
		{
			Slot = MakeUnique<FWorldState>();
			Slot->Label = World->GetDebugDisplayName();
		}
		GCurrentState = Slot.Get();
	}

	void ReleaseWorld(const UWorld* World)
	{
		using namespace ArcTerrainDiagPrivate_5d71;

		if (World == nullptr)
		{
			return;
		}
		const FObjectKey Key(World);
		if (TUniquePtr<FWorldState>* Slot = GWorldStates.Find(Key))
		{
			if (GCurrentState == Slot->Get())
			{
				GCurrentState = &GOrphanState;
			}
			GWorldStates.Remove(Key);
		}
	}

	FFrame& Frame()
	{
		return ArcTerrainDiagPrivate_5d71::State().Frame;
	}

	void ResetFrame()
	{
		using namespace ArcTerrainDiagPrivate_5d71;

		FWorldState& WorldState = State();
		WorldState.Frame = FFrame();
		WorldState.PrevTickAtlas = FAtlasCounters();
	}

	void CompleteTickTimings(FTimings& TickTimings)
	{
		const FTimings& Accumulated = ArcTerrainDiagPrivate_5d71::State().Frame.Tick;
		for (int32 Index = 0; Index < static_cast<int32>(ELeafTiming::Count); ++Index)
		{
			TickTimings.LeafSeconds[Index] = Accumulated.LeafSeconds[Index];
		}
		TickTimings.BlockingGameThreadSeconds = Accumulated.BlockingGameThreadSeconds;
		TickTimings.WorstSeconds = Accumulated.WorstSeconds;
		TickTimings.WorstKey = Accumulated.WorstKey;
		TickTimings.WorstWhat = Accumulated.WorstWhat;
	}

	void EndTick(const FTimings& TickTimings, double FrameDeltaSeconds, double DiagGatherSeconds)
	{
		using namespace ArcTerrainDiagPrivate_5d71;

		FWorldState& WorldState = State();
		if (!IsTiming())
		{
			WorldState.Frame.Tick = FTimings();
			return;
		}

		FWindow& Window = WorldState.Frame.Window;
		++Window.Ticks;
		Window.Sum.TotalSeconds += TickTimings.TotalSeconds;
		Window.SumFrameDeltaSeconds += FrameDeltaSeconds;
		Window.MaxFrameDeltaSeconds = FMath::Max(Window.MaxFrameDeltaSeconds, FrameDeltaSeconds);
		Window.SumDiagGatherSeconds += DiagGatherSeconds;
		Window.MaxDiagGatherSeconds = FMath::Max(Window.MaxDiagGatherSeconds, DiagGatherSeconds);
		Window.Sum.BlockingGameThreadSeconds += TickTimings.BlockingGameThreadSeconds;
		Window.MaxTotalSeconds = FMath::Max(Window.MaxTotalSeconds, TickTimings.TotalSeconds);
		Window.MaxBlockingGameThreadSeconds =
			FMath::Max(Window.MaxBlockingGameThreadSeconds, TickTimings.BlockingGameThreadSeconds);
		for (int32 Index = 0; Index < static_cast<int32>(EPhase::Count); ++Index)
		{
			Window.Sum.PhaseSeconds[Index] += TickTimings.PhaseSeconds[Index];
			Window.MaxPhaseSeconds[Index] =
				FMath::Max(Window.MaxPhaseSeconds[Index], TickTimings.PhaseSeconds[Index]);
		}
		for (int32 Index = 0; Index < static_cast<int32>(ELeafTiming::Count); ++Index)
		{
			Window.Sum.LeafSeconds[Index] += TickTimings.LeafSeconds[Index];
			Window.MaxLeafSeconds[Index] =
				FMath::Max(Window.MaxLeafSeconds[Index], TickTimings.LeafSeconds[Index]);
		}
		RecordWorst(Window.Sum, TickTimings.WorstWhat, TickTimings.WorstKey, TickTimings.WorstSeconds);

		const double GateSeconds =
			FMath::Max(TickTimings.TotalSeconds, TickTimings.BlockingGameThreadSeconds);
		if (GHitchMs > 0.0f && GateSeconds * 1000.0 >= static_cast<double>(GHitchMs))
		{
			++Window.HitchTicks;
			if (GateSeconds > Window.WorstHitchSeconds)
			{
				Window.WorstHitchSeconds = GateSeconds;
				Window.WorstHitchTick = TickTimings;
				Window.HitchThresholdMs = GHitchMs;
			}
		}

		WorldState.Frame.Tick = FTimings();
	}

	bool ShouldEmit()
	{
		using namespace ArcTerrainDiagPrivate_5d71;

		FWorldState& WorldState = State();
		const double Now = FPlatformTime::Seconds();
		if (WorldState.LastEmitSeconds <= 0.0)
		{
			WorldState.LastEmitSeconds = Now;
			return false;
		}
		const double Interval =
			static_cast<double>(FMath::Max(GDiagIntervalSeconds, MinDiagIntervalSeconds));
		if (Now - WorldState.LastEmitSeconds < Interval)
		{
			return false;
		}
		WorldState.Frame.Window.WindowWallSeconds = Now - WorldState.LastEmitSeconds;
		WorldState.LastEmitSeconds = Now;
		return true;
	}

	void SetTickStats(const FTimings& TickTimings, const FGauges& Gauges, double DiagGatherSeconds)
	{
#if STATS
		using namespace ArcTerrainDiagPrivate_5d71;

		if (GStatFrameNumber != GFrameCounter)
		{
			GStatFrameNumber = GFrameCounter;
			GStatTimings = FTimings();
			GStatGauges = FGauges();
			GStatAtlasDelta = FAtlasCounters();
			GStatGatherSeconds = 0.0;
		}

		GStatTimings.TotalSeconds += TickTimings.TotalSeconds;
		GStatTimings.BlockingGameThreadSeconds += TickTimings.BlockingGameThreadSeconds;
		for (int32 Index = 0; Index < static_cast<int32>(EPhase::Count); ++Index)
		{
			GStatTimings.PhaseSeconds[Index] += TickTimings.PhaseSeconds[Index];
		}
		for (int32 Index = 0; Index < static_cast<int32>(ELeafTiming::Count); ++Index)
		{
			GStatTimings.LeafSeconds[Index] += TickTimings.LeafSeconds[Index];
		}
		GStatGatherSeconds += DiagGatherSeconds;
		AccumulateGauges(GStatGauges, Gauges);

		FWorldState& WorldState = State();
		const FAtlasCounters& Atlas = WorldState.Frame.Atlas;
		GStatAtlasDelta.HeightSlicesGranted +=
			Atlas.HeightSlicesGranted - WorldState.PrevTickAtlas.HeightSlicesGranted;
		GStatAtlasDelta.HeightSlicesEvicted +=
			Atlas.HeightSlicesEvicted - WorldState.PrevTickAtlas.HeightSlicesEvicted;
		GStatAtlasDelta.WeightSlicesGranted +=
			Atlas.WeightSlicesGranted - WorldState.PrevTickAtlas.WeightSlicesGranted;
		GStatAtlasDelta.WeightSlicesEvicted +=
			Atlas.WeightSlicesEvicted - WorldState.PrevTickAtlas.WeightSlicesEvicted;
		WorldState.PrevTickAtlas = Atlas;

		SET_CYCLE_COUNTER(STAT_ArcTerrainTickTotal,
			FPlatformTime::SecondsToCycles64(GStatTimings.TotalSeconds));
		SET_CYCLE_COUNTER(STAT_ArcTerrainEditPins, PhaseCycles(GStatTimings, EPhase::EditPins));
		SET_CYCLE_COUNTER(STAT_ArcTerrainResidency, PhaseCycles(GStatTimings, EPhase::Residency));
		SET_CYCLE_COUNTER(STAT_ArcTerrainTileStream, PhaseCycles(GStatTimings, EPhase::TileStream));
		SET_CYCLE_COUNTER(STAT_ArcTerrainLighting, PhaseCycles(GStatTimings, EPhase::Lighting));
		SET_CYCLE_COUNTER(STAT_ArcTerrainFarBlocks, PhaseCycles(GStatTimings, EPhase::FarBlocks));
		SET_CYCLE_COUNTER(STAT_ArcTerrainComposite, PhaseCycles(GStatTimings, EPhase::Composite));
		SET_CYCLE_COUNTER(STAT_ArcTerrainAtlas, PhaseCycles(GStatTimings, EPhase::Atlas));
		SET_CYCLE_COUNTER(STAT_ArcTerrainCollision, PhaseCycles(GStatTimings, EPhase::Collision));
		SET_CYCLE_COUNTER(STAT_ArcTerrainSyncAnchor, PhaseCycles(GStatTimings, EPhase::SyncAnchor));
		SET_CYCLE_COUNTER(STAT_ArcTerrainBake, PhaseCycles(GStatTimings, EPhase::Bake));
		SET_CYCLE_COUNTER(STAT_ArcTerrainBlockingGT,
			FPlatformTime::SecondsToCycles64(GStatTimings.BlockingGameThreadSeconds));
		SET_CYCLE_COUNTER(STAT_ArcTerrainDiagGather,
			FPlatformTime::SecondsToCycles64(GStatGatherSeconds));
		SET_CYCLE_COUNTER(STAT_ArcTerrainTileKeyGather,
			LeafCycles(GStatTimings, ELeafTiming::TileKeyGather));
		SET_CYCLE_COUNTER(STAT_ArcTerrainTileKeyRecipe,
			LeafCycles(GStatTimings, ELeafTiming::TileKeyRecipe));
		SET_CYCLE_COUNTER(STAT_ArcTerrainTileKeyWeight,
			LeafCycles(GStatTimings, ELeafTiming::TileKeyWeight));
		SET_CYCLE_COUNTER(STAT_ArcTerrainTileKeyData,
			LeafCycles(GStatTimings, ELeafTiming::TileKeyData));
		SET_CYCLE_COUNTER(STAT_ArcTerrainCollisionModifierSync,
			LeafCycles(GStatTimings, ELeafTiming::CollisionModifierSync));
		SET_CYCLE_COUNTER(STAT_ArcTerrainCollisionHeightOpGather,
			LeafCycles(GStatTimings, ELeafTiming::CollisionHeightOpGather));
		SET_CYCLE_COUNTER(STAT_ArcTerrainCollisionHeightfieldBuild,
			LeafCycles(GStatTimings, ELeafTiming::CollisionHeightfieldBuild));
		SET_CYCLE_COUNTER(STAT_ArcTerrainCollisionApply,
			LeafCycles(GStatTimings, ELeafTiming::CollisionApply));
		SET_CYCLE_COUNTER(STAT_ArcTerrainSetHeightfieldPhysics,
			LeafCycles(GStatTimings, ELeafTiming::SetHeightfieldPhysics));
		SET_CYCLE_COUNTER(STAT_ArcTerrainSetHeightfieldBounds,
			LeafCycles(GStatTimings, ELeafTiming::SetHeightfieldBounds));
		SET_CYCLE_COUNTER(STAT_ArcTerrainSetHeightfieldNavigation,
			LeafCycles(GStatTimings, ELeafTiming::SetHeightfieldNavigation));
		SET_CYCLE_COUNTER(STAT_ArcTerrainHeightFinalizeMinMax,
			LeafCycles(GStatTimings, ELeafTiming::HeightFinalizeMinMax));
		SET_CYCLE_COUNTER(STAT_ArcTerrainHeightFinalizeEncode,
			LeafCycles(GStatTimings, ELeafTiming::HeightFinalizeEncode));
		SET_CYCLE_COUNTER(STAT_ArcTerrainHeightFinalizeWrite,
			LeafCycles(GStatTimings, ELeafTiming::HeightFinalizeWrite));
		SET_CYCLE_COUNTER(STAT_ArcTerrainAtlasResidencyReconcile,
			LeafCycles(GStatTimings, ELeafTiming::AtlasResidencyReconcile));
		SET_CYCLE_COUNTER(STAT_ArcTerrainAtlasAdmission,
			LeafCycles(GStatTimings, ELeafTiming::AtlasAdmission));
		SET_CYCLE_COUNTER(STAT_ArcTerrainAtlasHeightSnapshot,
			LeafCycles(GStatTimings, ELeafTiming::AtlasHeightSnapshot));
		SET_CYCLE_COUNTER(STAT_ArcTerrainAtlasWeightSnapshot,
			LeafCycles(GStatTimings, ELeafTiming::AtlasWeightSnapshot));
		SET_CYCLE_COUNTER(STAT_ArcTerrainAtlasCbtTopology,
			LeafCycles(GStatTimings, ELeafTiming::AtlasCbtTopology));
		SET_CYCLE_COUNTER(STAT_ArcTerrainAtlasCbtPublish,
			LeafCycles(GStatTimings, ELeafTiming::AtlasCbtPublish));
		SET_CYCLE_COUNTER(STAT_ArcTerrainAtlasPreviewBind,
			LeafCycles(GStatTimings, ELeafTiming::AtlasPreviewBind));
		SET_CYCLE_COUNTER(STAT_ArcTerrainLightingWindowReconcile,
			LeafCycles(GStatTimings, ELeafTiming::LightingWindowReconcile));
		SET_CYCLE_COUNTER(STAT_ArcTerrainLightingPayloadSnapshot,
			LeafCycles(GStatTimings, ELeafTiming::LightingPayloadSnapshot));
		SET_CYCLE_COUNTER(STAT_ArcTerrainLightingCompletionApply,
			LeafCycles(GStatTimings, ELeafTiming::LightingCompletionApply));
		SET_CYCLE_COUNTER(STAT_ArcTerrainFarWindowReconcile,
			LeafCycles(GStatTimings, ELeafTiming::FarWindowReconcile));
		SET_CYCLE_COUNTER(STAT_ArcTerrainFarPayloadSnapshot,
			LeafCycles(GStatTimings, ELeafTiming::FarPayloadSnapshot));
		SET_CYCLE_COUNTER(STAT_ArcTerrainFarCompletionApply,
			LeafCycles(GStatTimings, ELeafTiming::FarCompletionApply));

		SET_DWORD_STAT(STAT_ArcTerrainCells, GStatGauges.Cells);
		SET_DWORD_STAT(STAT_ArcTerrainCollisionPatches, GStatGauges.CollisionPatches);

		SET_DWORD_STAT(STAT_ArcTerrainHeightLoaded,
			SumTiers(GStatGauges.TilesLoaded[RailIndex(EArcTerrainRail::Height)]));
		SET_DWORD_STAT(STAT_ArcTerrainHeightInFlight,
			SumTiers(GStatGauges.TilesInFlight[RailIndex(EArcTerrainRail::Height)]));
		SET_DWORD_STAT(STAT_ArcTerrainHeightUnusable,
			SumTiers(GStatGauges.TilesUnusable[RailIndex(EArcTerrainRail::Height)]));
		SET_DWORD_STAT(STAT_ArcTerrainHeightManifest,
			SumTiers(GStatGauges.ManifestEntries[RailIndex(EArcTerrainRail::Height)]));

		SET_DWORD_STAT(STAT_ArcTerrainWeightLoaded,
			SumTiers(GStatGauges.TilesLoaded[RailIndex(EArcTerrainRail::Weight)]));
		SET_DWORD_STAT(STAT_ArcTerrainWeightInFlight,
			SumTiers(GStatGauges.TilesInFlight[RailIndex(EArcTerrainRail::Weight)]));
		SET_DWORD_STAT(STAT_ArcTerrainWeightUnusable,
			SumTiers(GStatGauges.TilesUnusable[RailIndex(EArcTerrainRail::Weight)]));
		SET_DWORD_STAT(STAT_ArcTerrainWeightManifest,
			SumTiers(GStatGauges.ManifestEntries[RailIndex(EArcTerrainRail::Weight)]));

		SET_DWORD_STAT(STAT_ArcTerrainDataLoaded,
			SumTiers(GStatGauges.TilesLoaded[RailIndex(EArcTerrainRail::Data)]));
		SET_DWORD_STAT(STAT_ArcTerrainDataInFlight,
			SumTiers(GStatGauges.TilesInFlight[RailIndex(EArcTerrainRail::Data)]));
		SET_DWORD_STAT(STAT_ArcTerrainDataUnusable,
			SumTiers(GStatGauges.TilesUnusable[RailIndex(EArcTerrainRail::Data)]));
		SET_DWORD_STAT(STAT_ArcTerrainDataManifest,
			SumTiers(GStatGauges.ManifestEntries[RailIndex(EArcTerrainRail::Data)]));

		SET_DWORD_STAT(STAT_ArcTerrainHeightSlicesGranted, GStatAtlasDelta.HeightSlicesGranted);
		SET_DWORD_STAT(STAT_ArcTerrainHeightSlicesEvicted, GStatAtlasDelta.HeightSlicesEvicted);
		SET_DWORD_STAT(STAT_ArcTerrainWeightSlicesGranted, GStatAtlasDelta.WeightSlicesGranted);
		SET_DWORD_STAT(STAT_ArcTerrainWeightSlicesEvicted, GStatAtlasDelta.WeightSlicesEvicted);
#endif // STATS
	}

	void LogFrame(const FGauges& Gauges)
	{
		const FFrame& Counters = ArcTerrainDiagPrivate_5d71::State().Frame;

		UE_LOGF(LogArcTerrainDiag, Log,
			"[Cells] terrains=%d live=%d created=%d (bounds=%d pin=%d content=%d) removed=%d "
			"compositecandidates=%d",
			Gauges.Terrains, Gauges.Cells,
			Counters.Cells.Created[0] + Counters.Cells.Created[1] + Counters.Cells.Created[2],
			Counters.Cells.Created[static_cast<int32>(ECellRail::Bounds)],
			Counters.Cells.Created[static_cast<int32>(ECellRail::Pin)],
			Counters.Cells.Created[static_cast<int32>(ECellRail::Content)],
			Counters.Cells.Removed, Counters.Cells.CompositeCandidates);

		for (int32 Rail = 0; Rail < ArcTerrain::NumTileRails; ++Rail)
		{
			const FStreamCounters& Stream0 = Counters.Stream[Rail][0];
			UE_LOGF(LogArcTerrainDiag, Log,
				"[Stream] %-6ls loaded=%d inflight=%d requested=%d arrived=%d released=%d "
				"manifestmiss=%d unusable=%d(held %d) manifest=%d",
				RailName(static_cast<EArcTerrainRail>(Rail)),
				Gauges.TilesLoaded[Rail][0], Gauges.TilesInFlight[Rail][0], Stream0.Requests,
				Stream0.Arrivals, Stream0.Releases, Stream0.ManifestMisses, Stream0.Unusable,
				Gauges.TilesUnusable[Rail][0], Gauges.ManifestEntries[Rail][0]);

			for (int32 Tier = 1; Tier < ArcTerrain::MaxTiers; ++Tier)
			{
				const FStreamCounters& StreamTier = Counters.Stream[Rail][Tier];
				const bool bTierActive = StreamTier.Requests != 0 || StreamTier.Arrivals != 0
					|| StreamTier.Releases != 0 || StreamTier.ManifestMisses != 0 || StreamTier.Unusable != 0
					|| Gauges.TilesLoaded[Rail][Tier] != 0 || Gauges.TilesInFlight[Rail][Tier] != 0
					|| Gauges.TilesUnusable[Rail][Tier] != 0 || Gauges.ManifestEntries[Rail][Tier] != 0;
				if (!bTierActive)
				{
					continue;
				}
				UE_LOGF(LogArcTerrainDiag, Log,
					"[Stream] %-6ls tier%d loaded=%d inflight=%d requested=%d arrived=%d released=%d "
					"manifestmiss=%d unusable=%d(held %d) manifest=%d",
					RailName(static_cast<EArcTerrainRail>(Rail)), Tier,
					Gauges.TilesLoaded[Rail][Tier], Gauges.TilesInFlight[Rail][Tier], StreamTier.Requests,
					StreamTier.Arrivals, StreamTier.Releases, StreamTier.ManifestMisses, StreamTier.Unusable,
					Gauges.TilesUnusable[Rail][Tier], Gauges.ManifestEntries[Rail][Tier]);
			}
		}

		for (int32 Rail = 0; Rail < ArcTerrain::NumTileRails; ++Rail)
		{
			const FBakeCounters& Bake0 = Counters.Bake[Rail][0];
			UE_LOGF(LogArcTerrainDiag, Log,
				"[Bake] %-6ls queue=%d inflight=%d candidates=%d steps=%d fresh=%d deferred=%d "
				"incomplete=%d wrote=%d writefailed=%d",
				RailName(static_cast<EArcTerrainRail>(Rail)),
				Gauges.BakeQueue[Rail][0], Gauges.BakeInFlight[Rail], Bake0.Candidates, Bake0.Steps,
				Bake0.Fresh, Bake0.Deferred, Bake0.Incomplete, Bake0.Writes, Bake0.WriteFailures);

			for (int32 Tier = 1; Tier < ArcTerrain::MaxTiers; ++Tier)
			{
				const FBakeCounters& BakeTier = Counters.Bake[Rail][Tier];
				const bool bTierActive = BakeTier.Candidates != 0 || BakeTier.Steps != 0
					|| BakeTier.Fresh != 0 || BakeTier.Deferred != 0 || BakeTier.Incomplete != 0
					|| BakeTier.Writes != 0 || BakeTier.WriteFailures != 0
					|| Gauges.BakeQueue[Rail][Tier] != 0;
				if (!bTierActive)
				{
					continue;
				}
				UE_LOGF(LogArcTerrainDiag, Log,
					"[Bake] %-6ls tier%d queue=%d candidates=%d steps=%d fresh=%d deferred=%d "
					"incomplete=%d wrote=%d writefailed=%d",
					RailName(static_cast<EArcTerrainRail>(Rail)), Tier,
					Gauges.BakeQueue[Rail][Tier], BakeTier.Candidates, BakeTier.Steps, BakeTier.Fresh,
					BakeTier.Deferred, BakeTier.Incomplete, BakeTier.Writes, BakeTier.WriteFailures);
			}
		}

		const FString CoarseMismatchField = Counters.Atlas.CoarseResolutionMismatches != 0
			? FString::Printf(TEXT(" coarsemismatch=%d"), Counters.Atlas.CoarseResolutionMismatches)
			: FString();
		UE_LOGF(LogArcTerrainDiag, Log,
			"[Atlas] walked=%d fromtile=%d noheight=%d refills=%d "
			"miss(notile=%d withheld=%d cutfail=%d%ls) heightslices=+%d/-%d weightslices=+%d/-%d | "
			"ready=%d deferred=%d inflight=%d completion=%d stale=%d admit=%d refused=%d "
			"topology=%d published=%d previewrebind=%d",
			Counters.Atlas.CellsWalked, Counters.Atlas.FillsFromTile,
			Counters.Atlas.FillsWithoutHeight, Counters.Atlas.Refills,
			Counters.Atlas.CellsNoTile, Counters.Atlas.CellsTileWithheld, Counters.Atlas.CellsCutFailed,
			*CoarseMismatchField,
			Counters.Atlas.HeightSlicesGranted, Counters.Atlas.HeightSlicesEvicted,
			Counters.Atlas.WeightSlicesGranted, Counters.Atlas.WeightSlicesEvicted,
			Counters.Atlas.Ready, Counters.Atlas.Deferred, Counters.Atlas.InFlight,
			Counters.Atlas.Completion, Counters.Atlas.Stale, Counters.Atlas.Admission,
			Counters.Atlas.Refusal, Counters.Atlas.TopologyBuild, Counters.Atlas.Publication,
			Counters.Atlas.PreviewRebind);

		UE_LOGF(LogArcTerrainDiag, Log,"[Lighting] ready=%d deferred=%d inflight=%d completion=%d stale=%d",
			Counters.Lighting.Ready, Counters.Lighting.Deferred, Counters.Lighting.InFlight,
			Counters.Lighting.Completion, Counters.Lighting.Stale);

		UE_LOGF(LogArcTerrainDiag, Log,"[Far] ready=%d deferred=%d inflight=%d completion=%d stale=%d",
			Counters.Far.Ready, Counters.Far.Deferred, Counters.Far.InFlight,
			Counters.Far.Completion, Counters.Far.Stale);

		UE_LOGF(LogArcTerrainDiag, Log,"[Collision] patches=%d applied=%d builds=%d nosource=%d",
			Gauges.CollisionPatches, Counters.Collision.Applied, Counters.Collision.Builds,
			Gauges.PatchesMissingHeightSource);

		UE_LOGF(LogArcTerrainDiag, Log,"%ls",
			*ArcTerrainDiagPrivate_5d71::FormatWindowTimings(Counters.Window,
				ArcTerrainDiagPrivate_5d71::State().Label));
	}

	void LogTerrainPipeline(const FArcTerrainPipelineSnapshot& Snapshot)
	{
		const FArcTerrainPipelineCounters& Interval = Snapshot.Interval;
		const FArcTerrainPipelineCounters& Lifetime = Snapshot.Lifetime;
		const FArcTerrainTileStoreMutationCounters& StoreInterval = Snapshot.Store.Interval;
		const FArcTerrainTileStoreMutationCounters& StoreLifetime = Snapshot.Store.Lifetime;
		FString HeightTiers;
		FString WeightTiers;
		FString StoreHeld;
		for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
		{
			if (Tier == 0)
			{
				HeightTiers = FString::Printf(TEXT("%d:%d"), Tier, Snapshot.ResidentHeightByTier[Tier]);
				WeightTiers = FString::Printf(TEXT("%d:%d"), Tier, Snapshot.ResidentWeightByTier[Tier]);
			}
			else
			{
				HeightTiers += FString::Printf(TEXT(",%d:%d"), Tier, Snapshot.ResidentHeightByTier[Tier]);
				WeightTiers += FString::Printf(TEXT(",%d:%d"), Tier, Snapshot.ResidentWeightByTier[Tier]);
			}
		}
		for (int32 Rail = 0; Rail < ArcTerrain::NumTileRails; ++Rail)
		{
			for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
			{
				if (StoreHeld.IsEmpty())
				{
					StoreHeld = FString::Printf(TEXT("%s%d:%d/%d/%d"),
						RailName(static_cast<EArcTerrainRail>(Rail)), Tier,
						Snapshot.Store.Loaded[Rail][Tier], Snapshot.Store.InFlight[Rail][Tier],
						Snapshot.Store.Unusable[Rail][Tier]);
				}
				else
				{
					StoreHeld += FString::Printf(TEXT(",%s%d:%d/%d/%d"),
						RailName(static_cast<EArcTerrainRail>(Rail)), Tier,
						Snapshot.Store.Loaded[Rail][Tier], Snapshot.Store.InFlight[Rail][Tier],
						Snapshot.Store.Unusable[Rail][Tier]);
				}
			}
		}

		FString Line = FString::Printf(
			TEXT("[TerrainPipeline] world=\"%s\" terrain=\"%s\" ageTicks=%" UINT64_FMT
				" storeEpoch=local:%" UINT64_FMT "/process:%u"),
			*Snapshot.WorldLabel, *Snapshot.TerrainPath, Snapshot.StateAgeTicks,
			Snapshot.Store.LoadedMutationSerial, Snapshot.Store.ProcessLoadedCoverageEpoch);
		Line += FString::Printf(
			TEXT(" storeI=hit:%" UINT64_FMT ",attempt:%" UINT64_FMT ",publish:%" UINT64_FMT
				",fail:%" UINT64_FMT ",asyncPublish:%" UINT64_FMT ",asyncFail:%" UINT64_FMT
				",release:%" UINT64_FMT),
			StoreInterval.SyncCacheHits, StoreInterval.SyncPhysicalAttempts, StoreInterval.SyncPublications,
			StoreInterval.SyncFailures, StoreInterval.AsyncPublications, StoreInterval.AsyncFailures,
			StoreInterval.Releases);
		Line += FString::Printf(
			TEXT(" storeL=hit:%" UINT64_FMT ",attempt:%" UINT64_FMT ",publish:%" UINT64_FMT
				",fail:%" UINT64_FMT ",asyncPublish:%" UINT64_FMT ",asyncFail:%" UINT64_FMT
				",release:%" UINT64_FMT),
			StoreLifetime.SyncCacheHits, StoreLifetime.SyncPhysicalAttempts, StoreLifetime.SyncPublications,
			StoreLifetime.SyncFailures, StoreLifetime.AsyncPublications, StoreLifetime.AsyncFailures,
			StoreLifetime.Releases);
		Line += FString::Printf(TEXT(" storeHeld[railTier:loaded/inflight/unusable]=[%s]"), *StoreHeld);
		Line += FString::Printf(
			TEXT(" streamI=skip:%" UINT64_FMT ",request:%" UINT64_FMT ",callback:%" UINT64_FMT
				",arrival:%" UINT64_FMT " streamL=skip:%" UINT64_FMT ",request:%" UINT64_FMT
				",callback:%" UINT64_FMT ",arrival:%" UINT64_FMT),
			Interval.StreamingSkippedAlreadyLoaded, Interval.StreamingAsyncRequests,
			Interval.StreamingValidCallbacks, Interval.StreamingArrivalEntries,
			Lifetime.StreamingSkippedAlreadyLoaded, Lifetime.StreamingAsyncRequests,
			Lifetime.StreamingValidCallbacks, Lifetime.StreamingArrivalEntries);
		Line += FString::Printf(
			TEXT(" authorityI=produced[release,arrive,pending,reset,bounds,layout]=[%" UINT64_FMT
				",%" UINT64_FMT ",%" UINT64_FMT ",%" UINT64_FMT ",%" UINT64_FMT ",%" UINT64_FMT
				"]consumed=[%" UINT64_FMT ",%" UINT64_FMT ",%" UINT64_FMT
				",%" UINT64_FMT ",%" UINT64_FMT ",%" UINT64_FMT "]"),
			Interval.AuthorityProduced[0], Interval.AuthorityProduced[1], Interval.AuthorityProduced[2],
			Interval.AuthorityProduced[3], Interval.AuthorityProduced[4], Interval.AuthorityProduced[5],
			Interval.AtlasAuthorityConsumed[0], Interval.AtlasAuthorityConsumed[1],
			Interval.AtlasAuthorityConsumed[2], Interval.AtlasAuthorityConsumed[3],
			Interval.AtlasAuthorityConsumed[4], Interval.AtlasAuthorityConsumed[5]);
		Line += FString::Printf(
			TEXT(" authorityL=produced[release,arrive,pending,reset,bounds,layout]=[%" UINT64_FMT
				",%" UINT64_FMT ",%" UINT64_FMT ",%" UINT64_FMT ",%" UINT64_FMT ",%" UINT64_FMT
				"]consumed=[%" UINT64_FMT ",%" UINT64_FMT ",%" UINT64_FMT
				",%" UINT64_FMT ",%" UINT64_FMT ",%" UINT64_FMT "]"),
			Lifetime.AuthorityProduced[0], Lifetime.AuthorityProduced[1], Lifetime.AuthorityProduced[2],
			Lifetime.AuthorityProduced[3], Lifetime.AuthorityProduced[4], Lifetime.AuthorityProduced[5],
			Lifetime.AtlasAuthorityConsumed[0], Lifetime.AtlasAuthorityConsumed[1],
			Lifetime.AtlasAuthorityConsumed[2], Lifetime.AtlasAuthorityConsumed[3],
			Lifetime.AtlasAuthorityConsumed[4], Lifetime.AtlasAuthorityConsumed[5]);
		Line += FString::Printf(
			TEXT(" residencyI=processEpoch:%" UINT64_FMT ",localEpoch:%" UINT64_FMT ",changed:%" UINT64_FMT
				",unchanged:%" UINT64_FMT " residencyL=processEpoch:%" UINT64_FMT ",localEpoch:%" UINT64_FMT
				",changed:%" UINT64_FMT ",unchanged:%" UINT64_FMT),
			Interval.ResidencyProcessEpochObservations, Interval.ResidencyLocalEpochObservations,
			Interval.ResidencyReconcileChanged, Interval.ResidencyReconcileUnchanged,
			Lifetime.ResidencyProcessEpochObservations, Lifetime.ResidencyLocalEpochObservations,
			Lifetime.ResidencyReconcileChanged, Lifetime.ResidencyReconcileUnchanged);
		Line += FString::Printf(
			TEXT(" atlasEventsI=effect[h:%" UINT64_FMT ",w:%" UINT64_FMT ",wake:%" UINT64_FMT
				"]ignored[filtered:%" UINT64_FMT ",grid:%" UINT64_FMT ",tier:%" UINT64_FMT
				",target:%" UINT64_FMT ",satisfied:%" UINT64_FMT "] atlasEventsL=effect[h:%" UINT64_FMT ",w:%" UINT64_FMT
				",wake:%" UINT64_FMT "]ignored[filtered:%" UINT64_FMT ",grid:%" UINT64_FMT
				",tier:%" UINT64_FMT ",target:%" UINT64_FMT ",satisfied:%" UINT64_FMT "]"),
			Interval.AtlasEffects[0], Interval.AtlasEffects[1], Interval.AtlasEffects[2],
			Interval.AtlasIgnored[0], Interval.AtlasIgnored[1], Interval.AtlasIgnored[2], Interval.AtlasIgnored[3],
			Interval.AtlasIgnored[4],
			Lifetime.AtlasEffects[0], Lifetime.AtlasEffects[1], Lifetime.AtlasEffects[2],
			Lifetime.AtlasIgnored[0], Lifetime.AtlasIgnored[1], Lifetime.AtlasIgnored[2], Lifetime.AtlasIgnored[3],
			Lifetime.AtlasIgnored[4]);
		Line += FString::Printf(
			TEXT(" resident=height[%s],weight[%s] atlasWork=revLag[r:%d,d:%d,l:%d,k:%d]authority:%d"
				",wakeH:%d,wakeW:%d,dirtyH:%d,dirtyW:%d,dirtyCells:%d,refill:%d,inflight:%d,pendingFill:%d"
				",cbtInFlight:%d,cbtUnpublished:%d"),
			*HeightTiers, *WeightTiers, Snapshot.bResidencyRevisionLag ? 1 : 0,
			Snapshot.bDesiredAdmissionRevisionLag ? 1 : 0, Snapshot.bLayoutGenerationLag ? 1 : 0,
			Snapshot.bRenderKnobRevisionLag ? 1 : 0, Snapshot.PendingAtlasAuthorityEvents,
			Snapshot.WokenHeightAdmissions, Snapshot.WokenWeightAdmissions, Snapshot.DirtyHeightSectors,
			Snapshot.DirtyWeightGroups, Snapshot.DirtyAtlasCells, Snapshot.PendingRefillCells,
			Snapshot.AtlasInFlight, Snapshot.PendingAtlasFills, Snapshot.bCbtTopologyInFlight ? 1 : 0,
			Snapshot.bCbtUnpublished ? 1 : 0);
		Line += FString::Printf(
			TEXT(" heightAdmission=ready:%d,missing:%d admitted=ready:%d,awaiting:%d staleSleeping:%d"
				" cellTags=compositePublished:%d,compositeLag:%d,tilePublished:%d,tileLag:%d,refill:%d,retry:%d"
				" cbt=published:%d,resourceMatch:%d"),
			Snapshot.PendingHeightSourceReady, Snapshot.PendingHeightSourceMissing,
			Snapshot.AdmittedHeightReady, Snapshot.AdmittedHeightAwaiting, Snapshot.StaleSleeping,
			Snapshot.CellsCompositePublished, Snapshot.CellsCompositeLagging, Snapshot.CellsTilePublished,
			Snapshot.CellsTileLagging, Snapshot.CellsRefillPending, Snapshot.CellsCompositeRetry,
			Snapshot.bCbtPublished ? 1 : 0, Snapshot.bCbtResourceGenerationMatch ? 1 : 0);
		Line += FString::Printf(
			TEXT(" collisionI=sync:%" UINT64_FMT ",queued:%" UINT64_FMT ",dequeued:%" UINT64_FMT
				",enqueue[ok,duplicate,latch]=[%" UINT64_FMT ",%" UINT64_FMT ",%" UINT64_FMT
				"]sat[count:%" UINT64_FMT
				",deadline:%" UINT64_FMT "] collisionL=sync:%" UINT64_FMT ",queued:%" UINT64_FMT
				",dequeued:%" UINT64_FMT ",enqueue[ok,duplicate,latch]=[%" UINT64_FMT
				",%" UINT64_FMT ",%" UINT64_FMT
				"]sat[count:%" UINT64_FMT ",deadline:%" UINT64_FMT "]"),
			Interval.CollisionSyncAttempts, Interval.CollisionQueuedAttempts, Interval.CollisionDequeued,
			Interval.CollisionEnqueue[0], Interval.CollisionEnqueue[1], Interval.CollisionEnqueue[2],
			Interval.CollisionCountSaturated, Interval.CollisionDeadlineSaturated,
			Lifetime.CollisionSyncAttempts, Lifetime.CollisionQueuedAttempts, Lifetime.CollisionDequeued,
			Lifetime.CollisionEnqueue[0], Lifetime.CollisionEnqueue[1], Lifetime.CollisionEnqueue[2],
			Lifetime.CollisionCountSaturated, Lifetime.CollisionDeadlineSaturated);
		Line += FString::Printf(
			TEXT(" collision=patches:%d,readySet:%d,readyOrdered:%d,retry:%d,authority:%d,components:%d"
				",latches:%d,eligibleUnqueued:%d,budget:%d/%.3fms"),
			Snapshot.CollisionPatches, Snapshot.CollisionReadySet, Snapshot.CollisionReadyOrdered,
			Snapshot.CollisionRetry, Snapshot.CollisionAuthorityEvents, Snapshot.CollisionComponents,
			Snapshot.CollisionHeightSourceLatches, Snapshot.CollisionEligibleButUnqueued,
			Snapshot.CollisionBudgetCount, Snapshot.CollisionBudgetMs);
		UE_LOGF(LogArcTerrainDiag, Log,"%ls", *Line);
	}

	void LogBakeProgress(const FGauges& Gauges)
	{
		using namespace ArcTerrainDiagPrivate_5d71;

		FWorldState& WorldState = State();
		if (GBakeProgress == 0)
		{
			WorldState.bBakeProgressActive = false;
			return;
		}

		int32 Remaining = 0;
		int32 Ready = 0;
		int32 Deferred = 0;
		int32 Probing = 0;
		int32 Evaluating = 0;
		int32 Writing = 0;
		GatherBakeProgress(Gauges, Remaining, Ready, Deferred, Probing, Evaluating, Writing);

		const double Now = FPlatformTime::Seconds();
		if (Remaining <= 0)
		{
			if (WorldState.bBakeProgressActive)
			{
				UE_LOGF(LogArcTerrainDiag, Log,"[BakeProgress] complete: %llu tiles in %.1f s.",
					WorldState.BakeProgressCompleted - WorldState.BakeProgressStartCompleted,
					Now - WorldState.BakeProgressStartSeconds);
				WorldState.bBakeProgressActive = false;
			}
			return;
		}

		if (!WorldState.bBakeProgressActive)
		{
			WorldState.bBakeProgressActive = true;
			WorldState.BakeProgressStartCompleted = WorldState.BakeProgressCompleted;
			WorldState.BakeProgressLastCompleted = WorldState.BakeProgressCompleted;
			WorldState.BakeProgressStartSeconds = Now;
			WorldState.BakeProgressLastSeconds = Now;
			UE_LOGF(LogArcTerrainDiag, Log,"[BakeProgress] started: remaining=%d queued=%d inflight=%d (probe=%d eval=%d write=%d) deferred=%d.",
				Remaining, Ready, Probing + Evaluating + Writing, Probing, Evaluating, Writing, Deferred);
			return;
		}

		const double Interval = static_cast<double>(FMath::Max(GBakeProgressIntervalSeconds,
			MinDiagIntervalSeconds));
		if (Now - WorldState.BakeProgressLastSeconds < Interval)
		{
			return;
		}

		const double Elapsed = Now - WorldState.BakeProgressLastSeconds;
		const uint64 Completed = WorldState.BakeProgressCompleted - WorldState.BakeProgressLastCompleted;
		const double TilesPerSecond = Elapsed > 0.0 ? static_cast<double>(Completed) / Elapsed : 0.0;
		const FString Eta = TilesPerSecond > KINDA_SMALL_NUMBER
			? FString::Printf(TEXT("%.1f s"), static_cast<double>(Remaining) / TilesPerSecond)
			: FString(TEXT("?"));
		UE_LOGF(LogArcTerrainDiag, Log,
			"[BakeProgress] remaining=%d queued=%d inflight=%d (probe=%d eval=%d write=%d) deferred=%d | "
			"%.2f tiles/s eta=%ls.",
			Remaining, Ready, Probing + Evaluating + Writing, Probing, Evaluating, Writing, Deferred,
			TilesPerSecond, *Eta);
		WorldState.BakeProgressLastCompleted = WorldState.BakeProgressCompleted;
		WorldState.BakeProgressLastSeconds = Now;
	}

	void CancelBakeProgress(const UWorld* World)
	{
		using namespace ArcTerrainDiagPrivate_5d71;

		if (World == nullptr)
		{
			return;
		}
		if (TUniquePtr<FWorldState>* Slot = GWorldStates.Find(FObjectKey(World)))
		{
			(*Slot)->bBakeProgressActive = false;
		}
	}

	void NoteBlockingGameThreadImpl(const TCHAR* What, FIntPoint Key, double Seconds)
	{
		FTimings& Tick = ArcTerrainDiagPrivate_5d71::State().Frame.Tick;
		Tick.BlockingGameThreadSeconds += Seconds;
		ArcTerrainDiagPrivate_5d71::RecordWorst(Tick, What, Key, Seconds);
	}

	void NoteBakeCompletedImpl()
	{
		++ArcTerrainDiagPrivate_5d71::State().BakeProgressCompleted;
	}

	void NoteBakeCoalesceImpl(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey, uint32 Invalidations, int32 Contributors, uint32 Serial)
	{
		const TCHAR* CoalesceRailName = TEXT("Height");
		switch (Rail)
		{
		case EArcTerrainRail::Weight:
			CoalesceRailName = TEXT("Weight");
			break;
		case EArcTerrainRail::Data:
			CoalesceRailName = TEXT("Data");
			break;
		default:
			break;
		}
		UE_LOGF(LogArcTerrainDiag, Log,"[BakeCoalesce] terrain=%ls rail=%ls tier=%d tile=(%d,%d) invalidations=%u contributors=%d serial=%u",
			*Terrain.GetName(), CoalesceRailName, Tier, TileKey.X, TileKey.Y,
			Invalidations, Contributors, Serial);
	}

	void LogHitch()
	{
		using namespace ArcTerrainDiagPrivate_5d71;

		const FWorldState& WorldState = State();
		const FWindow& Window = WorldState.Frame.Window;
		if (Window.HitchTicks <= 0)
		{
			return;
		}
		UE_LOGF(LogArcTerrainDiag, Warning,"[Hitch] world '%ls' %d of %d ticks over %.1f ms | worst tick: %ls",
			*WorldState.Label, Window.HitchTicks, Window.Ticks, Window.HitchThresholdMs,
			*FormatTimings(Window.WorstHitchTick));
	}

	void LogTerrainGrid(const AArcTerrain& Terrain, const FArcTerrainState& State)
	{
		using namespace ArcTerrainDiagPrivate_5d71;

		const FArcTerrainConfig& Config = Terrain.Config;
		UE_LOGF(LogArcTerrainDiag, Log,"[Grid] '%ls': texel=%d cm cell=%d m (%d quads) collisionpatch=%d m (%d quads)",
			*Terrain.GetName(), Config.TexelSizeCm(), static_cast<int32>(static_cast<uint32>(Config.CellExtent)),
			Config.QuadsPerCell(), static_cast<int32>(static_cast<uint32>(Terrain.CollisionPatchExtent)),
			Terrain.CollisionPatchQuads);

		// Same resolver the runtime residency/atlas phases use: Terrain.MaxResidentSectors is only tier 0's
		// share once a ladder is authored, so a local re-derivation reports a layout the terrain never had.
		const ArcTerrainSubsystemPrivate::FResolvedResidency Residency =
			ArcTerrainSubsystemPrivate::ResolveResidency(Terrain);
		const FArcTerrainTileAtlasLayout Layout =
			ArcTerrainSubsystemPrivate::ResolveTerrainAtlasLayout(Terrain, State, Residency);
		if (!Layout.IsValid())
		{
			UE_LOGF(LogArcTerrainDiag, Warning,
				"[Grid] '%ls' atlas: REFUSED — the resolved layout allocates nothing, so the terrain "
				"draws nothing (MaxResidentSectors %d, MaxResidentWeightCells %d).",
				*Terrain.GetName(), Residency.MaxResidentSectors, Residency.MaxResidentWeightCells);
		}
		else
		{
			const int64 HeightBytes = ArcTerrainAtlas::HeightSliceBytes(Layout.GetHeightSliceSide())
				* Layout.GetHeightArraySize();
			const int64 WeightBytes =
				ArcTerrainAtlas::WeightSliceBytes(Layout.GetWeightSliceSide(), Layout.NumWeightArrays)
				* Layout.GetWeightArraySize();
			UE_LOGF(LogArcTerrainDiag, Log,
				"[Grid] '%ls' atlas: height %d² x%d = %.1f MB%ls | "
				"weight %d² x%d x%d arrays = %.1f MB%ls",
				*Terrain.GetName(), Layout.GetHeightSliceSide(), Layout.GetHeightArraySize(),
				Megabytes(HeightBytes),
				*TrimNote(TEXT("MaxResidentSectors"), Residency.MaxResidentSectors, Layout.MaxResidentSectors),
				Layout.GetWeightSliceSide(), Layout.GetWeightArraySize(), Layout.NumWeightArrays,
				Megabytes(WeightBytes),
				*TrimNote(TEXT("MaxResidentWeightCells"), Residency.MaxResidentWeightCells,
					Layout.MaxResidentWeightCells));
		}

		// The manifest is only counted where something else already paid for it: at level 0 this line still
		// prints, and forcing the load would put a synchronous disk read behind a disabled cvar.
		FArcTerrainTileStoreStats Stats;
		ArcTerrainTileAccess::GatherStoreStats(Terrain, IsOn(), Stats);

		for (int32 RailIdx = 0; RailIdx < ArcTerrain::NumTileRails; ++RailIdx)
		{
			const EArcTerrainRail Rail = static_cast<EArcTerrainRail>(RailIdx);
			const FString ManifestText = Stats.bManifestLoaded
				? FString::FromInt(SumTiers(Stats.ManifestEntries[RailIdx]))
				: FString(TEXT("?"));
			FArcTerrainTileGrid Grid;
			if (!ArcTerrainTileAccess::ResolveGrid(Terrain, Rail, 0, Grid))
			{
				UE_LOGF(LogArcTerrainDiag, Warning,"[Grid] '%ls' %ls: NO LATTICE — this rail writes and reads nothing (manifest=%ls).",
					*Terrain.GetName(), RailName(Rail), *ManifestText);
				continue;
			}
			UE_LOGF(LogArcTerrainDiag, Log,"[Grid] '%ls' %-6ls tile=%d m quads=%d resolution=%d cellspertile=%d bytes=%" INT64_FMT
					" %ls manifest=%ls",
				*Terrain.GetName(), RailName(Rail), Grid.TileExtentM, Grid.TileQuads, Grid.Resolution(),
				Grid.CellsPerTileEdge, TileBodyBytes(Rail, Grid),
				Rail == EArcTerrainRail::Height ? TEXT("per tile") : TEXT("per plane"),
				*ManifestText);
		}

		const FString WriteDir = ArcTerrainTileNames::MakeTileDirForWrite(Terrain);
		const FString ReadDir = ArcTerrainTileNames::MakeTileDirForRead(Terrain);
		UE_LOGF(LogArcTerrainDiag, Log,"[Grid] '%ls' write dir: '%ls'", *Terrain.GetName(),
			WriteDir.IsEmpty() ? TEXT("<refused: unsaved map, /Temp/ or PIE>") : *WriteDir);
		if (ReadDir != WriteDir)
		{
			UE_LOGF(LogArcTerrainDiag, Log,"[Grid] '%ls' read dir:  '%ls'", *Terrain.GetName(),
				ReadDir.IsEmpty() ? TEXT("<none>") : *ReadDir);
		}
	}

	void NoteStreamRequestImpl(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey)
	{
		++Frame().Stream[RailIndex(Rail)][TierIndex(Tier)].Requests;
		if (IsVerbose())
		{
			if (Tier == 0)
			{
				UE_LOGF(LogArcTerrainDiag, Log,"[Stream] %ls tile (%d,%d) requested.", RailName(Rail),
					TileKey.X, TileKey.Y);
			}
			else
			{
				UE_LOGF(LogArcTerrainDiag, Log,"[Stream] %ls tier%d tile (%d,%d) requested.",
					RailName(Rail), Tier, TileKey.X, TileKey.Y);
			}
		}
	}

	void NoteCoarseResolutionMismatchImpl(FIntPoint SectorKey, int32 Tier, int32 PayloadResolution,
		int32 SliceSide)
	{
		FAtlasCounters& Atlas = Frame().Atlas;
		if (Atlas.CoarseResolutionMismatchLogs >= 4)
		{
			return;
		}
		++Atlas.CoarseResolutionMismatchLogs;
		UE_LOGF(LogArcTerrainDiag, Warning,"[CoarseMismatch] sector (%d,%d) tier%d | payload resolution=%d | slice side=%d.",
			SectorKey.X, SectorKey.Y, Tier, PayloadResolution, SliceSide);
	}

	void NoteAtlasCutFailureImpl(FIntPoint CellKey, FIntPoint TileKey, int32 PayloadRail,
		int32 PayloadResolution, int32 PayloadFloats, const FArcTerrainTileGrid& Grid,
		int32 ExpectedResolution)
	{
		FAtlasCounters& Atlas = Frame().Atlas;
		// A refusal that hits twelve cells of every admitted sector would bury the log in one second.
		if (Atlas.CutFailureLogs >= 4)
		{
			return;
		}
		++Atlas.CutFailureLogs;
		UE_LOGF(LogArcTerrainDiag, Warning,
			"[CutFail] cell (%d,%d) tile (%d,%d) | payload rail=%d res=%d floats=%d | grid res=%d "
			"tilequads=%d cellspertile=%d cellquads=%d | expected=%d",
			CellKey.X, CellKey.Y, TileKey.X, TileKey.Y, PayloadRail, PayloadResolution, PayloadFloats,
			Grid.Resolution(), Grid.TileQuads, Grid.CellsPerTileEdge,
			Grid.CellsPerTileEdge > 0 ? Grid.TileQuads / Grid.CellsPerTileEdge : 0, ExpectedResolution);
	}

	void NoteTileArrivedImpl(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey, uint32 PayloadId,
		int64 Bytes)
	{
		++Frame().Stream[RailIndex(Rail)][TierIndex(Tier)].Arrivals;
		if (IsVerbose())
		{
			if (Tier == 0)
			{
				UE_LOGF(LogArcTerrainDiag, Log,"[Stream] %ls tile (%d,%d) arrived: payload=%u bytes=%" INT64_FMT ".",
					RailName(Rail), TileKey.X, TileKey.Y, PayloadId, Bytes);
			}
			else
			{
				UE_LOGF(LogArcTerrainDiag, Log,"[Stream] %ls tier%d tile (%d,%d) arrived: payload=%u bytes=%" INT64_FMT ".",
					RailName(Rail), Tier, TileKey.X, TileKey.Y, PayloadId, Bytes);
			}
		}
	}

	void NoteTileReleasedImpl(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey)
	{
		++Frame().Stream[RailIndex(Rail)][TierIndex(Tier)].Releases;
		if (IsVerbose())
		{
			if (Tier == 0)
			{
				UE_LOGF(LogArcTerrainDiag, Log,"[Stream] %ls tile (%d,%d) released.", RailName(Rail),
					TileKey.X, TileKey.Y);
			}
			else
			{
				UE_LOGF(LogArcTerrainDiag, Log,"[Stream] %ls tier%d tile (%d,%d) released.",
					RailName(Rail), Tier, TileKey.X, TileKey.Y);
			}
		}
	}

	void NoteTileRejectedImpl(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey, EArcTerrainTileLoad Result)
	{
		using namespace ArcTerrainDiagPrivate_5d71;

		if (Result == EArcTerrainTileLoad::FileMissing)
		{
			++Frame().Stream[RailIndex(Rail)][TierIndex(Tier)].ManifestMisses;
		}
		else if (Result == EArcTerrainTileLoad::Unusable)
		{
			++Frame().Stream[RailIndex(Rail)][TierIndex(Tier)].Unusable;
		}
		if (IsVerbose())
		{
			if (Tier == 0)
			{
				UE_LOGF(LogArcTerrainDiag, Log,"[Stream] %ls tile (%d,%d) rejected: %ls.", RailName(Rail),
					TileKey.X, TileKey.Y, LoadResultName(Result));
			}
			else
			{
				UE_LOGF(LogArcTerrainDiag, Log,"[Stream] %ls tier%d tile (%d,%d) rejected: %ls.",
					RailName(Rail), Tier, TileKey.X, TileKey.Y, LoadResultName(Result));
			}
		}
	}

	void NoteTileWrittenImpl(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey, int64 Bytes, double Seconds,
		bool bWritten)
	{
		FBakeCounters& Bake = Frame().Bake[RailIndex(Rail)][TierIndex(Tier)];
		++(bWritten ? Bake.Writes : Bake.WriteFailures);
		if (IsVerbose())
		{
			if (Tier == 0)
			{
				UE_LOGF(LogArcTerrainDiag, Log,"[Bake] %ls tile (%d,%d) %ls: bytes=%" INT64_FMT " in %.1f ms.", RailName(Rail),
					TileKey.X, TileKey.Y, bWritten ? TEXT("written") : TEXT("WRITE FAILED"), Bytes,
					Seconds * 1000.0);
			}
			else
			{
				UE_LOGF(LogArcTerrainDiag, Log,"[Bake] %ls tier%d tile (%d,%d) %ls: bytes=%" INT64_FMT " in %.1f ms.", RailName(Rail),
					Tier, TileKey.X, TileKey.Y, bWritten ? TEXT("written") : TEXT("WRITE FAILED"), Bytes,
					Seconds * 1000.0);
			}
		}
	}

	void NoteCellCreatedImpl(ECellRail Producer, FIntPoint CellKey)
	{
		++Frame().Cells.Created[FMath::Clamp(static_cast<int32>(Producer), 0,
			static_cast<int32>(ECellRail::Count) - 1)];
		if (IsVerbose())
		{
			UE_LOGF(LogArcTerrainDiag, Log,"[Cells] cell (%d,%d) created by %ls.", CellKey.X, CellKey.Y,
				ArcTerrainDiagPrivate_5d71::CellRailName(Producer));
		}
	}

	void NoteCellRemovedImpl(FIntPoint CellKey)
	{
		++Frame().Cells.Removed;
		if (IsVerbose())
		{
			UE_LOGF(LogArcTerrainDiag, Log,"[Cells] cell (%d,%d) removed.", CellKey.X, CellKey.Y);
		}
	}

	void NoteCollisionAppliedImpl(FIntPoint PatchKey)
	{
		++Frame().Collision.Applied;
		if (IsVerbose())
		{
			UE_LOGF(LogArcTerrainDiag, Log,"[Collision] patch (%d,%d) heightfield applied.", PatchKey.X,
				PatchKey.Y);
		}
	}

	void NoteCollisionBuildImpl(FIntPoint PatchKey, bool bFromBakedTiles)
	{
		++Frame().Collision.Builds;
		if (IsVerbose())
		{
			UE_LOGF(LogArcTerrainDiag, Log,"[Collision] patch (%d,%d) heightfield built from %ls.",
				PatchKey.X, PatchKey.Y,
				bFromBakedTiles ? TEXT("baked tiles") : TEXT("the stamp evaluation"));
		}
	}
}
