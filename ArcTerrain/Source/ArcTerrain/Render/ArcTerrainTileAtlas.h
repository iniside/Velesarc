// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTerrainTileUpload.h"
#include "ArcTerrainTypes.h"
#include "Misc/Optional.h"
#include "Render/ArcTerrainDensityTiers.h"
#include "Runtime/ArcTerrainRuntimeWorkCoordinator.h"
#include "RHIFwd.h"
#include "SpanAllocator.h"
#include "Templates/RefCounting.h"

class FRHICommandListImmediate;
class FTextureRenderTargetResource;
struct IPooledRenderTarget;

/**
 * Runtime-only sector identity: a density tier plus FArcTerrainCellKey >> that tier's cell shift. Never
 * persisted, never an asset, no build key and no entry in any AArcTerrain registry — a sector exists only
 * for as long as it holds a slice of the tile atlas. The tier is part of the identity because two tiers
 * produce the SAME FIntPoint over different areas of the world.
 */
struct FArcTerrainSectorId
{
	int32 Tier = 0;

	FIntPoint Key = FIntPoint::ZeroValue;

	bool operator==(const FArcTerrainSectorId& Other) const
	{
		return Tier == Other.Tier && Key == Other.Key;
	}

	bool operator!=(const FArcTerrainSectorId& Other) const { return !(*this == Other); }
};

inline uint32 GetTypeHash(const FArcTerrainSectorId& SectorId)
{
	return HashCombineFast(GetTypeHash(SectorId.Key), static_cast<uint32>(SectorId.Tier));
}

/**
 * Runtime-only weight-group identity: a weight tier plus FArcTerrainCellKey >> that tier's SliceShift. The
 * tier is part of the identity for the same reason it is on FArcTerrainSectorId — two tiers produce the
 * SAME FIntPoint over different areas of the world.
 */
struct FArcTerrainWeightGroupId
{
	int32 Tier = 0;

	FIntPoint Key = FIntPoint::ZeroValue;

	bool operator==(const FArcTerrainWeightGroupId& Other) const
	{
		return Tier == Other.Tier && Key == Other.Key;
	}

	bool operator!=(const FArcTerrainWeightGroupId& Other) const { return !(*this == Other); }
};

inline uint32 GetTypeHash(const FArcTerrainWeightGroupId& GroupId)
{
	return HashCombineFast(GetTypeHash(GroupId.Key), static_cast<uint32>(GroupId.Tier));
}

namespace ArcTerrainAtlas
{
	/** Cell -> sector / weight-group key. Arithmetic right shift == floor division, so the mapping is
	 *  monotone across the origin (cell -1 belongs to sector -1, not sector 0). */
	inline FIntPoint GroupKeyFromCell(FArcTerrainCellKey Key, int32 Shift)
	{
		return FIntPoint(Key.X >> Shift, Key.Y >> Shift);
	}
}

/**
 * Resolved geometry of both atlas arrays. TWO ARRAYS WITH INDEPENDENT GRANULARITY, deliberately:
 *
 *  - height: PF_R32_FLOAT, one slice per SECTOR, side (HeightQuads << SectorShift) + 3;
 *  - weight: PF_B8G8R8A8, one slice per CELL by default, side (WeightQuads << WeightSliceShift) + 3,
 *    NumWeightArrays = ceil(NumChannels / 4) parallel arrays with the terrain-global channel mapping
 *    (array = ChannelIndex / 4, component = ChannelIndex % 4).
 *
 * The weight raster is ~4x denser than the height raster (ResolveWeightQuads floors WeightQuads at
 * HeightQuads), so a sector-sized weight slice costs ~16x a height slice per array. The height slice
 * index is therefore NOT reusable as the weight slice index: at the default WeightSliceShift = 0 that
 * would make 15 of a sector's 16 cells sample another cell's weights.
 *
 * RESIDENCY IS SPLIT TOO, not only the layout: the two arrays are sized by INDEPENDENT budgets
 * (MaxResidentSectors, MaxResidentWeightCells) and a weight slice has its own lifetime. Sizing the
 * weight array as a multiple of the sector budget made every metre of height range cost the weight
 * raster's 16x price per unit area, so raising the height radius was unaffordable.
 *
 * Each array carries ONE RESERVED FILL SLICE past the allocatable range (GetHeightFillSlice /
 * GetWeightFillSlice), held outside FSpanAllocator. A residency lookup that misses — or that lands
 * outside its ring — resolves to it, which is what makes "no data" stop being a state the renderer has
 * to branch on: the null-height slice reads BaseHeightCm.
 */
