// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Forest/ArcForestField.h"
#include "Forest/ArcForestSimTypes.h"

/**
 * Default height-band boundaries (model doc table, docs/reference/forest-sim-model.md).
 *
 * UNIT NOTE: these are expressed in METRES, matching the model doc. Band classification is
 * used ONLY to gate the per-band IntraLayerShading flag (see FArcForestLightModel); it never
 * touches the core height-sorted Beer-Lambert attenuation. If the caller works in a different
 * unit (e.g. Unreal centimetres) it MUST pass scaled thresholds to LayerForHeight, or a unit
 * mismatch will misclassify bands and silently disable/enable self-shading. Because the light
 * model receives a caller-computed Bands array (single source of truth = LayerForHeight), the
 * light model itself never assumes a unit.
 */
namespace ArcForestLight
{
	/** Field -> Shrub boundary. Below this a plant is in the Field band. */
	constexpr float DefaultShrubMinHeight = 1.5f;

	/** Shrub -> Understory boundary. */
	constexpr float DefaultUnderstoryMinHeight = 5.0f;

	/** Understory -> Canopy boundary. At/above this a plant is in the Canopy band. */
	constexpr float DefaultCanopyMinHeight = 15.0f;

	/**
	 * Default LAI-grid cells aggregated per transmittance-raster cell, per axis (so a 4x4 = 16-cell
	 * block). At the solver's 100 cm light cells that is a 4 m raster cell — coarse enough to make
	 * the raster ~16x cheaper than the LAI grid, fine enough to resolve a mature crown's shade
	 * footprint. Tests that need the raster to line up EXACTLY with the per-plant read pass 1.
	 */
	constexpr int32 DefaultTransmittanceStride = 4;

	/**
	 * Hard cap on transmittance-raster dimensions per axis, matching the PCG node's MaxGridDim.
	 *
	 * The LAI grid's own dims are an UNBOUNDED CeilToInt(span / CellSize), so a 500 m x 500 m volume
	 * at 100 cm cells is 250,000 cells. The raster must not inherit that: above the cap the effective
	 * stride GROWS (memory and exp() count stay bounded, resolution degrades) rather than the
	 * allocation. See the effective-stride derivation in ComputeIncidentLight.
	 */
	constexpr int32 MaxTransmittanceDim = 256;
}

