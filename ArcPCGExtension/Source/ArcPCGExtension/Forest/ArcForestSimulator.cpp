// Copyright Lukasz Baran. All Rights Reserved.

#include "Forest/ArcForestSimulator.h"

#include "ArcPCGExtensionModule.h"
#include "Forest/ArcForestField.h"
#include "Forest/ArcForestLightModel.h"
#include "Forest/ArcForestPatchField.h"
#include "Forest/ArcForestSpatialHash.h"
#include "Forest/ArcForestSpeciesDefinition.h"
#include "Async/ParallelFor.h"
#include "Math/RandomStream.h"

// =====================================================================================================
// TUNING SURFACE — the documented ecological model. Every constant/curve below is a knob the
// hostile reviewer and Step 8 tests scrutinise against the cited ecology (light-demanding vs
// shade-tolerant succession, self-thinning, regeneration keystone). Rationale is inline.
// =====================================================================================================
namespace ArcForestSimulatorLocal
{
	/** 1 m = 100 cm. Bounds2D/heights/radii are cm; SeedingDensity/PatchDensity are per m^2. */
	static constexpr float CmPerMeter = 100.0f;

	// --- Band classification (cm) ---------------------------------------------------------------
	// LayerForHeight defaults are in METRES (see ArcForestLightModel.h unit note); the solver works
	// in cm, so it MUST pass cm-scaled thresholds or self-shading bands are misclassified. These are
	// the doc-table boundaries (1.5 m / 5 m / 15 m) converted once, here.
	static constexpr float ShrubMinHeightCm = ArcForestLight::DefaultShrubMinHeight * CmPerMeter;       // 150
	static constexpr float UnderstoryMinHeightCm = ArcForestLight::DefaultUnderstoryMinHeight * CmPerMeter; // 500
	static constexpr float CanopyMinHeightCm = ArcForestLight::DefaultCanopyMinHeight * CmPerMeter;     // 1500

	// --- Seedling initial state -----------------------------------------------------------------
	// DECISION: every plant is born a tiny seedling well inside the Field band, regardless of its
	// species' mature form. This is the MC1 regeneration keystone: a canopy-species seedling starts
	// SHORT, lives in the shaded Field/Shrub band, and must survive + grow up through the bands —
	// it is NOT born into the canopy. A fixed small height/radius (not a species field) keeps that
	// invariant uniform across species. It holds for RECRUITS too: a recruit is born at exactly the
	// same size as a seed-time plant, which is why the numbers now live in ArcForestSimTypes.h (the
	// recruitment settings and the recruit spacing rule are written against them) and are aliased here.
	static constexpr float SeedlingHeightCm = ArcForestSim::SeedlingHeightCm;
	static constexpr float SeedlingCrownRadiusCm = ArcForestSim::SeedlingCrownRadiusCm;

	// --- Light splat density (CrownLAIDensity proxy) --------------------------------------------
	// DECISION: there is NO per-species leaf-area field yet. Every plant splats a uniform crown LAI
	// of 2.0 (typical single-tree/dense-shrub crown LAI is ~2-5) into the light accumulator. With
	// CanopyExtinction k defaulting to 0.5, one overlapping crown transmits exp(-1.0)=0.37, two
	// exp(-2.0)=0.14 — a closed canopy genuinely darkens the understory. CANDIDATE FUTURE SPECIES
	// FIELD: a per-species CrownLeafAreaIndex (a dense fir canopy vs a sparse birch) would refine
	// shade quality; for the MVP a documented constant keeps the tuning surface small.
	static constexpr float CrownLeafAreaIndex = 2.0f;

	// --- Light grid resolution ------------------------------------------------------------------
	// ~1 m cells (doc: "~1 m field"). Coarser than a seedling crown (seedling shade is negligible),
	// fine enough to resolve mature-crown shade footprints. Perf: one exp() per plant + crown-area
	// splat writes; bounded for the MVP region (see ArcForestLightModel.cpp M5 note).
	static constexpr float LightGridCellSizeCm = 100.0f;

	// --- Recruitment transmittance raster resolution ---------------------------------------------
	// NOT a constant here: the raster stride is FArcForestSimParams::RecruitStride (authored on the
	// node), which is where its cost/resolution rationale is documented. There is deliberately no local
	// literal — a third copy of "4" alongside ArcForestLight::DefaultTransmittanceStride and the setting
	// default is exactly the drift this note replaces.

	// --- Growth response f(incidentLight, LightNeeded, ShadeTolerance) ---------------------------
	// GROWTH CURVE (the ecological core). LightNeeded = Ln, ShadeTolerance = St.
	//   Demand(L) = L*(1+Ln) / (L+Ln)  — saturating (Michaelis-Menten), normalised so full light
	//     L=1 yields Demand=1 for EVERY species. Half-saturation sits at L=Ln: a high-Ln pioneer is
	//     strongly light-limited (low growth until light is high), a low-Ln shade species is more
	//     efficient and reaches high growth at lower light. Concave => diminishing returns.
	//   f = clamp( Demand + St*ShadeFloorGain*(1-Demand), 0, 1 ) — the shade-tolerance floor lifts
	//     low-light growth by up to ShadeFloorGain of the remaining headroom, scaled by St. In deep
	//     shade a tolerant plant (St~1) still makes slow positive growth; a pioneer (St~0) stalls.
	//   At full light f=1 for all: the pioneer-vs-tolerant SPEED tradeoff is carried by the species
	//     GrowthRate (pioneers author a larger GrowthRate for fast gap capture), while f carries the
	//     low-light EFFICIENCY tradeoff. This is the JABOWA/SORTIE growth-light response.
	static constexpr float ShadeFloorGain = 0.4f;

	// --- Vigour / stress curve ------------------------------------------------------------------
	// EffectiveNeed = Ln * (1 - St*ShadeReliefFactor): shade tolerance LOWERS the light a plant
	//   needs to stay healthy (a tolerant species survives deep shade a pioneer would die in).
	//   ShadeReliefFactor=0.8 => a fully tolerant species needs only 20% of a pioneer's light.
	// Per iteration, using the incident light L computed this iteration:
	//   L >= EffectiveNeed -> Vigor += VigorRecovery*(1-Vigor)   (recover toward 1)
	//   L <  EffectiveNeed -> Vigor -= VigorDecay*(EffectiveNeed-L)/EffectiveNeed  (decay by relative deficit)
	//   Vigor clamped to [0,1]. Deeper shade => larger relative deficit => faster decline.
	static constexpr float ShadeReliefFactor = 0.8f;
	static constexpr float VigorRecovery = 0.34f;
	static constexpr float VigorDecay = 0.5f;

	// --- Horizontal competition (self-thinning) -------------------------------------------------
	// SUPPRESSION RULE: within a band whose IntraLayerCompetition flag is ON, every pair of plants
	//   with OVERLAPPING crowns (dist < r_i + r_j) suppresses the WEAKER of the two. "Weaker" is a
	//   total order read from a vigour SNAPSHOT taken before the step (so the outcome is independent
	//   of traversal order): lower snapshot vigour loses; tie -> shorter plant loses; tie -> higher
	//   global index loses. The weaker plant accrues a vigour penalty of CompetitionPenaltyPerPair
	//   scaled by the overlap fraction (r_i+r_j-dist)/(r_i+r_j) in (0,1]. Penalties ACCUMULATE across
	//   all overlapping neighbours, then apply once — a plant crowded by many stronger neighbours is
	//   heavily suppressed and eventually dies (Yoda -3/2 / Reineke self-thinning), rather than being
	//   killed instantly by a single contact.
	static constexpr float CompetitionPenaltyPerPair = 0.15f;

	/** Floor for the per-band competition hash cell size (cm); guards a degenerate all-seedling band. */
	static constexpr float MinCompetitionCellCm = 1.0f;

	// --- Mortality ------------------------------------------------------------------------------
	// A plant is "starving" while Vigor < VigorMortalityFloor. It DIES when it has starved for
	// StarvationLimit consecutive iterations (a persistent-stress death, not a one-bad-iteration
	// death), or immediately if Vigor reaches 0. Recovering above the floor resets the counter.
	static constexpr float VigorMortalityFloor = 0.2f;
	static constexpr int32 StarvationLimit = 4;

	// Suitability sampling lives in ArcForestField::SampleFieldNearest — the single authority
	// shared with FArcForestPatchField (row-major, X fastest, index = Row*Dims.X + Col over
	// Bounds; empty grid -> 1.0).

	/** Michaelis-Menten growth-light response, normalised so full light (L=1) -> 1 for every species. */
	float GrowthResponse(float IncidentLight, float LightNeeded, float ShadeTolerance)
	{
		const float LightVal = FMath::Clamp(IncidentLight, 0.0f, 1.0f);
		const float ClampedLightNeeded = FMath::Max(LightNeeded, KINDA_SMALL_NUMBER);
		const float ClampedShadeTolerance = FMath::Clamp(ShadeTolerance, 0.0f, 1.0f);

		const float Demand = (LightVal * (1.0f + ClampedLightNeeded)) / (LightVal + ClampedLightNeeded);
		const float Response = Demand + (ClampedShadeTolerance * ShadeFloorGain * (1.0f - Demand));
		return FMath::Clamp(Response, 0.0f, 1.0f);
	}

}

