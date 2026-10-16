// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Box.h"

class UArcTerrainChannelSet;

/**
 * GPU-grass plan Step 2 — PURE CPU helpers behind UArcTerrainDataInterface.
 *
 * Deliberately free of every engine-render type (no FRDG*, no FRHI*, no FTextureRenderTargetResource):
 * this is the arithmetic the shader and the render-thread proxy MUST agree on, and a later
 * [test-author] step exercises it directly, which a render-thread-coupled header would make
 * impossible. Everything here is ARCPCGEXTENSION_API for exactly that reason.
 */

/** Namespace of every quantity below: TERRAIN-LOCAL cm (positions) / TEXELS of the source tile
 *  (origins, sub-rects). Never world space and never atlas UV — the shader derives UV. */
struct FArcPCGAtlasSlot
{
	/** Terrain cell key this slot stands for (FArcTerrainCellKey is FIntPoint). */
	FIntPoint Key = FIntPoint::ZeroValue;

	/** Top-left texel of the copied sub-rect INSIDE the cell's source height tile. */
	FIntPoint HeightSrcOrigin = FIntPoint::ZeroValue;

	/** Top-left texel of the copied sub-rect INSIDE the cell's source weight tile. */
	FIntPoint WeightSrcOrigin = FIntPoint::ZeroValue;

	/** Array slice this slot occupies in the height atlas AND the weight atlas (one weight slice per
	 *  slot — the filter channel's slice, selected per cell on the game thread). ALWAYS equals the
	 *  slot's row-major index in Slots — the shader recomputes it from (Key - OriginCell) folded
	 *  row-major, so the two must not diverge. */
	int32 Slice = INDEX_NONE;
};

/**
 * One execution's atlas plan: the full row-major rectangle of cells covering the execution bounds
 * (EVERY cell of the rectangle gets a slot, resident or not — residency is a render-thread fact the
 * proxy stamps into a bitmask, never a layout fact) plus the single sub-rect size all slices share.
 */
struct FArcPCGAtlasLayout
{
	/** Row-major over [OriginCell, OriginCell + CellCount): index = (Key.Y - Origin.Y) * CellCount.X
	 *  + (Key.X - Origin.X). Empty when the layout is degenerate. */
	TArray<FArcPCGAtlasSlot> Slots;

	/** Uniform copied sub-rect size, in texels, shared by every height slice. */
	FIntPoint HeightSubRect = FIntPoint::ZeroValue;

	/** Uniform copied sub-rect size, in texels, shared by every weight slice. (1,1) when the terrain
	 *  carries no weight channels. */
	FIntPoint WeightSubRect = FIntPoint::ZeroValue;

	/**
	 * COVERAGE SHORTFALL: MaxCells forced the cell rectangle to be smaller than the execution bounds
	 * actually span, so part of the bounds is not atlased at all and every point there is rejected as
	 * non-resident. The caller MUST surface this on the node — it is indistinguishable from "no grass
	 * authored" otherwise. Compare CellCount against RequestedCellCount for the magnitude.
	 *
	 * Split out of the old single bClamped deliberately: the sub-rect clamp below fires on routine
	 * geometry, so one merged flag could only ever be warned on with a flood of false positives.
	 */
	bool bCellRectClamped = false;

	/** BENIGN: at least one slot's padded sub-rect ran past its tile edge and was slid back inside.
	 *  Routine (the 2-texel pad exceeds the tile whenever the bounds reach a cell edge) and lossless —
	 *  the clamped window still covers that slot's required span. Diagnostic only; do NOT warn on it. */
	bool bSubRectClamped = false;

	/** Cell key of slot 0. NOT in the plan's literal struct sketch: the shader needs the atlas
	 *  origin/extent to turn a world position into a slot index, and re-deriving them from Slots at
	 *  every call site would be a second authority for the same rectangle. */
	FIntPoint OriginCell = FIntPoint::ZeroValue;

	/** Cells per axis. Slots.Num() == CellCount.X * CellCount.Y. */
	FIntPoint CellCount = FIntPoint::ZeroValue;

	/** Cells per axis BEFORE the MaxCells shrink — what the bounds actually span. Equals CellCount
	 *  whenever bCellRectClamped is false. */
	FIntPoint RequestedCellCount = FIntPoint::ZeroValue;
};

/**
 * Every free function below lives in this namespace, per CLAUDE.md's C++ Code Style rule (free
 * functions belong to a named namespace; a shared one is fine in a header).
 */
namespace ArcPCGTerrainGPU
{

/**
 * Lay out the per-cell sub-rect atlas covering LocalBounds (a TERRAIN-LOCAL box).
 *
 * The cell rectangle is derived EDGE-INCLUSIVELY, matching ArcTerrain::CellsIntersectingLocalBounds
 * bit for bit (a Min landing exactly on a cell boundary also takes the lower neighbour) — the
 * subsystem's gather uses that helper, so any other rule here would silently mis-key the gathered
 * snapshots against the slots.
 *
 * Each slot's sub-rect is the texel span the bounds touch inside that cell, PADDED by 2 texels per
 * side: one for the bilinear footprint of a sample landing on the span edge, one more for the
 * normal's central-difference tap. The size is then unified across slots (one Texture2DArray has one
 * size) and each origin clamped so origin + size stays inside the tile.
 *
 * MaxCells shrinks the rectangle (larger axis first, keeping the min corner) and sets
 * bCellRectClamped — a COVERAGE SHORTFALL the caller must surface on the node.
 * WeightTileRes < 4 means "terrain has no weight channels" and yields a (1,1) weight sub-rect.
 */
ARCPCGEXTENSION_API FArcPCGAtlasLayout ComputeAtlasLayout(const FBox& LocalBounds, double CellSize,
	int32 HeightTileRes, int32 WeightTileRes, int32 MaxCells);

/**
 * Cell-local fraction [0,1] -> continuous texel coordinate in a (TileRes = Quads + 3) apron tile.
 * THE shared formula: the terrain VF's inner grid spans texels [1, 1 + Quads] (tile texel 0 is the
 * apron), so fraction 0 sits at texel centre 1.5 and fraction 1 at 1.5 + Quads.
 */
ARCPCGEXTENSION_API float WorldToTileTexel(float Frac, int32 TileRes);

/**
 * TRI-STATE layer-filter resolution, run on the GAME THREAD (the shader never sees an FName and so
 * can never tell "channel absent" from "weight is zero"):
 *   - NAME_None                                  -> -1, bOutAuthoringError = false. Filter OFF, silent.
 *   - name set, ChannelSet null / name not found -> -1, bOutAuthoringError = TRUE. Filter OFF, and the
 *                                                  caller MUST surface it — a silent reject-everything
 *                                                  is indistinguishable from "no grass authored".
 *   - resolved                                   -> the channel's stable index, bOutAuthoringError = false.
 */
ARCPCGEXTENSION_API int32 ResolveLayerFilterChannel(FName LayerChannel,
	const UArcTerrainChannelSet* ChannelSet, bool& bOutAuthoringError);

} // namespace ArcPCGTerrainGPU
