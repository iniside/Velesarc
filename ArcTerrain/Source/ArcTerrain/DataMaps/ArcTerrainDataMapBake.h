// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTerrainTypes.h"
#include "DataMaps/ArcTerrainDataMapTypes.h"
#include "Composite/ArcTerrainStampMask.h"

#if WITH_EDITOR

#include "Tiles/ArcTerrainSourceCache.h"

struct FArcTerrainState;
class UArcTerrainStampComponent;

/**
 * One data mask of one cell, as produced by the bake.
 *
 * Payload layout is the apron-inclusive tile every ArcTerrain per-cell raster uses: row-major
 * (Quads+3)^2, interior vertex (c, r) at (r+1)*(Quads+3)+(c+1) — the layout
 * ArcTerrainSurface::BilinearApron reads. The apron buys correct interior bilinear gradients;
 * it stitches across a cell boundary only where both cells resolved the same Quads.
 *
 * Dequantization: value = QuantMin + (Texel / 255) * (QuantMax - QuantMin). QuantMax == QuantMin
 * means the mask decodes flat at QuantMin and every byte is 0.
 *
 * The window is TERRAIN-WIDE (ArcTerrainDataMaps::GatherMaskWindows), never fitted to this tile's
 * values: two neighbouring tiles must quantize their shared apron texel at the same step for it to
 * decode bit-identically on both sides.
 */
struct FArcTerrainDataMapCellMask
{
	EArcTerrainDataMap Map = EArcTerrainDataMap::Deposits;

	/** Quads per tile edge; the payload is (Quads+3)^2. Resolved per mask from its contributors'
	 *  source density, so it matches neither the other masks nor the composite raster. Zero means
	 *  nothing was produced for this cell — either no contributor decoded, or every contributor's
	 *  footprint intersected the cell bounds but covered no tile texel; a reader must key emptiness
	 *  on Quads == 0, not on bUnresolved. */
	int32 Quads = 0;

	TArray<uint8> Texels;

	float QuantMin = 0.0f;
	float QuantMax = 0.0f;

	/** Tile-texel bounds written by at least one contributor; Max is EXCLUSIVE. Outside it no
	 *  contributor covered the cell and the byte is meaningless. Under a rotated footprint the rect
	 *  over-estimates: its corners read a value no contributor wrote. */
	FIntRect CoverageRect = FIntRect(0, 0, 0, 0);

	/** A contributing source failed to decode, so the fold is missing a contribution. A reader must
	 *  treat the mask as absent for this cell — never as a zero reading. */
	bool bUnresolved = false;
};

/** Every mask with at least one contributing modifier over one cell, in EArcTerrainDataMap order. */
struct FArcTerrainDataMapCellBake
{
	FArcTerrainCellKey Key = FArcTerrainCellKey::ZeroValue;

	TArray<FArcTerrainDataMapCellMask> Masks;
};

namespace ArcTerrainDataMaps
{
	/** One mask's quantization window, in the mask's own unit. Always contains zero. */
	struct FArcTerrainDataMapWindow
	{
		EArcTerrainDataMap Map = EArcTerrainDataMap::Deposits;
		float Min = 0.0f;
		float Max = 0.0f;
	};
}

struct ARCTERRAIN_API FArcTerrainDataMapBakeScope
{
	explicit FArcTerrainDataMapBakeScope(FArcTerrainSourceCache& InSourceCache)
		: SourceCache(InSourceCache)
	{
	}

	FArcTerrainSourceCache& SourceCache;

	TSet<FString> WarnedUnresolvedSources;

	/** EArcTerrainDataMap values already warned about for saturating their quantization window. */
	TSet<uint8> WarnedSaturatedMasks;
};

/** Immutable input to one data-map tile worker. It owns every value the raster reads, including the
 * decoded source buffers. UObject and source-cache access ends when GatherDataMapTileSnapshot returns. */
struct ARCTERRAIN_API FArcTerrainDataMapTileSnapshot
{
	struct FContributor
	{
		FArcTerrainSourceCpuFloatsPtr Source;
		FMatrix LocalToPatch = FMatrix::Identity;
		FVector2D CoverageXY = FVector2D::ZeroVector;
		FVector2D FootprintCm = FVector2D::ZeroVector;
		FVector2f UVScale = FVector2f::ZeroVector;
		FVector2f UVBias = FVector2f::ZeroVector;
		float EncodingScale = 1.0f;
		float ZeroInEncoding = 0.0f;
		bool bBlendDataMaps = false;
		EArcTerrainBlendMode BlendMode = EArcTerrainBlendMode::Additive;
		ArcTerrainStampMask::FMaskParams Mask;
		uint32 GeometryIdentity = 0;
	};

	struct FMaskWork
	{
		EArcTerrainDataMap Map = EArcTerrainDataMap::Deposits;
		TArray<FContributor> Contributors;
		bool bUnresolved = false;
	};

	FIntPoint TileKey = FIntPoint::ZeroValue;
	int32 TileQuads = 0;
	double TexelToLocal = 0.0;
	bool bBakeStats = false;
	double GatherSeconds = 0.0;
	double MaskWindowGatherSeconds = 0.0;
	uint32 DataWindowRevision = 0;
	TArray<ArcTerrainDataMaps::FArcTerrainDataMapWindow> MaskWindows;
	TArray<FMaskWork> Work;
};

