// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Box2D.h"

#include "Forest/ArcForestNoise.h"
#include "Forest/ArcForestSimTypes.h"

/**
 * Shared scalar-field primitives for the forest solver — the SINGLE authority for sampling a
 * pre-baked 2D scalar grid, and for deriving that grid's dimensions / cell centres.
 *
 * PURITY: this whole file is UObject-free and allocation-free on the sample path, so every
 * function here is callable from the UE::Tasks worker the solve runs on (see the worker
 * contracts at ArcForestSimulator.h "PURITY (MC3)" and ArcForestPatchField.h "PURITY").
 * No PCG types, no engine subsystems, no GeometryCore (a TSampledScalarField2 dependency
 * would cost a whole module for ~20 lines of lerp).
 *
 * GRID CONVENTION (shared by every producer and consumer in the forest pipeline):
 *   - Row-major, X fastest: index = Row*Dims.X + Col.
 *   - The grid spans `Bounds` exactly; cell (Col,Row) covers
 *     [Min + Col*Size.X/Dims.X, Min + (Col+1)*Size.X/Dims.X) on X (likewise Y).
 *   - Cell CENTRES sit at (Col+0.5, Row+0.5) in cell units — see CellCentre.
 *   - An EMPTY Values view means "no field supplied" and samples as 1.0 (uniform / neutral),
 *     which is what makes an unwired habitat mask a no-op rather than a total blackout.
 */
namespace ArcForestField
{
	/**
	 * Nearest-cell lookup, clamped to grid bounds (never OOB). Empty grid -> 1.0.
	 *
	 * FloorToInt (not a cast, which truncates toward zero) so a position on a negative-coordinate
	 * bound maps to the correct cell. A degenerate axis (zero span) clamps that coordinate to
	 * cell 0. The returned value is clamped to [0,1] because every current consumer uses it
	 * directly as an acceptance probability.
	 *
	 * This is a behaviour-preserving consolidation of the two identical SampleSuitability copies
	 * that previously lived in ArcForestSimulator.cpp and ArcForestPatchField.cpp — the seeding
	 * RNG streams depend on it bit-for-bit, so it must NOT be "improved".
	 */
	ARCPCGEXTENSION_API float SampleFieldNearest(
		FVector2f Pos,
		const FBox2f& Bounds,
		TArrayView<const float> Values,
		FIntPoint Dims);

	/**
	 * Bilinear lookup over CELL CENTRES, clamped to grid bounds (never OOB). Empty grid -> 1.0.
	 *
	 * The four taps are the cells whose centres bracket Pos; positions in the outer half-cell
	 * border clamp to the edge cell (so the field is constant-extrapolated, never extrapolated
	 * linearly past the border). The result is clamped to [0,1], matching the nearest sampler.
	 *
	 * DEGENERATE AXIS: when Dims.X == 1 (resp. Dims.Y == 1) the two taps along that axis resolve
	 * to the SAME cell, so no interpolation happens along it and the result is exact — this is
	 * relied upon, not accidental. A zero-span Bounds axis behaves the same way.
	 *
	 * The MORPHOLOGY sampler. The suitability grid deliberately keeps the nearest-cell sampler — that
	 * one sits inside the fixed-draw RNG contract and must stay bit-identical — while the habitat
	 * response reads the morphology fields through this one, because at 200 cm/cell a nearest-cell
	 * read would quantise a species' habitat boundary onto visible grid steps.
	 */
	ARCPCGEXTENSION_API float SampleFieldBilinear(
		FVector2f Pos,
		const FBox2f& Bounds,
		TArrayView<const float> Values,
		FIntPoint Dims);

	/**
	 * Clamped grid dimensions for a planar footprint: ceil(span / CellSizeCm) per axis, clamped
	 * to [1, MaxGridDim]. Hitting MaxGridDim means the EFFECTIVE cell is coarser than CellSizeCm
	 * (large bounds grow the cell size instead of the memory), and cells are then NOT square on a
	 * non-square footprint — derive dx and dy separately from Bounds/Dims, never from CellSizeCm.
	 */
	ARCPCGEXTENSION_API FIntPoint ComputeGridDims(const FBox2f& Bounds2D, float CellSizeCm, int32 MaxGridDim);