struct ARCTERRAIN_API FArcTerrainTileAtlasLayout
{
	/** Cells per sector edge = 1 << SectorShift. */
	int32 SectorShift = 2;

	/** Cells per weight-slice edge = 1 << WeightSliceShift. Resolved to at most SectorShift. */
	int32 WeightSliceShift = 0;

	int32 HeightQuads = 0;
	int32 WeightQuads = 0;
	int32 NumWeightArrays = 0;

	/** Array whose clear value carries the base lane at 1, INDEX_NONE when the channel count cannot address
	 *  the base channel — every array then clears to zero, so no lane claims an unowned channel. Part of the
	 *  layout because it is baked into the array's FClearValueBinding. */
	int32 BaseWeightArrayIndex = INDEX_NONE;

	int32 MaxResidentSectors = 0;

	/** Weight-slice budget, resolved from arc.Terrain.MaxResidentWeightCells. Deliberately NOT a function
	 *  of MaxResidentSectors — the weight raster is 16x denser per unit area, so tying the two makes the
	 *  height radius cost weight-raster money. */
	int32 MaxResidentWeightCells = 0;

	/** FArcTerrainHeightOptions::BaseHeightCm — the value a height slice is cleared to. It is part of
	 *  the layout because it is baked into the array's FClearValueBinding, so a change has to rebuild
	 *  the arrays rather than only re-clear them. */
	float BaseHeightCm = 0.0f;

	int32 GetHeightSliceSide() const { return (HeightQuads << SectorShift) + 3; }
	int32 GetWeightSliceSide() const { return (WeightQuads << WeightSliceShift) + 3; }

	/** log2 of the weight groups along one sector edge — the group key of a cell shifted down by this is
	 *  the key of the sector that contains it. */
	int32 GetWeightGroupShift() const { return FMath::Max(SectorShift - WeightSliceShift, 0); }

	/** Slices FSpanAllocator may issue. The array is one larger; the extra index is the fill slice. */
	int32 GetHeightAllocatableSlices() const { return MaxResidentSectors; }
	int32 GetWeightAllocatableSlices() const { return MaxResidentWeightCells; }

	int32 GetHeightFillSlice() const { return GetHeightAllocatableSlices(); }
	int32 GetWeightFillSlice() const { return GetWeightAllocatableSlices(); }

	int32 GetHeightArraySize() const { return GetHeightAllocatableSlices() + 1; }
	int32 GetWeightArraySize() const { return GetWeightAllocatableSlices() + 1; }

	/** log2 of the height texels along one SECTOR edge. HeightQuads is a power of two (CellExtent and
	 *  TexelSize are both 2^k times 100 cm), so SectorKey = GlobalTexel >> this. */
	int32 GetSectorQuadsShift() const { return FMath::FloorLog2(static_cast<uint32>(FMath::Max(HeightQuads, 1))) + SectorShift; }

	/** log2 of the HEIGHT texels along one weight-slice edge — the weight raster has its own density, but
	 *  the residency key has to be derived from the same global texel index the height path uses. */
	int32 GetWeightQuadsShift() const { return FMath::FloorLog2(static_cast<uint32>(FMath::Max(HeightQuads, 1))) + WeightSliceShift; }

	bool IsValid() const;

	bool operator==(const FArcTerrainTileAtlasLayout& Other) const;
	bool operator!=(const FArcTerrainTileAtlasLayout& Other) const { return !(*this == Other); }
};