struct ARCTERRAIN_API FArcTerrainDataMapBakeMaskStats
{
	EArcTerrainDataMap Map = EArcTerrainDataMap::Deposits;
	int32 Contributors = 0;
	int32 FusedMasks = 0;
	int64 RectAreaSum = 0;
	int32 CoveredTexels = 0;
	double AccumMs = 0.0;
	double QuantMs = 0.0;
};

/** Worker result plus the warnings that must be emitted by the game-thread pass owner. */
struct ARCTERRAIN_API FArcTerrainDataMapTileBakeResult
{
	FArcTerrainDataMapCellBake Bake;
	TArray<EArcTerrainDataMap> SaturatedMasks;
	uint32 DataWindowRevision = 0;
	bool bBakeStats = false;
	int32 Resolution = 0;
	int32 TexelCount = 0;
	double GatherSeconds = 0.0;
	double MaskWindowGatherSeconds = 0.0;
	double AccumSeconds = 0.0;
	double QuantSeconds = 0.0;
	double StatsSeconds = 0.0;
	double WorkerSeconds = 0.0;
	TArray<FArcTerrainDataMapBakeMaskStats> MaskStats;
};

namespace ArcTerrainDataMaps
{
	/** One mask's resolved raster, before and after the per-mask byte ceiling. */
	struct FArcTerrainDataMapQuadResolve
	{
		int32 Quads = 0;
		int32 UncappedQuads = 0;
		int64 MaxBytes = 0;
	};

	/**
	 * Densest contributor's texels/cm -> the mask's quad count, clamped, then capped by
	 * arc.Terrain.DataMaps.MaxBytesPerMaskPerCell. Zero or non-finite density falls back to the
	 * composite raster. Shared with the key so the stored payload and the key that names it never
	 * resolve different rasters.
	 */
	ARCTERRAIN_API FArcTerrainDataMapQuadResolve ResolveMaskQuads(const FArcTerrainConfig& Config,
		double BestTexelsPerCm);

	/**
	 * Per-mask quantization windows of the WHOLE terrain, ascending by mask, one entry per mask some
	 * enabled modifier assigns a texture to.
	 *
	 * Derived from the authored decode alone and from nothing any tile rasterized. A source texel spans
	 * [0, 1], so one contributor decodes into the interval bounded by (0 - ZeroInEncoding) *
	 * EncodingScale and (1 - ZeroInEncoding) * EncodingScale; replace, alpha-blend, min and max all fold
	 * to a value between the two they fold, so the hull of those intervals contains every folded value.
	 * The hull is widened to contain zero, which is what makes a texel no contributor wrote decode as a
	 * genuine zero reading.
	 *
	 * TERRAIN-WIDE BECAUSE OF THE SEAM: a window fitted per tile quantizes the texel two tiles share at
	 * two different steps, so the sample jumps as it crosses the tile boundary.
	 *
	 * ADDITIVE contributors stack past the hull, and a float-format source carries texels outside
	 * [0, 1]; neither has an authored bound, so those texels saturate at the window and BakeDataMapTile
	 * warns once per pass per mask.
	 *
	 * GAME THREAD ONLY — reads the registered stamp set.
	 */
	ARCTERRAIN_API void GatherMaskWindows(const FArcTerrainState& State,
		TArray<FArcTerrainDataMapWindow>& OutWindows);

	ARCTERRAIN_API bool RefreshDataMaskWindows(FArcTerrainState& State, double& OutGatherSeconds);

	/** The returned view is backed by State.DataMaskWindows. */
	ARCTERRAIN_API TConstArrayView<FArcTerrainDataMapWindow> GetOrGatherMaskWindows(
		FArcTerrainState& State, double& OutGatherSeconds);

	/** State of arc.Terrain.DataMaps.BakeStats. GAME THREAD. */
	ARCTERRAIN_API bool IsBakeStatsEnabled();

	/** GAME THREAD ONLY. Resolves and pins source buffers, then deep-owns all raster parameters. */
	ARCTERRAIN_API void GatherDataMapTileSnapshot(
		const FTransform& TerrainTransform,
		FIntPoint TileKey,
		TConstArrayView<UArcTerrainStampComponent* const> SortedStamps,
		TConstArrayView<FArcTerrainDataMapWindow> MaskWindows,
		uint32 DataWindowRevision,
		double MaskWindowGatherSeconds,
		int32 TileQuads,
		double TexelToLocal,
		FArcTerrainDataMapBakeScope& Scope,
		FArcTerrainDataMapTileSnapshot& OutSnapshot);

	/** Pure worker raster. It touches neither UObject state nor the source cache and emits no logs. */
	ARCTERRAIN_API FArcTerrainDataMapTileBakeResult BakeDataMapTileSnapshot(
		const FArcTerrainDataMapTileSnapshot& Snapshot);

	/** GAME THREAD ONLY. Maintains once-per-pass saturation warnings after a worker result lands. */
	ARCTERRAIN_API void ReportDataMapTileBakeWarnings(const FArcTerrainDataMapTileBakeResult& Result,
		TConstArrayView<FArcTerrainDataMapWindow> MaskWindows, FArcTerrainDataMapBakeScope& Scope);

	/** GAME THREAD ONLY. Emits the existing BakeStats payload after the worker has returned. */
	ARCTERRAIN_API void ReportDataMapTileBakeStats(const FArcTerrainDataMapTileBakeResult& Result);

	/** Exact retained source bytes for one snapshot, counting each shared decoded buffer once. */
	ARCTERRAIN_API int64 SnapshotSourceBytes(const FArcTerrainDataMapTileSnapshot& Snapshot);
}

#endif // WITH_EDITOR
