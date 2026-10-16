// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcTerrainDiagnostics.h"
#include "ArcTerrainTileAccess.h"
#include "Lighting/ArcTerrainLightingPatch.h"
#include "Render/ArcTerrainTileAtlas.h"
#include "Tiles/ArcTerrainCellState.h"

class AArcTerrain;
class UArcTerrainChannelSet;
class UArcTerrainStampComponent;
struct FArcTerrainBakeTileRef;
struct FArcTerrainCollisionPatch;
struct FArcTerrainStampEntry;
struct FArcTerrainState;

enum class ETileSourceState : uint8
{
	Absent,
	KnownUnloaded,
	Loaded
};

struct FTileSourceCapture
{
	ETileSourceState State = ETileSourceState::Absent;
	FArcTerrainTilePayloadPtr Payload;
	uint32 PayloadId = 0;
};

namespace ArcTerrainLeafTiming_8f21
{
	inline double Begin()
	{
		return ArcTerrainDiag::IsTiming() ? FPlatformTime::Seconds() : 0.0;
	}

	inline void End(ArcTerrainDiag::ELeafTiming Leaf, double StartSeconds)
	{
		if (StartSeconds != 0.0)
		{
			ArcTerrainDiag::Frame().Tick.LeafSeconds[static_cast<int32>(Leaf)] +=
				FPlatformTime::Seconds() - StartSeconds;
		}
	}
}

namespace ArcTerrainSubsystemPrivate
{
	// Seconds after game-world BeginPlay during which the patches under a pawn sync-build their
	// collision and ring-exit teardown is suppressed (PIE-entry / late-possession window).
	constexpr double GSyncCollisionWindowSeconds = 5.0;

	// XY half-extent (cm) of the box used to find the collision patches under a pawn during the sync
	// window — covers a pawn standing on a patch boundary. NOTE: applied in TERRAIN-LOCAL units (assumes
	// terrain scale ~1, like SyncBuildCollisionAroundAnchor's radius); a scaled terrain only
	// under/overshoots the OUTER patches — the one containing the pawn/anchor is always included.
	constexpr double GPawnCellProbeExtent = 200.0;

	// Sectors per axis either side of an invoker the PREVIEW window may span. The ring geometry needs ~3 at
	// the authored defaults (2.7 km outer radius over 1024 m sectors); the cap only bounds the walk under a
	// degenerate terrain scale, and it keeps the invoker centred so the nearest sectors are the ones kept.
	// It is calibrated in SECTORS — a caller whose unit is something else brings its own MaxSpan.
	constexpr int32 GMaxSectorWindowSpan = 32;

	inline FTileSourceCapture CaptureTileSource(
		const AArcTerrain& Terrain,
		EArcTerrainRail Rail,
		int32 Tier,
		FIntPoint TileKey,
		const FArcTerrainTileGrid* KnownGrid)
	{
		FTileSourceCapture Capture;
		if (!ArcTerrainTileAccess::HasTile(Terrain, Rail, Tier, TileKey, KnownGrid))
		{
			Capture.State = ETileSourceState::Absent;
			return Capture;
		}
		Capture.Payload = ArcTerrainTileAccess::FindLoadedTile(Terrain, Rail, Tier, TileKey);
		if (!Capture.Payload.IsValid())
		{
			Capture.State = ETileSourceState::KnownUnloaded;
			return Capture;
		}
		Capture.State = ETileSourceState::Loaded;
		Capture.PayloadId = Capture.Payload->PayloadId;
		return Capture;
	}

	/** Inclusive sector-key rectangle; MaxX < MinX means empty. */
	struct FSectorWindow
	{
		int32 MinX = 0;
		int32 MinY = 0;
		int32 MaxX = -1;
		int32 MaxY = -1;

		/** MaxSpan, not the radius, decided the width — the window covers less than what was asked for. */
		bool bSpanClamped = false;

		bool operator==(const FSectorWindow& Other) const
		{
			return MinX == Other.MinX && MinY == Other.MinY && MaxX == Other.MaxX && MaxY == Other.MaxY
				&& bSpanClamped == Other.bSpanClamped;
		}

		bool operator<(const FSectorWindow& Other) const
		{
			if (MinX != Other.MinX)
			{
				return MinX < Other.MinX;
			}
			if (MinY != Other.MinY)
			{
				return MinY < Other.MinY;
			}
			if (MaxX != Other.MaxX)
			{
				return MaxX < Other.MaxX;
			}
			if (MaxY != Other.MaxY)
			{
				return MaxY < Other.MaxY;
			}
			return static_cast<int32>(bSpanClamped) < static_cast<int32>(Other.bSpanClamped);
		}
	};

