// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "Forest/ArcForestField.h"
#include "Forest/ArcForestSimTypes.h"

class UArcForestSpeciesDefinition;

/**
 * Structure-of-arrays result of a forest simulation run: one entry per SURVIVING plant,
 * all arrays parallel (same length == survivor count, == Num()).
 *
 * This is the ecological state the pure solver produces; the PCG layer (Step 7) adds
 * per-point presentation data (InstanceSeed, surface Z projection, config soft-paths).
 * MatureLayer is the SPECIES mature-form band (authoring/mesh metadata) — NOT the live
 * competition band; per MC1 the live band is derived from Height and is not persisted here.
 */
struct FArcForestResult
{
	/** Plant XY positions in world units (cm), same frame as FArcForestSimParams::Bounds2D. */
	TArray<FVector2f> PosXY;

	/** Index into UArcForestSpeciesDefinition::Species for each survivor. */
	TArray<int32> Species;

	/** Species mature-form band (mesh/authoring metadata only — not the live band). */
	TArray<EArcForestLayer> MatureLayer;

	/** Final crown (influence) radius in cm. */
	TArray<float> CrownRadius;

	/** Final height in cm. */
	TArray<float> Height;

	/** Iterations survived (0 at seed time, +1 per completed iteration a plant lived through). */
	TArray<int32> Age;

	/** Final vigour in [0,1] — how suppressed the plant is (1 = healthy, 0 = dying). */
	TArray<float> Vigor;

	/** Dominant conspecific patch id (from FArcForestPatchField), or INDEX_NONE if none. */
	TArray<int32> PatchId;

	/** Survivor count (all arrays share this length). */
	int32 Num() const
	{
		return PosXY.Num();
	}
};

/**
 * Resumable working state of a forest solve: the live plant population as parallel SoA arrays
 * (mirrors FArcForestResult plus the solver-only StarvationCounter) PLUS two resume cursors so an
 * external driver can seed, advance the K-iteration loop in chunks, and emit. The solve is
 * bit-identical regardless of how the iterations are chunked because ALL live state lives here and
 * only pure scratch is re-declared per StepIterations call.
 *
 * Plain SoA, mirroring FArcForestResult's header-only style: NO ARCPCGEXTENSION_API on the struct,
 * no UObject/UPROPERTY, no .generated.h.
 */
struct FArcForestSolveState
{
	/** Plant XY positions in world units (cm), same frame as FArcForestSimParams::Bounds2D. */
	TArray<FVector2f> Pos;

	/** Index into UArcForestSpeciesDefinition::Species for each plant. */
	TArray<int32> Species;

	/** Current crown (influence) radius in cm. */
	TArray<float> CrownRadius;

	/** Current height in cm. */
	TArray<float> Height;

	/** Iterations survived so far (0 at seed time, +1 per completed iteration). */
	TArray<int32> Age;

	/** Current vigour in [0,1] (1 = healthy, 0 = dying). */
	TArray<float> Vigor;

	/** Dominant conspecific patch id (from FArcForestPatchField), or INDEX_NONE if none. */
	TArray<int32> PatchId;

	/** Consecutive iterations spent below the mortality vigour floor (solver-only). */
	TArray<int32> StarvationCounter;

	/** Resume cursor: number of completed iterations so far (0 at seed time). */
	int32 Iteration = 0;

	/** True once SeedInitial has populated this state. */
	bool bSeeded = false;

	/** Plant count (all arrays share this length). */
	int32 Num() const
	{
		return Pos.Num();
	}

	/** Reserve every parallel array up front (seed time knows the total candidate count). */
	void Reserve(int32 Count)
	{
		Pos.Reserve(Count);
		Species.Reserve(Count);
		CrownRadius.Reserve(Count);
		Height.Reserve(Count);
		Age.Reserve(Count);
		Vigor.Reserve(Count);
		PatchId.Reserve(Count);
		StarvationCounter.Reserve(Count);
	}

	void Add(FVector2f InPos, int32 InSpecies, float InRadius, float InHeight, int32 InPatchId)
	{
		Pos.Add(InPos);
		Species.Add(InSpecies);
		CrownRadius.Add(InRadius);
		Height.Add(InHeight);
		Age.Add(0);
		Vigor.Add(1.0f);
		PatchId.Add(InPatchId);
		StarvationCounter.Add(0);
	}

	/** Copy element From -> To across every parallel array (used by the compaction write cursor). */
	void MoveElement(int32 To, int32 From)
	{
		Pos[To] = Pos[From];
		Species[To] = Species[From];
		CrownRadius[To] = CrownRadius[From];
		Height[To] = Height[From];
		Age[To] = Age[From];
		Vigor[To] = Vigor[From];
		PatchId[To] = PatchId[From];
		StarvationCounter[To] = StarvationCounter[From];
	}

	void SetNum(int32 NewNum)
	{
		Pos.SetNum(NewNum);
		Species.SetNum(NewNum);
		CrownRadius.SetNum(NewNum);
		Height.SetNum(NewNum);
		Age.SetNum(NewNum);
		Vigor.SetNum(NewNum);
		PatchId.SetNum(NewNum);
		StarvationCounter.SetNum(NewNum);
	}
};