namespace ArcTerrainAtlas
{
	/** Origin, in texels of its SECTOR's height slice, of one cell's apron-inclusive tile. Neighbouring
	 *  cells' tiles overlap by 3 texels; the shared texels carry the same value from either composite,
	 *  which is what makes the overlapping writes order-independent. */
	inline FIntPoint HeightTileOriginInSlice(FArcTerrainSectorId SectorId, FArcTerrainCellKey Key,
		const FArcTerrainTileAtlasLayout& Layout)
	{
		if (SectorId.Tier != 0)
		{
			return FIntPoint::ZeroValue;
		}
		return FIntPoint(
			(Key.X - (SectorId.Key.X << Layout.SectorShift)) * Layout.HeightQuads,
			(Key.Y - (SectorId.Key.Y << Layout.SectorShift)) * Layout.HeightQuads);
	}

	/** Bytes of one PF_R32_FLOAT height slice. */
	inline int64 HeightSliceBytes(int64 SliceSide)
	{
		return SliceSide * SliceSide * 4;
	}

	/** Bytes of one weight slice across the NumWeightArrays parallel PF_B8G8R8A8 arrays. */
	inline int64 WeightSliceBytes(int64 SliceSide, int64 NumWeightArrays)
	{
		return SliceSide * SliceSide * 4 * NumWeightArrays;
	}

	/** Allocatable slices whose whole array — allocatable plus the reserved fill slice — stays within
	 *  BudgetBytes. 0 means not even one allocatable slice fits alongside the fill slice. */
	inline int32 MaxAllocatableSlicesWithinBudget(int64 SliceBytes, int64 BudgetBytes)
	{
		if (SliceBytes <= 0)
		{
			return MAX_int32;
		}
		const int64 ArraySlices = BudgetBytes / SliceBytes;
		return static_cast<int32>(
			FMath::Clamp(ArraySlices - 1, static_cast<int64>(0), static_cast<int64>(MAX_int32)));
	}

	/** Upper bound of the terrain's resolved weight raster from Config alone — the stamp-source-dimension
	 *  cap in ArcTerrainSubsystemPrivate::ResolveWeightQuads only ever lowers it. */
	inline int32 WeightQuadsUpperBound(int32 HeightQuads, int32 AuthoredCellWeightQuads)
	{
		return FMath::Clamp(AuthoredCellWeightQuads, HeightQuads, ArcTerrain::MaxWeightSliceSide - 3);
	}

	/** arc.Terrain.Atlas.HeightBudgetMB / arc.Terrain.Atlas.WeightBudgetMB in bytes. */
	ARCTERRAIN_API int64 GetHeightBudgetBytes();
	ARCTERRAIN_API int64 GetWeightBudgetBytes();

	/**
	 * Resolve + VALIDATE the layout. The dimension check spans BOTH arrays, because the weight side is
	 * normally the larger one; on an overflow the offending shift is lowered automatically with a
	 * one-shot warning, never a check() in the middle of a frame. MaxResidentSectors and
	 * MaxResidentWeightCells may also come back LOWERED (array-layer ceiling, applied to each budget
	 * SEPARATELY — the weight arrays are parallel, so the ceiling is per array; then the byte ceiling of
	 * arc.Terrain.Atlas.HeightBudgetMB / WeightBudgetMB) — callers must trim residency by the resolved
	 * values, not by the requested ones, or eviction runs backwards. Both come back 0 — an invalid layout
	 * that allocates nothing — when a single slice plus the fill slice is already over its budget.
	 */
	ARCTERRAIN_API FArcTerrainTileAtlasLayout ResolveTileAtlasLayout(int32 HeightQuads, int32 WeightQuads,
		int32 NumChannels, int32 MaxResidentSectors, int32 MaxResidentWeightCells, int32 DesiredSectorShift,
		int32 DesiredWeightSliceShift, float BaseHeightCm, bool bEmitWarnings = true);
}

/**
 * One residency lookup table: a square window of atlas slice indices over integer keys, row-major, with
 * every key that is not resident holding FillSlice.
 *
 * This is what replaces "the face id IS the slice". A leaf resolves an integer key from its global texel
 * index, offsets it against Center, and reads a slice — so a face may span any number of resident
 * sectors and any number of unresident ones. THE RANGE TEST IS MANDATORY, not defensive: an
 * out-of-bounds Buffer<uint> read returns 0 on D3D, which is a perfectly valid slice index, so a lookup
 * without it would carpet the far field in sector 0's terrain.
 */
