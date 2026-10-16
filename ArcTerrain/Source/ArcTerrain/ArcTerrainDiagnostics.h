// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTerrainTileFormat.h"
#include "ArcTerrainTileIO.h"
#include "Render/ArcTerrainDensityTiers.h"

#include "Stats/Stats.h"

class AArcTerrain;
class UWorld;
struct FArcTerrainState;
struct FArcTerrainPipelineSnapshot;

ARCTERRAIN_API DECLARE_LOG_CATEGORY_EXTERN(LogArcTerrainDiag, Log, All);

DECLARE_STATS_GROUP(TEXT("ArcTerrain"), STATGROUP_ArcTerrain, STATCAT_Advanced);

/**
 * One knob — arc.Terrain.Diag — over every terrain counter: 0 off, 1 summaries once per
 * arc.Terrain.DiagIntervalSeconds, 2 additionally one line per event (tile arrival, read rejection, tile
 * write, cell create/destroy, collision build).
 *
 * AT LEVEL 0 NO COUNTER IS FORMATTED: every Note entry point below is an inline test of one int, and the
 * out-of-line implementation — which is where any FString::Printf lives — is not reached. LogTerrainGrid
 * is the one exception and says so at its own declaration.
 *
 * GAME THREAD. The counters are a plain struct, deliberately not atomic: every producer is a game-thread
 * path (the stream publishes and releases there, the bakes and the atlas run in Tick).
 */
namespace ArcTerrainDiag
{
	ARCTERRAIN_API extern int32 GLevel;

	inline bool IsOn() { return GLevel > 0; }

	inline bool IsVerbose() { return GLevel > 1; }

	inline int32 RailIndex(EArcTerrainRail Rail)
	{
		return FMath::Clamp(static_cast<int32>(Rail), 0, ArcTerrain::NumTileRails - 1);
	}

	inline int32 TierIndex(int32 Tier)
	{
		return FMath::Clamp(Tier, 0, ArcTerrain::MaxTiers - 1);
	}

	ARCTERRAIN_API const TCHAR* RailName(EArcTerrainRail Rail);

	/** Which producer materialised a cell — the answer to "what is growing the cell count". */
	enum class ECellRail : uint8
	{
		/** MarkBoundsDirty: an edit's bounds. */
		Bounds = 0,
		/** PinCellsInBounds: a selection or edit pin. */
		Pin = 1,
		/** The residency phase's content walk: authored content in a granted sector with no tile of its own. */
		Content = 2,
		Count = 3,
	};

	struct FStreamCounters
	{
		int32 Requests = 0;
		int32 Arrivals = 0;
		int32 Releases = 0;
		int32 ManifestMisses = 0;
		int32 Unusable = 0;
	};

	struct FBakeCounters
	{
		int32 Candidates = 0;
		int32 Steps = 0;
		int32 Fresh = 0;
		int32 Deferred = 0;
		int32 Incomplete = 0;
		int32 Writes = 0;
		int32 WriteFailures = 0;
		int32 Completed = 0;
	};

	struct FCellCounters
	{
		int32 Created[static_cast<int32>(ECellRail::Count)] = {};
		int32 Removed = 0;
		int32 CompositeCandidates = 0;
	};

	/** Where one atlas cell fill took its heights from. */
	enum class EAtlasFillSource : uint8
	{
		/** Height texels cut out of a baked tile. */
		Tile = 0,
		/** Neither: the fill carries only weights. */
		NoHeight = 1,
	};

	struct FAtlasCounters
	{
		int32 FillsFromTile = 0;
		int32 FillsWithoutHeight = 0;
		int32 Refills = 0;
		int32 HeightSlicesGranted = 0;
		int32 HeightSlicesEvicted = 0;
		int32 WeightSlicesGranted = 0;
		int32 WeightSlicesEvicted = 0;
		int32 CellsWalked = 0;
		/** The three disjoint reasons a visited cell ends with no height bytes. */
		int32 CellsNoTile = 0;
		int32 CellsTileWithheld = 0;
		int32 CellsCutFailed = 0;
		int32 CutFailureLogs = 0;
		int32 CoarseResolutionMismatches = 0;
		int32 CoarseResolutionMismatchLogs = 0;
		int32 Ready = 0;
		int32 Deferred = 0;
		int32 InFlight = 0;
		int32 Completion = 0;
		int32 Stale = 0;
		int32 Admission = 0;
		int32 Refusal = 0;
		int32 TopologyBuild = 0;
		int32 Publication = 0;
		int32 PreviewRebind = 0;
	};