namespace ArcForestSimulatorLocal
{
	/**
	 * Seed the initial population (MC2 — NO competition term; no plants exist yet) via per-stratum,
	 * per-species Poisson-disk dart-throwing.
	 *
	 * MODEL: species no longer contend for a single winner-take-all slot per point. Each species
	 * scatters INDEPENDENTLY at its own TargetDensity (a per-species plants-per-m^2 candidate budget,
	 * scaled by the global SeedingDensity multiplier), so strata STACK — a canopy tree and a ground
	 * fern can occupy the same XY. Species are grouped into strata by their MATURE height
	 * (LayerForHeight(MaxHeight, …), NOT the authored MatureLayer, which stays mesh-only metadata):
	 * minimum spacing is enforced WITHIN a stratum (same-stratum crowns reject each other) and NEVER
	 * across strata — that cross-stratum non-interaction is what lets the layers stack.
	 *
	 * PER-DART POSITION (Gaussian-around-parents = a direct Thomas process): concentration now comes
	 * from WHERE the dart is drawn, not from an acceptance bias. The per-species clump PARENT lattice
	 * built by FArcForestPatchField::Build (intensity PatchDensity, suitability-thinned, per-species
	 * decorrelated) is the offspring source. Per dart, with clumped probability
	 *   p = WPatch / (1 + WPatch)
	 * the dart is an OFFSPRING — pick a parent uniformly and place it at Parent + Gaussian(0, σ),
	 * σ = that species' PatchRadiusSigma — otherwise it is a plain UNIFORM dart over Bounds. A species
	 * with NO parents (σ ≤ MinSigma or PatchDensity = 0) always falls back to uniform. WPatch is
	 * therefore a CLUMPED-FRACTION dial: 0 → p=0, every dart uniform (no clumping); large → p→1, every
	 * dart an offspring. (Params.WPatch is a plain unclamped field; p is safe for every reachable
	 * WPatch ≥ 0, and a negative p simply leaves bClumped false.)
	 *
	 * Because Parents.Num() > 0 implies σ > MinSigma by construction in Build, the degenerate
	 * "all offspring exactly on the parent" case cannot occur. An offspring whose Gaussian offset
	 * leaves the region is CLAMPED to Bounds (never redrawn — see the fixed-draw contract); the
	 * Poisson-disk floor below still spreads any resulting edge pile-up.
	 *
	 * PER-DART ACCEPTANCE is SUITABILITY x PER-SPECIES HABITAT RESPONSE:
	 *     AcceptProb = SampleFieldNearest(Pos) * ArcForestField::EvaluateHabitatResponse(species, Pos)
	 * (empty grid → suitability 1.0; a species with no authored FieldResponses → response exactly 1.0,
	 * which makes the product an IEEE-754 identity and this path bit-identical to the pre-habitat
	 * solver). The habitat term is RNG-FREE and therefore cannot perturb the draw contract below — it
	 * changes the VALUE of AcceptProb, never the count or order of draws.
	 * NOTE this thins WITHIN a clump only; the parent-placement bias that decides WHERE a species'
	 * clumps sit at all lives in FArcForestPatchField::Build.
	 * The old floor-accept/affinity acceptance is GONE — keeping it would double-count WPatch, which
	 * already sets the clumped fraction. The affinity field is not sampled on this path at all.
	 *
	 * PATCH ID: a clumped dart inherits its parent's stable id; a uniform dart gets INDEX_NONE.
	 *
	 * POISSON-DISK: an accepted dart is additionally rejected if a same-stratum placed plant j lies
	 * within SpacingFraction·(r_i + r_j), using MATURE MaxCrownRadius (the plant's future footprint,
	 * not the 5 cm seedling radius). An inline incremental uniform grid over the stratum's placed set
	 * accelerates the neighbour scan; the shared FArcForestSpatialHash is deliberately NOT touched
	 * (its non-owning-view ownership fights incremental insert and it is on the proven-parallel
	 * competition path).
	 *
	 * DETERMINISM — the FIXED 7-DRAW CONTRACT (load-bearing): SERIAL, single FRandomStream(Params.Seed),
	 * the ONLY solver RNG (the patch field owns its own per-species streams). Fixed traversal: strata
	 * 0..GArcForestLayerCount-1, species in ascending table index within a stratum, darts
	 * 0..CandidateCount within a species. EXACTLY SEVEN draws per dart, ALL UNCONDITIONAL and emitted
	 * BEFORE any branching, in this order:
	 *   1 DrawBranch  = FRand()                 clumped-vs-uniform selector
	 *   2 DrawParent  = FRand()                 parent pick
	 *   3 U1          = FRand()                 Box–Muller radius variate
	 *   4 U2          = FRand()                 Box–Muller angle variate
	 *   5 DrawUniformX = FRandRange(X)          uniform-branch position (FRandRange == 1 draw)
	 *   6 DrawUniformY = FRandRange(Y)
	 *   7 DrawAccept  = FRand()                 suitability acceptance
	 * Emitting all seven up front keeps the stream position-aligned across every branch — WPatch, a
	 * species having or lacking parents, acceptance, and the Poisson-disk rejection all consume the
	 * SAME number of draws. Box–Muller (2 uniforms → a 2D Gaussian offset) is used precisely because it
	 * is FIXED-COUNT; FRandomStream::GetUnitVector/VRand and Marsaglia-polar are REJECTION-loops and
	 * are banned here. Out-of-bounds Gaussian offsets are CLAMPED, never redrawn. Suitability sampling,
	 * parent access and the Poisson-disk rejection are all RNG-free.
	 */
	void SeedPopulation(
		const FArcForestSimParams& Params,
		const TArray<FArcForestSpeciesEntry>& Table,
		TArrayView<const float> SuitabilityGrid,
		FIntPoint GridDims,
		const FArcForestPatchField& PatchField,
		const FArcForestHabitatInputs& Habitat,
		FArcForestSolveState& OutPop)
	{
		const FBox2f& Bounds = Params.Bounds2D;
		const FVector2f BoundsSize = Bounds.GetSize();
		const float AreaMeters2 =
			(BoundsSize.X / CmPerMeter) * (BoundsSize.Y / CmPerMeter);
		if (AreaMeters2 <= 0.0f)
		{
			return;
		}

		const int32 NumSpecies = Table.Num();

		// Group species into strata by MATURE height (LayerForHeight(MaxHeight, …) — NOT the authored
		// MatureLayer). Members of a stratum reject each other at seed time; different strata stack.
		TArray<int32> StratumSpecies[GArcForestLayerCount];
		for (int32 SpeciesIndex = 0; SpeciesIndex < NumSpecies; ++SpeciesIndex)
		{
			const EArcForestLayer Stratum = FArcForestLightModel::LayerForHeight(
				Table[SpeciesIndex].MaxHeight, ShrubMinHeightCm, UnderstoryMinHeightCm, CanopyMinHeightCm);
			StratumSpecies[static_cast<int32>(Stratum)].Add(SpeciesIndex);
		}

		// Reserve the SoA arrays against the TOTAL candidate count across every species. This is an upper
		// bound (Poisson-disk rejection and suitability acceptance only ever remove darts), so it costs one
		// allocation per array instead of a growth series over the whole seeding pass. RNG-free — reserving
		// changes no draw and no placement.
		int32 TotalCandidates = 0;
		for (int32 SpeciesIndex = 0; SpeciesIndex < NumSpecies; ++SpeciesIndex)
		{
			TotalCandidates += FMath::Max(0, FMath::RoundToInt(
				Table[SpeciesIndex].TargetDensity * Params.SeedingDensity * AreaMeters2));
		}
		OutPop.Reserve(TotalCandidates);

		// Single SERIAL stream from Params.Seed drives ALL seeding draws (position + accept). This is the
		// ONLY RNG in the solver — the patch field owns its own per-species streams.
		FRandomStream Rng(Params.Seed);

		for (int32 Stratum = 0; Stratum < GArcForestLayerCount; ++Stratum)
		{
			const TArray<int32>& Members = StratumSpecies[Stratum];
			if (Members.Num() == 0)
			{
				continue;
			}

			// Largest mature crown in this stratum bounds the Poisson-disk neighbour scan.
			float StratumMaxMatureRadius = 0.0f;
			for (int32 MemberIndex = 0; MemberIndex < Members.Num(); ++MemberIndex)
			{
				StratumMaxMatureRadius =
					FMath::Max(StratumMaxMatureRadius, Table[Members[MemberIndex]].MaxCrownRadius);
			}

			const float CellSize =
				FMath::Max(StratumMaxMatureRadius * Params.SpacingFraction, MinCompetitionCellCm);

			// Inline incremental placed-set for THIS stratum (do NOT reuse the shared FArcForestSpatialHash).
			TArray<FVector2f> PlacedPos;
			TArray<float> PlacedMatureRadius;
			TMap<FIntPoint, TArray<int32>> PlacedGrid;

			for (int32 MemberIndex = 0; MemberIndex < Members.Num(); ++MemberIndex)
			{
				const int32 SpeciesIndex = Members[MemberIndex];
				const FArcForestSpeciesEntry& Entry = Table[SpeciesIndex];
				const float RadiusI = Entry.MaxCrownRadius;
				const int32 CandidateCount = FMath::Max(0, FMath::RoundToInt(
					Entry.TargetDensity * Params.SeedingDensity * AreaMeters2));
				// True upper bound on a placed plant's MinDist (PlacedMatureRadius[j] <= StratumMax).
				const float QueryRadius = Params.SpacingFraction * (RadiusI + StratumMaxMatureRadius);

				// Clump parents for THIS species (RNG-free reads; stable for the whole dart loop). An
				// empty parent set forces every dart of this species down the uniform branch.
				const TArrayView<const FVector2f> Parents = PatchField.GetParentPositions(SpeciesIndex);
				const TArrayView<const int32> ParentIds = PatchField.GetParentIds(SpeciesIndex);
				const float Sigma = PatchField.GetPatchRadiusSigma(SpeciesIndex);

				// Clumped fraction (see doc block). Parents.Num()>0 ⇒ Sigma > MinSigma by construction.
				const float ClumpedProbability = Params.WPatch / (1.0f + Params.WPatch);

				for (int32 Candidate = 0; Candidate < CandidateCount; ++Candidate)
				{
					// THE FIXED 7-DRAW BLOCK (determinism-critical): all seven draws are UNCONDITIONAL and
					// precede every branch below, so the stream stays position-aligned regardless of
					// WPatch, parent availability, acceptance or Poisson-disk rejection.
					const float DrawBranch = Rng.FRand();
					const float DrawParent = Rng.FRand();
					const float U1 = Rng.FRand();
					const float U2 = Rng.FRand();
					const float DrawUniformX = static_cast<float>(Rng.FRandRange(Bounds.Min.X, Bounds.Max.X));
					const float DrawUniformY = static_cast<float>(Rng.FRandRange(Bounds.Min.Y, Bounds.Max.Y));
					const float DrawAccept = Rng.FRand();

					const bool bClumped = (Parents.Num() > 0) && (DrawBranch < ClumpedProbability);

					FVector2f Pos(DrawUniformX, DrawUniformY);
					int32 PatchId = INDEX_NONE;
					if (bClumped)
					{
						const int32 ParentIndex = FMath::Clamp(
							FMath::FloorToInt(DrawParent * Parents.Num()), 0, Parents.Num() - 1);

						// Box–Muller (FIXED-COUNT, no rejection): u1 guarded off zero for ln(), u2 is the
						// angle and needs no guard.
						const float GuardedU1 = FMath::Max(U1, UE_SMALL_NUMBER);
						const float Radius = Sigma * FMath::Sqrt(-2.0f * FMath::Loge(GuardedU1));
						const float Theta = 2.0f * UE_PI * U2;
						const FVector2f Offset(Radius * FMath::Cos(Theta), Radius * FMath::Sin(Theta));

						// Out-of-bounds offspring are CLAMPED back into the region — never redrawn.
						const FVector2f Offspring = Parents[ParentIndex] + Offset;
						Pos = FVector2f(
							FMath::Clamp(Offspring.X, Bounds.Min.X, Bounds.Max.X),
							FMath::Clamp(Offspring.Y, Bounds.Min.Y, Bounds.Max.Y));
						PatchId = ParentIds[ParentIndex];
					}

					// Acceptance is suitability * the species' habitat response. WPatch already spent its
					// influence on the position distribution above, so there is still deliberately NO
					// affinity term here.
					//
					// This thins WITHIN a clump; the parent-placement bias that decides WHERE a species'
					// clumps sit at all lives in FArcForestPatchField::Build.
					//
					// DOES NOT TOUCH THE STREAM. EvaluateHabitatResponse is RNG-free, so the FIXED
					// 7-DRAW CONTRACT documented above still holds exactly: this changes the VALUE of
					// AcceptProb, never the count or order of draws. An empty response set returns
					// exactly 1.0f, so an unauthored species is bit-identical to the pre-habitat solver.
					const float HabitatResponse =
						ArcForestField::EvaluateHabitatResponse(Habitat, SpeciesIndex, Pos, Params.Seed);
					const float AcceptProb =
						FMath::Clamp(
							ArcForestField::SampleFieldNearest(Pos, Bounds, SuitabilityGrid, GridDims)
								* HabitatResponse,
							0.0f, 1.0f);
					if (DrawAccept >= AcceptProb)
					{
						continue;
					}

					// Poisson-disk rejection (RNG-free): reject if any same-stratum placed plant j is closer
					// than SpacingFraction·(r_i + r_j), pairwise with mature crown radii.
					const FIntPoint CenterCell(
						FMath::FloorToInt(Pos.X / CellSize), FMath::FloorToInt(Pos.Y / CellSize));
					const int32 CellRange = FMath::CeilToInt(QueryRadius / CellSize);
					bool bTooClose = false;
					for (int32 OffY = -CellRange; OffY <= CellRange && !bTooClose; ++OffY)
					{
						for (int32 OffX = -CellRange; OffX <= CellRange && !bTooClose; ++OffX)
						{
							const FIntPoint Cell(CenterCell.X + OffX, CenterCell.Y + OffY);
							const TArray<int32>* Bucket = PlacedGrid.Find(Cell);
							if (Bucket == nullptr)
							{
								continue;
							}
							for (int32 BucketIndex = 0; BucketIndex < Bucket->Num(); ++BucketIndex)
							{
								const int32 PlacedIndex = (*Bucket)[BucketIndex];
								const float MinDist =
									Params.SpacingFraction * (RadiusI + PlacedMatureRadius[PlacedIndex]);
								if ((PlacedPos[PlacedIndex] - Pos).SizeSquared() < FMath::Square(MinDist))
								{
									bTooClose = true;
									break;
								}
							}
						}
					}
					if (bTooClose)
					{
						continue;
					}

					const int32 NewIdx = PlacedPos.Add(Pos);
					PlacedMatureRadius.Add(RadiusI);
					PlacedGrid.FindOrAdd(CenterCell).Add(NewIdx);
					OutPop.Add(Pos, SpeciesIndex, SeedlingCrownRadiusCm, SeedlingHeightCm, PatchId);
				}
			}
		}
	}

