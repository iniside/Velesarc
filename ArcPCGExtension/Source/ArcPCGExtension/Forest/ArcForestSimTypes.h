// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "ArcForestSimTypes.generated.h"

/**
 * Vertical height bands used by the forest solver.
 *
 * These are NOT species identities — they are light-attenuation steps and horizontal
 * competition domains. Per MC1 (see docs/reference/forest-sim-model.md), a plant occupies
 * the band its CURRENT height falls in and graduates upward as it grows: a young canopy
 * species lives in the Shrub/Understory band (shaded by taller plants) until it grows into
 * the Canopy. The species-level MatureLayer is authoring/mesh-filtering metadata only and
 * must never drive competition or light bucketing.
 *
 * BlueprintType so it is selectable on the species data asset in the editor. The DisplayName in
 * each entry below is the label shown in the editor dropdown. A plant's LIVE band is decided by its
 * CURRENT height against the cm thresholds noted per entry — it climbs bands as it grows.
 */
UENUM(BlueprintType)
enum class EArcForestLayer : uint8
{
	/** ~0.7-1.5 m: ferns, brambles, tall herbs, tussocks — densest, dominates on-foot. */
	Field UMETA(DisplayName = "Field / High Ground Cover"),

	/** ~1.5-5 m: shrubs, saplings, hazel — shades the field layer. */
	Shrub UMETA(DisplayName = "Shrub"),

	/** ~5-15 m: small/young trees, shade-tolerant — decides who reaches the canopy. */
	Understory UMETA(DisplayName = "Understory / Subcanopy"),

	/** 15 m+: mature dominant trees — first shade source, drives everything below. */
	Canopy UMETA(DisplayName = "Canopy")
};

/** Number of height bands; keep in sync with EArcForestLayer. */
static constexpr int32 GArcForestLayerCount = 4;

/**
 * The habitat axes a species can key its distribution off.
 *
 * A CLOSED, NODE-DERIVED SET — deliberately an enum and NOT an FName. Every axis here is produced by
 * the node itself (three from the baked surface height grid, one from analytic noise), so there is no
 * user-supplied channel to name, no typo path, and no name coupling between the species asset and the
 * bake to police. Adding an axis is one case here plus one arm in the single switch in
 * ArcForestField::EvaluateHabitatResponse.
 *
 * WHERE EACH VALUE COMES FROM, and what "absent" means per axis (see FArcForestHabitatInputs):
 *   Slope / SunExposure / Elevation  — bilinear samples of the derived morphology grids
 *                                      (Forest/ArcForestTerrainFields.h). Absent == empty grid.
 *   Moisture                         — analytic fBm evaluated at the exact position
 *                                      (Forest/ArcForestNoise.h). Absent == Frequency <= 0.
 *
 * Lives here, not on the species asset, so the solver-side LUT bundle (Forest/ArcForestField.h) can
 * name an axis without pulling in the species data asset or Engine's curve types.
 */
UENUM(BlueprintType)
enum class EArcForestHabitatField : uint8
{
	/** Terrain steepness normalised by the node's MaxSlopeDegrees; 0 = flat, 1 = at/over the max. */
	Slope UMETA(DisplayName = "Slope"),

	/** How well the slope faces the sun azimuth; 0 = facing away, 0.5 = flat ground, 1 = facing it. */
	SunExposure UMETA(DisplayName = "Sun Exposure"),

	/** World Z normalised by the node's ABSOLUTE elevation range (never per-partition extrema). */
	Elevation UMETA(DisplayName = "Elevation"),

	/** Analytic noise field; 0 = driest, 1 = wettest. No terrain data is authored for it. */
	Moisture UMETA(DisplayName = "Moisture")
};

/** Number of habitat axes; keep in sync with EArcForestHabitatField. */
static constexpr int32 GArcForestHabitatFieldCount = 4;

/**
 * The birth state of EVERY new plant, whatever its species' mature form (the MC1 regeneration
 * keystone — see the DECISION block in ArcForestSimulator.cpp): a tiny seedling well inside the Field
 * band that must survive and grow up through the bands, never a plant born into the canopy.
 *
 * They live HERE rather than only in the solver .cpp because two things outside that file now need the
 * same numbers: FArcForestSimParams::RecruitQueryHeightCm defaults to the seedling height (the raster
 * must be sampled at the height of the plant recruitment is placing), and the recruitment spacing rule
 * is written against the seedling crown radius. One authority, no drifting literals.
 */
namespace ArcForestSim
{
	/** Height (cm) a newly seeded or recruited plant starts at. */
	constexpr float SeedlingHeightCm = 10.0f;