	// Keeps the MaxNum best candidates ordered best-first without ever holding the full set. IsBetter is
	// consulted strictly, so on a tie the earlier-inserted candidate keeps the better slot.
	template <typename CandidateType, typename IsBetterType>
	void InsertBoundedCandidate(TArray<CandidateType>& Ordered, int32 MaxNum,
		const CandidateType& Candidate, const IsBetterType& IsBetter)
	{
		if (MaxNum <= 0)
		{
			return;
		}
		const bool bFull = Ordered.Num() >= MaxNum;
		if (bFull && !IsBetter(Candidate, Ordered.Last()))
		{
			return;
		}
		int32 InsertIndex = Ordered.Num();
		while (InsertIndex > 0 && IsBetter(Candidate, Ordered[InsertIndex - 1]))
		{
			--InsertIndex;
		}
		if (bFull)
		{
			Ordered.Pop(EAllowShrinking::No);
		}
		Ordered.Insert(Candidate, InsertIndex);
	}

	inline double ScheduleExponentialRetry(uint8& RetryCount, double BaseDelaySeconds, double Now)
	{
		RetryCount = FMath::Min<uint8>(RetryCount + 1, 5);
		const uint32 RetryShift = RetryCount - 1;
		return Now + BaseDelaySeconds * static_cast<double>(1u << RetryShift);
	}

	template <typename WantedType>
	void SortPinnedExemptWanted(TArray<WantedType>& Wanted)
	{
		Wanted.Sort([](const WantedType& A, const WantedType& B)
		{
			if (A.bPinned != B.bPinned)
			{
				return A.bPinned;
			}
			return A.DistSq < B.DistSq;
		});
	}

	template <typename WantedType>
	void TrimPinnedExemptWantedSet(TArray<WantedType>& Wanted, int32 Cap, int32 NumPinned)
	{
		if (Wanted.Num() > Cap)
		{
			Wanted.SetNum(FMath::Max(Cap, NumPinned), EAllowShrinking::No);
		}
	}

	template <typename WantedType>
	void TrimCoarseWantedSet(TArray<WantedType>& Wanted, int32 Budget)
	{
		Wanted.Sort([](const WantedType& A, const WantedType& B)
		{
			return A.DistSq > B.DistSq;
		});
		if (Wanted.Num() > Budget)
		{
			Wanted.SetNum(Budget, EAllowShrinking::No);
		}
	}

	struct FCompositeCandidate
	{
		AArcTerrain* Terrain = nullptr;
		FArcTerrainState* State = nullptr;
		FArcTerrainCellKey Key = FArcTerrainCellKey::ZeroValue;
		double DistSq = 0.0;
		bool bPinned = false; // edit-pinned cells composite at TOP priority
	};

	struct FCompositeCandidateOrder
	{
		bool operator()(const FCompositeCandidate& A, const FCompositeCandidate& B) const
		{
			if (A.bPinned != B.bPinned)
			{
				return A.bPinned;
			}
			return A.DistSq < B.DistSq;
		}
	};

	struct FCollisionBuildCandidate
	{
		AArcTerrain* Terrain = nullptr;
		FArcTerrainState* State = nullptr;
		FArcTerrainCollisionKey Key = FArcTerrainCollisionKey::ZeroValue;
		double DistSq = 0.0;
	};

	// Nearest-to-invoker first: over budget it is the far edge of the ring that waits a tick, never the
	// patch a pawn is standing on.
	struct FCollisionCandidateOrder
	{
		bool operator()(const FCollisionBuildCandidate& A, const FCollisionBuildCandidate& B) const
		{
			return A.DistSq < B.DistSq;
		}
	};

	// THE RESIDENCY UNIT IS THE SECTOR. Ranking cells and then admitting whole sectors over the result is
	// non-monotone: the boundary of a cell set cuts sectors anywhere, so the outer ring of sectors is
	// structurally unadmittable, and pinning cells far from the camera evicts near ones — pulling a
	// finished sector out of the atlas mid-edit.
	struct FWantedSector
	{
		FArcTerrainSectorId Id;
		double DistSq = TNumericLimits<double>::Max();
		bool bPinned = false;
	};

	struct FWantedWeightGroup
	{
		FArcTerrainWeightGroupId Id;
		double DistSq = TNumericLimits<double>::Max();
		bool bPinned = false;
	};

	inline FWantedSector& FindOrAddWantedSector(TArray<FWantedSector>& Wanted,
		TMap<FArcTerrainSectorId, int32>& IndexById, FArcTerrainSectorId Id)
	{
		const int32* Existing = IndexById.Find(Id);
		if (Existing != nullptr)
		{
			return Wanted[*Existing];
		}
		FWantedSector& Sector = Wanted[IndexById.Add(Id, Wanted.AddDefaulted())];
		Sector.Id = Id;
		return Sector;
	}