	/** Fill Bands[i] = LayerForHeight(Height[i]) with cm-scaled thresholds (MC1 — live band). */
	void ComputeBands(const FArcForestSolveState& Pop, TArray<EArcForestLayer>& OutBands, bool bParallel)
	{
		const int32 NumPlants = Pop.Num();
		OutBands.SetNumUninitialized(NumPlants);

		// Per-plant, fully independent (each i is the sole writer of OutBands[i]).
		const EParallelForFlags Flags =
			bParallel ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread;
		ParallelFor(NumPlants, [&Pop, &OutBands](int32 Index)
		{
			OutBands[Index] = FArcForestLightModel::LayerForHeight(
				Pop.Height[Index], ShrubMinHeightCm, UnderstoryMinHeightCm, CanopyMinHeightCm);
		}, Flags);
	}

	/** Asymptotic growth toward the species max, gated by the growth-light response f. */
	void GrowPlants(
		const FArcForestSimParams& Params,
		const TArray<FArcForestSpeciesEntry>& Table,
		TArrayView<const float> IncidentLight,
		FArcForestSolveState& Pop)
	{
		const int32 NumPlants = Pop.Num();

		// Per-plant, fully independent (each i writes only Height[i]/CrownRadius[i]).
		const EParallelForFlags Flags =
			Params.bParallel ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread;
		ParallelFor(NumPlants, [&Table, &IncidentLight, &Pop](int32 Index)
		{
			const FArcForestSpeciesEntry& Entry = Table[Pop.Species[Index]];
			const float Response = GrowthResponse(IncidentLight[Index], Entry.LightNeeded, Entry.ShadeTolerance);

			// GrowthRate = fraction of the REMAINING gap to the max closed per iteration under full
			// light; f scales it down in shade. Asymptotic, so it never overshoots the mature form.
			const float Rate = Entry.GrowthRate * Response;

			const float HeightGap = Entry.MaxHeight - Pop.Height[Index];
			if (HeightGap > 0.0f)
			{
				Pop.Height[Index] = FMath::Min(
					Pop.Height[Index] + (Rate * HeightGap), Entry.MaxHeight);
			}

			const float RadiusGap = Entry.MaxCrownRadius - Pop.CrownRadius[Index];
			if (RadiusGap > 0.0f)
			{
				Pop.CrownRadius[Index] = FMath::Min(
					Pop.CrownRadius[Index] + (Rate * RadiusGap), Entry.MaxCrownRadius);
			}
		}, Flags);
	}

	/** Update vigour from whether the incident light meets the species' (tolerance-relieved) need. */
	void UpdateVigor(
		const TArray<FArcForestSpeciesEntry>& Table,
		TArrayView<const float> IncidentLight,
		FArcForestSolveState& Pop,
		bool bParallel)
	{
		const int32 NumPlants = Pop.Num();

		// Per-plant, fully independent (each i writes only Vigor[i]).
		const EParallelForFlags Flags =
			bParallel ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread;
		ParallelFor(NumPlants, [&Table, &IncidentLight, &Pop](int32 Index)
		{
			const FArcForestSpeciesEntry& Entry = Table[Pop.Species[Index]];
			const float LightNeeded = FMath::Max(Entry.LightNeeded, KINDA_SMALL_NUMBER);
			const float ShadeTolerance = FMath::Clamp(Entry.ShadeTolerance, 0.0f, 1.0f);
			const float EffectiveNeed =
				FMath::Max(LightNeeded * (1.0f - (ShadeTolerance * ShadeReliefFactor)), KINDA_SMALL_NUMBER);

			const float Light = FMath::Clamp(IncidentLight[Index], 0.0f, 1.0f);
			if (Light >= EffectiveNeed)
			{
				Pop.Vigor[Index] += VigorRecovery * (1.0f - Pop.Vigor[Index]);
			}
			else
			{
				const float RelativeDeficit = (EffectiveNeed - Light) / EffectiveNeed;
				Pop.Vigor[Index] -= VigorDecay * RelativeDeficit;
			}
			Pop.Vigor[Index] = FMath::Clamp(Pop.Vigor[Index], 0.0f, 1.0f);
		}, Flags);
	}

	/**
	 * The documented "weaker of two" total order, factored out so it is recomputed IDENTICALLY from
	 * either endpoint of a pair (it reads only the frozen VigorSnapshot / Height and the global indices,
	 * never any per-endpoint state): lower snapshot vigour loses; tie -> shorter plant loses; tie ->
	 * higher global index loses. Purity is what makes the per-plant gather (below) yield the same term
	 * SET as the old symmetric scatter — only the summation order changes, and that is canonicalised.
	 */
	FORCEINLINE int32 WeakerOfPair(
		int32 GlobalI, int32 GlobalJ, TArrayView<const float> VigorSnapshot, TArrayView<const float> Height)
	{
		if (VigorSnapshot[GlobalI] != VigorSnapshot[GlobalJ])
		{
			return (VigorSnapshot[GlobalI] < VigorSnapshot[GlobalJ]) ? GlobalI : GlobalJ;
		}
		if (Height[GlobalI] != Height[GlobalJ])
		{
			return (Height[GlobalI] < Height[GlobalJ]) ? GlobalI : GlobalJ;
		}
		return FMath::Max(GlobalI, GlobalJ);
	}