	/** Crown/influence radius (cm) a newly seeded or recruited plant starts at. */
	constexpr float SeedlingCrownRadiusCm = 5.0f;
}

/**
 * Fixed salt mixed into the per-iteration RECRUITMENT random stream so it is disjoint from the seeding
 * stream (which is a plain FRandomStream(Params.Seed)).
 *
 * The value is arbitrary but must never change: it is part of the reproducibility contract for any
 * saved seed. It is a NAMED constant precisely so the derivation reads as
 * (Seed, salt, 1-based iteration) — the three inputs that make the stream CHUNK-INVARIANT, since all
 * three live in Params/State and none of them is a per-call cursor.
 */
static constexpr uint32 ArcForestRecruitSalt = 0x5EED1A57u;

/**
 * Global tuning parameters handed to the pure forest solver.
 *
 * DESIGN DECISION — this is a PLAIN (non-USTRUCT) struct, deliberately. It is part of the
 * pure-function contract consumed by the worker-thread solver (Step 6), which takes plain
 * arrays and has zero UObject/PCG dependency so a later GPU port only rewrites the core.
 * A USTRUCT would require UPROPERTY reflection, and the per-band flag sets are naturally
 * TStaticArray<bool,4> which is not a UPROPERTY-friendly type. The PCG settings node
 * (Step 7) exposes these scalars individually as UPROPERTYs and populates this struct on
 * the game thread before dispatching the solver — so editor exposure lives on the settings
 * object, not here.
 *
 * Bounds2D is the XY footprint of the 3D surface bounds (L2): competition and crown
 * distance are measured in the XY plane, Z is projection-only (points are dropped onto the
 * surface afterward). Distances are therefore L2 in XY, never full 3D.
 */
struct FArcForestSimParams
{
	/** RNG seed for the whole run; identical seed → identical survivors (determinism). */
	int32 Seed = 0;

	/**
	 * GLOBAL DENSITY MULTIPLIER applied on top of every species' per-species TargetDensity. 1.0 = seed
	 * each species at exactly its authored TargetDensity; 2.0 = twice as many candidates of every species;
	 * 0.5 = half. It scales the whole mix uniformly and does NOT change the relative balance between
	 * species (that lives in each row's TargetDensity).
	 */
	float SeedingDensity = 1.0f;

	/**
	 * Fraction of a species' MATURE crown radius used as the seed-time minimum spacing between plants.
	 * 1.0 = mature crowns just touch (plants seeded a full mature-radius apart); <1.0 lets seedlings start
	 * closer together and thin out later through competition. Lower values = a denser initial scatter that
	 * self-thins; higher values = a sparser, already-spaced initial layout.
	 */
	float SpacingFraction = 0.8f;

	/** Number of growth/competition iterations (K); controls forest maturity. */
	int32 Iterations = 32;

	/** Beer-Lambert extinction coefficient k for top-down light attenuation. */
	float CanopyExtinction = 0.5f;

	/**
	 * Patchiness dial — the CLUMPED FRACTION of seed-time darts. A dart is drawn as a Gaussian
	 * offspring of one of the species' clump parents with probability p = WPatch / (1 + WPatch), and
	 * uniformly over the bounds otherwise: 0 = every dart uniform (no clumping), 1 = half clumped,
	 * large = nearly all clumped. It no longer weights an affinity/acceptance term, and it never
	 * picks a winner species. A species with no clump parents is always uniform regardless.
	 */
	float WPatch = 1.0f;

	/**
	 * Run the trivially-independent per-plant solver stages and the competition gather across worker
	 * threads (ParallelFor). Plain field, no CVar — keeps the pure-core contract self-contained. The
	 * competition gather is a per-plant sole-writer over a deterministic neighbour order (QueryRadius
	 * scans cells in a fixed loop), so the result is bit-identical whether this is true or false; the
	 * flag is a pure speed/serial toggle. (Incoming penalties are additionally summed in a canonical
	 * neighbour-index order as defensive cross-platform bit-reproducibility — not what makes serial==
	 * parallel on a single binary.)
	 */
	bool bParallel = true;

	/**
	 * Per-band horizontal competition (radius pushing / suppression) toggle.
	 * Default on in all bands — guarantees "nothing grows inside anything".
	 * Indexed by EArcForestLayer.
	 */
	TStaticArray<bool, GArcForestLayerCount> IntraLayerCompetition = InitOnFlags();

	/**
	 * Per-band vertical self-shading (a tall fern shades a short fern by light, not just
	 * radius push). Default on for the Field band only, off above. Indexed by EArcForestLayer.
	 */
	TStaticArray<bool, GArcForestLayerCount> IntraLayerShading = InitFieldOnlyFlags();