	/**
	 * World XY centre of grid cell (Col, Row). Caller guarantees GridDims.X/Y > 0 (ComputeGridDims
	 * never returns less than 1).
	 */
	ARCPCGEXTENSION_API FVector2f CellCentre(const FBox2f& Bounds2D, FIntPoint GridDims, int32 Col, int32 Row);
}

/**
 * A scalar field that OWNS its samples — a convenience wrapper for producers/consumers that need
 * to carry a grid around as one value.
 *
 * NOT a replacement for the view-based API above. The production call sites receive
 * TArrayView<const float> straight out of FPCGForestSimContext and MUST keep doing so: passing
 * this struct by value at a per-candidate call site would copy up to MaxGridDim^2 = 65,536 floats
 * per call. Use the free functions for anything on the sampling hot path.
 */
struct FArcForestScalarField
{
	/** World XY footprint (cm) the grid spans. */
	FBox2f Bounds = FBox2f(ForceInit);

	/** X = columns, Y = rows. */
	FIntPoint Dims = FIntPoint::ZeroValue;

	/** Row-major samples, X fastest (length Dims.X*Dims.Y). Empty -> samples as 1.0. */
	TArray<float> Values;

	/** Nearest-cell sample; delegates to ArcForestField::SampleFieldNearest. */
	float SampleNearest(FVector2f Pos) const
	{
		return ArcForestField::SampleFieldNearest(Pos, Bounds, Values, Dims);
	}

	/** Bilinear sample; delegates to ArcForestField::SampleFieldBilinear. */
	float SampleBilinear(FVector2f Pos) const
	{
		return ArcForestField::SampleFieldBilinear(Pos, Bounds, Values, Dims);
	}
};

namespace ArcForestField
{
	/**
	 * Number of samples in a flattened response curve, over the CLOSED interval [0,1].
	 *
	 * 64 with linear interpolation between neighbours: the sample spacing is 1/63 ≈ 0.0159 of the
	 * input range, and since the interpolation is linear the reconstruction error on a smooth authored
	 * curve is O(spacing²·curvature) — well under the precision anyone authors a habitat preference to.
	 * The endpoints are EXACT (index 0 is the curve at 0.0, index 63 the curve at 1.0), which is what
	 * makes a curve that deliberately reaches 0 at one extreme still reach exactly 0 in the LUT.
	 */
	static constexpr int32 ResponseLutSize = 64;
}

/**
 * ONE species field response, FLATTENED for the worker: which axis it reads, its combine weight, and
 * its authored curve resampled to a fixed LUT.
 *
 * WHY FLATTENED — the EDITOR MUTATION RACE, and that is the whole reason. The solve spans multiple
 * polled frames on a UE::Tasks worker while the editor stays fully interactive, so a designer can drag
 * keys in the curve editor mid-solve and mutate the FRichCurve::Keys array the worker would be reading
 * (a TArray resize under a concurrent reader — torn reads, not merely stale values). Sampling the
 * curve ONCE on the game thread before UE::Tasks::Launch removes the race by construction.
 *
 * TWO JUSTIFICATIONS THAT ARE FALSE — do not restate them:
 *   (i)  "an unpinned ExternalCurve could be GC'd, so reading it from the worker is a UAF." It is a
 *        UPROPERTY TObjectPtr (Curves/CurveFloat.h) inside a UPROPERTY TArray on the species asset,
 *        and the asset is pinned by FPCGForestSimContext::SpeciesPin for the worker's whole run. GC
 *        traverses from the pinned root, so the curve object cannot be collected.
 *   (ii) "reading the curve from the worker breaks the purity contract." It does not: the contract at
 *        ArcForestSimulator.h bans UObject-graph MUTATION and explicitly permits reading the species
 *        asset const, which GetRichCurveConst() would be.
 * The race is the reason. Fix the reason, not a fiction.
 *
 * POD by construction — a plain C array, no TArray, so a whole response is 264 bytes of contiguous
 * memory with no allocation and no indirection on the sample path.
 */
struct FArcForestFieldResponseLut
{
	/** Which habitat axis feeds this response's curve. */
	EArcForestHabitatField Field = EArcForestHabitatField::Slope;

	/**
	 * Compensatory-multiplicative exponent, already clamped to >= 0 at flatten time. 0 makes the
	 * response a no-op (Pow(x,0) == 1); 1 passes the curve value straight through; >1 sharpens.
	 */
	float Weight = 1.0f;

