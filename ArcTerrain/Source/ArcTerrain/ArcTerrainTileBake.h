// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "ArcTerrainTileFormat.h"
#include "Composite/ArcTerrainCpuComposite.h"
#include "IO/IoHash.h"
#include "Tasks/Task.h"

class AArcTerrain;
class UArcTerrainStampComponent;
struct FArcTerrainState;
struct FArcTerrainDataMapTileBakeResult;

namespace ArcTerrainDataMaps
{
	struct FArcTerrainDataMapKeyInputs;
}

namespace ArcTerrainTileCache
{
	// Forward-declared rather than included: ArcTerrainTileCache.h pulls DDC headers, and DerivedDataCache
	// is a PRIVATE dependency of this module — modules that include this one through ArcTerrainSubsystem.h
	// have no include path to it.
	struct FAsyncGet;
	using FAsyncGetPtr = TSharedPtr<FAsyncGet, ESPMode::ThreadSafe>;
}

/** One tile's finished off-thread payload. Floats and Planes retain the unencoded evaluation output for
 *  DDC; EncodedHeader/EncodedBody are the immutable on-disk payload prepared by the worker. */
struct FArcTerrainTileBakeResult
{
	TArray<float> Floats;

	TArray<TArray<uint8>> Planes;

	/** Height's per-tile quantization window, retained separately because the game thread reports it
	 *  against the terrain's authored bounds before publishing the already encoded payload. */
	float QuantMin = 0.0f;
	float QuantMax = 0.0f;

	/** The header and body are made alongside the pure CPU evaluation, never from UObjects. */
	FArcTerrainTileHeader EncodedHeader;
	TArray<uint8> EncodedBody;

	/** Weight-only terminal forms. Empty deletes an existing tile; overflow records a latched refusal. */
	bool bWeightFinalized = false;
	bool bWeightEmpty = false;
	bool bWeightOverflow = false;

	/** Indices into Planes, in exact file/DDC order, after the same zero/base-plane filtering as before. */
	TArray<int32> KeptWeightPlaneIndices;

	/** Data's worker-owned masks and saturation report. Empty outside the data rail. */
	TSharedPtr<FArcTerrainDataMapTileBakeResult, ESPMode::ThreadSafe> DataMapBake;
};

enum class EArcTerrainBakeReservationKind : uint8
{
	Probe,
	Evaluation,
	Write
};

struct FArcTerrainTileBakeInputs
{
	FArcTerrainTileGrid Grid;
	FBox GatherBounds;
	TArray<UArcTerrainStampComponent*> SortedStamps;
};

/** One tile's in-flight bake. Keyed on the TILE, not on a cell: the unit the payload is written in has
 *  no cell state to hang off, and a tile spans cells that settle independently. */
struct FArcTerrainPendingTileBake
{
	/** In-flight non-blocking DDC probe on CacheKey, mutually exclusive with Evaluation. Dropping the
	 *  handle abandons the request — it never reaches back into terrain state. */
	ArcTerrainTileCache::FAsyncGetPtr Probe;

	UE::Tasks::TTask<FArcTerrainTileBakeResult> Evaluation;

	FIoHash CacheKey;

	/** FArcTerrainTileDecision::DirtySerial as it stood when CacheKey was computed. The decision this
	 *  payload produces is recorded AT THIS SERIAL, not at the one current when it lands: an edit
	 *  arriving mid-flight moves the serial, and recording the finished (now stale) payload against the
	 *  newer serial would tell the memo the tile is up to date. */
	uint32 DirtySerial = 0;

	/** Global channel indices the evaluation was launched for, in body order. Empty off the weight rail. */
	TArray<int32> ChannelIndices;

	int32 Resolution = 0;

	int64 ReservedBytes = 0;

	uint64 ReservationId = 0;

	/** The payload came from the DDC, so the put is skipped. */
	bool bFromCache = false;
};

/** One tile's memoised decision plus its incomplete-gather backoff. */
struct FArcTerrainTileDecision
{
	/** The cache key of the last decision taken for this tile — written, dropped or refused. Zero = no
	 *  decision yet. In-memory only: a fresh session re-decides every tile it touches, which a DDC hit
	 *  answers without re-evaluating. */
	FIoHash WrittenKey;

	uint32 DirtySerial = 0;