	struct FLightingCounters
	{
		int32 Ready = 0;
		int32 Deferred = 0;
		int32 InFlight = 0;
		int32 Completion = 0;
		int32 Stale = 0;
	};

	struct FFarCounters
	{
		int32 Ready = 0;
		int32 Deferred = 0;
		int32 InFlight = 0;
		int32 Completion = 0;
		int32 Stale = 0;
	};

	struct FCollisionCounters
	{
		/** Heightfields handed to a patch component, whatever produced them. */
		int32 Applied = 0;
		/** Heightfields whose geometry was produced this frame. */
		int32 Builds = 0;
		int32 CacheHits = 0;
		int32 CacheMisses = 0;
	};

	/** Tick phases, in execution order. Indices are the array positions of FTimings::PhaseSeconds. */
	enum class EPhase : uint8
	{
		EditPins = 0,
		Residency = 1,
		TileStream = 2,
		Lighting = 3,
		FarBlocks = 4,
		Composite = 5,
		Atlas = 6,
		Collision = 7,
		SyncAnchor = 8,
		Bake = 9,
		Count = 10,
	};

	enum class ELeafTiming : uint8
	{
		TileKeyGather = 0,
		TileKeyRecipe = 1,
		TileKeyWeight = 2,
		TileKeyData = 3,
		CollisionModifierSync = 4,
		CollisionHeightOpGather = 5,
		CollisionHeightfieldBuild = 6,
		CollisionApply = 7,
		SetHeightfieldPhysics = 8,
		SetHeightfieldBounds = 9,
		SetHeightfieldNavigation = 10,
		HeightFinalizeMinMax = 11,
		HeightFinalizeEncode = 12,
		HeightFinalizeWrite = 13,
		AtlasResidencyReconcile = 14,
		AtlasAdmission = 15,
		AtlasHeightSnapshot = 16,
		AtlasWeightSnapshot = 17,
		AtlasCbtTopology = 18,
		AtlasCbtPublish = 19,
		AtlasPreviewBind = 20,
		LightingWindowReconcile = 21,
		LightingPayloadSnapshot = 22,
		LightingCompletionApply = 23,
		FarWindowReconcile = 24,
		FarPayloadSnapshot = 25,
		FarCompletionApply = 26,
		Count = 27,
	};

	/** Milliseconds, not events — the half the counters cannot answer. BlockingGameThreadSeconds is the one
	 *  that names a hitch: work that stalls the frame rather than running on a task. */
	struct FTimings
	{
		double PhaseSeconds[static_cast<int32>(EPhase::Count)] = {};
		double LeafSeconds[static_cast<int32>(ELeafTiming::Count)] = {};

		double TotalSeconds = 0.0;

		/** NOT bounded by TotalSeconds. The blocking scopes also cover LoadTileSync, which editor bakes
		 *  (ArcForest, ArcTown, PCG) call off-tick — that time accumulates into whichever frame is open and
		 *  is charged to it, so it can dwarf that tick's total. The hitch gate reads the larger of the two
		 *  for exactly this reason. */
		double BlockingGameThreadSeconds = 0.0;

		/** The most expensive instrumented operation, by name and key. The key is on the lattice its name
		 *  implies: cell, tile or collision patch. */
		double WorstSeconds = 0.0;
		FIntPoint WorstKey = FIntPoint::ZeroValue;
		const TCHAR* WorstWhat = nullptr;
	};

	struct FWindow
	{
		int32 Ticks = 0;
		FTimings Sum;
		double MaxPhaseSeconds[static_cast<int32>(EPhase::Count)] = {};
		double MaxLeafSeconds[static_cast<int32>(ELeafTiming::Count)] = {};
		double MaxTotalSeconds = 0.0;
		double MaxBlockingGameThreadSeconds = 0.0;
		int32 HitchTicks = 0;
		double WorstHitchSeconds = 0.0;
		FTimings WorstHitchTick;
		float HitchThresholdMs = 0.0f;

		double SumDiagGatherSeconds = 0.0;
		double MaxDiagGatherSeconds = 0.0;

		/** Real elapsed time the window covered, from the same clock ShouldEmit() gates on. */
		double WindowWallSeconds = 0.0;
		/** FApp::GetDeltaTime(), summed and maxed per tick — the frame this terrain's own time is a share of. */
		double SumFrameDeltaSeconds = 0.0;
		double MaxFrameDeltaSeconds = 0.0;
	};