struct FArcTerrainResidencyRing
{
	/** (2 * Radius + 1)^2 slice indices, row-major from (Center - Radius). */
	TArray<uint32> Slices;

	FIntPoint Center = FIntPoint::ZeroValue;

	/** Half-width in keys. NEGATIVE means the window is empty and every lookup returns FillSlice. */
	int32 Radius = -1;

	uint32 FillSlice = 0;

	/** The tier's ladder entry: the rings are the only vehicle the ladder has onto the render thread. */
	int32 CumulativeShift = 0;

	int32 GetSide() const { return Radius >= 0 ? 2 * Radius + 1 : 0; }

	bool operator==(const FArcTerrainResidencyRing& Other) const
	{
		return Center == Other.Center && Radius == Other.Radius && FillSlice == Other.FillSlice
			&& CumulativeShift == Other.CumulativeShift && Slices == Other.Slices;
	}
};

/** The rings a leaf consults: one height window PER TIER and one weight window PER TIER, each indexed by
 *  its own rail's tier. The keys are INDEPENDENT — a height tier keys on
 *  GlobalTexel >> (BaseSectorQuadsShift + CumulativeShift) and a weight tier on
 *  GlobalTexel >> (BaseWeightQuadsShift + CumulativeShift), and the two ladders may differ in length. */
struct FArcTerrainResidencyRings
{
	TArray<FArcTerrainResidencyRing, TInlineAllocator<ArcTerrain::MaxTiers>> HeightTiers;
	TArray<FArcTerrainResidencyRing, TInlineAllocator<ArcTerrain::MaxTiers>> Weight;

	bool operator==(const FArcTerrainResidencyRings& Other) const
	{
		return HeightTiers == Other.HeightTiers && Weight == Other.Weight;
	}
};

namespace ArcTerrainAtlas
{
	/** Per-tier scalars indexing the CONCATENATED height-ring buffer. */
	struct FArcTerrainHeightRingScalars
	{
		FUintVector4 Offset = FUintVector4(0u, 0u, 0u, 0u);
		FIntVector4 Radius = FIntVector4(-1, -1, -1, -1);
		FIntVector4 CenterX = FIntVector4(0, 0, 0, 0);
		FIntVector4 CenterY = FIntVector4(0, 0, 0, 0);
		FIntVector4 CumulativeShift = FIntVector4(0, 0, 0, 0);
		uint32 NumTiers = 0;
		uint32 FillSlice = 0;
	};

	ARCTERRAIN_API void PackHeightRings(TConstArrayView<FArcTerrainResidencyRing> Rings,
		TArray<uint32>& OutSlices, FArcTerrainHeightRingScalars& OutScalars);

	/** Per-tier scalars indexing the CONCATENATED weight-ring buffer. */
	struct FArcTerrainWeightRingScalars
	{
		FUintVector4 Offset = FUintVector4(0u, 0u, 0u, 0u);
		FIntVector4 Radius = FIntVector4(-1, -1, -1, -1);
		FIntVector4 CenterX = FIntVector4(0, 0, 0, 0);
		FIntVector4 CenterY = FIntVector4(0, 0, 0, 0);
		FIntVector4 CumulativeShift = FIntVector4(0, 0, 0, 0);
		uint32 NumTiers = 0;
		uint32 FillSlice = 0;
	};

	ARCTERRAIN_API void PackWeightRings(TConstArrayView<FArcTerrainResidencyRing> Rings,
		TArray<uint32>& OutSlices, FArcTerrainWeightRingScalars& OutScalars);
}

/** One SOURCE's contribution to a fill pass, fully resolved on the game thread: one cell of the sector at
 *  tier 0, or the one coarse tile covering the whole sector above it. */
struct FArcTerrainAtlasCellFill
{
	enum class EHeightWrite : uint8
	{
		None,
		Tier0Cell,
		CoarseWholeSlice
	};

	FArcTerrainCellKey Key = FArcTerrainCellKey::ZeroValue;

	int32 HeightSlice = INDEX_NONE;
	FIntPoint HeightDestOrigin = FIntPoint::ZeroValue;
	int32 HeightTileRes = 0;
	EHeightWrite HeightWrite = EHeightWrite::None;
	FVector2f HeightRange = FVector2f(TNumericLimits<float>::Max(), -TNumericLimits<float>::Max());