	/**
	 * Resolve one iteration of horizontal competition across all bands, then apply the accumulated
	 * suppression. Snapshot-then-apply keeps the outcome order-independent (see suppression rule).
	 *
	 * PARALLEL GATHER (Step 6): the per-band scatter (`Penalty[Weaker] += …`) is reformulated as a
	 * per-plant incoming-penalty GATHER so each plant is the sole writer of `Penalty[GlobalI]` (no
	 * cross-thread race). Each plant queries ALL its neighbours (not the `j > i` upper triangle),
	 * recomputes the weaker-of-pair via WeakerOfPair, keeps only the terms where IT is the weaker, then
	 * SORTS those terms by NeighbourGlobal before summing. Float `+=` is non-associative, so the sort is
	 * what makes the sum bit-identical regardless of the hash-bucket visitation order (and therefore
	 * regardless of whether the loop runs serial or across worker threads).
	 */
	void ResolveCompetition(
		const FArcForestSimParams& Params,
		TArrayView<const EArcForestLayer> Bands,
		FArcForestSolveState& Pop)
	{
		const int32 NumPlants = Pop.Num();
		if (NumPlants < 2)
		{
			return;
		}

		// Snapshot vigour BEFORE any penalty so "weaker of two" is stable regardless of order.
		TArray<float> VigorSnapshot = Pop.Vigor;
		TArray<float> Penalty;
		Penalty.SetNumZeroed(NumPlants);

		// Reused per-band scratch: the band's plant positions (backing store for the hash's
		// non-owning view) and the local->global index map. Rebuilt each band.
		FArcForestSpatialHash Hash;
		TArray<FVector2f> BandPos;
		TArray<int32> BandGlobal;

		const EParallelForFlags Flags =
			Params.bParallel ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread;

		for (int32 Band = 0; Band < GArcForestLayerCount; ++Band)
		{
			if (!Params.IntraLayerCompetition[Band])
			{
				continue;
			}

			BandPos.Reset();
			BandGlobal.Reset();
			float MaxRadius = 0.0f;
			for (int32 Index = 0; Index < NumPlants; ++Index)
			{
				if (static_cast<int32>(Bands[Index]) == Band)
				{
					BandGlobal.Add(Index);
					BandPos.Add(Pop.Pos[Index]);
					MaxRadius = FMath::Max(MaxRadius, Pop.CrownRadius[Index]);
				}
			}

			if (BandGlobal.Num() < 2 || MaxRadius <= 0.0f)
			{
				continue;
			}

			// Cell ~ band max crown radius keeps buckets small; the query neighbourhood is then a
			// handful of cells. BandPos is fully built above and NOT modified below, so the hash's
			// non-owning view stays valid for the duration of this band's queries (concurrent
			// read-only QueryRadius is safe against a fully-built hash).
			const float CellSize = FMath::Max(MaxRadius, MinCompetitionCellCm);
			Hash.Build(BandPos, CellSize);

			// Per-plant gather. Each task owns a `Nearby` scratch array (QueryRadius clears it per call,
			// so it is alloc-free after warmup) — no process-lifetime thread_local state.
			TArray<TArray<int32>> NearbyContexts;
			ParallelForWithTaskContext(NearbyContexts, BandGlobal.Num(),
				[&Hash, &BandGlobal, &BandPos, &VigorSnapshot, &Penalty, &Pop, MaxRadius]
				(TArray<int32>& Nearby, int32 LocalI)
				{
					// Stack-local incoming penalty terms for this one plant.
					struct FIncoming
					{
						int32 NeighbourGlobal;
						float Term;
					};
					TArray<FIncoming, TInlineAllocator<16>> Incoming;

					const int32 GlobalI = BandGlobal[LocalI];
					const float RadiusI = Pop.CrownRadius[GlobalI];

					// Any overlapping crown j has centre within r_i + r_j <= r_i + MaxRadius.
					const float QueryRadius = RadiusI + MaxRadius;
					Hash.QueryRadius(BandPos[LocalI], QueryRadius, Nearby);

					for (int32 LocalJ : Nearby)
					{
						const int32 GlobalJ = BandGlobal[LocalJ];
						// Iterate ALL neighbours (both directions); skip only self.
						if (GlobalJ == GlobalI)
						{
							continue;
						}

						const float RadiusJ = Pop.CrownRadius[GlobalJ];
						const float SumRadii = RadiusI + RadiusJ;
						if (SumRadii <= 0.0f)
						{
							continue;
						}

						const float Dist = (BandPos[LocalI] - BandPos[LocalJ]).Size();
						if (Dist >= SumRadii)
						{
							continue; // crowns do not overlap
						}

						const float OverlapFraction = (SumRadii - Dist) / SumRadii; // (0,1]

						// This plant only accrues a penalty when IT is the weaker of the pair. Recomputed
						// identically from either endpoint, so the term set matches the old scatter exactly.
						const int32 Weaker = WeakerOfPair(GlobalI, GlobalJ, VigorSnapshot, Pop.Height);
						if (Weaker == GlobalI)
						{
							Incoming.Add(FIncoming{ GlobalJ, CompetitionPenaltyPerPair * OverlapFraction });
						}
					}

					// Canonical summation order: sort by NeighbourGlobal (unique keys), THEN sum. This is
					// what makes the float accumulation bit-identical across serial/parallel runs.
					Incoming.Sort([](const FIncoming& A, const FIncoming& B)
					{
						return A.NeighbourGlobal < B.NeighbourGlobal;
					});

					float Sum = 0.0f;
					for (int32 TermIndex = 0; TermIndex < Incoming.Num(); ++TermIndex)
					{
						Sum += Incoming[TermIndex].Term;
					}

					// Single write per plant (each plant belongs to exactly one band) — no race.
					Penalty[GlobalI] = Sum;
				}, Flags);
		}

		for (int32 Index = 0; Index < NumPlants; ++Index)
		{
			Pop.Vigor[Index] = FMath::Clamp(Pop.Vigor[Index] - Penalty[Index], 0.0f, 1.0f);
		}
	}

	/**
	 * Update starvation counters, mark the dead, and compact all SoA arrays with a single write
	 * cursor (two-pass removal: mark, then compact — never RemoveAt in a per-element loop). Returns
	 * the surviving count.
	 */
	int32 ApplyMortalityAndCompact(FArcForestSolveState& Pop, bool bParallel)
	{
		const int32 NumPlants = Pop.Num();

		// Pass 1 — mark. A plant dies after StarvationLimit consecutive sub-floor iterations, or
		// immediately at zero vigour. Per-plant, fully independent (each i writes only
		// StarvationCounter[i]/bDead[i]).
		TArray<bool> bDead;
		bDead.SetNumUninitialized(NumPlants);
		const EParallelForFlags Flags =
			bParallel ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread;
		ParallelFor(NumPlants, [&Pop, &bDead](int32 Index)
		{
			if (Pop.Vigor[Index] < VigorMortalityFloor)
			{
				++Pop.StarvationCounter[Index];
			}
			else
			{
				Pop.StarvationCounter[Index] = 0;
			}

			bDead[Index] = (Pop.Vigor[Index] <= 0.0f) || (Pop.StarvationCounter[Index] >= StarvationLimit);
		}, Flags);

		// Pass 2 — compact survivors down with a stable write cursor.
		int32 Write = 0;
		for (int32 Read = 0; Read < NumPlants; ++Read)
		{
			if (bDead[Read])
			{
				continue;
			}
			if (Write != Read)
			{
				Pop.MoveElement(Write, Read);
			}
			++Write;
		}
		Pop.SetNum(Write);
		return Write;
	}

	// =================================================================================================
	// RECRUITMENT (stage h) — new seedlings establishing under the standing stand between iterations.
	// =================================================================================================

	/**
	 * Reused scratch for ONE recruitment pass. Hoisted into the per-StepIterations block and Reset()
	 * per iteration for the same reason as every other buffer there: the solve runs on a UE::Tasks
	 * worker and must not allocate per iteration. Reset() keeps the allocations of every member —
	 * TArray::Reset preserves its buffer and TMap::Reset preserves its pair storage — so only the
	 * first recruiting iteration allocates.
	 *
	 * The spacing grid is deliberately an INTRUSIVE SINGLY-LINKED CHAIN (cell -> head index, plus one
	 * "next entry in my cell" slot per entry) rather than the obvious TMap<FIntPoint, TArray<int32>>.
	 * A map of arrays is NOT allocation-free across iterations: TMap::Reset destroys every value, so
	 * each recruiting iteration would free and then re-allocate one TArray per occupied cell. With an
	 * int32 value type there is nothing to free, so Reset genuinely leaves the whole structure warm.
	 * A flat dense CellHead array (no hash at all) was rejected: its size is Bounds2D/CellSize, both
	 * authored and unrelated to the population, so a large region with tight spacing would allocate
	 * orders of magnitude more cells than there are plants — and a live plant outside Bounds2D (which
	 * a hand-built solve state can hold) would have to be clamped into an edge cell, changing which
	 * darts it rejects. The map stays sparse in the number of OCCUPIED cells and needs neither.
	 *
	 * Chain order is LIFO rather than insertion order; that is invisible because the only thing the
	 * scan computes is "does ANY neighbour reject this dart", which is an order-independent OR that
	 * short-circuits.
	 *
	 * It is PURE SCRATCH — nothing here survives the call, which is what keeps the solve bit-identical
	 * no matter how the iterations are chunked (all live state is in FArcForestSolveState).
	 */
	struct FRecruitScratch
	{
		/** Spacing-hash entry positions: every live plant, then each accepted recruit as it is placed. */
		TArray<FVector2f> Pos;

		/** LIVE crown radius per hash entry, parallel to Pos (a recruit's is SeedlingCrownRadiusCm). */
		TArray<float> Radius;

