// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Box2D.h"

#include "Forest/ArcForestField.h"

/**
 * Terrain-morphology habitat fields, DERIVED from the surface height grid the forest node already
 * bakes (one ProjectPoint per cell). Nothing here samples a surface, reads a landscape, or needs a
 * painted channel: the node never learns what produced the surface, which is what keeps the locked
 * source contract in docs/reference/forest-sim-model.md intact.
 *
 * PURITY: UObject-free and PCG-free — same worker-safety class as ArcForestField.h. (This file has
 * no .generated.h of its own, but ArcForestField.h now includes ArcForestNoise.h, which carries one
 * for its reflected params struct, so the shared class is "no UObject, no PCG", not
 * "reflection-free". Reflection metadata alone costs nothing at runtime and nothing on a worker.)
 * (see the PURITY contracts at ArcForestSimulator.h and ArcForestPatchField.h). It is CALLED on the
 * game thread today (in the node's prebake, right after the height grid is filled) but it belongs on
 * the solver side of the purity boundary and must stay callable from a UE::Tasks worker.
 *
 * ----------------------------------------------------------------------------------------------
 * RESOLVABLE-FEATURE LIMIT — read before adding a field here.
 * ----------------------------------------------------------------------------------------------
 * These fields inherit the suitability grid's resolution: 200 cm/cell by default
 * (PCGForestSimSettings.cpp SuitabilityCellSizeCm), and MaxGridDim = 256 CLAMPS the dimensions, so
 * on a large footprint the effective cell STRETCHES (a 1 km volume is ~390 cm/cell, and the cells
 * stop being square on a non-square footprint). A finite-difference operator needs about two cells
 * to express a feature, so the smallest morphological feature resolvable here is roughly
 * ~4 m — worse on a big volume.
 *
 * Slope and aspect are defensible at that spacing: they are FIRST derivatives, so their magnitude
 * is independent of the cell size. Curvature/concavity is NOT, and was deliberately dropped rather
 * than shipped here: a discrete Laplacian scales as 1/dx^2, so at this spacing it resolves only
 * landform-scale bowls while the default PatchRadiusSigma is 300 cm — a clump smaller than one
 * cell. Moisture is therefore analytic noise (Forest/ArcForestNoise), which has no grid and hence
 * no resolution problem at all. Do not reintroduce a curvature field without first answering the
 * resolution question.
 */

/** Absolute normalisation ranges for the derived morphology fields (authored on the node). */
struct FArcForestTerrainFieldParams
{
	/**
	 * Slope in degrees that maps to 1.0; steeper clamps at 1.0. <= 0 degenerates to a binary field
	 * (any non-zero gradient -> 1.0) rather than dividing by zero.
	 */
	float MaxSlopeDegrees = 45.0f;

	/**
	 * Sun azimuth in DEGREES, and the axis convention is normative — the SunExposure field is
	 * meaningless without it:
	 *
	 *   0 degrees   = the world +X axis (UE's "forward").
	 *   90 degrees  = the world +Y axis (UE's "right").
	 *   Angles increase from +X toward +Y, exactly as FMath::Atan2(Y, X) measures them.
	 *   The angle is measured in the world XY plane only; world Z is irrelevant.
	 *
	 * Semantics: this is the direction a slope must FACE to be fully lit. A slope whose downhill
	 * direction is +X reads exposure 1.0 at SunAzimuthDegrees == 0, and 0.0 at 180. Deliberately
	 * NOT phrased as a compass bearing ("due south"), because that would not say which world axis
	 * it means.
	 */
	float SunAzimuthDegrees = 0.0f;