	/**
	 * The authored curve at ResponseLutSize evenly spaced inputs across [0,1], each already clamped
	 * to [0,1] at flatten time.
	 *
	 * Zero-initialised, which is NOT a neutral default: an all-zero LUT multiplies a species' response
	 * to zero and excludes it everywhere. That is unreachable in production because the flattener never
	 * EMITS a response it did not fill (an unauthored curve is dropped, not zero-filled) — but do not
	 * hand-construct one of these and expect a no-op.
	 */
	float Samples[ArcForestField::ResponseLutSize] = {};
};

/**
 * ONE species' flattened RECRUITMENT SHADE PREFERENCE: transmittance in [0,1] -> preference in [0,1].
 *
 * Same flatten-on-the-game-thread discipline and same reason as FArcForestFieldResponseLut (the editor
 * mutation race — a designer dragging curve keys while the worker reads FRichCurve::Keys), and the same
 * POD-by-construction shape. It is a SEPARATE type rather than a fifth EArcForestHabitatField because
 * its input is not a habitat axis at all: transmittance is produced BY the simulation, per iteration,
 * and is not available to the seed-time habitat response.
 *
 * WHY A CURVE AND NOT A REUSE OF THE VIGOUR EffectiveNeed FORMULA: that formula is a THRESHOLD
 * monotonically increasing in light, so a faithful reuse could only make a shade-tolerant species
 * INDIFFERENT to bright cells — never preferential toward dark ones — and "seedlings appear
 * preferentially under canopy shade" would be unsatisfiable. A curve gives a genuine unimodal optimum.
 *
 * bAuthored FALSE is the neutral state and is what an unauthored (keyless) curve flattens to: the
 * recruitment acceptance then multiplies by exactly 1.0f. An all-zero Samples table with bAuthored true
 * would exclude the species everywhere — the flattener never emits that (it drops keyless curves), but
 * do not hand-construct one and expect a no-op.
 */
struct FArcForestShadeResponseLut
{
	/** False = no curve authored for this species; the preference factor is exactly 1.0. */
	bool bAuthored = false;

	/** The authored curve at ResponseLutSize evenly spaced transmittances across [0,1], clamped to [0,1]. */
	float Samples[ArcForestField::ResponseLutSize] = {};
};

/**
 * One species' flattened response set — parallel in meaning to FArcForestSpeciesEntry::FieldResponses,
 * but NOT necessarily the same length: the flattener DROPS responses whose curve carries no keys (see
 * ArcForestSpeciesHabitat::FlattenFieldResponses). An empty set means "this species has no habitat
 * preference", which evaluates to exactly 1.0 and reproduces the pre-habitat behaviour bit-for-bit.
 */
struct FArcForestSpeciesResponseLuts
{
	TArray<FArcForestFieldResponseLut> Responses;
};

/**
 * The habitat bundle handed to the solver — everything the per-species habitat response needs,
 * gathered on the game thread and then read (never mutated) by the worker.
 *
 * It is already threaded through every solver entry point as a TRAILING DEFAULTED parameter, so the
 * steps that add the actual fields (terrain morphology grids, the moisture noise params, the
 * per-species curve LUTs) need to touch no call site — production or test. Fields are added here as
 * their consumers land; nothing speculative goes in ahead of a reader.
 *
 * CONTRACT for anything added later:
 *   - UObject-free and POD-by-value. The solve spans multiple polled frames on a UE::Tasks worker
 *     while the editor stays interactive, so the worker must not read anything a designer can
 *     mutate underneath it (that is why authored curves get FLATTENED to LUTs on the GT rather
 *     than sampled as FRuntimeFloatCurve from the worker).
 *   - No non-owning views into caller temporaries; own the data or take it by value.
 */
struct FArcForestHabitatInputs
{
	/**
	 * Parameters for the analytic MOISTURE field (Forest/ArcForestNoise.h). Not a grid and not a bake:
	 * whoever consumes it calls ArcForestNoise::SampleNoise01 at the exact position it needs, which is
	 * why moisture has none of the resolution limits the morphology grids inherit.
	 *
	 * Copied by value from the node's authored settings on the game thread, so the worker never reads
	 * anything a designer can mutate mid-solve — the same rule the curve LUTs will follow.
	 *
	 * NOTHING CONSUMES THIS YET. The per-species habitat response that samples it lands with its own
	 * step; carrying the parameters here first is what makes the noise step verifiable in isolation
	 * (behaviour-neutral by construction).
	 */
	FArcForestNoiseParams MoistureNoise;