		/** Uniform-grid cell -> index into Pos of the FIRST entry of that cell's chain. */
		TMap<FIntPoint, int32> CellHead;

		/** Next entry in the same cell's chain, parallel to Pos; INDEX_NONE ends the chain. */
		TArray<int32> NextInCell;

		/** Tallest LIVE plant per species index — the maturity test's input. */
		TArray<float> SpeciesMaxHeight;

		/** Species indices eligible to recruit this iteration, ASCENDING (the canonical dart order). */
		TArray<int32> MatureSpecies;

		/** Dart count per entry of MatureSpecies, parallel to it. */
		TArray<int32> Shares;
	};

	/** Uniform-grid cell of a position, matching seeding's FloorToInt convention (handles negatives). */
	FIntPoint RecruitCell(FVector2f Position, float CellSize)
	{
		return FIntPoint(
			FMath::FloorToInt(Position.X / CellSize),
			FMath::FloorToInt(Position.Y / CellSize));
	}

	/**
	 * Push hash entry HashIndex (an index into Scratch.Pos) onto the front of Cell's chain. The caller
	 * must already have appended the entry's Pos/Radius; this appends the matching NextInCell slot, so
	 * all three arrays stay exactly parallel.
	 *
	 * FindOrAdd is given an EXPLICIT INDEX_NONE default — the one-argument overload would
	 * default-construct the int32 to 0, which is a VALID entry index and would silently splice entry 0
	 * onto the front of every newly-touched cell.
	 */
	void InsertRecruitHashEntry(FRecruitScratch& Scratch, FIntPoint Cell, int32 HashIndex)
	{
		int32& Head = Scratch.CellHead.FindOrAdd(Cell, INDEX_NONE);
		Scratch.NextInCell.Add(Head);
		Head = HashIndex;
	}

	/**
	 * The per-iteration recruitment random stream.
	 *
	 * CHUNK-INVARIANCE (load-bearing, and the whole reason this is a function): the stream is derived
	 * ONLY from (Params.Seed, ArcForestRecruitSalt, the 1-based iteration number). All three are
	 * properties of the SOLVE, not of the call — the iteration number is State.Iteration, the resume
	 * cursor that persists in the solve state — so iteration N is handed the identical stream whether it
	 * ran in a chunk of one, a chunk of four, or a single run to completion. A stream carried across
	 * iterations (or seeded from a per-call counter) would break exactly that.
	 *
	 * The salt keeps this disjoint from the seeding stream FRandomStream(Params.Seed); the odd
	 * golden-ratio multiplier decorrelates consecutive iterations so iteration 1 and iteration 2 do not
	 * produce near-identical dart sequences. Masked to a non-negative seed so the mix never depends on
	 * signed-overflow behaviour inside FRandomStream.
	 */
	FRandomStream MakeRecruitStream(int32 Seed, int32 IterationOneBased)
	{
		const uint32 Mixed = static_cast<uint32>(Seed)
			^ ArcForestRecruitSalt
			^ (static_cast<uint32>(IterationOneBased) * 2654435761u);
		return FRandomStream(static_cast<int32>(Mixed & 0x7FFFFFFFu));
	}

	/**
	 * The population ceiling: the authored value when non-zero, otherwise the SAME expression seeding
	 * uses for its total candidate budget (per-species TargetDensity x global SeedingDensity x area,
	 * rounded and floored at 0, summed over species).
	 *
	 * RNG-free and iteration-independent, so it is computed once per StepIterations call rather than per
	 * iteration. HONEST LIMIT (also on the setting): the derived value bounds SEEDED candidates, not a
	 * mature carrying capacity, so a stand with weak mortality can sit at it permanently.
	 */
	int32 ResolvePopulationCeiling(
		const FArcForestSimParams& Params,
		const TArray<FArcForestSpeciesEntry>& Table,
		float AreaMeters2)
	{
		if (Params.PopulationCeiling > 0)
		{
			return Params.PopulationCeiling;
		}

		int32 Derived = 0;
		for (int32 SpeciesIndex = 0; SpeciesIndex < Table.Num(); ++SpeciesIndex)
		{
			Derived += FMath::Max(0, FMath::RoundToInt(
				Table[SpeciesIndex].TargetDensity * Params.SeedingDensity * AreaMeters2));
		}
		return Derived;
	}