	/**
	 * ABSOLUTE world-Z (cm) mapping to Elevation 0.0 and 1.0; outside the range clamps.
	 *
	 * Absolute, never the grid's observed min/max: the bake footprint is the PCG execution bounds,
	 * i.e. the PARTITION CELL when the component is partitioned (a supported mode). Per-cell extrema
	 * would normalise the same hillside to 0.2 in one cell and 0.9 in its neighbour, so an
	 * elevation-keyed species distribution would break at every partition seam.
	 *
	 * Min == Max (or Max < Min) -> the field is a constant 0.5, never a divide by zero.
	 */
	float ElevationMinCm = 0.0f;
	float ElevationMaxCm = 0.0f;
};

namespace ArcForestTerrainFields
{
	/**
	 * Derive Slope / SunExposure / Elevation, each normalised to [0,1], from a baked height grid.
	 *
	 * HeightGrid / HitGrid are row-major, X fastest, length Dims.X*Dims.Y (the shared grid
	 * convention documented in ArcForestField.h). HitGrid is the SURFACE-VALIDITY MASK: false means
	 * the cell's ProjectPoint missed and its height is a fallback constant, not a measurement.
	 * There is no in-band no-data marker to infer instead — the fallback Z is an ordinary float a
	 * genuine hit can equal, and suitability 0 also occurs on a real hit with Density 0.
	 *
	 * NO-DATA HANDLING: heights are first REPLICATED from the nearest valid cell (multi-source BFS
	 * over 4-neighbours, i.e. nearest in Manhattan distance, ties resolved by scan order) so a
	 * fallback-Z hole never enters the difference as a cliff.
	 *
	 * What replication does and does NOT guarantee — read this before writing a test against it.
	 * A hole takes its nearest valid cell's height, so the difference ACROSS the hole boundary is
	 * zero: no cliff, which is the whole point. It does NOT make the hole read as flat ground. A
	 * fully invalid row through a uniform hillside is filled from the adjacent valid row, so it
	 * INHERITS that row's along-row gradient and reports that row's slope and aspect — the hole
	 * continues its neighbour's morphology, which is the intended behaviour. Concretely, on a
	 * 3x3 grid with Dx=Dy=200 and h(col)=100*col, an entirely invalid middle row fills to
	 * [0,100,200], so its centre cell reports dh/dx=0.5 -> slope atan(0.5)=26.57deg -> 0.59 at
	 * MaxSlopeDegrees=45, and exposure 0.0 at azimuth 0. NOT slope 0 / exposure 0.5.
	 * Stamping a genuinely flat stripe there would be worse: it would be an artifact line that
	 * attracts flat-preferring species straight across the hillside.
	 *
	 * The flat triple (slope 0, exposure 0.5, elevation 0.5) is produced in exactly ONE case: no
	 * cell in the grid is valid at all, so there is nothing to replicate from.
	 *
	 * DIFFERENCING: central difference (2*d denominator) on interior cells, ONE-SIDED difference
	 * with a d denominator (NOT 2*d) on the first/last row/column. A clamped-neighbour central
	 * difference would put the two taps one cell apart over a 2*d denominator and so HALVE the
	 * gradient along the entire footprint border — which under partitioned generation is every
	 * partition seam. dx and dy are derived SEPARATELY from Bounds/Dims, never from a nominal cell
	 * size, because MaxGridDim makes cells non-square (see the header comment above).
	 *
	 * A degenerate axis (Dims.X == 1, Dims.Y == 1, or a zero-span Bounds axis) contributes a zero
	 * gradient along that axis. An empty or mis-sized grid yields three EMPTY fields, which sample
	 * as the neutral 1.0 per ArcForestField's contract.
	 *
	 * OutSlope/OutSunExposure/OutElevation are fully overwritten (Bounds, Dims and Values).
	 */
	ARCPCGEXTENSION_API void Derive(
		TArrayView<const float> HeightGrid,
		TArrayView<const bool> HitGrid,
		FIntPoint Dims,
		const FBox2f& Bounds,
		const FArcForestTerrainFieldParams& Params,
		FArcForestScalarField& OutSlope,
		FArcForestScalarField& OutSunExposure,
		FArcForestScalarField& OutElevation);
}
