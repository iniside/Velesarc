// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Forest/ArcForestField.h"
#include "Forest/ArcForestSpatialHash.h"

struct FArcForestSimParams;
class UArcForestSpeciesDefinition;

/**
 * Per-species clump-parent lattice — the parent stage of an inhomogeneous Thomas cluster process.
 *
 * Real forest floors are conspecific PATCHES (clonal spread, dispersal limitation, microsite
 * filtering), not a uniform species mix. This field reproduces that: for each species it
 * scatters cluster "parent" centres, and additionally defines an affinity scalar as a sum of
 * Gaussian bumps around those parents.
 *
 * PRIMARY CONSUMER: the forest seeder reads the PARENT SET itself — GetParentPositions /
 * GetParentIds / GetPatchRadiusSigma — and places a clumped dart directly as
 * `Parent + Gaussian(0, σ)` (the offspring stage of the Thomas process), which is what produces
 * genuine visible aggregation. SampleAffinity / SampleDominantPatchId are NO LONGER on the seeding
 * path (the old `suitability + w_patch·affinity` acceptance was dropped because concentration now
 * comes from the position distribution); they remain as an analysis/observability surface.
 *
 * INHOMOGENEOUS (the Thomas twist): parents are NOT uniform. Candidate centres are drawn
 * uniformly over the bounds at the species' expected intensity, then THINNED by the pre-baked
 * suitability grid — a candidate at position p is accepted with probability = suitability(p).
 * Parents therefore concentrate in suitable microsites. (Thinning of a homogeneous candidate
 * set by the suitability field == an inhomogeneous Poisson parent process; we use a
 * deterministic candidate COUNT = round(PatchDensity·Area) rather than sampling a Poisson
 * variate, for exact reproducibility — the accepted set is still spatially inhomogeneous.)
 *
 * AFFINITY: each parent contributes exp(−dist² / (2σ²)), σ = species PatchRadiusSigma.
 * SampleAffinity sums the bumps of nearby parents. Only parents within TruncationSigmas·σ
 * (≈3σ, exp(−4.5)≈0.011) are queried — the Gaussian tail beyond that is dropped, so the
 * field is O(local parent density) per sample instead of O(all parents). Truncation is a
 * documented approximation, not exact.
 *
 * PATCH ID: every accepted parent carries a stable, globally-unique id (assigned in Build
 * order across all species). SampleDominantPatchId reports the id of the parent contributing
 * the most at a position. Because all of a species' parents share one σ, the largest bump ==
 * the NEAREST parent, so "dominant" == nearest. Tie-break: smallest PatchId (stable across
 * runs). No parent within the truncation radius → INDEX_NONE. Downstream keys mesh variants
 * (core-vs-edge, per-clone tint) off this id.
 *
 * DETERMINISM + PER-SPECIES DECORRELATION: each species gets its own FRandomStream seeded
 * from HashCombine(uint32(Params.Seed), uint32(SpeciesIndex + 1)). The per-species mix means
 * different species' parent lattices are statistically independent — patches INTERLEAVE
 * rather than co-locate — while identical inputs reproduce identical fields. (+1 keeps
 * species 0 off the raw seed value.)
 *
 * PURITY: Core math only — no PCG, no UObject mutation. Build reads the species data asset
 * (const, thread-safe read) purely for its scalar table, matching the Step 6 solver contract;
 * everything else is plain arrays, so this lives inside the worker-thread solver boundary.
 *
 * LIFETIME: each species owns a FArcForestSpatialHash that holds a NON-OWNING view over that
 * species' parent-positions array (see FArcForestSpatialHash's contract). Those arrays are
 * members here and are populated in full BEFORE any hash is built, so no reallocation ever
 * invalidates a hash's view. Do not resize the per-species arrays after Build.
 *
 * ACCESSORS: GetParentPositions / GetParentIds / GetNumSpecies / GetPatchRadiusSigma expose each
 * species' parent point set. These are the SEEDER'S production inputs (offspring placement, PatchId
 * inheritance) as well as the test-observability surface for a pair-correlation g(r) validation
 * (clustering at r≈σ, g(r) > 1 near the patch radius). An empty parent set for a species (σ ≤
 * MinSigma, PatchDensity = 0, a candidate count that rounds to 0, or every candidate
 * suitability-thinned) is the documented "no clumping — scatter uniformly" signal.
 */