	/**
	 * ONE recruitment pass. Returns the number of recruits appended (0 <= Recruited <= Attempts).
	 *
	 * BUDGET: Attempts = Min(Round(RecruitmentRate * Ceiling), Max(0, Ceiling - live)). EXACTLY Attempts
	 * darts are thrown and then the pass ends — there is deliberately NO accept-until-quota retry loop.
	 * An unbounded loop could not terminate on a saturated stand (every dart rejected by spacing) and
	 * would hang the worker that AbortInternal blocks the game thread on (PCGForestSimSettings.h,
	 * SolveTask.Wait()). Fewer recruits on a crowded stand is the correct answer, not a reason to retry.
	 *
	 * SPECIES SPLIT: the budget is divided among the MATURE species (a species with a live plant at or
	 * above its MaturityHeightCm, or MaturityHeightCm <= 0) in proportion to RecruitmentWeight, visited
	 * in ASCENDING species index. Each share is FLOORED and the rounding remainder is given to the FIRST
	 * (lowest-index) mature species, so the shares sum to exactly Attempts and can never sum above it.
	 *
	 * ACCEPTANCE = suitability x habitat response x shade preference:
	 *     SampleFieldNearest(pos) * EvaluateHabitatResponse(species, pos)
	 *         * EvaluateShadeResponse(species, Transmittance(pos))
	 * All three are RNG-free.
	 *
	 * THE SUITABILITY TERM IS LOAD-BEARING, not symmetry with seeding for its own sake. The suitability
	 * grid is the node's only statement of "a plant can exist here at all": both the surface Density mask
	 * and every ProjectPoint miss are folded into it upstream, and nothing downstream re-checks it — Emit
	 * is a straight SoA copy, the PCG emit loop filters nothing, and mortality reads vigour and light but
	 * never position. A recruit on a masked or off-surface cell would therefore be emitted at the
	 * SurfaceHeightGrid fallback Z (the bounds' mid-Z), floating or buried, and a recruit added on the
	 * final iteration faces no mortality at all. Sampled NEAREST, matching seeding: the same authority,
	 * the same cell, and an empty grid answers exactly 1.0 so a stand built without a suitability grid is
	 * unaffected. It cannot perturb the draw contract — see DETERMINISM below.
	 *
	 * The per-species habitat responses are NOT a substitute for it: ArcForestTerrainFields fills miss
	 * cells by replicating the nearest VALID morphology, so slope/elevation/sun at a hole read as
	 * plausible terrain rather than as no-data, and a species with no authored FieldResponses (the
	 * shipping default) scores exactly 1.0 everywhere anyway.
	 *
	 * DETERMINISM: SERIAL, single stream from MakeRecruitStream, three UNCONDITIONAL draws per dart
	 * (x, y, accept) emitted before any branch so acceptance and spacing rejection cannot shift the
	 * stream. Appends happen in dart order, which is the canonical index order the rest of the solver
	 * depends on: WeakerOfPair breaks ties by FMath::Max(GlobalI, GlobalJ) (higher index loses) and
	 * compaction preserves index order. There is NO parallel path and there cannot be one — a single
	 * serial stream cannot be consumed out of order and still be deterministic.
	 *
	 * VARIABLE DRAW COUNT IS INTENTIONAL — do NOT "fix" this into a fixed-draw block the way seeding's
	 * 7-draw contract is. Attempts legitimately varies with the live count, the stream is rebuilt per
	 * iteration from (Seed, salt, iteration) and is shared with nothing downstream, so a differing number
	 * of draws in iteration N cannot desynchronise anything: iteration N+1 starts from its own stream.
	 */
	int32 RecruitPlants(
		const FArcForestSimParams& Params,
		const TArray<FArcForestSpeciesEntry>& Table,
		TArrayView<const float> SuitabilityGrid,
		FIntPoint GridDims,
		const FArcForestHabitatInputs& Habitat,
		const FArcForestScalarField& Transmittance,
		int32 Ceiling,
		int32 IterationOneBased,
		FRecruitScratch& Scratch,
		FArcForestSolveState& Pop)
	{
		const int32 NumLive = Pop.Num();
		const int32 NumSpecies = Table.Num();
		if (NumSpecies == 0 || Ceiling <= 0)
		{
			return 0;
		}

		const int32 Budget =
			FMath::Max(0, FMath::RoundToInt(Params.RecruitmentRate * static_cast<float>(Ceiling)));
		const int32 Attempts = FMath::Min(Budget, FMath::Max(0, Ceiling - NumLive));
		if (Attempts <= 0)
		{
			return 0;
		}

		// --- Maturity: tallest LIVE plant per species (RNG-free, one pass) --------------------------
		Scratch.SpeciesMaxHeight.SetNumUninitialized(NumSpecies);
		for (int32 SpeciesIndex = 0; SpeciesIndex < NumSpecies; ++SpeciesIndex)
		{
			Scratch.SpeciesMaxHeight[SpeciesIndex] = -TNumericLimits<float>::Max();
		}
		for (int32 Index = 0; Index < NumLive; ++Index)
		{
			const int32 SpeciesId = Pop.Species[Index];
			if (Scratch.SpeciesMaxHeight.IsValidIndex(SpeciesId))
			{
				Scratch.SpeciesMaxHeight[SpeciesId] =
					FMath::Max(Scratch.SpeciesMaxHeight[SpeciesId], Pop.Height[Index]);
			}
		}

		Scratch.MatureSpecies.Reset();
		float WeightSum = 0.0f;
		for (int32 SpeciesIndex = 0; SpeciesIndex < NumSpecies; ++SpeciesIndex)
		{
			const FArcForestSpeciesEntry& Entry = Table[SpeciesIndex];
			const float Weight = FMath::Max(Entry.RecruitmentWeight, 0.0f);
			if (Weight <= 0.0f)
			{
				continue; // authored out of recruitment entirely
			}

			// MaturityHeightCm <= 0 means "recruits from the first iteration"; otherwise the species
			// needs a standing plant that has reached it. A species with no live plants at all has
			// SpeciesMaxHeight == -FLT_MAX and therefore only recruits if its gate is <= 0 — an
			// extinct species with a real maturity height does not come back from nothing.
			const bool bMature =
				(Entry.MaturityHeightCm <= 0.0f) ||
				(Scratch.SpeciesMaxHeight[SpeciesIndex] >= Entry.MaturityHeightCm);
			if (!bMature)
			{
				continue;
			}

			Scratch.MatureSpecies.Add(SpeciesIndex);
			WeightSum += Weight;
		}

		// UE_SMALL_NUMBER, not 0: RecruitmentWeight is only ClampMin="0.0", so a sum of DENORMAL
		// positive weights would clear a > 0.0f test and then divide the share ratio below by ~1e-45.
		if (Scratch.MatureSpecies.Num() == 0 || WeightSum <= UE_SMALL_NUMBER)
		{
			return 0; // nothing is reproducing yet — and NOT ONE DRAW is consumed
		}

		// --- Split the budget: floored proportional shares, remainder to the first mature species ----
		const int32 NumMature = Scratch.MatureSpecies.Num();
		Scratch.Shares.SetNumUninitialized(NumMature);
		int32 Assigned = 0;
		for (int32 MatureIndex = 0; MatureIndex < NumMature; ++MatureIndex)
		{
			const float Weight = FMath::Max(Table[Scratch.MatureSpecies[MatureIndex]].RecruitmentWeight, 0.0f);

			// Guard the RATIO, not just the resulting share. RecruitmentWeight has no authored upper
			// bound, so Attempts * Weight can overflow to +inf (and inf/inf gives NaN) — and casting a
			// non-finite float to int32 inside FloorToInt is UNDEFINED BEHAVIOUR, landing on INT_MIN in
			// practice. That would clamp the HEAVIEST species to a 0 share and hand its entire budget to
			// index 0 via the remainder rule below: exactly the inverse of the intended split. The test
			// is written as "< Attempts" rather than an FMath::Min so it is false for NaN as well as for
			// an over-cap ratio, without depending on Min's NaN-ordering; a ratio of exactly Attempts
			// (the single-mature-species case) takes the cap branch and floors to the same value.
			const float ShareRatio = static_cast<float>(Attempts) * Weight / WeightSum;
			const int32 CappedShare =
				(ShareRatio < static_cast<float>(Attempts)) ? FMath::FloorToInt(ShareRatio) : Attempts;
			const int32 Share = FMath::Clamp(CappedShare, 0, Attempts - Assigned);
			Scratch.Shares[MatureIndex] = Share;
			Assigned += Share;
		}
		// Flooring can only ever under-assign, so this is always >= 0 and the shares now sum to exactly
		// Attempts. Giving it to index 0 (rather than spreading it) keeps the rule trivially auditable.
		Scratch.Shares[0] += Attempts - Assigned;

		// --- Spacing hash over the LIVE population --------------------------------------------------
		// Rule: reject a dart within Max(RecruitMinSpacingCm, SpacingFraction * (SeedlingCrownRadiusCm +
		// neighbour's LIVE CrownRadius)). LIVE, not mature: seeding rejects on mature radii because it is
		// laying out a stand that will grow into itself, whereas a recruit is a 5 cm plant slotting into
		// an existing stand and a mature-radius test would forbid all understory. The absolute floor is
		// what stops recruits clotting in the few darkest cells acceptance concentrates them in.
		float MaxLiveRadius = SeedlingCrownRadiusCm;
		for (int32 Index = 0; Index < NumLive; ++Index)
		{
			MaxLiveRadius = FMath::Max(MaxLiveRadius, Pop.CrownRadius[Index]);
		}

		const float SpacingFraction = FMath::Max(Params.SpacingFraction, 0.0f);
		const float MinSpacingFloor = FMath::Max(Params.RecruitMinSpacingCm, 0.0f);
		// True upper bound on any neighbour's MinDist (Radius[j] <= MaxLiveRadius, and a recruit's
		// SeedlingCrownRadiusCm is included in that bound), so a one-cell-ring scan cannot miss a rejector.
		const float QueryRadius =
			FMath::Max(MinSpacingFloor, SpacingFraction * (SeedlingCrownRadiusCm + MaxLiveRadius));
		const float CellSize = FMath::Max(QueryRadius, MinCompetitionCellCm);

		Scratch.Pos.Reset();
		Scratch.Radius.Reset();
		Scratch.NextInCell.Reset();
		Scratch.CellHead.Reset();
		Scratch.Pos.Reserve(NumLive + Attempts);
		Scratch.Radius.Reserve(NumLive + Attempts);
		Scratch.NextInCell.Reserve(NumLive + Attempts);
		for (int32 Index = 0; Index < NumLive; ++Index)
		{
			const int32 HashIndex = Scratch.Pos.Add(Pop.Pos[Index]);
			Scratch.Radius.Add(Pop.CrownRadius[Index]);
			InsertRecruitHashEntry(Scratch, RecruitCell(Pop.Pos[Index], CellSize), HashIndex);
		}

		// ONE reserve for the whole append loop — eight parallel SoA arrays, so growing them per Add()
		// would be eight reallocations per recruit (mirrors seeding's Reserve(TotalCandidates)).
		Pop.Reserve(NumLive + Attempts);

		const FBox2f& Bounds = Params.Bounds2D;
		FRandomStream Rng = MakeRecruitStream(Params.Seed, IterationOneBased);
		const int32 CellRange = FMath::CeilToInt(QueryRadius / CellSize);
		int32 Recruited = 0;

		for (int32 MatureIndex = 0; MatureIndex < NumMature; ++MatureIndex)
		{
			const int32 SpeciesIndex = Scratch.MatureSpecies[MatureIndex];
			const int32 Darts = Scratch.Shares[MatureIndex];
			for (int32 Dart = 0; Dart < Darts; ++Dart)
			{
				// Three UNCONDITIONAL draws, emitted before every branch below, so neither acceptance nor
				// spacing rejection can shift the stream (the same discipline as seeding's fixed block —
				// what varies here is only the NUMBER of darts, which is per-iteration by design).
				const float DrawX = static_cast<float>(Rng.FRandRange(Bounds.Min.X, Bounds.Max.X));
				const float DrawY = static_cast<float>(Rng.FRandRange(Bounds.Min.Y, Bounds.Max.Y));
				const float DrawAccept = Rng.FRand();

				const FVector2f DartPos(DrawX, DrawY);

				// The raster is ONE STAGE STALE by decision: it was captured at stage (c), before growth,
				// competition and mortality, so it carries shade cast partly by plants that died this
				// iteration and ignores this iteration's growth. Accepted — a recruitment decision at 4 m
				// raster resolution does not warrant a second full light sweep per iteration.
				//
				// Sampled BILINEARLY (see the trailing-block note at the call site): the raster footprint
				// always covers Bounds2D, so a dart can never sample outside it.
				const float ShadeFactor = ArcForestField::EvaluateShadeResponse(
					Habitat, SpeciesIndex, Transmittance.SampleBilinear(DartPos));
				const float HabitatFactor = ArcForestField::EvaluateHabitatResponse(
					Habitat, SpeciesIndex, DartPos, Params.Seed);

				// Suitability gates the other two: it is the "can a plant exist here at all" mask (the
				// surface Density mask plus every ProjectPoint miss), and no later stage re-checks it.
				// NEAREST — seeding's own authority and cell; an empty grid returns exactly 1.0, so a
				// stand run without a suitability grid is unaffected.
				const float SuitabilityFactor = ArcForestField::SampleFieldNearest(
					DartPos, Bounds, SuitabilityGrid, GridDims);

				const float AcceptProb =
					FMath::Clamp(SuitabilityFactor * HabitatFactor * ShadeFactor, 0.0f, 1.0f);
				if (DrawAccept >= AcceptProb)
				{
					continue;
				}

				// Spacing (RNG-free). Same-iteration recruits are already in the hash, so recruits are
				// tested against each other exactly as they are against standing plants.
				const FIntPoint CentreCell = RecruitCell(DartPos, CellSize);
				bool bTooClose = false;
				for (int32 OffY = -CellRange; OffY <= CellRange && !bTooClose; ++OffY)
				{
					for (int32 OffX = -CellRange; OffX <= CellRange && !bTooClose; ++OffX)
					{
						const FIntPoint Cell(CentreCell.X + OffX, CentreCell.Y + OffY);
						const int32* HeadPtr = Scratch.CellHead.Find(Cell);
						if (HeadPtr == nullptr)
						{
							continue;
						}
						// Walk this cell's intrusive chain. Visitation order is LIFO rather than insertion
						// order, which is invisible here: the scan only asks whether ANY neighbour rejects
						// the dart, and it short-circuits on the first one.
						for (int32 NeighbourIndex = *HeadPtr;
							NeighbourIndex != INDEX_NONE;
							NeighbourIndex = Scratch.NextInCell[NeighbourIndex])
						{
							const float MinDist = FMath::Max(
								MinSpacingFloor,
								SpacingFraction * (SeedlingCrownRadiusCm + Scratch.Radius[NeighbourIndex]));
							if ((Scratch.Pos[NeighbourIndex] - DartPos).SizeSquared() < FMath::Square(MinDist))
							{
								bTooClose = true;
								break;
							}
						}
					}
				}
				if (bTooClose)
				{
					continue;
				}

				// PatchId is INDEX_NONE for every recruit, by decision: the conspecific patch field is
				// destroyed after seeding (it holds non-owning spatial-hash views and must not persist
				// into a chunked lifetime), so a recruit cannot mint an id, and inheriting a neighbour's
				// would falsely claim clump membership the Thomas process never assigned. The PCG output
				// attribute already defaults to INDEX_NONE and writes it verbatim.
				Pop.Add(DartPos, SpeciesIndex, SeedlingCrownRadiusCm, SeedlingHeightCm, INDEX_NONE);

				const int32 HashIndex = Scratch.Pos.Add(DartPos);
				Scratch.Radius.Add(SeedlingCrownRadiusCm);
				InsertRecruitHashEntry(Scratch, CentreCell, HashIndex);
				++Recruited;
			}
		}

		return Recruited;
	}