	/** The DirtySerial WrittenKey was decided at. Equal means nothing has touched this tile since, so
	 *  gathering the tile inputs and hashing them would reproduce the same key. */
	uint32 DecidedSerial = 0;

	uint32 CoalescedInvalidations = 0;

	/** The terrain bake pass at which this key may first leave the ready queue. */
	uint64 ReadyAfterBakePass = 0;

	/** Game-thread time before which an incomplete gather is not re-attempted. Without it one stamp with
	 *  an undecodable texture holds the head of its rail's candidate list every tick — the order is
	 *  stable — and starves every other tile of the budget. */
	double RetryAfter = 0.0;

	/** Consecutive incomplete gathers; the exponent of the retry backoff. */
	uint8 RetryCount = 0;

	bool bRetryDeferred = false;
};

/** One rail's tile-bake bookkeeping. */
struct FArcTerrainTileRailState
{
	struct FWriteReservation
	{
		uint64 ReservationId = 0;
		uint64 Generation = 0;
		int64 ReservedBytes = 0;
		uint32 DirtySerial = 0;
	};
	struct FDeferredTile
	{
		FIntPoint TileKey;
		double RetryAfter = 0.0;
	};

	TMap<FIntPoint, FArcTerrainPendingTileBake> Pending;

	TMap<FIntPoint, FArcTerrainTileDecision> Decisions;

	/** Tiles whose bytes are on a write task. AT MOST ONE WRITE PER KEY: while a key is here the bake
	 *  refuses to take the tile again. Cleared by the game-thread completion. */
	TSet<FIntPoint> WritesInFlight;
	TMap<FIntPoint, FWriteReservation> WriteReservations;
	uint64 WriteGeneration = 1;

	TArray<FIntPoint> ReadyQueue;
	TSet<FIntPoint> ReadyKeys;
	int32 ReadyHead = 0;

	TArray<FDeferredTile> DeferredQueue;
	TSet<FIntPoint> DeferredKeys;

	int32 ProbesInFlight = 0;
	int32 EvaluationsInFlight = 0;
	int32 WritesInFlightCount = 0;
	int64 ProbeReservedBytes = 0;
	int64 EvaluationReservedBytes = 0;
	int64 WriteReservedByteCount = 0;
	int32 RetriesDeferred = 0;

	bool TryGetNextRetryAfter(double& OutRetryAfter) const
	{
		if (DeferredQueue.IsEmpty())
		{
			return false;
		}
		OutRetryAfter = DeferredQueue.HeapTop().RetryAfter;
		return true;
	}

	void AddPending(FIntPoint TileKey, FArcTerrainPendingTileBake&& PendingBake)
	{
		check(!Pending.Contains(TileKey));
		check(PendingBake.ReservationId != 0);
		if (FArcTerrainTileDecision* Decision = Decisions.Find(TileKey))
		{
			ClearRetryDeferred(TileKey, *Decision);
		}
		if (PendingBake.Evaluation.IsValid())
		{
			++EvaluationsInFlight;
			EvaluationReservedBytes += PendingBake.ReservedBytes;
		}
		else
		{
			check(PendingBake.Probe.IsValid());
			++ProbesInFlight;
			ProbeReservedBytes += PendingBake.ReservedBytes;
		}
		Pending.Add(TileKey, MoveTemp(PendingBake));
	}

	FArcTerrainPendingTileBake TakePending(FIntPoint TileKey)
	{
		FArcTerrainPendingTileBake* PendingBake = Pending.Find(TileKey);
		check(PendingBake != nullptr);
		if (PendingBake->Evaluation.IsValid())
		{
			check(EvaluationsInFlight > 0);
			--EvaluationsInFlight;
			check(EvaluationReservedBytes >= PendingBake->ReservedBytes);
			EvaluationReservedBytes -= PendingBake->ReservedBytes;
		}
		else
		{
			check(PendingBake->Probe.IsValid());
			check(ProbesInFlight > 0);
			--ProbesInFlight;
			check(ProbeReservedBytes >= PendingBake->ReservedBytes);
			ProbeReservedBytes -= PendingBake->ReservedBytes;
		}
		FArcTerrainPendingTileBake Result = MoveTemp(*PendingBake);
		Pending.Remove(TileKey);
		return Result;
	}