/**
 * The pure forest simulator — the integration heart wiring Steps 2-5 (types, spatial hash,
 * light model, patch field) into a seed -> iterate -> survivors solver.
 *
 * PURITY (MC3): Core math only. No PCG, no UObject-graph mutation, no live surface sampling.
 * Suitability arrives as a pre-baked plain grid (sampled on the game thread by Step 7). The
 * whole run is therefore safe on a worker thread and a later GPU port rewrites only this core.
 * The species data asset is read const, purely for its scalar table.
 *
 * DETERMINISM (MANDATORY): identical (Params, Species, SuitabilityGrid, GridDims) -> identical
 * result. A single FRandomStream seeded from Params.Seed drives seeding; the patch field uses
 * its own per-species streams (also seed-derived). No FMath::Rand, no wall-clock, no
 * order-unstable steps: every per-iteration sort is a total order (index tie-break) and every
 * pairwise decision (competition weaker-of-two, mortality) reads a snapshot so the outcome is
 * independent of traversal order.
 *
 * HABITAT INPUTS: every entry point takes a TRAILING DEFAULTED `const FArcForestHabitatInputs&`
 * (see ArcForestField.h). Omitting it selects the shared empty instance, which means "no
 * self-derived habitat" and reproduces the suitability-only behaviour exactly — that is why
 * adding it required no change at any existing call site. It is read-only on the worker and must
 * outlive the call; do not pass a temporary that dies before Simulate/StepIterations returns.
 *
 * See ArcForestSimulator.cpp for the full documented tuning surface (seeding score, growth
 * response f, vigour/stress curve, competition suppression, mortality threshold, the
 * CrownLAIDensity proxy, and ShouldYield semantics) — those curves are the model.
 */
class FArcForestSimulator
{
public:
	/**
	 * Run the simulation to completion, or until ShouldYield() first returns true.
	 *
	 * SHOULDYIELD SEMANTICS: this is a single, non-resumable Simulate call. ShouldYield is a
	 * cooperative EARLY-ABORT check evaluated once between iterations (after each completed
	 * iteration). When it returns true the loop stops and the CURRENT survivors are returned as
	 * a valid (if less mature) result — never a partial/torn state. Step 7 passes a functor
	 * polling Context->ShouldStop() so the PCG scheduler can reclaim the worker.
	 *
	 * @param Params          Global tuning + Bounds2D (world XY, cm). Seed drives determinism.
	 * @param Species         Species table; array index is the emitted Species id.
	 * @param SuitabilityGrid Row-major (X fastest) pre-baked suitability in [0,1] over Bounds2D,
	 *                        index = Row*GridDims.X + Col. Empty -> uniform suitability 1.0.
	 * @param GridDims        Suitability grid dimensions (X = columns, Y = rows).
	 * @param ShouldYield     Early-abort poll, checked between iterations (see semantics above).
	 * @param Habitat         Self-derived habitat inputs — see the HABITAT note below.
	 * @return                SoA survivor arrays (all parallel, length == survivor count).
	 */
	ARCPCGEXTENSION_API FArcForestResult Simulate(
		const FArcForestSimParams& Params,
		const UArcForestSpeciesDefinition& Species,
		TArrayView<const float> SuitabilityGrid,
		FIntPoint GridDims,
		TFunctionRef<bool()> ShouldYield,
		const FArcForestHabitatInputs& Habitat = FArcForestHabitatInputs::Empty()) const;

	/** Convenience overload that runs to completion (ShouldYield always false). */
	ARCPCGEXTENSION_API FArcForestResult Simulate(
		const FArcForestSimParams& Params,
		const UArcForestSpeciesDefinition& Species,
		TArrayView<const float> SuitabilityGrid,
		FIntPoint GridDims,
		const FArcForestHabitatInputs& Habitat = FArcForestHabitatInputs::Empty()) const;

	/**
	 * Seed the initial population into State (resumable-driver entry point). Builds the conspecific
	 * FArcForestPatchField LOCALLY — it holds non-owning spatial-hash views and must NOT persist past
	 * seeding — seeds via SeedPopulation, then sets State.bSeeded=true and State.Iteration=0.
	 */
	ARCPCGEXTENSION_API void SeedInitial(
		FArcForestSolveState& State,
		const FArcForestSimParams& Params,
		const UArcForestSpeciesDefinition& Species,
		TArrayView<const float> SuitabilityGrid,
		FIntPoint GridDims,
		const FArcForestHabitatInputs& Habitat = FArcForestHabitatInputs::Empty()) const;

	/**
	 * Advance the solve by up to MaxIters iterations (MaxIters <= 0 runs to completion). All
	 * per-iteration scratch is declared locally; the only live state persists in State, so the result
	 * is bit-identical regardless of how the iterations are chunked. Returns true once the solve is
	 * finished (State.Iteration >= Params.Iterations, or the population died out).
	 */
	ARCPCGEXTENSION_API bool StepIterations(
		FArcForestSolveState& State,
		const FArcForestSimParams& Params,
		const UArcForestSpeciesDefinition& Species,
		TArrayView<const float> SuitabilityGrid,
		FIntPoint GridDims,
		int32 MaxIters,
		const FArcForestHabitatInputs& Habitat = FArcForestHabitatInputs::Empty()) const;

	/** Emit the SoA survivor result from State, resolving each survivor's species MatureLayer. */
	ARCPCGEXTENSION_API FArcForestResult Emit(
		const FArcForestSolveState& State,
		const UArcForestSpeciesDefinition& Species) const;
};