	/**
	 * Master switch for in-simulation RECRUITMENT (new seedlings established under the standing
	 * canopy between iterations).
	 *
	 * DEFAULTS OFF, and that default is load-bearing: it is what makes the recruitment machinery
	 * behaviour-neutral for every existing solve and every existing test. Off, the solver does not even
	 * build the query-height transmittance raster recruitment needs, so the disabled path costs nothing
	 * beyond this one branch — no extra allocation, no extra exp() over the light grid.
	 *
	 * It also gates no RNG draw of its own while off, so the seeding stream stays bit-identical.
	 */
	bool bEnableRecruitment = false;

	/**
	 * Fraction of the population ceiling ATTEMPTED per iteration: Budget = Round(Rate * Ceiling), and
	 * exactly that many darts are thrown (fewer once the stand is close to the ceiling). It is an
	 * attempt rate, NOT an establishment rate — habitat response, shade preference and the minimum
	 * spacing all reject darts, so the realised recruit count is always at or below this.
	 */
	float RecruitmentRate = 0.1f;

	/**
	 * Hard cap on the live plant count, and the base the per-iteration recruitment budget is a fraction
	 * of. 0 (the default) DERIVES it as Sum over species of Max(0, Round(TargetDensity * SeedingDensity
	 * * AreaMeters2)) — literally the same expression seeding uses for its candidate budget, so "the
	 * ceiling" means "as many plants as a fresh seeding pass would have attempted". A non-zero value
	 * overrides that derivation.
	 *
	 * HONEST LIMIT: the derived value is an upper bound on SEEDED candidates, not a mature carrying
	 * capacity. With weak mortality a stand can sit at the ceiling permanently, at which point
	 * Attempts is 0 every iteration and recruitment quietly stops.
	 */
	int32 PopulationCeiling = 0;

	/**
	 * LAI-grid cells aggregated per transmittance-raster cell, per axis, for the recruitment raster
	 * (see FArcForestLightModel::ComputeIncidentLight). At the solver's 100 cm light cells the default 4
	 * is a 4 m raster cell: ~16x cheaper than a 1:1 raster (the LAI grid's dims are an UNBOUNDED
	 * ceil(span / 100 cm), so a 500 m x 500 m volume is 250,000 cells and as many exp() calls), and
	 * still fine enough to resolve a mature crown's shade footprint against a decision whose spatial
	 * precision is a seedling's 5 cm crown. Aggregation averages LAI BEFORE the exp(), which slightly
	 * under-states a small dense crown's shade (Jensen).
	 */
	int32 RecruitStride = 4;

	/**
	 * Height (cm) the transmittance raster is computed for — the shade a plant of THIS height would
	 * read. Defaults to the seedling height because that is the plant recruitment actually places; a
	 * different value answers "could a taller sapling establish here", which is a different question.
	 */
	float RecruitQueryHeightCm = ArcForestSim::SeedlingHeightCm;

	/**
	 * Absolute floor (cm) on the distance between a recruit and its nearest neighbour, under the
	 * recruitment spacing rule Max(RecruitMinSpacingCm, SpacingFraction * (SeedlingCrownRadiusCm +
	 * neighbour's LIVE CrownRadius)).
	 *
	 * The floor is what makes the rule usable: recruit-vs-recruit the radius term is only
	 * 0.8 * (5 + 5) = 8 cm, and because acceptance peaks in the same few dark cells the recruits would
	 * pack into those cells as a clot. Note the rule uses the neighbour's LIVE radius, never its mature
	 * MaxCrownRadius — seeding rejects on mature radii because it is placing a stand that will grow into
	 * itself, whereas a recruit is a 5 cm plant slotting into an existing stand and a mature-radius
	 * rejection would forbid all understory.
	 */
	float RecruitMinSpacingCm = 100.0f;

	/** XY footprint of the surface bounds; all planar competition/crown math lives here. */
	FBox2f Bounds2D = FBox2f(ForceInit);

private:
	/** All-true default for the competition flag set. */
	static TStaticArray<bool, GArcForestLayerCount> InitOnFlags()
	{
		TStaticArray<bool, GArcForestLayerCount> Flags;
		for (int32 Index = 0; Index < GArcForestLayerCount; ++Index)
		{
			Flags[Index] = true;
		}
		return Flags;
	}

	/** Field-band-only default for the self-shading flag set. */
	static TStaticArray<bool, GArcForestLayerCount> InitFieldOnlyFlags()
	{
		TStaticArray<bool, GArcForestLayerCount> Flags;
		for (int32 Index = 0; Index < GArcForestLayerCount; ++Index)
		{
			Flags[Index] = false;
		}
		Flags[static_cast<int32>(EArcForestLayer::Field)] = true;
		return Flags;
	}
};