/**
 * Vertical light-transfer model — the correctness-critical MC1 mechanism.
 *
 * MODEL (MC1, docs/reference/forest-sim-model.md): a plant's incident light is attenuated by
 * everything currently STRICTLY TALLER than it — not by a fixed species/layer order, not by
 * itself, not by shorter plants. Beer-Lambert: light = exp(-k * LAI_from_taller_plants), with
 * k = FArcForestSimParams::CanopyExtinction.
 *
 * SINGLE-PASS HEIGHT-ORDERED REALIZATION:
 *  - Sort plant indices by CURRENT height, descending (tallest first), with a total-order,
 *    deterministic tie-break by ascending index.
 *  - Maintain a per-cell LAI accumulator grid over Bounds2D.
 *  - Sweep in height order: FIRST read the accumulated LAI at a plant's cell (contributed only
 *    by already-processed, i.e. taller, plants) -> incident = exp(-k * LAI_read); THEN splat the
 *    plant's own crown LAI into the accumulator (disc rasterization over the cells its crown
 *    radius covers, weighted by its per-plant crown LAI-density). Read-before-write guarantees a
 *    plant only sees shade from taller plants.
 *
 * TIE RULE (equal current height) — DECISION: equal-height plants do NOT shade each other.
 *  This is the "strictly taller" rule taken literally and is the order-INDEPENDENT choice. It is
 *  realized by processing each exactly-equal-height GROUP in two phases: all reads for the group
 *  first, then all writes for the group. A naive per-plant read-then-write would let an
 *  equal-height plant processed earlier shade a later one, making the result depend on the tie
 *  ordering; the two-phase group split removes that dependence entirely. Determinism is still
 *  total because the sort tie-breaks by index, but the tie rule makes the OUTPUT independent of
 *  how ties happen to be ordered.
 *
 * INTRA-LAYER SELF-SHADING FLAG (FArcForestSimParams::IntraLayerShading, indexed by band):
 *  The core sweep is purely height-based and knows nothing of bands. The flag modulates
 *  SAME-BAND shading only; CROSS-BAND (vertical) shading — the flagship canopy-shades-understory
 *  mechanism — is ALWAYS on and never gated. To implement the flag coherently the accumulator is
 *  split into one grid PER BAND. When a plant in band B reads its cell:
 *    - IntraLayerShading[B] ON  -> it sums ALL band grids (a taller same-band plant shades it).
 *    - IntraLayerShading[B] OFF -> it sums all band grids EXCEPT its own band B (same-band plants
 *      do not shade it), but STILL receives shade from every higher band.
 *  A plant always splats into its own band's grid, so it still shades LOWER bands regardless of
 *  its own flag — the flag is strictly intra-layer. With the default flags (Field ON, others OFF)
 *  this yields: dense self-shading among ferns, no self-shading within shrub/understory/canopy,
 *  full top-down shading between bands.
 *
 * WHY THERE IS NO STANDALONE SampleLight(pos, atHeight): under MC1 the LAI "above" a query depends on
 * the query HEIGHT, and the retained structure is aggregated per BAND, not per continuous height —
 * band boundaries do not align with an arbitrary query height, so a post-sweep scalar grid cannot
 * answer SampleLight soundly. A raster summed from the FINISHED band grids is WRONG in exactly the
 * way that matters: the finished grids contain EVERY plant, whereas the per-plant read is
 * strictly-taller-only, so such a raster claims the stand shades itself at a height where the
 * per-plant path correctly reports full light (acutely so right after seeding, when every plant sits
 * at the same seedling height and nothing shades anything).
 *
 * The ONLY sound construction is to snapshot the accumulator MID-SWEEP, and that is what the
 * optional OutTransmittance raster does — see the parameter docs on ComputeIncidentLight. It is
 * therefore produced BY the sweep and cannot be recovered afterwards from its outputs.
 * OutIncidentLight remains the authoritative per-plant light the simulator consumes.
 *
 * PURITY: Core math only — no PCG, no UObject instances, no engine sampling. Depends only on the
 * plain FArcForestSimParams / EArcForestLayer contract, so it lives inside the worker-thread solver
 * boundary and a later GPU port rewrites only this core.
 *
 * DETERMINISM: identical inputs (+ seed baked into the plant arrays upstream) -> identical output.
 * Height sort is a total order (index tie-break); all accumulation happens in a fixed order.
 */