	/**
	 * The three derived terrain-morphology fields (Forest/ArcForestTerrainFields.h), each normalised to
	 * [0,1] over the same footprint and dims as the suitability grid.
	 *
	 * Sampled BILINEARLY by the habitat response, not nearest-cell: at 200 cm/cell (stretching further
	 * once MaxGridDim = 256 bites) a nearest-cell read would quantise a species' habitat boundary onto
	 * visible grid steps, and a slope-keyed distribution would show the raster. The suitability grid
	 * keeps its nearest-cell sampler — that one is inside the fixed-draw RNG contract and must stay
	 * bit-identical.
	 *
	 * ABSENT (Values empty) means the field was never derived — the response that reads it is SKIPPED
	 * entirely, NOT evaluated at the sampler's neutral 1.0. See the response-skip rule below.
	 */
	FArcForestScalarField SlopeField;
	FArcForestScalarField SunExposureField;
	FArcForestScalarField ElevationField;

	/**
	 * Per-species flattened response curves, indexed by SPECIES INDEX in
	 * UArcForestSpeciesDefinition::Species — the same index the solver uses everywhere else.
	 *
	 * A short or empty array is not an error: an out-of-range species index (or an empty response set)
	 * means "no habitat preference" and evaluates to exactly 1.0. That is what keeps every entry point
	 * that omits the trailing Habitat argument bit-identical to the pre-habitat solver.
	 */
	TArray<FArcForestSpeciesResponseLuts> SpeciesResponses;

	/**
	 * Per-species flattened RECRUITMENT shade-preference curves, indexed by the same species index as
	 * SpeciesResponses, and filled by the same game-thread flatten pass
	 * (ArcForestSpeciesHabitat::FlattenFieldResponses).
	 *
	 * A short/empty array or an entry with bAuthored == false means "no shade preference" and
	 * contributes exactly 1.0 to the recruitment acceptance — which is why an unauthored species
	 * recruits on habitat response alone rather than being excluded.
	 *
	 * It rides on this bundle rather than on a recruitment-specific one because the bundle already IS
	 * "the GT-flattened, worker-safe, by-value view of the species table" and is already threaded
	 * through every solver entry point as a trailing defaulted parameter.
	 */
	TArray<FArcForestShadeResponseLut> SpeciesShadeResponses;

	/**
	 * The shared "no habitat inputs" instance, and the default argument for every solver entry
	 * point. A function-local static so there is no static-initialisation-order dependency
	 * between translation units.
	 *
	 * READ THIS BEFORE CONSUMING MoistureNoise. Empty() is NOT default-constructed: it explicitly
	 * sets MoistureNoise.Frequency = 0, because FArcForestNoiseParams default-constructs to a LIVE
	 * Perlin field (that is the right AUTHORED default on the node, wrong for "nobody supplied an
	 * input"). SampleNoise01 guards non-positive Frequency and returns 0.5 for every noise type, so
	 * moisture against Empty() is a flat 0.5.
	 *
	 * Two consequences a consumer must not get wrong:
	 *  - 0.5 is a MIDPOINT, not a no-op. A moisture curve EVALUATED at 0.5 can still suppress a
	 *    species outright, so 0.5 must never be inherited by accident. THE DECISION IS TAKEN and is
	 *    the response-skip rule below: a Moisture response solved against Empty() (or against any
	 *    Frequency <= 0) is SKIPPED — it contributes a factor of exactly 1.0 — it is NOT evaluated at
	 *    0.5.
	 *  - Only this singleton is neutral. `FArcForestHabitatInputs Inputs;` constructed by hand is
	 *    live noise again. If you want neutral, pass Empty() or zero the Frequency yourself.
	 */
	static ARCPCGEXTENSION_API const FArcForestHabitatInputs& Empty();
};