	void RemovePending(FIntPoint TileKey)
	{
		if (!Pending.Contains(TileKey))
		{
			return;
		}
		TakePending(TileKey);
	}

	void ResetPending()
	{
		Pending.Reset();
		ProbesInFlight = 0;
		EvaluationsInFlight = 0;
		ProbeReservedBytes = 0;
		EvaluationReservedBytes = 0;
	}

	FWriteReservation BeginWrite(FIntPoint TileKey, uint64 ReservationId, int64 ReservedBytes,
		uint32 LaunchDirtySerial)
	{
		check(!WritesInFlight.Contains(TileKey));
		FWriteReservation Reservation;
		Reservation.ReservationId = ReservationId;
		Reservation.Generation = WriteGeneration;
		Reservation.ReservedBytes = ReservedBytes;
		Reservation.DirtySerial = LaunchDirtySerial;
		WritesInFlight.Add(TileKey);
		WriteReservations.Add(TileKey, Reservation);
		++WritesInFlightCount;
		WriteReservedByteCount += ReservedBytes;
		return Reservation;
	}

	bool FinishWrite(FIntPoint TileKey, uint64 ReservationId, uint64 Generation)
	{
		FWriteReservation* Reservation = WriteReservations.Find(TileKey);
		if (Reservation == nullptr || Reservation->ReservationId != ReservationId
			|| Reservation->Generation != Generation)
		{
			return false;
		}
		check(WritesInFlightCount > 0);
		check(WriteReservedByteCount >= Reservation->ReservedBytes);
		WritesInFlight.Remove(TileKey);
		--WritesInFlightCount;
		WriteReservedByteCount -= Reservation->ReservedBytes;
		WriteReservations.Remove(TileKey);
		return true;
	}

	void InvalidateWrites()
	{
		++WriteGeneration;
		if (WriteGeneration == 0)
		{
			++WriteGeneration;
		}
		WritesInFlight.Reset();
		WriteReservations.Reset();
		WritesInFlightCount = 0;
		WriteReservedByteCount = 0;
	}

	void ResetWorkQueue()
	{
		ReadyQueue.Reset();
		ReadyKeys.Reset();
		ReadyHead = 0;
		DeferredQueue.Reset();
		DeferredKeys.Reset();
		RetriesDeferred = 0;
	}

	void ResetDecisions()
	{
		Decisions.Reset();
		DeferredQueue.Reset();
		DeferredKeys.Reset();
		RetriesDeferred = 0;
	}

	void EnqueueReady(FIntPoint TileKey)
	{
		bool bAlreadyQueued = false;
		ReadyKeys.Add(TileKey, &bAlreadyQueued);
		if (!bAlreadyQueued)
		{
			ReadyQueue.Add(TileKey);
		}
	}

	bool TakeReady(FIntPoint& OutTileKey)
	{
		while (ReadyHead < ReadyQueue.Num())
		{
			const FIntPoint TileKey = ReadyQueue[ReadyHead++];
			if (ReadyKeys.Remove(TileKey) > 0)
			{
				OutTileKey = TileKey;
				if (ReadyHead >= 64 && ReadyHead * 2 >= ReadyQueue.Num())
				{
					ReadyQueue.RemoveAt(0, ReadyHead, EAllowShrinking::No);
					ReadyHead = 0;
				}
				return true;
			}
		}
		ReadyQueue.Reset();
		ReadyHead = 0;
		return false;
	}

	int32 NumReady() const
	{
		return ReadyKeys.Num();
	}

	void RemoveReady(FIntPoint TileKey)
	{
		ReadyKeys.Remove(TileKey);
	}

	void Defer(FIntPoint TileKey, double RetryAfter)
	{
		RemoveReady(TileKey);
		bool bAlreadyDeferred = false;
		DeferredKeys.Add(TileKey, &bAlreadyDeferred);
		if (!bAlreadyDeferred)
		{
			DeferredQueue.HeapPush(FDeferredTile{ TileKey, RetryAfter },
				[](const FDeferredTile& Left, const FDeferredTile& Right)
				{
					return Left.RetryAfter > Right.RetryAfter;
				});
		}
	}