	/** Emit the SoA survivor result, resolving each survivor's species MatureLayer for the mesh stage. */
	FArcForestResult Emit(const FArcForestSolveState& Pop, const TArray<FArcForestSpeciesEntry>& Table)
	{
		FArcForestResult Result;
		const int32 NumPlants = Pop.Num();

		Result.PosXY = Pop.Pos;
		Result.Species = Pop.Species;
		Result.CrownRadius = Pop.CrownRadius;
		Result.Height = Pop.Height;
		Result.Age = Pop.Age;
		Result.Vigor = Pop.Vigor;
		Result.PatchId = Pop.PatchId;

		Result.MatureLayer.SetNumUninitialized(NumPlants);
		for (int32 Index = 0; Index < NumPlants; ++Index)
		{
			Result.MatureLayer[Index] = Table[Pop.Species[Index]].MatureLayer;
		}

		// Final-stand diagnostics (NOT consumed by any code path — kept for tuning; single cheap pass).
		// Species composition answers "what actually grew", live-band distribution shows whether
		// succession happened (plants graduating Field->Canopy by height, MC1), and the height/crown
		// spread shows maturity. All are "might need it next" knobs while dialling the model in.
		if (NumPlants > 0)
		{
			static const TCHAR* const BandLabels[GArcForestLayerCount] = {
				TEXT("Field"), TEXT("Shrub"), TEXT("Understory"), TEXT("Canopy") };

			TArray<int32> PerSpecies;
			PerSpecies.SetNumZeroed(Table.Num());
			int32 PerBand[GArcForestLayerCount] = { 0, 0, 0, 0 };
			float HeightMin = TNumericLimits<float>::Max();
			float HeightMax = -TNumericLimits<float>::Max();
			double HeightSum = 0.0;
			double CrownSum = 0.0;
			for (int32 Index = 0; Index < NumPlants; ++Index)
			{
				const int32 SpeciesId = Pop.Species[Index];
				if (PerSpecies.IsValidIndex(SpeciesId))
				{
					++PerSpecies[SpeciesId];
				}
				const EArcForestLayer Band = FArcForestLightModel::LayerForHeight(
					Pop.Height[Index], ShrubMinHeightCm, UnderstoryMinHeightCm, CanopyMinHeightCm);
				++PerBand[static_cast<int32>(Band)];
				HeightMin = FMath::Min(HeightMin, Pop.Height[Index]);
				HeightMax = FMath::Max(HeightMax, Pop.Height[Index]);
				HeightSum += Pop.Height[Index];
				CrownSum += Pop.CrownRadius[Index];
			}

			FString SpeciesBreakdown;
			for (int32 SpeciesIndex = 0; SpeciesIndex < Table.Num(); ++SpeciesIndex)
			{
				SpeciesBreakdown += FString::Printf(TEXT("%s%s=%d"),
					(SpeciesIndex > 0) ? TEXT(", ") : TEXT(""),
					*Table[SpeciesIndex].Name.ToString(), PerSpecies[SpeciesIndex]);
			}
			FString BandBreakdown;
			for (int32 Band = 0; Band < GArcForestLayerCount; ++Band)
			{
				BandBreakdown += FString::Printf(TEXT("%s%s=%d"),
					(Band > 0) ? TEXT(", ") : TEXT(""), BandLabels[Band], PerBand[Band]);
			}
			UE_LOGF(LogArcPCGForest, Log,"ForestSim: final species — %ls", *SpeciesBreakdown);
			UE_LOGF(LogArcPCGForest, Log,"ForestSim: final live bands — %ls", *BandBreakdown);
			UE_LOGF(LogArcPCGForest, Log,"ForestSim: final maturity — height cm min=%.1f max=%.1f mean=%.1f, crownRadius mean=%.1f.",
				HeightMin, HeightMax, static_cast<float>(HeightSum / NumPlants),
				static_cast<float>(CrownSum / NumPlants));
		}
		return Result;
	}
}

void FArcForestSimulator::SeedInitial(
	FArcForestSolveState& State,
	const FArcForestSimParams& Params,
	const UArcForestSpeciesDefinition& Species,
	TArrayView<const float> SuitabilityGrid,
	FIntPoint GridDims,
	const FArcForestHabitatInputs& Habitat) const
{
	using namespace ArcForestSimulatorLocal;

	// Fresh state: SeedPopulation appends, so an external driver reusing a State object gets a clean run.
	State = FArcForestSolveState();
	State.Iteration = 0;

	const TArray<FArcForestSpeciesEntry>& Table = Species.Species;
	if (Table.Num() == 0)
	{
		State.bSeeded = true;
		return;
	}

	// Build the conspecific patch field LOCALLY (MC2 seeding input; owns per-species streams and holds
	// non-owning spatial-hash views). Only SeedPopulation consumes it — it must NOT persist past seeding.
	FArcForestPatchField PatchField;
	PatchField.Build(Species, Params, SuitabilityGrid, GridDims, Habitat);

	// Seed (no competition term — no plants exist yet).
	SeedPopulation(Params, Table, SuitabilityGrid, GridDims, PatchField, Habitat, State);
	UE_LOGF(LogArcPCGForest, Log,"ForestSim: seeded %d plants (density=%.3f).",
		State.Num(), Params.SeedingDensity);

	// Per-species seeding breakdown (diagnostic). Each species now scatters INDEPENDENTLY via
	// per-stratum Poisson-disk dart-throwing at its own TargetDensity (× the global SeedingDensity),
	// so strata stack and there is no winner-take-all — a mixed stand across strata is the expected,
	// healthy shape. A count far below TargetDensity·Area for a species is thinning: low suitability
	// (acceptance is suitability x the species' habitat response) plus min-spacing rejection, which
	// BITES HARDER at high
	// WPatch because offspring land close together inside a clump. That is clumping working, not a
	// monoculture bug. WPatch is the clumped-fraction dial p=WPatch/(1+WPatch): 0 = every dart uniform,
	// large = every dart a Gaussian offspring of a clump parent. It never picks a winner species.
	{
		TArray<int32> SeededPerSpecies;
		SeededPerSpecies.SetNumZeroed(Table.Num());
		for (int32 Index = 0; Index < State.Num(); ++Index)
		{
			const int32 SpeciesId = State.Species[Index];
			if (SeededPerSpecies.IsValidIndex(SpeciesId))
			{
				++SeededPerSpecies[SpeciesId];
			}
		}
		FString Breakdown;
		for (int32 SpeciesIndex = 0; SpeciesIndex < Table.Num(); ++SpeciesIndex)
		{
			Breakdown += FString::Printf(TEXT("%s%s(td=%.3f)=%d"),
				(SpeciesIndex > 0) ? TEXT(", ") : TEXT(""),
				*Table[SpeciesIndex].Name.ToString(),
				Table[SpeciesIndex].TargetDensity,
				SeededPerSpecies[SpeciesIndex]);
		}
		UE_LOGF(LogArcPCGForest, Log,"ForestSim: seeded per species (WPatch=%.3f): %ls", Params.WPatch, *Breakdown);
	}

	State.bSeeded = true;
}