namespace ArcForestField
{
	/**
	 * The COMBINED per-species habitat response at Pos, in [0,1]. This is the Π term of the acceptance
	 * formula; the caller multiplies it by the suitability sample itself:
	 *
	 *     AcceptProb = Suitability(Pos) * Π_f Pow(Clamp(Lut_f(Value_f(Pos)), 0, 1), Weight_f)
	 *
	 * where Value_f is the axis's [0,1] input (a BILINEAR morphology-grid sample, or analytic noise
	 * evaluated at Pos) and Lut_f is that response's flattened curve. The compensatory-multiplicative
	 * Pow(Clamp(v,0,1), Weight) shape is the house convention, shared with FArcTQSStep (ArcTargetQuery),
	 * FArcUtilityConsideration (ArcAI) and FArcKnowledgeScorer.
	 *
	 * RETURNS EXACTLY 1.0f — no arithmetic performed at all — when the species has no flattened
	 * responses (empty SpeciesResponses, an out-of-range index, or an empty response set). Multiplying
	 * the suitability by an exact 1.0f is an IEEE-754 identity, which is what makes an unauthored
	 * FieldResponses list BIT-IDENTICAL to the pre-habitat solver rather than merely close.
	 *
	 * THE RESPONSE-SKIP RULE (normative, and it applies to ALL FOUR axes, not just Moisture). A
	 * response whose INPUT IS NOT SUPPLIED is skipped — factor exactly 1.0 — never evaluated at some
	 * stand-in input value:
	 *   - Slope / SunExposure / Elevation: not supplied == the grid's Values are empty.
	 *   - Moisture: not supplied == MoistureNoise.Frequency <= 0 (the documented Empty() marker).
	 * WHY, and why the samplers' own neutral value is NOT good enough: an empty grid samples as 1.0
	 * precisely so an unwired MASK multiplies to nothing — but that neutrality does not survive being
	 * fed through a curve. Lut(1.0) is whatever the designer authored at the top of the range, and a
	 * species keyed to gentle slopes would read its own worst input and vanish. Skipping is the only
	 * choice that is genuinely neutral on all four axes.
	 * HONEST CONSEQUENCE: a designer who authors Frequency = 0 wanting a deliberately flat 0.5 moisture
	 * field gets the response skipped instead. "Flat" and "absent" are indistinguishable here by
	 * design; author a flat curve, not a flat field.
	 *
	 * ALSO RNG-FREE, and that is load-bearing: SeedPopulation runs a documented FIXED 7-DRAW CONTRACT,
	 * so this function changes the VALUE of AcceptProb and must never change the number or order of
	 * FRandomStream draws. It takes no stream, and must not acquire one.
	 *
	 * Worker-safe: reads only the by-value bundle and calls pure functions.
	 *
	 * @param SolveSeed Params.Seed — the noise axes' seed. Passed rather than stored on the bundle so
	 *                  there is one authority for the solve seed (FArcForestSimParams::Seed).
	 */
	ARCPCGEXTENSION_API float EvaluateHabitatResponse(
		const FArcForestHabitatInputs& Habitat,
		int32 SpeciesIndex,
		FVector2f Pos,
		int32 SolveSeed);

	/**
	 * Linear lookup of a flattened response LUT at a [0,1] input. Endpoints are exact; inputs outside
	 * [0,1] clamp. Exposed for the flattener's own round-trip checks and for tests — the production
	 * path reaches it through EvaluateHabitatResponse.
	 */
	ARCPCGEXTENSION_API float SampleResponseLut(const FArcForestFieldResponseLut& Lut, float Value01);

	/**
	 * The shared interpolation core of SampleResponseLut, over a bare sample table. Exists so the
	 * recruitment shade LUT (FArcForestShadeResponseLut, whose input is transmittance and which
	 * therefore carries no habitat axis or Weight) reads through the SAME arithmetic instead of a
	 * second copy of the lerp.
	 */
	ARCPCGEXTENSION_API float SampleLutSamples(const float (&Samples)[ResponseLutSize], float Value01);

	/**
	 * The recruitment shade-preference factor at a given transmittance, in [0,1]. Returns exactly 1.0f
	 * for a species with no authored curve (an out-of-range index, or bAuthored == false), which is what
	 * makes the shade term a multiplicative no-op rather than an exclusion.
	 */
	ARCPCGEXTENSION_API float EvaluateShadeResponse(
		const FArcForestHabitatInputs& Habitat,
		int32 SpeciesIndex,
		float Transmittance01);
}