class ARCPCGEXTENSION_API FArcForestLightModel
{
public:
	/**
	 * Compute per-plant incident light under the MC1 height-ordered vertical transfer model.
	 *
	 * CONTRACT: Positions/Heights/CrownRadii/CrownLAIDensity are parallel arrays of equal length
	 * (one entry per plant). Bands is either empty (flag gating disabled -> behaves as if all
	 * IntraLayerShading were ON, i.e. pure height-based shading) or parallel to the plant arrays.
	 * OutIncidentLight is resized to the plant count and filled with each plant's incident light
	 * in [0,1]; index i corresponds to plant i (NOT to the internal height-sorted order).
	 *
	 * @param Positions        Plant XY positions (same unit as Bounds2D / CellSize).
	 * @param Heights           Current plant heights; drives the tallest-first sweep order.
	 * @param CrownRadii        Per-plant crown (influence) radius for the LAI splat disc.
	 * @param CrownLAIDensity   Per-plant LAI added to each cell its crown disc covers.
	 * @param Bands             Per-plant height band (from LayerForHeight); empty disables the flag.
	 * @param Params            Global tuning (CanopyExtinction k, IntraLayerShading, Bounds2D).
	 * @param CellSize          LAI grid resolution (~1 m for the field band; <=0 defaults to 1.0).
	 * @param OutIncidentLight  Filled with per-plant incident light, parallel to Positions.
	 *
	 * OPTIONAL QUERY-HEIGHT TRANSMITTANCE RASTER (OutTransmittance, non-null = requested).
	 *
	 * Per (strided) cell it holds the incident light a HYPOTHETICAL plant of height QueryHeightCm
	 * would read at that cell — the field a recruitment pass needs to decide whether a seedling can
	 * establish there. It is filled ONLY when OutTransmittance is non-null; the null path performs no
	 * extra work and allocates nothing, which is what keeps this parameter behaviour-neutral for every
	 * existing caller.
	 *
	 * IT IS NOT A POST-SWEEP SUM OVER THE BAND GRIDS. Two capture cases, both required:
	 *  1. GROUP-BOUNDARY capture — the raster is snapshotted immediately BEFORE Phase A of the first
	 *     equal-height group whose GroupHeight <= QueryHeightCm, i.e. straight after Phase B of the
	 *     last group STRICTLY TALLER than the query. At that instant the accumulators hold writes from
	 *     exactly the taller groups, which is bit-identical to what a plant of that height reads.
	 *  2. POST-SWEEP fallback — if the sweep never reaches a group at or below the query height (the
	 *     PRODUCTION case: after the first iteration every live plant has grown above the seedling
	 *     query height), every plant is strictly taller, so the correct snapshot IS the finished state
	 *     and the raster is emitted after the loop. Omitting this case would leave a REUSED buffer
	 *     holding the previous iteration's values — stale, not merely coarse.
	 * A NaN GroupHeight fails `<= QueryHeightCm` and so never triggers case 1; it falls through to the
	 * post-sweep emit, which is the conservative answer.
	 *
	 * SELF-BAND: the same rule as the per-plant read. If Bands is present and
	 * IntraLayerShading[QueryBand] is OFF, the query band's own grid is excluded from the sum; every
	 * other band still shades. With Bands absent the read is all-ON, matching the sweep.
	 *
	 * STRIDE / RESOLUTION: the raster aggregates a Stride x Stride block of LAI cells and takes ONE
	 * exp() per raster cell of the block's MEAN LAI. exp(-k*mean(LAI)) != mean(exp(-k*LAI)) (Jensen —
	 * averaging before a convex transform), so any Stride > 1 is a deliberate systematic SMOOTHING of
	 * the field, not tolerance noise: it under-states the darkness of a small dense crown inside an
	 * otherwise-open block. Pass Stride = 1 for a cell-exact raster (that is how the equality against
	 * the per-plant read is tested). The dims are additionally capped at
	 * ArcForestLight::MaxTransmittanceDim per axis by growing the EFFECTIVE stride, because the LAI
	 * grid's own dims are unbounded.
	 *
	 * EARLY-OUTS: every early return below (no plants, malformed parallel arrays, unresolvable bounds)
	 * happens BEFORE the grids exist. Each of them still sizes the raster from Params.Bounds2D and the
	 * stride and fills it with 1.0 — mirroring the full-light prefill of OutIncidentLight, so an empty
	 * or malformed stand reads as "no shade anywhere" rather than as stale or absent data. If
	 * Params.Bounds2D is itself invalid there is no footprint to size from, so the field is left EMPTY,
	 * which FArcForestScalarField's samplers already answer as 1.0.
	 *
	 * @param OutTransmittance  Optional query-height raster; null (default) = not requested.
	 * @param QueryHeightCm     Height of the hypothetical plant the raster is computed for. Same unit
	 *                          as Heights. Only meaningful when OutTransmittance is non-null.
	 * @param Stride            LAI cells aggregated per raster cell per axis; clamped to >= 1.
	 * @param QueryBand         Band the query height falls in, for the self-shading gate. Supplied by
	 *                          the caller rather than derived here because band classification needs
	 *                          UNIT-MATCHING thresholds and this model deliberately never assumes a
	 *                          unit (see the unit note at the top of this file); the caller already
	 *                          owns that single source of truth via LayerForHeight.
	 */
	void ComputeIncidentLight(
		TArrayView<const FVector2f> Positions,
		TArrayView<const float> Heights,
		TArrayView<const float> CrownRadii,
		TArrayView<const float> CrownLAIDensity,
		TArrayView<const EArcForestLayer> Bands,
		const FArcForestSimParams& Params,
		float CellSize,
		TArray<float>& OutIncidentLight,
		FArcForestScalarField* OutTransmittance = nullptr,
		float QueryHeightCm = 0.0f,
		int32 Stride = ArcForestLight::DefaultTransmittanceStride,
		EArcForestLayer QueryBand = EArcForestLayer::Field) const;

	/**
	 * Classify a height into its band using explicit (unit-matching) boundaries. This is the single
	 * canonical classifier; the simulator builds the Bands array with it so the light model never
	 * has to assume a unit. Boundaries are lower bounds: Height < ShrubMinHeight -> Field, etc.
	 */
	static EArcForestLayer LayerForHeight(
		float Height,
		float ShrubMinHeight = ArcForestLight::DefaultShrubMinHeight,
		float UnderstoryMinHeight = ArcForestLight::DefaultUnderstoryMinHeight,
		float CanopyMinHeight = ArcForestLight::DefaultCanopyMinHeight);
};