	// Squared WORLD-space XY distance from the nearest invoker to a TERRAIN-LOCAL XY rect: the invoker
	// is clamped to the rect in local space (closest-point-on-rect — an invoker standing on it measures
	// 0; center-distance would be WRONG here, the ring radii can be smaller than a cell half-diagonal),
	// then that closest point is transformed back to WORLD and the distance measured there — the ring
	// radii are WORLD cm, so a scaled terrain must never compare them against terrain-local units. Exact
	// under uniform scale + rotation. XY-only mirrors the cells' XY-partition semantics (a high editor
	// camera still keeps the ground under it resident); Max() when there are no invokers.
	inline double RectInvokerDistSq(double MinX, double MinY, double MaxX, double MaxY,
		const FTransform& TerrainTransform, TConstArrayView<FVector> WorldInvokers)
	{
		double BestDistSq = TNumericLimits<double>::Max();
		for (const FVector& Invoker : WorldInvokers)
		{
			const FVector Local = TerrainTransform.InverseTransformPosition(Invoker);
			const FVector ClosestLocal(
				FMath::Clamp(Local.X, MinX, MaxX),
				FMath::Clamp(Local.Y, MinY, MaxY),
				0.0);
			const FVector ClosestWorld = TerrainTransform.TransformPosition(ClosestLocal);
			const double DX = Invoker.X - ClosestWorld.X;
			const double DY = Invoker.Y - ClosestWorld.Y;
			BestDistSq = FMath::Min(BestDistSq, DX * DX + DY * DY);
		}
		return BestDistSq;
	}

	inline double CellInvokerDistSq(FArcTerrainCellKey Key, double CellSize, const FTransform& TerrainTransform,
		TConstArrayView<FVector> WorldInvokers)
	{
		const double MinX = static_cast<double>(Key.X) * CellSize;
		const double MinY = static_cast<double>(Key.Y) * CellSize;
		return RectInvokerDistSq(MinX, MinY, MinX + CellSize, MinY + CellSize, TerrainTransform,
			WorldInvokers);
	}

	// The same measure on the COLLISION lattice. The collision ring keeps no hot record to cache it in —
	// its pass set is small (the ring is a fraction of the preview radius) and is recomputed each tick,
	// which is also what frees it from the residency phase's refresh order.
	inline double PatchInvokerDistSq(FArcTerrainCollisionKey Key, double PatchSize,
		const FTransform& TerrainTransform, TConstArrayView<FVector> WorldInvokers)
	{
		const double MinX = static_cast<double>(Key.X) * PatchSize;
		const double MinY = static_cast<double>(Key.Y) * PatchSize;
		return RectInvokerDistSq(MinX, MinY, MinX + PatchSize, MinY + PatchSize, TerrainTransform,
			WorldInvokers);
	}

	/** Weight group -> the coarse weight tile covering it, on that tier's own lattice. ONE tile: the
	 *  ladder drops any rung whose group would not fit inside a tile. */
	inline FIntPoint CoarseWeightGroupTileKey(const FArcTerrainTileGrid& TierGrid, FIntPoint GroupKey,
		int32 SliceShift)
	{
		return ArcTerrainTileAccess::TileKeyFromCell(TierGrid,
			FArcTerrainCellKey(GroupKey.X << SliceShift, GroupKey.Y << SliceShift));
	}

	// Sector -> the coarse height tile covering it, on that tier's own lattice. Derived through the grid
	// rather than reused as-is: the sector lives on AtlasLayout.SectorShift, which the atlas may have
	// lowered below the authored value the tile extent was validated against.
	inline FIntPoint CoarseSectorTileKey(const FArcTerrainTileGrid& TierGrid, FIntPoint SectorKey,
		int32 CellSectorShift)
	{
		return ArcTerrainTileAccess::TileKeyFromCell(TierGrid,
			FArcTerrainCellKey(SectorKey.X << CellSectorShift, SectorKey.Y << CellSectorShift));
	}

	double UpdateCellDistances(TArray<FArcTerrainCellHot>& HotCells, TConstArrayView<int32> CellIndices,
		double CellSize, const FTransform& TerrainTransform, TConstArrayView<FVector> WorldInvokers);
	void ResetCellAtlasPublication(FArcTerrainCellState& Cell);
	void FinalizeCompositeAttempt(FArcTerrainState& State, FArcTerrainCellKey Key, FArcTerrainCellHot& Hot,
		FArcTerrainCellState& Cell, double Now);
	FBox RasterGatherBounds(FIntPoint TexelOrigin, int32 Resolution, double TexelToLocal);
}