bool FArcForestSimulator::StepIterations(
	FArcForestSolveState& State,
	const FArcForestSimParams& Params,
	const UArcForestSpeciesDefinition& Species,
	TArrayView<const float> SuitabilityGrid,
	FIntPoint GridDims,
	int32 MaxIters,
	const FArcForestHabitatInputs& Habitat) const
{
	using namespace ArcForestSimulatorLocal;

	const TArray<FArcForestSpeciesEntry>& Table = Species.Species;

	// Reused scratch buffers to avoid per-iteration reallocation. Declared LOCAL to this call: the only
	// live state persists in State, so the solve is bit-identical no matter how iterations are chunked.
	const FArcForestLightModel LightModel;
	TArray<EArcForestLayer> Bands;
	TArray<float> CrownLAIDensity;
	TArray<float> IncidentLight;

	// Query-height transmittance raster: the light a hypothetical plant of RecruitQueryHeightCm would
	// receive per cell, which is what the recruitment pass needs. Part of the reused-scratch block for the
	// same reason as the others — the solve runs on a UE::Tasks worker and must not allocate per
	// iteration; the light model re-sizes it with SetNumUninitialized, so only the first iteration
	// allocates. The recruitment spacing hash below joins the same block on the same terms.
	//
	// Emitted ONLY when recruitment is enabled. Off (the default) the pointer is null and the light model
	// skips the whole raster, which is what keeps this exactly behaviour-neutral for existing solves.
	//
	// SAMPLED BILINEARLY, deliberately, and the trailing-block geometry is the reason this note exists.
	// The raster's Bounds span Dims * Stride * CellSize — the raster's own cell rectangle — which for a
	// grid whose dims are not an exact multiple of the stride reaches PAST the LAI grid's real extent.
	// Bilinear taps are cell centres, so the LAST block of an axis has a nominal centre that can sit
	// beyond the grid the block's mean was computed over. Consequences, and why this is still the right
	// sampler:
	//   - No dart can sample outside the raster: the footprint always covers Bounds2D (it is >= the LAI
	//     grid extent, which is >= Bounds2D), so every tap is an existing cell and every value is a real
	//     mean over cells that exist — never invented data, never a NaN.
	//   - What the partial block costs is a sub-block SPATIAL bias on the final row/column only: the
	//     block's data centroid lies inward of its nominal centre, so the interpolation there is
	//     shifted by at most one raster cell (Stride * CellSize = 4 m at the defaults).
	//   - SampleFieldNearest is immune to this, but it would quantise the shade preference onto visible
	//     4 m steps across the WHOLE field — a worse artefact everywhere in exchange for exactness on one
	//     edge row. Recruitment is a preference, not a threshold, so smooth wins.
	FArcForestScalarField RecruitTransmittance;
	FArcForestScalarField* const TransmittancePtr = Params.bEnableRecruitment ? &RecruitTransmittance : nullptr;
	FRecruitScratch RecruitScratch;

	// QueryBand must be passed EXPLICITLY: the parameter defaults to EArcForestLayer::Field, and with the
	// shipped Field-only IntraLayerShading an omitted argument would include all four band grids for a
	// non-Field query height — maximum shade, silently wrong. Classified with the cm-scaled thresholds the
	// solver already owns (the light model deliberately never assumes a unit).
	const EArcForestLayer RecruitQueryBand = FArcForestLightModel::LayerForHeight(
		Params.RecruitQueryHeightCm, ShrubMinHeightCm, UnderstoryMinHeightCm, CanopyMinHeightCm);

	// The ceiling is RNG-free and iteration-independent, so it is resolved once per call rather than per
	// iteration. Area is the Bounds2D footprint in m^2, exactly as seeding computes it.
	//
	// RECRUITMENT REQUIRES A VALID Bounds2D, and that is checked rather than assumed: with an invalid or
	// degenerate footprint the light model derives the raster from the plant AABB instead, so the raster
	// and the dart positions would be in DIFFERENT frames — samples would silently land on the wrong
	// cells (both samplers clamp, so it would read as plausible edge values, not as an error). A
	// zero/negative area therefore disables recruitment entirely, which is also what seeding does.
	//
	// The test is a PER-AXIS restatement of ResolveBounds' own (bIsValid, Size.X > 0, Size.Y > 0), NOT the
	// area product, because the product is not equivalent: a doubly-inverted box (Max.X < Min.X AND
	// Max.Y < Min.Y — which FBox2f(Min, Max) happily constructs with bIsValid = 1, validating nothing)
	// has two negative extents and therefore a POSITIVE area. Gating on area would call recruitment
	// active while the light model rejected the same box and fell through to the plant-AABB branch —
	// precisely the two-different-frames failure this gate exists to prevent, reachable from a
	// hand-built FArcForestSimParams. The area is still what the ceiling is derived from.
	const FVector2f RecruitBoundsSize = Params.Bounds2D.GetSize();
	const float RecruitAreaMeters2 =
		(RecruitBoundsSize.X / CmPerMeter) * (RecruitBoundsSize.Y / CmPerMeter);
	const bool bRecruitmentActive =
		Params.bEnableRecruitment && (Params.Bounds2D.bIsValid != 0) &&
		(RecruitBoundsSize.X > 0.0f) && (RecruitBoundsSize.Y > 0.0f);
	const int32 RecruitCeiling =
		bRecruitmentActive ? ResolvePopulationCeiling(Params, Table, RecruitAreaMeters2) : 0;

	const int32 Iterations = FMath::Max(0, Params.Iterations);
	const int32 Effective = (MaxIters <= 0) ? Iterations : MaxIters;

	for (int32 Local = 0; Local < Effective && State.Iteration < Iterations; ++Local, ++State.Iteration)
	{
		const int32 NumPlants = State.Num();
		if (NumPlants == 0)
		{
			// Whole stand died out — nothing left to simulate. Recruitment cannot bring it back: stage (h)
			// sits below this break, and a stand with no live plants has no shade raster and (for any
			// species with a real MaturityHeightCm) no reproductively mature parent either.
			break;
		}

		// a. Live bands from CURRENT height (MC1 — recomputed every iteration; plants graduate up).
		ComputeBands(State, Bands, Params.bParallel);

		// b. Uniform per-plant crown LAI proxy (see CrownLeafAreaIndex note).
		CrownLAIDensity.SetNumUninitialized(NumPlants);
		for (int32 Index = 0; Index < NumPlants; ++Index)
		{
			CrownLAIDensity[Index] = CrownLeafAreaIndex;
		}

		// c. Incident light via the MC1 height-ordered top-down sweep. The same sweep also emits the
		// RecruitQueryHeightCm transmittance raster when recruitment is on — it can only be captured DURING
		// the sweep (mid-sweep accumulator snapshot), never reconstructed from IncidentLight afterwards.
		// This is the ONLY place the raster is produced, which is what makes stage (h) one stage stale.
		LightModel.ComputeIncidentLight(
			State.Pos, State.Height, State.CrownRadius, CrownLAIDensity, Bands, Params,
			LightGridCellSizeCm, IncidentLight,
			TransmittancePtr, Params.RecruitQueryHeightCm, Params.RecruitStride, RecruitQueryBand);

		// d. Light-gated growth and e. vigour update both consume THIS iteration's light snapshot.
		GrowPlants(Params, Table, IncidentLight, State);
		UpdateVigor(Table, IncidentLight, State, Params.bParallel);

		// e2. Horizontal competition (uses the just-updated vigour; snapshot-then-apply).
		ResolveCompetition(Params, Bands, State);

		// Diagnostic: the vigour distribution the mortality step is about to act on this iteration.
		// A flat population curve with vigour staying well above VigorMortalityFloor confirms that
		// recovery/competition are in balance (no self-thinning); vigour driven low but a flat curve
		// would instead point at the starvation-counter / recovery interaction.
		float VigorMin = TNumericLimits<float>::Max();
		double VigorSum = 0.0;
		int32 BelowFloor = 0;
		for (int32 Index = 0; Index < NumPlants; ++Index)
		{
			const float Vigor = State.Vigor[Index];
			VigorMin = FMath::Min(VigorMin, Vigor);
			VigorSum += Vigor;
			BelowFloor += (Vigor < VigorMortalityFloor) ? 1 : 0;
		}

		// f. Mortality + two-pass compaction.
		ApplyMortalityAndCompact(State, Params.bParallel);

		// g. Age every survivor (iterations survived).
		for (int32 Index = 0; Index < State.Num(); ++Index)
		{
			++State.Age[Index];
		}

		// Survivor count BEFORE recruitment, so "survivors"/"died" keep their exact pre-recruitment
		// meaning in the log below ("standing" is the post-recruitment count).
		const int32 NumAfter = State.Num();

		// h. Recruitment — new seedlings establish under the standing stand. It runs AFTER the aging loop
		// (so a recruit ends this iteration at Age 0, having faced no mortality) and immediately BEFORE
		// the per-iteration log, which is the only ordering under which that log can carry the recruited
		// count the plan requires of it. Reads the stage-(c) raster, which is therefore one stage stale
		// (documented on RecruitPlants).
		int32 Recruited = 0;
		if (bRecruitmentActive)
		{
			Recruited = RecruitPlants(
				Params, Table, SuitabilityGrid, GridDims, Habitat, RecruitTransmittance, RecruitCeiling,
				/*IterationOneBased=*/State.Iteration + 1, RecruitScratch, State);
		}

		// "survivors" is deliberately the PRE-recruitment count, so survivors/died stay a consistent pair
		// against this iteration's mortality. "standing" is what the next iteration actually starts from
		// (survivors + recruited) — without it the next line's survivors count appears to jump.
		const float VigorMean = NumPlants > 0 ? static_cast<float>(VigorSum / NumPlants) : 0.0f;
		UE_LOGF(LogArcPCGForest, Log,
			"ForestSim: iter %d/%d — survivors %d (died %d); vigor min=%.3f mean=%.3f, "
			"belowFloor(<%.2f)=%d, recruited=%d, standing=%d.",
			State.Iteration + 1, Iterations, NumAfter, NumPlants - NumAfter,
			VigorMin, VigorMean, VigorMortalityFloor, BelowFloor, Recruited, State.Num());
	}

	return State.Iteration >= Iterations || State.Num() == 0;
}

FArcForestResult FArcForestSimulator::Emit(
	const FArcForestSolveState& State,
	const UArcForestSpeciesDefinition& Species) const
{
	return ArcForestSimulatorLocal::Emit(State, Species.Species);
}

FArcForestResult FArcForestSimulator::Simulate(
	const FArcForestSimParams& Params,
	const UArcForestSpeciesDefinition& Species,
	TArrayView<const float> SuitabilityGrid,
	FIntPoint GridDims,
	TFunctionRef<bool()> ShouldYield,
	const FArcForestHabitatInputs& Habitat) const
{
	// Thin wrapper: seed, then step one iteration at a time, polling ShouldYield BETWEEN iterations to
	// preserve the "checked once between iterations, returns current survivors" early-abort semantics.
	FArcForestSolveState State;
	SeedInitial(State, Params, Species, SuitabilityGrid, GridDims, Habitat);

	bool bDone = false;
	while (!bDone)
	{
		bDone = StepIterations(State, Params, Species, SuitabilityGrid, GridDims, /*MaxIters=*/1, Habitat);
		if (!bDone && ShouldYield())
		{
			break;
		}
	}

	return Emit(State, Species);
}

FArcForestResult FArcForestSimulator::Simulate(
	const FArcForestSimParams& Params,
	const UArcForestSpeciesDefinition& Species,
	TArrayView<const float> SuitabilityGrid,
	FIntPoint GridDims,
	const FArcForestHabitatInputs& Habitat) const
{
	// Thin wrapper: seed, run every iteration to completion, emit.
	FArcForestSolveState State;
	SeedInitial(State, Params, Species, SuitabilityGrid, GridDims, Habitat);
	StepIterations(State, Params, Species, SuitabilityGrid, GridDims, /*MaxIters=*/0, Habitat);
	return Emit(State, Species);
}
