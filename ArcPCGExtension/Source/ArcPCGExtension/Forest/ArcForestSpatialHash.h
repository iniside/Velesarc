// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Uniform 2D spatial hash grid over plant XY positions, used to accelerate the
 * horizontal neighbour queries the forest competition step (Step 6) needs.
 *
 * Pure C++ (Core math only) — no PCG, no UObject — so it lives inside the pure
 * worker-thread solver boundary alongside the rest of the sim core.
 *
 * The grid buckets point INDICES (into the caller's positions array) by their
 * floor(pos / CellSize) cell. QueryRadius scans only the ceil(Radius / CellSize)
 * neighbourhood of cells around the query centre, then distance-tests each
 * candidate against the exact radius — the grid supplies candidates, the exact
 * test filters. Cell size should be chosen ≈ the max crown radius of the band
 * being resolved so most queries touch a small, bounded cell neighbourhood.
 *
 * LIFETIME CONTRACT: Build stores a TArrayView over the caller's positions and
 * does NOT copy them. The caller MUST keep the backing positions array alive and
 * unmodified for as long as this hash is queried. This is the low-footprint
 * choice (no per-rebuild copy) and is safe here because the solver owns both the
 * positions array and the hash within a single band/iteration scope, rebuilding
 * the hash (see Reset) each time radii grow.
 */
class FArcForestSpatialHash
{
public:
	// Non-copyable: PositionsView is a non-owning view. When this hash is bundled with the
	// array it views (as it is inside a per-species patch/plant struct), a copy of the bundle
	// would leave the copied view pointing at the source's buffer → dangling. Move keeps the
	// view valid when the owner also moves. Copy is forbidden to make that mistake a compile
	// error rather than a use-after-free.
	FArcForestSpatialHash() = default;
	FArcForestSpatialHash(const FArcForestSpatialHash&) = delete;
	FArcForestSpatialHash& operator=(const FArcForestSpatialHash&) = delete;
	FArcForestSpatialHash(FArcForestSpatialHash&&) = default;
	FArcForestSpatialHash& operator=(FArcForestSpatialHash&&) = default;

	/**
	 * Bucket every position index into its grid cell. Retains the map's existing
	 * allocations (via Reset) so repeated per-band/per-iteration rebuilds are
	 * reallocation-friendly. Stores a view over Positions — see LIFETIME CONTRACT.
	 *
	 * @param Positions  Point XY positions; indices into this array are returned by QueryRadius.
	 * @param InCellSize Grid cell edge length in the same units as Positions (> 0).
	 */
	void Build(TArrayView<const FVector2f> Positions, float InCellSize);

	/**
	 * Append the indices of all stored points within Radius of Center.
	 * OutIndices is CLEARED at the start (not appended to) — every call returns a
	 * fresh candidate set. Only the ceil(Radius / CellSize) cell neighbourhood is
	 * scanned; each candidate is then exact-distance-tested against Radius.
	 */
	void QueryRadius(FVector2f Center, float Radius, TArray<int32>& OutIndices) const;

	/**
	 * Clear all buckets but retain the map's bucket allocations for cheap reuse on
	 * the next Build. Also drops the stored positions view. Called by Build.
	 */
	void Reset();

	/** Grid cell containing Position, using FloorToInt so negative coordinates map correctly. */
	FIntPoint CellOf(FVector2f Position) const;

private:
	/** Point indices bucketed by grid cell. */
	TMap<FIntPoint, TArray<int32>> Cells;

	/** Non-owning view of the caller's positions — see LIFETIME CONTRACT. */
	TArrayView<const FVector2f> PositionsView;

	/** Grid cell edge length; set by Build, must be > 0 for CellOf/QueryRadius to be valid. */
	float CellSize = 1.0f;
};