	void ReleaseDeferred(double Now)
	{
		while (!DeferredQueue.IsEmpty() && DeferredQueue.HeapTop().RetryAfter <= Now)
		{
			FDeferredTile Deferred;
			DeferredQueue.HeapPop(Deferred,
				[](const FDeferredTile& Left, const FDeferredTile& Right)
				{
					return Left.RetryAfter > Right.RetryAfter;
				}, EAllowShrinking::No);
			FArcTerrainTileDecision* Decision = Decisions.Find(Deferred.TileKey);
			if (Decision == nullptr || !Decision->bRetryDeferred || Decision->RetryAfter != Deferred.RetryAfter)
			{
				continue;
			}
			if (DeferredKeys.Remove(Deferred.TileKey) == 0)
			{
				continue;
			}
			ClearRetryDeferred(Deferred.TileKey, *Decision);
			EnqueueReady(Deferred.TileKey);
		}
	}

	void MarkRetryDeferred(FArcTerrainTileDecision& Decision)
	{
		if (!Decision.bRetryDeferred)
		{
			Decision.bRetryDeferred = true;
			++RetriesDeferred;
		}
	}

	void ClearRetryDeferred(FIntPoint TileKey, FArcTerrainTileDecision& Decision)
	{
		if (Decision.bRetryDeferred)
		{
			check(RetriesDeferred > 0);
			Decision.bRetryDeferred = false;
			--RetriesDeferred;
		}
		DeferredKeys.Remove(TileKey);
	}
};

namespace ArcTerrainTileBake
{
	/** False (OutGrid untouched) when the rail's lattice is unusable — the caller has no tile rather than
	 *  a tile on a fabricated grid. ResolvedCellWeightQuads is UArcTerrainSubsystem's ResolveWeightQuads
	 *  and is only read on the weight rail. */
	ARCTERRAIN_API bool ResolveTileGrid(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		int32 ResolvedCellWeightQuads, FArcTerrainTileGrid& OutGrid);

	ARCTERRAIN_API bool ResolveBakeTileGrid(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		int32 ResolvedCellWeightQuads, FArcTerrainTileGrid& OutGrid);

	void GatherTileKeysIntersectingBounds(const FArcTerrainTileGrid& Grid, const FBox& LocalBounds,
		TArray<FIntPoint>& OutKeys);

	ARCTERRAIN_API bool GatherTileBakeInputs(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey,
		const AArcTerrain& Terrain, FArcTerrainState& State,
		FArcTerrainTileBakeInputs& OutInputs);

	/** Terrain-local AABB of everything MakeTileRaster samples — the tile's extent plus its apron, Z left
	 *  effectively unbounded, edge-inclusive. THE CONTRIBUTOR-GATHER AUTHORITY for the bake AND for the
	 *  cache key: the two must gather over the same box or the key stops noticing stamps that move bytes. */
	FBox TileGatherBounds(const FArcTerrainTileGrid& Grid, FIntPoint TileKey);

	/** The tile's sample grid: TileQuads + 3 samples from global index TileKey * TileQuads - 1, spaced
	 *  TileExtent / TileQuads — the composite's apron convention at the tile's own density. */
	ARCTERRAIN_API ArcTerrainCpuComposite::FCellRaster MakeTileRaster(const FArcTerrainTileGrid& Grid,
		FIntPoint TileKey);

	FIntPoint TileKeyFromCell(const FArcTerrainTileGrid& Grid, FArcTerrainCellKey CellKey);

	/**
	 * DDC key of one tile's payload: this rail's own format GUID, the rail, the tile key and the whole
	 * resolved lattice, over the composite recipe key of the stamps covering the tile — plus, on the
	 * weight rail, the present weight-layer identities the recipe key excludes and, on the data rail,
	 * the data-mask key.
	 *
	 * TAKES THE ACTOR, not FArcTerrainConfig: the recipe key folds HeightOptions.BaseHeightCm and the
	 * terrain transform, and FArcTerrainState carries neither. Keyed on (Config, State) alone, a
	 * BaseHeightCm edit would reproduce the same key and every tile would keep the old heights.
	 *
	 * Zero hash when the rail's lattice is unusable.
	 */
	FIoHash ComputeTileCacheKey(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey,
		const AArcTerrain& Terrain, const FArcTerrainState& State,
		const FArcTerrainTileBakeInputs& Inputs,
		const ArcTerrainDataMaps::FArcTerrainDataMapKeyInputs* DataInputs);
}

#endif // WITH_EDITOR