	/** The source's rect of the loaded height tile, HeightTileRes^2 float32 tightly packed (terrain-local
	 *  cm). Cut on the game thread; the render thread stages it and writes BakedHeightRHI. Empty means the
	 *  source has no tile, which is also what tells the caller not to stamp the slice as holding one. */
	TArray<uint8> HeightStagingBytes;

	/** PF_R32_FLOAT, terrain-local cm: the staging texture the render thread fills from
	 *  HeightStagingBytes. A cell that ever composited writes its own slice sub-rect from
	 *  RenderCompositeCell instead. */
	FTextureRHIRef BakedHeightRHI;

	int32 WeightSlice = INDEX_NONE;
	FIntPoint WeightDestOrigin = FIntPoint::ZeroValue;
	int32 WeightTileRes = 0;

	/** Live BGRA8 weight tiles, indexed by atlas array (they already carry the terrain-global
	 *  channel/4, channel%4 packing), or empty. */
	TArray<FTextureRenderTargetResource*> LiveWeightResources;

	/** The cell's sub-rect of each plane of the loaded weight tile, WeightTileRes^2 bytes each, in the
	 *  order BakedWeightSliceChannels names. */
	TArray<TArray<uint8>> WeightStagingPlanes;

	/** PF_G8, one slice per channel PRESENT in this cell: the staging array the render thread fills from
	 *  WeightStagingPlanes. */
	FTextureRHIRef BakedWeightRHI;

	/** Source slice -> GLOBAL channel index, resolved from the plane's channel-name hash against the
	 *  terrain's CURRENT channel set (CutCellWeightPlanes) — never a value read straight out of the file. */
	TArray<int32> BakedWeightSliceChannels;
};

/** One frame's worth of atlas writes for one terrain. */
struct FArcTerrainAtlasFillJob
{
	FArcTerrainTileAtlasLayout Layout;

	/** Slices whose whole area is cleared before the cell writes — a freshly allocated slice only. A
	 *  cleared height texel holds Layout.BaseHeightCm, which IS what an unstamped cell wants. */
	TArray<int32> HeightSlicesToClear;
	TArray<int32> WeightSlicesToClear;

	TArray<FArcTerrainAtlasCellFill> Cells;

	/** Pooled staging textures the cells above read from. The job is destroyed by the render command that
	 *  ran ExecuteFill, so holding them here is what keeps them out of the pool until GraphBuilder.Execute()
	 *  has performed the copies that read them. A job carrying staging is therefore move-only. */
	FArcTerrainStagingTextures Staging;

	TArray<FArcTerrainRuntimeReservationToken> ReservationTokens;

	bool IsEmpty() const { return Cells.IsEmpty() && HeightSlicesToClear.IsEmpty() && WeightSlicesToClear.IsEmpty(); }
};

/** RENDER-THREAD STATE. Created on first fill and rebuilt whenever the layout changes. */
struct FArcTerrainTileAtlasResources
{
	enum class EHeightBoundsMode : uint8
	{
		Tier0Contributors,
		CoarseWholeSlice
	};

	FArcTerrainTileAtlasLayout Layout;
	TRefCountPtr<IPooledRenderTarget> HeightAtlas;
	TArray<TRefCountPtr<IPooledRenderTarget>> WeightAtlases;

	/** Render-thread CPU mirror. A finite reversed range requires the actor-wide fallback. */
	TArray<FVector2f> HeightSliceBounds;
	TArray<EHeightBoundsMode> HeightSliceBoundsModes;
	TArray<FVector2f> Tier0HeightCellBounds;
	TBitArray<> DirtyHeightSliceBounds;
};

namespace ArcTerrainTileAtlasRender
{
	/** A finite reversed range is the one invalid/global-fallback sentinel used by atlas metadata. */
	ARCTERRAIN_API FVector2f InvalidHeightRange();

