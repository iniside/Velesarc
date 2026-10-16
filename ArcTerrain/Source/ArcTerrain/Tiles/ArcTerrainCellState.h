// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTerrainTypes.h"
#include "Tiles/ArcTerrainTileSet.h"

/** The per-tick-touched slice of one cell, held in a contiguous array so the phases that walk a set of
 *  cell indices touch it without paging in the ~700-byte FArcTerrainCellState behind it.
 *
 *  Key is the cell's map key; FArcTerrainCellState::HotIndex is the reverse link. Both directions are
 *  maintained exclusively by FArcTerrainState::FindOrAddCell / RemoveCell / EmptyCells. */
struct FArcTerrainCellHot
{
	FArcTerrainCellHot()
		: bResident(0)
		, bDirty(1)
		, bQueued(0)
	{
	}

	FArcTerrainCellKey Key = FArcTerrainCellKey::ZeroValue;

	/** Transient: squared WORLD-space XY distance from the nearest invoker to
	 *  the cell's XY RECT (closest-point-on-rect, measured in world cm² — 0 when standing on the
	 *  cell; ring radii are world units, never terrain-local); Max() when there are no invokers.
	 *  Drives the composite budget sort.
	 *
	 *  REFRESHED ONLY FOR THE CELLS OF THE SECTORS THE RESIDENCY PHASE RANKS — not for every cell every
	 *  tick. A reader outside that set sees a stale value. */
	double InvokerDistSq = TNumericLimits<double>::Max();

	/** Edit-pin expiry (FPlatformTime::Seconds domain; 0 = never pinned). While PinnedUntil > now the
	 *  cell counts as wanted-resident REGARDLESS of invoker distance, is exempt from the
	 *  MaxResidentSectors eviction cap (its whole sector is promoted), and its dirty composite sorts at
	 *  TOP priority — so an artist editing a stamp bigger than the preview ring sees the whole
	 *  footprint. Set from OnStampChanged (rolling arc.Terrain.EditPinSeconds window) and, in editor
	 *  worlds, continuously re-stamped while the stamp component / its owner actor is selected. Not
	 *  serialized; expires silently. */
	double PinnedUntil = 0.0;

	/** Requested per-cell density. 0 = the raster density (ArcTerrainSubsystemPrivate::CompositeQuads).
	 *
	 *  NOTHING WRITES IT: density is uniformly the raster density, and the only input that can move it —
	 *  Config.QuadsPerCell() — reaches the subsystem through OnTerrainConfigChanged, which empties the cell
	 *  set outright. The field stays reserved for the content-driven per-cell density a later CBT step
	 *  introduces, strictly BELOW the raster. */
	int32 DesiredQuads = 0;

	/** Transient, recomputed every Tick: this cell's SECTOR is resident (post LRU cap). Residency is
	 *  sector-granular — a cell is resident if and only if its sector is, which is what makes atlas
	 *  admission monotone in distance without a second rule. */
	uint32 bResident : 1;

	uint32 bDirty : 1;

	/** This cell has an unconsumed entry in FArcTerrainState::DirtyQueue: the dedup for that queue, and
	 *  one of the residency phase's work-in-flight holds — an edit that created the cell must not have it
	 *  torn down before the bake phase has looked at it. */
	uint32 bQueued : 1;
};

// The residency phase scans one of these per cell and nothing else; four to a cache line is the whole
// point of the split, so a field added here has to displace one rather than grow the record.
static_assert(sizeof(FArcTerrainCellHot) == 32, "FArcTerrainCellHot must stay at 32 bytes");

/** Per-cell composite state: the GPU tile set + monotonic version (bumped on the game thread when a
 *  composite is enqueued), plus the residency scratch and the atlas publication tags. */
struct FArcTerrainCellState
{
	FArcTerrainTileSet TileSet;
	uint64 Version = 0;

	/** Index of this cell's record in FArcTerrainState::HotCells. */
	int32 HotIndex = INDEX_NONE;

	/** False when the LAST composite dropped any height-relevant pass because its GPU source failed
	 *  to resolve (transient upload/provider failure). The published tile is drawn (better than a hole)
	 *  but is never treated as the settled answer for the cell. */
	bool bCompositeFullyResolved = true;

	/** True when the last published tile was usable but incomplete and should be recomposited after
	 *  CompositeResolveRetryAfter. Kept separate from bDirty so the partial tile keeps drawing while the
	 *  missing source recovers. */
	bool bCompositeResolveRetryPending = false;

	/** Next game-thread time at which an incomplete composite may be retried. A failed source resolve
	 *  schedules another composite, but this deadline prevents a permanently invalid stamp from
	 *  consuming the composite budget every frame. Explicit stamp changes bypass the deadline. */
	double CompositeResolveRetryAfter = 0.0;

	/** Consecutive incomplete composites, used for a short exponential retry backoff. */
	uint8 CompositeResolveRetryCount = 0;

	/** Set by CompositeCell, consumed by the atlas pass: this cell's WEIGHT slice region is behind its
	 *  tiles. The single-cell post-edit update path — sector ADMISSION fills every cell of the sector, but
	 *  the composite loop is budgeted, so a cell that recomposites long after its sector was admitted
	 *  would otherwise never write again. */
	bool bAtlasRefillPending = false;

	/** THE ATLAS PUBLICATION TAG: the Version whose composited height tile CompositeCell wrote into this
	 *  cell's sector slice (0 = none). Non-zero is what stops the baked-tile atlas fill overwriting a
	 *  cell that has composited in this session; an unpublished cell's region holds either a stale
	 *  composite or the slice clear at BaseHeightCm. Zeroed whenever the slice is (re)cleared at sector
	 *  admission, which is also where the cell is re-dirtied so it republishes. */
	uint64 AtlasHeightVersion = 0;

	/** FArcTerrainTilePayload::PayloadId of the height tile loaded for this cell (0 = none), refreshed by
	 *  the atlas fill pass. Monotonic across the process, which is what makes a re-read of the same tile
	 *  key read as newer than the one already in the slice. */
	uint64 HeightTileSerial = 0;

	/** The HeightTileSerial the cell's slice region was last filled FROM A TILE. Behind HeightTileSerial
	 *  means the tile is newer than whatever the slice holds — composite or an older tile — and the atlas
	 *  fill takes the tile even though the cell has published a composite this session. */
	uint64 AtlasHeightTileSerial = 0;

	uint64 AtlasWeightTileSerial = 0;

};
