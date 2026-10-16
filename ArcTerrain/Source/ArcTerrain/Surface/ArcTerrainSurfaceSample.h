// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IO/IoHash.h"

// Pure CPU surface cache for ArcTerrain sampling. Deliberately free of editor-only and GPU
// includes: callers populate it from a cell's sub-rect of the BAKED HEIGHT TILE
// (ArcTerrainTileAccess::LoadCellHeightsSync) or, where nothing shaped the cell, from a constant tile
// at the terrain's BaseHeightCm; the cache itself (and the samplers below) are plain
// arrays + math, so editor, PIE, cooked and tests all run the one code path.
//
// Tile layout (the cell sub-rect of the on-disk tile): row-major (Quads+3)^2 with a 1-texel apron on
// every side. Interior vertex (c, r), c,r in [0, Quads], lives at index (r+1)*(Quads+3)+(c+1).
//
// COMPOSITE-RASTER caveat: the baked tile is on the terrain's COMPOSITE raster
// (TileRes == CompositeQuads + 3). RenderQuads below is therefore the tile's own quad count
// (TileRes - 3); sampling resolution is the composite's.
//
// NO LAYER WEIGHTS. Material layer weights are deliberately absent from this cache and are never
// sampled on the CPU: weights are consumed GPU-side by a separate node.
struct FArcTerrainSurfaceCellCache
{
	FIntPoint Key = FIntPoint::ZeroValue;

	/** Quads per edge of the tile below == TileRes - 3 (the COMPOSITE raster, not the mesh's). */
	int32 RenderQuads = 0;

	/** Row-major (RenderQuads+3)^2 heights, terrain-local Z in cm, 1-texel apron per side. */
	TArray<float> HeightsWithApron;

	/** Same layout as HeightsWithApron; precomputed finite-difference normals, terrain-local. */
	TArray<FVector3f> NormalsWithApron;
};

struct FArcTerrainSurfaceCache
{
	double CellSize = 0.0;
	int32 QuadsPerCell = 0;

	/** Max-EXCLUSIVE on both axes. */
	FIntRect CellBounds;

	FTransform TerrainTransform = FTransform::Identity;

	FBox WorldBounds = FBox(ForceInit);

	/** Fold of per-cell baked height payloads (key, tile res, quantization window, texel bytes) plus
	 *  geometry; identity for consumers that need a content key. */
	FIoHash ContentHash;

	TArray<FArcTerrainSurfaceCellCache> Cells;

	/** Three cases: a key with a readable baked payload maps to its own entry; a key nothing shaped maps
	 *  to the shared flat entry; a key whose registered payload is unreadable is absent, and absent is a
	 *  hole. */
	TMap<FIntPoint, int32> KeyToIndex;
};

namespace ArcTerrainSurface
{
	/**
	 * Builds one cell's cache from an apron height tile in cm. Pure CPU: copies the height tile
	 * and precomputes finite-difference normals (apron neighbours make interior/edge normals exact
	 * and identical across adjacent cells).
	 *
	 * HeightsWithApron must be (RenderQuads+3)^2.
	 */
	ARCTERRAIN_API FArcTerrainSurfaceCellCache BuildCellCache(
		FIntPoint Key,
		const TArray<float>& HeightsWithApron,
		int32 RenderQuads,
		double CellSize);

	/**
	 * Builds the cache of a cell no stamp or modifier shapes: a tile constant at HeightCm, whose
	 * finite-difference normals are +Z everywhere. Carries no cell identity, so ONE entry may back every
	 * such key in a cache.
	 */
	ARCTERRAIN_API FArcTerrainSurfaceCellCache BuildFlatCellCache(
		float HeightCm,
		int32 RenderQuads,
		double CellSize);

	/**
	 * Bilinear sample of an apron tile at fractional cell coordinates (FracX, FracY in [0,1] across
	 * the cell). Grid is row-major (Quads+3)^2 with a 1-texel apron per side; interior vertex (c, r)
	 * is at (r+1)*(Quads+3)+(c+1). Exact at integer vertices (lerp weights degenerate to 0/1).
	 */
	ARCTERRAIN_API float BilinearApron(TConstArrayView<float> Grid, int32 Quads, float FracX, float FracY);

	/** Component-wise bilinear over an FVector3f apron tile (same layout as BilinearApron); NOT renormalized. */
	ARCTERRAIN_API FVector3f BilinearApronVec(TConstArrayView<FVector3f> Grid, int32 Quads, float FracX, float FracY);
}