	/** Everything that HAPPENED since the last emit. Reset on emit, not per tick. */
	struct FFrame
	{
		FStreamCounters Stream[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers];
		FBakeCounters Bake[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers];
		FCellCounters Cells;
		FAtlasCounters Atlas;
		FLightingCounters Lighting;
		FFarCounters Far;
		FCollisionCounters Collision;
		FTimings Tick;
		FWindow Window;
	};

	/** Everything that EXISTS right now, summed over every registered terrain — the counters cannot answer
	 *  it, and it is the half of the line that says whether tiles come back down again. */
	struct FGauges
	{
		int32 TilesLoaded[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
		int32 TilesInFlight[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
		int32 TilesUnusable[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
		int32 ManifestEntries[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
		int32 BakeQueue[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
		int32 BakeProbes[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
		int32 BakeEvaluations[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
		int32 BakeWrites[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
		int32 BakeDeferred[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
		int32 BakeInFlight[ArcTerrain::NumTileRails] = {};
		int32 Terrains = 0;
		int32 Cells = 0;
		int32 CollisionPatches = 0;
		int32 PatchesMissingHeightSource = 0;
	};

	/** Milliseconds above which one breakdown line is emitted REGARDLESS of GLevel — a hitch has to report
	 *  itself, not wait to be reproduced with logging on. Read against the LARGER of the tick total and the
	 *  blocking-game-thread total, so an off-tick synchronous read reports too. 0 stops the accumulation
	 *  and the emit; the phase markers themselves are unconditional. */
	ARCTERRAIN_API extern float GHitchMs;

	ARCTERRAIN_API extern float GDiagIntervalSeconds;

	ARCTERRAIN_API extern int32 GBakeProgress;

	ARCTERRAIN_API extern float GBakeProgressIntervalSeconds;

	inline bool AreStatsCollecting()
	{
#if STATS
		return FThreadStats::IsCollectingData();
#else
		return false;
#endif
	}

	/** Whether anything accumulates or formats. */
	inline bool IsTiming() { return GLevel > 0 || GHitchMs > 0.0f || AreStatsCollecting(); }

	/** For the counters STATGROUP_ArcTerrain publishes: they have to be tallied with no cvar set. */
	inline bool IsCounting() { return IsOn() || AreStatsCollecting(); }

	/** Every counter, window and emit throttle below is keyed by world and resolves through the one bound
	 *  here; PIE-from-editor ticks two subsystems per frame. Bound for the rest of the frame, never
	 *  unbound: an editor bake's off-tick NoteBlockingGameThread lands on the world that ticked last. */
	ARCTERRAIN_API void SetTickingWorld(const UWorld* World);

	ARCTERRAIN_API void ReleaseWorld(const UWorld* World);

	ARCTERRAIN_API FFrame& Frame();

	ARCTERRAIN_API void ResetFrame();

	ARCTERRAIN_API void CompleteTickTimings(FTimings& TickTimings);

	/** The hitch gate is evaluated HERE, against this tick alone; only the emit is per-window, so a spike
	 *  inside a one-second window still elects itself instead of being averaged away. FrameDeltaSeconds is
	 *  FApp::GetDeltaTime() for this tick — the frame our own time is a share of. */
	ARCTERRAIN_API void EndTick(const FTimings& TickTimings, double FrameDeltaSeconds,
		double DiagGatherSeconds);

	/** True at most once per arc.Terrain.DiagIntervalSeconds, and closes the window when it returns true. */
	ARCTERRAIN_API bool ShouldEmit();

	ARCTERRAIN_API void SetTickStats(const FTimings& TickTimings, const FGauges& Gauges,
		double DiagGatherSeconds);

	/** The level-1 emit: five sections over the window. */
	ARCTERRAIN_API void LogFrame(const FGauges& Gauges);
	ARCTERRAIN_API void LogTerrainPipeline(const FArcTerrainPipelineSnapshot& Snapshot);

	ARCTERRAIN_API void LogBakeProgress(const FGauges& Gauges);

	ARCTERRAIN_API void CancelBakeProgress(const UWorld* World);

	ARCTERRAIN_API void LogHitch();

	ARCTERRAIN_API void NoteBlockingGameThreadImpl(const TCHAR* What, FIntPoint Key, double Seconds);

	/** Work that stalls the game thread rather than running on a task. */
	inline void NoteBlockingGameThread(const TCHAR* What, FIntPoint Key, double Seconds)
	{
		if (IsTiming())
		{
			NoteBlockingGameThreadImpl(What, Key, Seconds);
		}
	}

	/** Times its scope into the blocking-game-thread total and the worst-step slot. */
	struct FScopedBlockingGameThread
	{
		FScopedBlockingGameThread(const TCHAR* InWhat, FIntPoint InKey)
			: What(InWhat), Key(InKey), StartSeconds(IsTiming() ? FPlatformTime::Seconds() : 0.0)
		{
		}

		~FScopedBlockingGameThread()
		{
			if (StartSeconds != 0.0)
			{
				NoteBlockingGameThreadImpl(What, Key, FPlatformTime::Seconds() - StartSeconds);
			}
		}

		FScopedBlockingGameThread(const FScopedBlockingGameThread&) = delete;
		FScopedBlockingGameThread& operator=(const FScopedBlockingGameThread&) = delete;

	private:
		const TCHAR* What;
		FIntPoint Key;
		double StartSeconds;
	};

	/** One line per terrain and rail at registration, UNCONDITIONAL — the one entry point that formats at
	 *  level 0: without the lattice, the byte sizes and the two directories nothing else in this file can be
	 *  interpreted. It costs no disk read at level 0: the manifest is counted only where something else has
	 *  already loaded it, and prints as '?' otherwise. */
	ARCTERRAIN_API void LogTerrainGrid(const AArcTerrain& Terrain, const FArcTerrainState& State);

	ARCTERRAIN_API void NoteStreamRequestImpl(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey);
	ARCTERRAIN_API void NoteTileArrivedImpl(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey,
		uint32 PayloadId, int64 Bytes);
	ARCTERRAIN_API void NoteTileReleasedImpl(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey);
	ARCTERRAIN_API void NoteTileRejectedImpl(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey,
		EArcTerrainTileLoad Result);
	ARCTERRAIN_API void NoteTileWrittenImpl(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey, int64 Bytes,
		double Seconds, bool bWritten);
	ARCTERRAIN_API void NoteCellCreatedImpl(ECellRail Producer, FIntPoint CellKey);
	ARCTERRAIN_API void NoteCellRemovedImpl(FIntPoint CellKey);
	ARCTERRAIN_API void NoteCollisionAppliedImpl(FIntPoint PatchKey);
	ARCTERRAIN_API void NoteCollisionBuildImpl(FIntPoint PatchKey, bool bFromBakedTiles);

	inline void NoteStreamRequest(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey)
	{
		if (IsOn())
		{
			NoteStreamRequestImpl(Rail, Tier, TileKey);
		}
	}

	inline void NoteTileArrived(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey, uint32 PayloadId,
		int64 Bytes)
	{
		if (IsOn())
		{
			NoteTileArrivedImpl(Rail, Tier, TileKey, PayloadId, Bytes);
		}
	}

	inline void NoteTileReleased(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey)
	{
		if (IsOn())
		{
			NoteTileReleasedImpl(Rail, Tier, TileKey);
		}
	}

	inline void NoteTileRejected(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey,
		EArcTerrainTileLoad Result)
	{
		if (IsOn())
		{
			NoteTileRejectedImpl(Rail, Tier, TileKey, Result);
		}
	}

	inline void NoteTileWritten(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey, int64 Bytes,
		double Seconds, bool bWritten)
	{
		if (IsOn())
		{
			NoteTileWrittenImpl(Rail, Tier, TileKey, Bytes, Seconds, bWritten);
		}
	}

	inline void NoteCellCreated(ECellRail Producer, FIntPoint CellKey)
	{
		if (IsOn())
		{
			NoteCellCreatedImpl(Producer, CellKey);
		}
	}

	inline void NoteCellRemoved(FIntPoint CellKey)
	{
		if (IsOn())
		{
			NoteCellRemovedImpl(CellKey);
		}
	}

	/** Bulk removal, for the paths that drop the whole cell map at once instead of walking RemoveCell. */
	inline void NoteCellsRemoved(int32 Count)
	{
		if (IsOn() && Count > 0)
		{
			Frame().Cells.Removed += Count;
		}
	}

	/** The apply choke point: a heightfield reached a patch component. Says nothing about what produced it. */
	inline void NoteCollisionApplied(FIntPoint PatchKey)
	{
		if (IsOn())
		{
			NoteCollisionAppliedImpl(PatchKey);
		}
	}

	inline void NoteCollisionBuild(FIntPoint PatchKey, bool bFromBakedTiles)
	{
		if (IsOn())
		{
			NoteCollisionBuildImpl(PatchKey, bFromBakedTiles);
		}
	}

	inline void NoteCompositeCandidates(int32 Count)
	{
		if (IsOn())
		{
			Frame().Cells.CompositeCandidates += Count;
		}
	}

	inline void NoteBakeCandidates(EArcTerrainRail Rail, int32 Tier, int32 Count)
	{
		if (IsOn())
		{
			Frame().Bake[RailIndex(Rail)][TierIndex(Tier)].Candidates += Count;
		}
	}

	inline void NoteBakeSteps(EArcTerrainRail Rail, int32 Tier, int32 Count)
	{
		if (IsOn())
		{
			Frame().Bake[RailIndex(Rail)][TierIndex(Tier)].Steps += Count;
		}
	}

	inline void NoteBakeFresh(EArcTerrainRail Rail, int32 Tier)
	{
		if (IsOn())
		{
			++Frame().Bake[RailIndex(Rail)][TierIndex(Tier)].Fresh;
		}
	}

	inline void NoteBakeDeferred(EArcTerrainRail Rail, int32 Tier)
	{
		if (IsOn())
		{
			++Frame().Bake[RailIndex(Rail)][TierIndex(Tier)].Deferred;
		}
	}

	inline void NoteBakeIncomplete(EArcTerrainRail Rail, int32 Tier)
	{
		if (IsOn())
		{
			++Frame().Bake[RailIndex(Rail)][TierIndex(Tier)].Incomplete;
		}
	}

	ARCTERRAIN_API void NoteBakeCompletedImpl();

	inline void NoteBakeCompleted()
	{
		if (GBakeProgress != 0)
		{
			NoteBakeCompletedImpl();
		}
	}

	ARCTERRAIN_API void NoteBakeCoalesceImpl(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey, uint32 Invalidations, int32 Contributors, uint32 Serial);

	inline void NoteBakeCoalesce(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey, uint32 Invalidations, int32 Contributors, uint32 Serial)
	{
		if (GBakeProgress != 0)
		{
			NoteBakeCoalesceImpl(Terrain, Rail, Tier, TileKey, Invalidations, Contributors, Serial);
		}
	}

	inline void NoteAtlasFill(EAtlasFillSource Source)
	{
		if (IsOn())
		{
			FAtlasCounters& Atlas = Frame().Atlas;
			switch (Source)
			{
			case EAtlasFillSource::Tile:
				++Atlas.FillsFromTile;
				break;
			default:
				++Atlas.FillsWithoutHeight;
				break;
			}
		}
	}

	enum class EAtlasHeightMiss : uint8
	{
		NoTile,
		Withheld,
		CutFailed,
		CoarseResolutionMismatch,
	};

	/** It NAMES no check: the call site cannot see which one refused, and a mirrored copy of the conditions
	 *  would drift away from the real ones. */
	ARCTERRAIN_API void NoteAtlasCutFailureImpl(FIntPoint CellKey, FIntPoint TileKey, int32 PayloadRail,
		int32 PayloadResolution, int32 PayloadFloats, const FArcTerrainTileGrid& Grid,
		int32 ExpectedResolution);

	inline void NoteAtlasCutFailure(FIntPoint CellKey, FIntPoint TileKey, int32 PayloadRail,
		int32 PayloadResolution, int32 PayloadFloats, const FArcTerrainTileGrid& Grid,
		int32 ExpectedResolution)
	{
		if (IsVerbose())
		{
			NoteAtlasCutFailureImpl(CellKey, TileKey, PayloadRail, PayloadResolution, PayloadFloats, Grid,
				ExpectedResolution);
		}
	}

	ARCTERRAIN_API void NoteCoarseResolutionMismatchImpl(FIntPoint SectorKey, int32 Tier,
		int32 PayloadResolution, int32 SliceSide);

	inline void NoteCoarseResolutionMismatch(FIntPoint SectorKey, int32 Tier, int32 PayloadResolution,
		int32 SliceSide)
	{
		if (IsVerbose())
		{
			NoteCoarseResolutionMismatchImpl(SectorKey, Tier, PayloadResolution, SliceSide);
		}
	}

	inline void NoteAtlasCellWalked()
	{
		if (IsOn())
		{
			++Frame().Atlas.CellsWalked;
		}
	}

	inline void NoteAtlasHeightMiss(EAtlasHeightMiss Miss)
	{
		if (IsOn())
		{
			FAtlasCounters& Atlas = Frame().Atlas;
			switch (Miss)
			{
			case EAtlasHeightMiss::NoTile:
				++Atlas.CellsNoTile;
				break;
			case EAtlasHeightMiss::Withheld:
				++Atlas.CellsTileWithheld;
				break;
			case EAtlasHeightMiss::CoarseResolutionMismatch:
				++Atlas.CoarseResolutionMismatches;
				break;
			default:
				++Atlas.CellsCutFailed;
				break;
			}
		}
	}

	inline void NoteAtlasRefill()
	{
		if (IsOn())
		{
			++Frame().Atlas.Refills;
		}
	}

	inline void NoteSliceGranted(bool bWeight)
	{
		if (IsCounting())
		{
			FAtlasCounters& Atlas = Frame().Atlas;
			++(bWeight ? Atlas.WeightSlicesGranted : Atlas.HeightSlicesGranted);
		}
	}

	/** Called from FArcTerrainTileAtlas's own eviction choke points, so admission-pressure evictions and the
	 *  wholesale release count the same as the residency-driven ones. */
	inline void NoteSlicesEvicted(bool bWeight, int32 Count)
	{
		if (IsCounting() && Count > 0)
		{
			FAtlasCounters& Atlas = Frame().Atlas;
			(bWeight ? Atlas.WeightSlicesEvicted : Atlas.HeightSlicesEvicted) += Count;
		}
	}

	inline void NoteAtlasReady()
	{
		if (IsOn())
		{
			++Frame().Atlas.Ready;
		}
	}

	inline void NoteAtlasInFlight()
	{
		if (IsOn())
		{
			++Frame().Atlas.InFlight;
		}
	}

	inline void NoteAtlasDeferred()
	{
		if (IsOn())
		{
			++Frame().Atlas.Deferred;
		}
	}

	inline void NoteAtlasAdmission()
	{
		if (IsOn())
		{
			++Frame().Atlas.Admission;
		}
	}

	inline void NoteAtlasRefusal()
	{
		if (IsOn())
		{
			++Frame().Atlas.Refusal;
		}
	}

	inline void NoteAtlasTopologyBuild()
	{
		if (IsOn())
		{
			++Frame().Atlas.TopologyBuild;
		}
	}

	inline void NoteAtlasPublication()
	{
		if (IsOn())
		{
			++Frame().Atlas.Publication;
		}
	}

	inline void NoteAtlasPreviewRebind()
	{
		if (IsOn())
		{
			++Frame().Atlas.PreviewRebind;
		}
	}

	inline void NoteLightingReady()
	{
		if (IsOn())
		{
			++Frame().Lighting.Ready;
		}
	}

	inline void NoteLightingDeferred()
	{
		if (IsOn())
		{
			++Frame().Lighting.Deferred;
		}
	}

	inline void NoteLightingInFlight()
	{
		if (IsOn())
		{
			++Frame().Lighting.InFlight;
		}
	}

	inline void NoteLightingCompletion()
	{
		if (IsOn())
		{
			++Frame().Lighting.Completion;
		}
	}

	inline void NoteLightingStale()
	{
		if (IsOn())
		{
			++Frame().Lighting.Stale;
		}
	}

	inline void NoteFarReady()
	{
		if (IsOn())
		{
			++Frame().Far.Ready;
		}
	}

	inline void NoteFarDeferred()
	{
		if (IsOn())
		{
			++Frame().Far.Deferred;
		}
	}

	inline void NoteFarInFlight()
	{
		if (IsOn())
		{
			++Frame().Far.InFlight;
		}
	}

	inline void NoteFarCompletion()
	{
		if (IsOn())
		{
			++Frame().Far.Completion;
		}
	}

	inline void NoteFarStale()
	{
		if (IsOn())
		{
			++Frame().Far.Stale;
		}
	}

	inline void NoteCollisionCacheHit()
	{
		if (IsOn())
		{
			++Frame().Collision.CacheHits;
		}
	}

	inline void NoteCollisionCacheMiss()
	{
		if (IsOn())
		{
			++Frame().Collision.CacheMisses;
		}
	}
}