namespace ArcTerrainAtlasFillLocal_2c74
{
	bool CutCellWeightPlanes(FArcTerrainCellKey Key, const FArcTerrainTileGrid& Grid,
		const FArcTerrainTilePayloadPtr& Payload, const UArcTerrainChannelSet& ChannelSet,
		int32 ExpectedResolution, TArray<TArray<uint8>>& OutPlanes, TArray<int32>& OutChannels,
		bool& bOutUnresolvable);
}

namespace ArcTerrainResidencyIndex_c4e8
{
	void RemoveStampCoverage(FArcTerrainState& State, FArcTerrainStampEntry& Entry);
	void AddStampCoverage(const AArcTerrain& Terrain, FArcTerrainState& State, FArcTerrainStampEntry& Entry);
	void RebuildIndexes(const AArcTerrain& Terrain, FArcTerrainState& State);
	void EnqueueCompositeReady(FArcTerrainState& State, FArcTerrainCellKey Key);
	void NotePendingHeightChanged(FArcTerrainState& State, FIntPoint TileKey);
	void NotePendingHeightReset(FArcTerrainState& State);
}

namespace ArcTerrainCollisionIndex_e7b2
{
	void RebuildStampIndex(FArcTerrainState& State);
	void NoteStampInserted(FArcTerrainState& State, UArcTerrainStampComponent& Stamp, int32 StampIndex);
	void RemoveStampCollisionCoverage(FArcTerrainState& State, FArcTerrainStampEntry& Entry);
	void UpdateStampCollisionCoverage(const AArcTerrain& Terrain, FArcTerrainState& State,
		FArcTerrainStampEntry& Entry);
	void RebuildCollisionMeshIndex(const AArcTerrain& Terrain, FArcTerrainState& State);
	void EnqueueReady(FArcTerrainState& State, const AArcTerrain& Terrain, FArcTerrainCollisionKey Key);
	FArcTerrainCollisionPatch& EnsurePatch(FArcTerrainState& State, FArcTerrainCollisionKey Key);
	void NoteBootstrapKey(FArcTerrainState& State, FArcTerrainCollisionKey Key);
	void ClearScheduling(FArcTerrainState& State);
	void ForgetPatch(FArcTerrainState& State, FArcTerrainCollisionKey Key);
	void PushRetry(FArcTerrainState& State, FArcTerrainCollisionKey Key, double RetryAfter);
	void RebuildRanks(FArcTerrainState& State, const AArcTerrain& Terrain);
}

namespace ArcTerrainLightingIndex_6b19
{
	void EnqueueReady(FArcTerrainState& State, const AArcTerrain& Terrain, FArcTerrainCellKey Key);
	void ClearScheduling(FArcTerrainState& State);
	void RecreateFlatPackedHeightTexture(AArcTerrain& Terrain, FArcTerrainState& State);
	void SeedCurrentWindow(FArcTerrainState& State, const AArcTerrain& Terrain);
	void OrphanInFlight(FArcTerrainState& State);
}

namespace ArcTerrainFarBlockIndex_2c08
{
	void EnqueueReady(FArcTerrainState& State, const AArcTerrain& Terrain, FArcTerrainFarBlockKey Key);
	void ClearScheduling(FArcTerrainState& State);
	void SeedCurrentWindow(FArcTerrainState& State, const AArcTerrain& Terrain);
	void OrphanInFlight(FArcTerrainState& State);
}

#if WITH_EDITOR
namespace ArcTerrainTileWrite_8a3f
{
	bool DropTile(const AArcTerrain& Terrain, const FString& TileDir, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey);
}

namespace ArcTerrainTileSeedLocal_c9f2
{
	void AccumulateAffectedTiles(FArcTerrainState& State, TConstArrayView<FArcTerrainBakeTileRef> Affected);
	void FlushTileInvalidations(AArcTerrain& Terrain, FArcTerrainState& State);
	void RebuildAndSeedAllTileRails(AArcTerrain& Terrain, FArcTerrainState& State);
	void DropRemovedAuthoredTiers(AArcTerrain& Terrain, FArcTerrainState& State, EArcTerrainRail Rail);
}

namespace ArcTerrainStampGraph_8b21
{
	void RemoveStampDependency(
		FArcTerrainState& State,
		const FArcTerrainBakeTileRef& Ref,
		UArcTerrainStampComponent& Stamp);
	void UpdateStampTileDependencies(
		FArcTerrainState& State,
		AArcTerrain& Terrain,
		FArcTerrainStampEntry& Entry,
		TConstArrayView<FArcTerrainBakeTileRef> OldCoverage,
		TConstArrayView<FArcTerrainBakeTileRef> NewCoverage,
		TArray<FArcTerrainBakeTileRef>& OutAffected);
}
#endif