	/** Render-thread metadata mutations. RecomputeHeightSliceBounds folds every dirty tier-0 slice once. */
	ARCTERRAIN_API void ResetHeightSliceBounds(FArcTerrainTileAtlasResources& Resources, int32 SliceIndex);
	ARCTERRAIN_API void ReplaceTier0HeightCellBounds(FArcTerrainTileAtlasResources& Resources,
		int32 SliceIndex, FArcTerrainCellKey CellKey, FVector2f HeightRange);
	ARCTERRAIN_API void ReplaceCoarseHeightSliceBounds(FArcTerrainTileAtlasResources& Resources,
		int32 SliceIndex, FVector2f HeightRange);
	ARCTERRAIN_API void InvalidateHeightCellBounds(FArcTerrainTileAtlasResources& Resources,
		int32 SliceIndex, FArcTerrainCellKey CellKey);
	ARCTERRAIN_API void RecomputeHeightSliceBounds(FArcTerrainTileAtlasResources& Resources);

	/** RENDER THREAD, before ExecuteFill: turns every cell's staged bytes into a pooled staging texture
	 *  and points the cell's Baked*RHI at it. Creation and the lock-write cannot happen on the game
	 *  thread, and the textures are adopted by Job.Staging so they outlive the graph that reads them. */
	ARCTERRAIN_API void ResolveStagingTextures(FRHICommandListImmediate& RHICmdList,
		FArcTerrainAtlasFillJob& Job);

	/** Render-thread entry: (re)create the arrays for Job.Layout, clear the newly allocated slices, then
	 *  write every cell. Pattern: IESTextureManager (MultiFrame RDG texture + ConvertToExternalTexture).
	 *  bLogVerbose is ArcTerrainDiag::IsWeightCompositeVerbose() READ ON THE GAME THREAD by the caller and
	 *  carried in — the cvar itself is game-thread-only and this runs on the render thread. */
	ARCTERRAIN_API void ExecuteFill(FRHICommandListImmediate& RHICmdList,
		const TSharedRef<FArcTerrainTileAtlasResources, ESPMode::ThreadSafe>& Resources,
		const FArcTerrainAtlasFillJob& Job, bool bLogVerbose);
}

/**
 * GAME-THREAD residency registry of one terrain's tile atlas: sector -> height slice, weight group ->
 * weight slice, plus the refusal policy.
 *
 * WHY A REFUSAL POLICY IS LOAD-BEARING: FSpanAllocator::Allocate never fails, it grows and hands back
 * an index past the end of the array. Every allocation here is therefore gated on the live allocation count against
 * ArraySize; on a full atlas the farthest registration that is unpinned, farther than the candidate and
 * not of a finer tier is evicted, and when none qualifies the sector is refused (it simply does not draw)
 * with a one-shot warning.
 */
class ARCTERRAIN_API FArcTerrainTileAtlas
{
public:
	FArcTerrainTileAtlas();
	~FArcTerrainTileAtlas();

	/** Adopt a layout. A CHANGE drops every registration and the GPU arrays — slice indices are only
	 *  meaningful against the layout they were allocated under. */
	bool SetLayout(const FArcTerrainTileAtlasLayout& NewLayout);
	const FArcTerrainTileAtlasLayout& GetLayout() const { return Layout; }

	bool IsSectorAdmitted(FArcTerrainSectorId SectorId) const { return Sectors.Contains(SectorId); }

	int32 GetAdmittedSectorCount() const { return Sectors.Num(); }
	int32 FindHeightSlice(FArcTerrainSectorId SectorId) const;

	/** An admitted sector whose allocation was refused carries INDEX_NONE, so admitted alone would publish slice -1. */
	bool TryGetReadyHeightSlice(FArcTerrainSectorId SectorId, int32& OutSlice) const;
	int32 FindWeightSlice(FArcTerrainWeightGroupId GroupId) const;

	uint32 GetSectorHeightTileFingerprint(FArcTerrainSectorId SectorId) const;
	void SetSectorHeightTileFingerprint(FArcTerrainSectorId SectorId, uint32 Fingerprint);
	bool MarkSectorRingReady(FArcTerrainSectorId SectorId);
	bool IsSectorRingReady(FArcTerrainSectorId SectorId) const;
	bool IsWeightGroupRingReady(FArcTerrainWeightGroupId GroupId) const;
	bool IsSectorAwaitingTile(FArcTerrainSectorId SectorId) const;
	bool IsWeightGroupAwaitingTile(FArcTerrainWeightGroupId GroupId) const;
	void SetSectorAwaitingTile(FArcTerrainSectorId SectorId, bool bAwaiting);
	void SetWeightGroupAwaitingTile(FArcTerrainWeightGroupId GroupId, bool bAwaiting);