class ARCPCGEXTENSION_API FArcForestPatchField
{
public:
	// Non-copyable: each species' FArcForestSpatialHash holds a non-owning view into that
	// species' ParentPositions array. A copy would deep-copy ParentPositions but leave the
	// hash's view pointing at the source's buffer → dangling read (use-after-free). Move is
	// safe: moving the outer TArray steals the element buffer, so the ParentPositions heap
	// allocations keep their addresses and the views stay valid.
	FArcForestPatchField() = default;
	FArcForestPatchField(const FArcForestPatchField&) = delete;
	FArcForestPatchField& operator=(const FArcForestPatchField&) = delete;
	FArcForestPatchField(FArcForestPatchField&&) = default;
	FArcForestPatchField& operator=(FArcForestPatchField&&) = default;

	/**
	 * Build every species' parent set + acceleration hash from the species table.
	 *
	 * @param Species         Species table; index in Species.Species is the SpeciesIndex used
	 *                        by all sample/accessor calls. PatchDensity (parents per m²) and
	 *                        PatchRadiusSigma (cm) drive each species' process.
	 * @param Params          Global params; Seed drives per-species RNG, Bounds2D (world XY,
	 *                        cm) is the scatter region and area source.
	 * @param SuitabilityGrid Row-major (X fastest) pre-baked suitability in [0,1], indexed
	 *                        y*GridDims.X + x, spanning Params.Bounds2D. Empty grid → uniform
	 *                        suitability 1.0 (all candidates accepted). Values outside [0,1]
	 *                        are clamped before use as an acceptance probability.
	 * @param GridDims        Grid dimensions (X = columns, Y = rows).
	 * @param Habitat         Self-derived habitat inputs (morphology fields, noise params,
	 *                        per-species response LUTs). TRAILING DEFAULTED: omitting it means
	 *                        "no habitat inputs", which is exactly today's behaviour — parent
	 *                        thinning is suitability-only. Held by const reference and read only,
	 *                        so it must outlive the call (the shared empty instance is static).
	 */
	void Build(
		const UArcForestSpeciesDefinition& Species,
		const FArcForestSimParams& Params,
		TArrayView<const float> SuitabilityGrid,
		FIntPoint GridDims,
		const FArcForestHabitatInputs& Habitat = FArcForestHabitatInputs::Empty());

	/** Conspecific-affinity scalar at Pos for the given species (sum of nearby Gaussian bumps). */
	float SampleAffinity(int32 SpeciesIndex, FVector2f Pos) const;

	/** Stable id of the dominant (nearest) parent at Pos, or INDEX_NONE if none within ~3σ. */
	int32 SampleDominantPatchId(int32 SpeciesIndex, FVector2f Pos) const;

	/**
	 * Combined single-query form: returns the affinity and writes the dominant parent id.
	 * The two convenience calls above delegate here — use this when both are needed.
	 */
	float SampleAffinity(int32 SpeciesIndex, FVector2f Pos, int32& OutDominantPatchId) const;

	/** Number of species built (== Species.Species.Num() at Build time). */
	int32 GetNumSpecies() const { return PerSpecies.Num(); }

	/** Accepted parent centres for a species (test observability for g(r)); empty if OOB. */
	TArrayView<const FVector2f> GetParentPositions(int32 SpeciesIndex) const;

	/** Stable parent ids for a species, parallel to GetParentPositions; empty if OOB. */
	TArrayView<const int32> GetParentIds(int32 SpeciesIndex) const;

	/** The Gaussian σ (cm) used for a species' bumps; 0 if OOB / non-patching. */
	float GetPatchRadiusSigma(int32 SpeciesIndex) const;

private:
	/** Per-species parent set + its acceleration hash. Arrays are stable after Build. */
	struct FSpeciesPatches
	{
		/** Accepted parent centres in world XY (cm). Backing store for Hash's view. */
		TArray<FVector2f> ParentPositions;

		/** Globally-unique stable id per parent, parallel to ParentPositions. */
		TArray<int32> ParentIds;

		/** Gaussian σ (cm) for this species' bumps. */
		float Sigma = 0.0f;

		/** Truncation query radius (TruncationSigmas·σ) for affinity/dominant lookups. */
		float QueryRadius = 0.0f;

		/** Hash over ParentPositions (non-owning view — see class LIFETIME note). */
		FArcForestSpatialHash Hash;
	};

	/** Indexed by SpeciesIndex. */
	TArray<FSpeciesPatches> PerSpecies;
};