	uint32 GetWeightGroupTileFingerprint(FArcTerrainWeightGroupId GroupId) const;
	void SetWeightGroupTileFingerprint(FArcTerrainWeightGroupId GroupId, uint32 Fingerprint);
	bool MarkWeightGroupRingReady(FArcTerrainWeightGroupId GroupId);

	/**
	 * Snapshot the whole residency as the lookup windows the renderer indexes per leaf. Both rails
	 * (the CBT base mesh feeding the predicate and the render bindings feeding the vertex factory) take
	 * the SAME snapshot, so they cannot disagree about which sector a leaf belongs to.
	 */
	void BuildResidencyRings(const FArcTerrainTierLadder& Ladder, const FArcTerrainWeightLadder& WeightLadder,
		FArcTerrainResidencyRings& OutRings) const;

	/** Refresh the eviction priority of an admitted sector. */
	void UpdateSectorPriority(FArcTerrainSectorId SectorId, double DistSq, bool bPinned);

	struct FHeightAdmissionResult
	{
		bool bAdmitted = false;
		int32 AssignedSlice = INDEX_NONE;
		uint64 SliceGeneration = 0;
		TOptional<FArcTerrainSectorId> EvictedId;
		uint64 EvictedSliceGeneration = 0;
	};

	FHeightAdmissionResult AdmitSectorTracked(FArcTerrainSectorId Id, double DistSq, bool bPinned);

	bool AdmitSector(FArcTerrainSectorId SectorId, double DistSq, bool bPinned);

	void EvictSector(FArcTerrainSectorId SectorId);

	uint64 GetHeightSliceGeneration(FArcTerrainSectorId SectorId) const;
	uint64 GetResourceGeneration() const { return ResourceGeneration; }

	/** Admitted sector ids (unordered). */
	void GetAdmittedSectors(TArray<FArcTerrainSectorId>& OutSectors) const;


	bool IsWeightGroupAdmitted(FArcTerrainWeightGroupId GroupId) const { return WeightSliceByGroup.Contains(GroupId); }

	struct FWeightAdmissionResult
	{
		bool bAdmitted = false;
		int32 AssignedSlice = INDEX_NONE;
		uint64 SliceGeneration = 0;
		TOptional<FArcTerrainWeightGroupId> EvictedId;
		uint64 EvictedSliceGeneration = 0;
	};

	FWeightAdmissionResult AdmitWeightGroupTracked(FArcTerrainWeightGroupId Id, double DistSq, bool bPinned);

	bool AdmitWeightGroup(FArcTerrainWeightGroupId GroupId, double DistSq, bool bPinned);

	void EvictWeightGroup(FArcTerrainWeightGroupId GroupId);

	uint64 GetWeightSliceGeneration(FArcTerrainWeightGroupId GroupId) const;

	/** Refresh the eviction priority of an admitted weight group. */
	void UpdateWeightGroupPriority(FArcTerrainWeightGroupId GroupId, double DistSq, bool bPinned);

	/** Admitted weight-group ids (unordered). */
	void GetAdmittedWeightGroups(TArray<FArcTerrainWeightGroupId>& OutGroups) const;

	int32 GetAdmittedWeightGroupCount() const { return WeightSliceByGroup.Num(); }

	/** DIAGNOSTICS ONLY: which of AdmitWeightGroup's refusal clauses a group currently trips, evaluated
	 *  against the same live state that function reads — CandidateDistSq included, or the distance clause
	 *  would report a refusal as "not admitted". Returns the granted case too, so a caller can log an
	 *  outcome for every group without a second derivation. */
	FString DescribeWeightAdmission(FArcTerrainWeightGroupId GroupId, double CandidateDistSq) const;


	/** Sector containing a weight group. Arithmetic right shift == floor division, so it is monotone
	 *  across the origin. */
	FArcTerrainSectorId SectorKeyFromWeightGroup(FIntPoint GroupKey) const
	{
		const int32 GroupShift = Layout.GetWeightGroupShift();
		return FArcTerrainSectorId{ 0, FIntPoint(GroupKey.X >> GroupShift, GroupKey.Y >> GroupShift) };
	}

	/** The GPU arrays, or null before the first fill. Render-thread contents — the game thread may hold
	 *  the handle but must not touch the pooled targets. */
	const TSharedPtr<FArcTerrainTileAtlasResources, ESPMode::ThreadSafe>& GetResources() const { return Resources; }

	/** Drop every registration and release the GPU arrays. */
	void ReleaseAll();

	/** Hand a fill job to the render thread. An empty job still goes through until the arrays exist —
	 *  they are a function of the LAYOUT, because they carry the fill slices. */
	void EnqueueFill(FArcTerrainAtlasFillJob&& Job);

private:
	struct FSectorRegistration
	{
		int32 HeightSlice = INDEX_NONE;
		uint64 SliceGeneration = 0;
		double DistSq = 0.0;
		bool bPinned = false;
		/** 0 on a fresh registration, so a recycled slice never claims to hold the tiles its predecessor
		 *  did. */
		uint32 HeightTileFingerprint = 0;
		// Unfilled must publish as the fill-slice hole so the shader falls through to the coarser tier.
		bool bRingReady = false;
		bool bAwaitingTile = false;
	};

	/** One weight group's slice and its own eviction priority — the group's lifetime, not the sector's. */
	struct FWeightGroupRegistration
	{
		int32 Slice = INDEX_NONE;
		uint64 SliceGeneration = 0;
		double DistSq = 0.0;
		bool bPinned = false;
		/** 0 on a fresh registration, so a recycled slice never claims to hold the tiles its predecessor
		 *  did. */
		uint32 WeightTileFingerprint = 0;
		bool bRingReady = false;
		bool bAwaitingTile = false;
	};

	uint64 BumpHeightSliceGeneration(int32 Slice);
	uint64 BumpWeightSliceGeneration(int32 Slice);

	/**
	 * Farthest admitted sector that is unpinned, strictly farther than the candidate, and not of a tier
	 * FINER than the candidate's — false when none qualifies. Both extra clauses live HERE and not at the
	 * caller: AdmitSector re-queries in a loop, so rejecting a returned victim without evicting it hands
	 * back the same victim forever.
	 */
	bool FindEvictionVictim(int32 CandidateTier, double CandidateDistSq, FArcTerrainSectorId& OutVictim) const;

	/**
	 * Farthest admitted weight group that is unpinned, strictly farther than the candidate, and not of a
	 * tier FINER than the candidate's — false when none qualifies. Both extra clauses live HERE and not at
	 * the caller, for the reason FindEvictionVictim gives.
	 */
	bool FindWeightEvictionVictim(int32 CandidateTier, double CandidateDistSq,
		FArcTerrainWeightGroupId& OutVictim) const;

	FArcTerrainTileAtlasLayout Layout;

	TMap<FArcTerrainSectorId, FSectorRegistration> Sectors;
	TMap<FArcTerrainWeightGroupId, FWeightGroupRegistration> WeightSliceByGroup;

	FSpanAllocator HeightSliceAllocator;
	FSpanAllocator WeightSliceAllocator;
	TArray<uint64> HeightSliceGenerations;
	TArray<uint64> WeightSliceGenerations;
	uint64 ResourceGeneration = 0;

	/** Render-thread state, kept alive independently of this object's teardown. */
	TSharedPtr<FArcTerrainTileAtlasResources, ESPMode::ThreadSafe> Resources;

	/** One-shot: a sector admission found no evictable slot. */
	bool bWarnedSectorRefused = false;

	/** One-shot: a weight-group admission found no evictable slot. */
	bool bWarnedWeightGroupRefused = false;

	/** One-shot PER TIER and per terrain: a shared latch lets the first clamp silence the sensor for every
	 *  other tier and every other terrain, which is exactly the multi-terrain case it exists to catch. */
	mutable bool bWarnedHeightRingClamped[ArcTerrain::MaxTiers] = {};
	mutable bool bWarnedWeightRingClamped[ArcTerrain::MaxTiers] = {};
};
