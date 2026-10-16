// Copyright Lukasz Baran. All Rights Reserved.

// ArcForestPatchClusteringTests.cpp
// TIER 1 pure-core CQTest coverage for conspecific patch clustering (Forest/ArcForestPatchField +
// the Gaussian-around-parents dart placement in Forest/ArcForestSimulator::SeedPopulation). Two
// independent checks:
//   * Clustering: with a strong clumped fraction (WPatch), a patch species' seeded plants aggregate —
//     measured with the BORDER METHOD (interior sub-window subjects, neighbours searched over the FULL
//     point set), so the Clark-Evans aggregation index R < 1 cannot be manufactured by the boundary
//     clamp in SeedPopulation nor by uncorrected edge bias. See the method header.
//   * Decorrelation: two species' parent lattices are statistically independent (per-species RNG
//     offset), so their parent sets are not identical/co-located.
// The clustering check is a FIXED-seed DETERMINISTIC ANCHOR: the seed is fixed and the solver is
// deterministic, so every measured value below reproduces exactly. There is no slack tolerance band —
// only fixed, reproducible measured values pinned against hard thresholds.

#include "CQTest.h"

#include "ArcForestTestHelpers.h"

#include "Forest/ArcForestSimTypes.h"
#include "Forest/ArcForestSimulator.h"
#include "Forest/ArcForestSpeciesDefinition.h"
#include "Forest/ArcForestPatchField.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcForestPatchTest, Log, All);

namespace ArcForestPatchClusteringTestsLocal
{
	using namespace ArcForestTestHelpers;

	/** Mean nearest-neighbour distance across a position set (brute force; test-scale N only). */
	float MeanNearestNeighbour(const TArray<FVector2f>& Positions)
	{
		const int32 Count = Positions.Num();
		if (Count < 2)
		{
			return 0.0f;
		}
		double Sum = 0.0;
		for (int32 I = 0; I < Count; ++I)
		{
			float BestSq = TNumericLimits<float>::Max();
			for (int32 J = 0; J < Count; ++J)
			{
				if (J == I)
				{
					continue;
				}
				const float DistSq = (Positions[I] - Positions[J]).SizeSquared();
				if (DistSq < BestSq)
				{
					BestSq = DistSq;
				}
			}
			Sum += FMath::Sqrt(BestSq);
		}
		return static_cast<float>(Sum / Count);
	}

	/**
	 * WHOLE-WINDOW Clark-Evans aggregation index R = observedMeanNN / expectedMeanNN, where the CSR
	 * (Poisson) expectation for N points over area A is 0.5 / sqrt(N / A). R < 1 => clustered, ~1 =>
	 * random, > 1 => dispersed. Returns 0 for degenerate inputs.
	 *
	 * DIAGNOSTIC ONLY — no assertion is pinned on this value any more. It carries TWO uncontrolled
	 * biases at this fixture's scale: (a) boundary subjects have no neighbours outside the window, which
	 * inflates mean-NN, and (b) SeedPopulation CLAMPS out-of-bounds Gaussian offspring onto the boundary
	 * lines instead of redrawing them (ArcForestSimulator.cpp:309-313), which concentrates points on the
	 * edge in the CLUMPED arm only and deflates mean-NN there. Use InteriorClarkEvans below for anything
	 * load-bearing; this one is retained purely so the log shows what the uncorrected estimator says.
	 */
	float ClarkEvans(const TArray<FVector2f>& Positions, float AreaCm2)
	{
		const int32 Count = Positions.Num();
		if (Count < 2 || AreaCm2 <= 0.0f)
		{
			return 0.0f;
		}
		const float Density = static_cast<float>(Count) / AreaCm2;
		const float Expected = 0.5f / FMath::Sqrt(Density);
		if (Expected <= 0.0f)
		{
			return 0.0f;
		}
		return MeanNearestNeighbour(Positions) / Expected;
	}

	/** Result of the border-method (interior-window) Clark-Evans estimator. */
	struct FInteriorClarkEvans
	{
		/** Number of SUBJECT points, i.e. points that fell inside the inset interior window. */
		int32 InteriorCount = 0;
		/** Mean nearest-neighbour distance over the subjects (neighbours drawn from the FULL set). */
		float MeanNN = 0.0f;
		/** MeanNN / (0.5 / sqrt(InteriorCount / InteriorArea)). */
		float R = 0.0f;
	};

	/**
	 * BORDER-METHOD Clark-Evans index — the standard edge-corrected spatial-statistics estimator.
	 *
	 * SUBJECTS are only the points inside `Bounds` inset by `InsetCm` on every side. NEIGHBOURS are
	 * searched over the FULL point set, INCLUDING the points in the excluded border band. Both halves
	 * of that sentence are load-bearing:
	 *   - Restricting SUBJECTS to the interior means every subject's true nearest neighbour lies inside
	 *     the full window whenever InsetCm covers the plausible NN range, so Donnelly-style edge bias
	 *     (boundary subjects missing their outside neighbours) vanishes instead of being merely bounded.
	 *     It also means no CLAMPED boundary point is ever a subject, so the per-axis clamp in
	 *     SeedPopulation cannot lower the measured mean NN.
	 *   - Searching NEIGHBOURS over the full set is what makes the estimator unbiased. A naive
	 *     interior-only search would delete each subject's genuine out-of-window neighbours and
	 *     reintroduce a WORSE truncation than the one being corrected (it would inflate mean-NN, i.e.
	 *     push R up, which for a "R < 1" assert would be conservative but for the control arm's
	 *     two-sided pin would be a real defect).
	 * The CSR expectation uses the INTERIOR count over the INTERIOR area — the subject population and
	 * the area they were sampled from must match, or the density term is wrong.
	 *
	 * Returns a zeroed struct for degenerate inputs (empty interior window, < 2 subjects, < 2 points).
	 */
	FInteriorClarkEvans InteriorClarkEvans(const TArray<FVector2f>& Positions, const FBox2f& Bounds, float InsetCm)
	{
		FInteriorClarkEvans Out;
		const int32 Count = Positions.Num();
		if (Count < 2 || InsetCm < 0.0f)
		{
			return Out;
		}

		const FVector2f InnerMin(Bounds.Min.X + InsetCm, Bounds.Min.Y + InsetCm);
		const FVector2f InnerMax(Bounds.Max.X - InsetCm, Bounds.Max.Y - InsetCm);
		if (InnerMax.X <= InnerMin.X || InnerMax.Y <= InnerMin.Y)
		{
			return Out;
		}
		const float InteriorArea = (InnerMax.X - InnerMin.X) * (InnerMax.Y - InnerMin.Y);

		double Sum = 0.0;
		int32 Subjects = 0;
		for (int32 I = 0; I < Count; ++I)
		{
			const FVector2f& Subject = Positions[I];
			const bool bInterior =
				Subject.X >= InnerMin.X && Subject.X <= InnerMax.X &&
				Subject.Y >= InnerMin.Y && Subject.Y <= InnerMax.Y;
			if (!bInterior)
			{
				continue;
			}

			// Neighbour search deliberately spans the FULL set, border band included.
			float BestSq = TNumericLimits<float>::Max();
			for (int32 J = 0; J < Count; ++J)
			{
				if (J == I)
				{
					continue;
				}
				const float DistSq = (Subject - Positions[J]).SizeSquared();
				if (DistSq < BestSq)
				{
					BestSq = DistSq;
				}
			}
			Sum += FMath::Sqrt(BestSq);
			++Subjects;
		}

		if (Subjects < 2)
		{
			return Out;
		}

		const float Expected = 0.5f / FMath::Sqrt(static_cast<float>(Subjects) / InteriorArea);
		if (Expected <= 0.0f)
		{
			return Out;
		}

		Out.InteriorCount = Subjects;
		Out.MeanNN = static_cast<float>(Sum / Subjects);
		Out.R = Out.MeanNN / Expected;
		return Out;
	}

	/** Species-1 seed counts + Clark-Evans indices from one seeding run at a given WPatch. */
	struct FSp1ClusterStats
	{
		/** Species-1 plants realized over the whole window. */
		int32 Count = 0;
		/** Subjects retained by the interior window. */
		int32 InteriorCount = 0;
		/** Mean nearest-neighbour distance over the subjects — logged so R can be checked by hand. */
		float MeanNN = 0.0f;
		/** BORDER-METHOD index — the ASSERTED value. */
		float R = 0.0f;
		/** Uncorrected whole-window index — logged for reference, never asserted. */
		float WholeWindowR = 0.0f;
	};

	FSp1ClusterStats MeasureSp1Cluster(
		const FArcForestSimulator& Simulator,
		const UArcForestSpeciesDefinition& Def,
		const FArcForestSimParams& Base,
		float WPatch,
		float InsetCm)
	{
		FArcForestSimParams Params = Base;
		Params.WPatch = WPatch;
		const TArray<float> EmptySuitability;
		const FArcForestResult Seeded = Simulator.Simulate(Params, Def, EmptySuitability, FIntPoint(0, 0));

		TArray<FVector2f> Sp1;
		for (int32 Index = 0; Index < Seeded.Num(); ++Index)
		{
			if (Seeded.Species[Index] == 1)
			{
				Sp1.Add(Seeded.PosXY[Index]);
			}
		}

		const FBox2f& Bounds = Base.Bounds2D;
		const float WholeArea = (Bounds.Max.X - Bounds.Min.X) * (Bounds.Max.Y - Bounds.Min.Y);
		const FInteriorClarkEvans Interior = InteriorClarkEvans(Sp1, Bounds, InsetCm);

		FSp1ClusterStats Stats;
		Stats.Count = Sp1.Num();
		Stats.InteriorCount = Interior.InteriorCount;
		Stats.MeanNN = Interior.MeanNN;
		Stats.R = Interior.R;
		Stats.WholeWindowR = ClarkEvans(Sp1, WholeArea);
		return Stats;
	}
}

TEST_CLASS(ArcForestPatchClustering, "ArcPCGExtension.ForestSim.PatchClustering")
{
	// CLUSTERING — INTERIOR-WINDOW (border-method) Clark-Evans, re-baselined for GAUSSIAN-AROUND-PARENTS
	// seeding (commits fa74bcadc0e2 + 628946456873) on an ENLARGED 80x80 m region.
	//
	// ALL ASSERTIONS BELOW ARE LOAD-BEARING — there is no "reference-only" arm in this test. The relative
	// separation is the strongest proof (see SIGNIFICANCE), followed by the absolute R < 1 on the clumped
	// arm and the LOWER pin on the uniform control arm; those three fail independently OF ONE ANOTHER on
	// different regressions. Scope that claim precisely: it is independence among the three R assertions.
	// NONE of them is independent of the Count == 1262 anchor, which is an exact equality on a shared
	// fixture and therefore moves under almost any parameter or stream change — it is the most sensitive
	// tripwire here and it fires first in most scenarios.
	// ONE EXCEPTION, stated so the list is not an overclaim: the control arm's UPPER pin (< 1.1406) has NO
	// independent detection power even among the R assertions. The Poisson-disk floor is shared by both
	// arms, so a more-dispersing floor raises the clumped arm's R and trips the absolute R < 1.0 assert
	// first. Keep the upper pin as a cheap BACKSTOP, not as an independent detector.
	//
	// OVERTURNED — the stale rationalization this header used to carry. The old text claimed that "the
	// hard min-spacing floor keeps the ABSOLUTE Clark-Evans index >= 1 no matter how sharp the affinity
	// is". That claim was TRUE ONLY OF THE OLD MODEL (uniform dart placement + affinity ACCEPTANCE
	// thinning): there, every candidate position was drawn uniformly over the bounds, so the surviving
	// accepted set stayed quasi-uniform — acceptance can only DELETE points, it can never move one
	// toward a conspecific — and the Poisson-disk floor on top of that made the realized pattern
	// mildly DISPERSED (R >= 1). It does NOT transfer to the current model. SeedPopulation now places
	// each dart DIRECTLY as Parent + Gaussian(0, sigma) with probability p = WPatch/(1+WPatch) (a Thomas
	// process); concentration comes from the POSITION distribution, so the point pattern is genuinely
	// aggregated before the floor ever sees it, and the floor only bites when sigma approaches the
	// spacing h. Hence the fixture below is pinned to the LOW-SpacingFraction regime.
	//
	// ESTIMATOR — BORDER METHOD, edge-corrected BY CONSTRUCTION (not "conservative by bias").
	// Subjects are only the species-1 points inside the bounds inset by 2*sigma (sigma = 220 cm =>
	// 440 cm inset, DERIVED IN CODE from the fixture's PatchRadiusSigma, never hardcoded); their nearest
	// neighbours are searched over the FULL species-1 set including the excluded border band; the CSR
	// expectation uses the interior count over the interior area. This discharges BOTH confounds the
	// previous whole-window version could not separate:
	//   (1) Donnelly edge bias — that correction adjusts the EXPECTED NN distance for a bounded window
	//       (the old header wrongly described it as correcting the OBSERVED mean-NN). With interior-only
	//       subjects the subjects' neighbourhoods are complete inside the full window, so there is no
	//       truncation left to correct and no unknown-sign residual to argue about.
	//   (2) Boundary-clamp artifact — SeedPopulation CLAMPS out-of-bounds Gaussian offspring per axis
	//       instead of redrawing them (ArcForestSimulator.cpp:309-313), concentrating ~4% of clumped
	//       darts (at 80x80 m; it was ~8.8% at the old 40x40 m) onto the four boundary lines. That
	//       concentration exists ONLY in the clumped arm. The border method removes its influence
	//       DETERMINISTICALLY, not statistically, by three independent steps:
	//         - Every clamped point has a coordinate EXACTLY at the region edge, so it is unconditionally
	//           outside the 440 cm inset and can never be a SUBJECT.
	//         - It survives only as a NEIGHBOUR, which is correct — it is a real plant — but it sits
	//           >= 440 cm from any subject while mean NN is ~111 cm, so its chance of being any subject's
	//           nearest neighbour is negligible (~4.5e-6 per subject).
	//         - The seven-draw block is UNCONDITIONAL, so a clamped dart being rejected cannot shift the
	//           RNG stream for any other dart either.
	//       MAGNITUDE, derived from the logged values below (derivation, NOT an assertion): an earlier
	//       revision of this header claimed the clamp deflated whole-window R by "an amount comparable to
	//       the whole margin" (~1.6%). That was an a-priori estimate and it is not what the data shows.
	//       The control arm (WPatch=0) is clamp-FREE — uniform darts come from FRandRange and always land
	//       strictly INSIDE THE REGION BOUNDS, never clamped onto a boundary line (this is the bounds
	//       sense, not the 440 cm inset sense: plenty of uniform darts fall in the border band and are
	//       excluded as subjects, 1294 -> 1023). Its shift 1.0650 -> 1.0606 = -0.0044 is therefore PURE
	//       edge-bias removal. The clumped arm shifted only -0.0024. Attributing the same PROPORTIONAL
	//       edge shift to both (-0.413%: 0.9584 * 1.0606/1.0650 = 0.95444) implies the clamp was
	//       deflating whole-window R by about +0.0016, i.e. ~3.6% of the 0.0440 margin — an order of
	//       magnitude smaller than the old claim. (Proportional, not additive, because R is a ratio; the
	//       additive reading would give +0.0020 / 4.5%, the same conclusion either way.)
	// The whole-window index is still computed and logged for reference; NOTHING asserts on it.
	//
	// PINNED FIXTURE (do not re-tune): Bilberry PatchRadiusSigma 220 / MaxCrownRadius 90 /
	// PatchDensity 0.015, SpacingFraction 0.2 (LOW — a REQUIREMENT, not incidental), seed 77,
	// 8000x8000 cm (6400 m^2), SeedingDensity 3.0, Iterations 0 (pure seeding).
	//   - PatchDensity 0.015 over 6400 m^2 => 96 clump parents.
	//   - Bilberry TargetDensity 0.075 * SeedingDensity 3.0 * 6400 m^2 = 1440 darts => 13.15 realized
	//     offspring per clump (1262/96), versus 12.79 (307/24) on the old 40x40 m region. The enlargement
	//     was NOT exactly yield-neutral: realized acceptance moved 85.3% -> 87.6% (fewer densely-packed
	//     boundary pile-ups getting rejected once the clamped fraction halved). It IS cliff-neutral,
	//     though, and for a reason that does not depend on that drift: darts-per-parent is EXACTLY 15 in
	//     both fixtures (1440/96 = 360/24, every density being per-area), and 13.15 realized sits nowhere
	//     near either named cliff (~5 low, ~23.67 high). Enlarging quadruples N (halving the sampling SD
	//     of R) and halves the clamped fraction.
	//   - Spacing floor h = 0.2 * (90 + 90) = 36 cm; sigma/h ~= 6.1, so the floor is NOT binding
	//     (observed mean NN is several times the floor). At the DEFAULT SpacingFraction 0.8 the floor is
	//     144 cm, sigma/h ~= 1.5, the floor dominates the Gaussian spread and R climbs back over 1 —
	//     that is why SpacingFraction MUST stay low here.
	//   - The fixture is pinned between TWO NAMED CLIFFS on the CLUMPED arm. LOW cliff: dropping
	//     offspring/clump toward ~5 (e.g. lower PatchDensity/smaller sigma) measured R = 0.8984 — still
	//     aggregated, a wider margin. HIGH cliff: RAISING offspring/clump (e.g. a higher TargetDensity or
	//     SeedingDensity — the previous TargetDensity 0.2 gives 40 offspring/clump) walks R back ABOVE 1:
	//     measured sweep 23.67 offspring/clump -> R = 1.0323; 30 offspring/clump -> R = 1.0683.
	//     UNVERIFIED-BY-ASSERTION: those three sweep values (0.8984 / 1.0323 / 1.0683) were measured by
	//     hand on the OLD 40x40 m whole-window fixture and NO assertion in this file pins them. Treat
	//     them as directional guidance about where the cliffs are, not as guarantees; re-measure before
	//     relying on them.
	//   - Species 0 (Moss) PatchRadiusSigma = 500 vs species 1's 220 is LOAD-BEARING, not decoration —
	//     see the comment on the Moss row in the fixture below. Do not "simplify" the two to one value.
	//
	// KNOWN GAP (for a later step — do NOT read this test as covering it): the whole-window index is
	// computed and logged but asserted by NOTHING, so all BOUNDARY-BAND spatial structure is unasserted.
	// A change to the out-of-bounds policy — clamp -> reflect, or clamp -> discard — alters the plant
	// distribution along the region edge, which is exactly where a per-PCG-cell forest sim would show a
	// visible SEAM between adjacent cells. Neither R assert would move, because subjects sit 440 cm
	// inside. Only the Count anchor would move, and that anchor's documented remedy is to re-baseline it.
	//
	// MEASURED REFERENCE RUN (this branch, the fixture above; interior window = 71.2 x 71.2 m):
	//   WPatch 8 (clumped, p = 8/9 of darts are Gaussian offspring):
	//       sp1 N=1262  interior N=993   R_interior=0.9560  (whole-window, unasserted: 0.9584)
	//   WPatch 0 (every dart uniform, identical seed/spacing/floor):
	//       sp1 N=1294  interior N=1023  R_interior=1.0606  (whole-window, unasserted: 1.0650)
	//   NOTE the interior index is LOWER than the whole-window one on the clumped arm (0.9560 vs
	//   0.9584). That settles the old open question in the clumped arm's favour: removing the clamped
	//   boundary points as SUBJECTS did not erode the aggregation signal, it slightly strengthened it,
	//   so the previously-measured margin was NOT a boundary-clamp artifact.
	//
	// SIGNIFICANCE — recomputed for THIS fixture, no external citation.
	//   WHAT THE SD IS, AND IS NOT: sigma_R below is the CSR-NULL sampling SD. It is the right yardstick
	//   for "is 0.9560 distinguishable from complete spatial randomness?" — it is NOT a calibrated
	//   run-to-run reproducibility margin, which is the other job this block is doing. Under the CLUSTERED
	//   alternative, within-clump nearest-neighbour distances are correlated (~13 offspring share a
	//   parent), so the effective independent-unit count sits somewhere between 96 (parents) and 993
	//   (points): the clumped arm's true perturbation SD is LARGER, plausibly by 1.5-3x. Read "2.65 sigma"
	//   as an UPPER BOUND on robustness. The mirror case holds on the control arm — it is Poisson-disk
	//   inhibited (hard-core), so its true SD is SMALLER than CSR and 1.634% over-estimates it.
	//   Single-arm Clark-Evans sampling SD: sigma_R = 0.52272 / sqrt(N_interior).
	//     clumped  N=993  -> sigma_R = 1.659%
	//     control  N=1023 -> sigma_R = 1.634%
	//   Absolute assert (R_interior < 1): margin 1 - 0.9560 = 0.0440 => 2.65 sigma.
	//   Difference statistic D = R_control - R_clumped = 0.1046, with
	//     sigma_D = sqrt(sigma_A^2 + sigma_B^2) = 2.329%  =>  D is 4.49 sigma above zero,
	//     and 2.34 sigma above the asserted -0.05 threshold (0.1046 - 0.05 = 0.0546).
	//   (For the record, the OLD 40x40 m whole-window fixture was weaker in both: sigma_R = 2.98% at
	//   N=307, sigma_D = 4.17%, D = 0.0938 => 2.25 sigma vs zero and only 1.05 sigma above the -0.05
	//   threshold. A "~3.1 sigma" figure previously stated here was simply wrong arithmetic.)
	//
	// THE ACTUAL PROOF IS DETERMINISTIC, NOT STATISTICAL. In SeedPopulation, `Pos` is initialised to
	// (DrawUniformX, DrawUniformY) BEFORE the `bClumped` branch, and the seven-draw block is
	// UNCONDITIONAL, so the RNG stream stays position-aligned regardless of which branch is taken. A
	// TRUE REVERT of the Gaussian branch to uniform placement therefore makes the WPatch=8 and WPatch=0
	// arms produce BIT-IDENTICAL point sets: D collapses to exactly 0, and the relative assert fires
	// DETERMINISTICALLY, not with some probability. The sigma figures above govern only PARTIAL
	// degradation (a widened sigma, a weakened clumped fraction, a floor that starts to bite), which is
	// the regime where margin-in-sigma actually matters.
	//
	// CONTROL-ARM CLIFF — runs in the OPPOSITE direction to the clumped arm's cliffs. The control pin is
	// TWO-SIDED on purpose: an unbounded-above `R > 1` would let a change that makes the Poisson-disk
	// floor MORE dispersing silently widen the relative gap and make the discriminator EASIER to pass
	// while real clumping degrades. The upper bound blocks that. The lower bound has its own hazard:
	// LOWERING TargetDensity or SeedingDensity shrinks N, which RAISES the CSR mean-NN expectation,
	// un-binds the 36 cm spacing floor (the only thing dispersing the uniform arm) and drives the control
	// toward R = 1 — an INNOCUOUS red with no production regression behind it.
	//   NOTE THE CONTRADICTION, deliberately: the "LOW cliff" note above suggests dropping toward ~5
	//   offspring/clump to widen the CLUMPED arm's margin if it goes red. That remedy would be applied by
	//   lowering exactly the parameters this control arm depends on, and would take the CONTROL arm red.
	//   The two arms cannot both be tuned by moving density. Fix the production regression instead.
	//
	// WHAT REGRESSES IT: reverting SeedPopulation's position generation to uniform + affinity
	// acceptance (or any change that stops offspring being drawn around a parent) puts the pattern back
	// in the R > 1 regime the WPatch=0 arm measures, and collapses the relative gap to zero -> RED.
	TEST_METHOD(Clustering_PatchSpeciesAggregates)
	{
		using namespace ArcForestPatchClusteringTestsLocal;

		TArray<FArcForestSpeciesEntry> Table;
		// Species 0: broad ground cover, larger patches, fills the ambient gaps.
		// LOAD-BEARING: PatchRadiusSigma 500 here vs 220 on species 1 is a deliberate ASYMMETRY, not a
		// stylistic choice. It is what makes a wrong-species SIGMA read detectable: if GetPatchRadiusSigma
		// (ArcForestSimulator.cpp:275) is ever indexed by the wrong species, the measured species uses
		// sigma = 500, its 4-sigma clump span (20 m) swamps the 8.2 m mean parent spacing, and the pattern
		// degenerates toward CSR. UNVERIFIED-BY-ASSERTION: "the R assertions then go RED" is the intended
		// mechanism but it has never been measured — treat it as design intent, not proven coverage.
		// SCOPE, precisely: this asymmetry guards GetPatchRadiusSigma ONLY. It does NOT guard
		// GetParentPositions (ArcForestSimulator.cpp:273) — if only the PARENT-POSITIONS index were wrong,
		// offspring would cluster around Moss's parents at sigma 220, still genuinely clustered, and every
		// R assert here would stay GREEN. Only the Count anchor would move. Do not "simplify" this.
		FArcForestSpeciesEntry Moss = MakeSpecies(TEXT("Moss"), EArcForestLayer::Field, 100.0f, 80.0f, 0.2f,
			0.05f, 500.0f, 0.02f); // TargetDensity 0.05 = broad sparse background
		Moss.LightNeeded = 0.3f;
		Moss.ShadeTolerance = 0.9f;
		Table.Add(MoveTemp(Moss));
		// Species 1: the measured patch species. Sparse parents (96) + tight sigma -> Gaussian offspring
		// pack into visible clumps. TargetDensity 0.075 * SeedingDensity 3.0 * 6400 m^2 = 1440 darts over
		// 96 parents = 15 darts per parent, 13.15 realized offspring per clump (1262/96).
		FArcForestSpeciesEntry Bilberry = MakeSpecies(TEXT("Bilberry"), EArcForestLayer::Field, 110.0f, 90.0f, 0.2f,
			0.075f, 220.0f, 0.015f); // PatchDensity 0.015 = 96 tight clumps
		Bilberry.LightNeeded = 0.3f;
		Bilberry.ShadeTolerance = 0.9f;
		Table.Add(MoveTemp(Bilberry));
		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));

		const float HalfCm = 4000.0f; // 80 m x 80 m (6400 m^2) — enlarged from 40 m, CLIFF-NEUTRAL
		const FArcForestSimulator Simulator;

		// Interior-window inset = 2 * the MEASURED species' PatchRadiusSigma, read from the fixture so a
		// sigma edit moves the estimator with it. 2 * 220 = 440 cm; interior window 7120 x 7120 cm, i.e.
		// 79.2% of the area and ~79% of the points retained as subjects (993/1262 clumped, 1023/1294
		// uniform — matching the area fraction, as an unbiased interior window should).
		const float PatchSigmaCm = Def->Species[1].PatchRadiusSigma;
		const float InteriorInsetCm = 2.0f * PatchSigmaCm;

		// Shared params; the ONLY difference between the two runs is WPatch (clumped vs all-uniform).
		FArcForestSimParams Base;
		Base.Seed = 77;
		Base.SeedingDensity = 3.0f;
		Base.Iterations = 0; // pure seeding: measure the seed-time patch structure, no mortality bias
		// LOW seed spacing (0.2 of summed crown radii) — a HARD REQUIREMENT of this fixture, not a
		// nicety. It puts the Poisson-disk floor at h = 36 cm against sigma = 220 cm (sigma/h ~= 6.1),
		// so the floor does not flatten the Gaussian offspring cloud back toward uniform. At the 0.8
		// default (h = 144 cm, sigma/h ~= 1.5) the floor dominates and R returns above 1.
		Base.SpacingFraction = 0.2f;
		Base.Bounds2D = CentredBounds(HalfCm);

		const FSp1ClusterStats Affinity =
			MeasureSp1Cluster(Simulator, *Def, Base, /*WPatch=*/8.0f, InteriorInsetCm);
		const FSp1ClusterStats NoAffinity =
			MeasureSp1Cluster(Simulator, *Def, Base, /*WPatch=*/0.0f, InteriorInsetCm);

		// MeanNN is logged so a reader can check the CSR expectation (0.5 / sqrt(interiorN / interiorArea))
		// by hand against the reported R without re-deriving it from the fixture.
		UE_LOGF(LogArcForestPatchTest, Display,"[Clustering] clumped sp1 N=%d interiorN=%d meanNN=%.2f Rint=%.4f Rwhole=%.4f   "
				"uniform sp1 N=%d interiorN=%d meanNN=%.2f Rint=%.4f Rwhole=%.4f",
			Affinity.Count, Affinity.InteriorCount, Affinity.MeanNN, Affinity.R, Affinity.WholeWindowR,
			NoAffinity.Count, NoAffinity.InteriorCount, NoAffinity.MeanNN, NoAffinity.R,
			NoAffinity.WholeWindowR);

		// Species-count anchor re-derived under Gaussian-around-parents seeding on the ENLARGED region:
		// 1440 darts (TargetDensity 0.075 * SeedingDensity 3.0 * 6400 m^2), suitability-only acceptance
		// (uniform grid -> 1.0), minus Poisson-disk rejections against the shared Field-stratum placed
		// set -> 1262 realized (was 307 on the old 40x40 m region).
		ASSERT_THAT(IsTrue(Affinity.Count == 1262,
			TEXT("Reference clumped patch-species seed count is 1262 (Gaussian-around-parents seeding "
				"regression anchor, 80x80 m region)")));
		ASSERT_THAT(IsTrue(Affinity.R > 0.0f && NoAffinity.R > 0.0f,
			TEXT("Both runs must retain enough interior subjects to compute a Clark-Evans index")));

		// THE PAYOFF — ABSOLUTE aggregation, measured on the INTERIOR window. R < 1 means the realized
		// mean nearest-neighbour distance of interior subjects (neighbours drawn from the full set) is
		// BELOW the complete-spatial-randomness expectation for the same interior N and area, i.e. the
		// pattern is genuinely CLUSTERED and not merely "less dispersed than the uniform arm" — and not
		// an artifact of clamped boundary points, which the interior window excludes as subjects. This
		// assertion runs the once-wrong line: the Gaussian-offspring branch in SeedPopulation
		// (Pos = Parents[ParentIndex] + BoxMuller(U1,U2,Sigma), taken when DrawBranch < WPatch/(1+WPatch)).
		// Revert that branch to the previous uniform FRandRange(X)/FRandRange(Y) placement and the pattern
		// returns to the mildly-dispersed regime the WPatch=0 arm measures (R = 1.0606) -> RED.
		// Measured 0.9560 => 2.65 sigma below 1 (see SIGNIFICANCE in the header).
		ASSERT_THAT(IsTrue(Affinity.R < 1.0f,
			TEXT("Gaussian-around-parents seeding must produce genuine aggregation: interior-window "
				"Clark-Evans R < 1 (measured 0.9560; uniform-placement arm is 1.0606)")));

		// CONTROL ARM — TWO-SIDED pin, both sides load-bearing.
		// LOWER bound (> 1.02): under the SAME spacing floor, uniform placement must be DISPERSED, not
		// merely "less clustered". If it falls toward 1 the floor has stopped dispersing uniform darts and
		// the relative discriminator below loses its reference. Set at a round 1.02 floor, which sits
		// 2.48 sigma below the measured 1.0606 (sigma_R = 1.634% on this arm).
		// UPPER bound (< 1.1406 = measured + 0.08): a cheap BACKSTOP, not an independent detector — see the
		// header. A more-dispersing floor would trip the absolute R < 1.0 assert and the Count anchor long
		// before it reached here. It is kept only so the control arm cannot drift upward and silently widen
		// the relative gap. NEAR-RED it admits: raising SpacingFraction 0.2 -> 0.3 lifts the floor to 54 cm
		// (exact) and pushes this arm to roughly 1.13-1.14 — the EDGE of the pin, from a pure fixture edit
		// with no production regression behind it. UNVERIFIED-BY-ASSERTION: whether that actually trips
		// 1.1406 has NOT been measured; a hard-core NN model calibrated against the h=36 cm control run
		// puts it just BELOW the pin. Treat it as a hazard to re-measure, not as a known red.
		// See CONTROL-ARM CLIFF in the header: this arm's own failure mode is the opposite of the clumped
		// arm's, and lowering TargetDensity/SeedingDensity to rescue the clumped arm takes this one red.
		ASSERT_THAT(IsTrue(NoAffinity.R > 1.02f,
			TEXT("Uniform-placement control arm must be DISPERSED (interior Clark-Evans R > 1.02): the "
				"spacing floor disperses uniform darts (measured 1.0606). If this falls toward 1 the "
				"control arm no longer refutes the overturned uniform-placement claim — check whether N "
				"dropped (lower TargetDensity/SeedingDensity un-binds the 36 cm floor).")));
		ASSERT_THAT(IsTrue(NoAffinity.R < 1.1406f,
			TEXT("Uniform-placement control arm must not become MORE dispersed than its reference "
				"(interior Clark-Evans R < 1.1406, measured 1.0606): a more-dispersing spacing floor would "
				"widen the clumped/uniform gap and make the relative discriminator below easier to pass "
				"while real aggregation degrades.")));

		// RELATIVE DISCRIMINATOR — the STRONGEST of the three, and deterministic under a true revert.
		// Both arms share seed, spacing, floor and the unconditional seven-draw RNG block, and `Pos` is
		// initialised to the uniform draw before the bClumped branch — so reverting the Gaussian branch to
		// uniform placement makes the two arms BIT-IDENTICAL and drives this difference to exactly 0.
		// Statistically (for PARTIAL degradation): D = 0.1046, sigma_D = 2.329% => 4.49 sigma above zero
		// and 2.34 sigma above this -0.05 threshold, versus the absolute assert's 2.65 sigma.
		ASSERT_THAT(IsTrue(Affinity.R < NoAffinity.R - 0.05f,
			TEXT("Clumped (Gaussian-around-parents) placement must be measurably more aggregated than the "
				"uniform control by more than a 0.05 margin (measured 0.9560 vs 1.0606): if this narrows, "
				"the clumped/uniform separation this test relies on is eroding even if the absolute R < 1 "
				"anchor still passes.")));
	}

	// DECORRELATION — the two species' patch parent lattices are independent (per-species seed
	// offset), so their accepted parent sets are not identical, and both species actually seed plants
	// (the mix is not a monoculture). Built directly against FArcForestPatchField for the parent sets.
	TEST_METHOD(Decorrelation_DistinctParentLattices)
	{
		using namespace ArcForestPatchClusteringTestsLocal;

		TArray<FArcForestSpeciesEntry> Table;
		FArcForestSpeciesEntry Moss = MakeSpecies(TEXT("Moss"), EArcForestLayer::Field, 100.0f, 80.0f, 0.2f,
			0.1f, 300.0f, 0.03f); // TargetDensity unused here (drives PatchField.Build directly)
		Moss.LightNeeded = 0.3f;
		Moss.ShadeTolerance = 0.9f;
		Table.Add(MoveTemp(Moss));
		FArcForestSpeciesEntry Bilberry = MakeSpecies(TEXT("Bilberry"), EArcForestLayer::Field, 110.0f, 90.0f, 0.2f,
			0.1f, 300.0f, 0.03f); // TargetDensity unused here (drives PatchField.Build directly)
		Bilberry.LightNeeded = 0.3f;
		Bilberry.ShadeTolerance = 0.9f;
		Table.Add(MoveTemp(Bilberry));
		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));

		FArcForestSimParams Params;
		Params.Seed = 77;
		Params.Bounds2D = CentredBounds(2000.0f);

		const TArray<float> EmptySuitability;
		FArcForestPatchField PatchField;
		PatchField.Build(*Def, Params, EmptySuitability, FIntPoint(0, 0));

		ASSERT_THAT(IsTrue(PatchField.GetNumSpecies() == 2,
			TEXT("Patch field must build one parent set per species")));

		const TArrayView<const FVector2f> Parents0 = PatchField.GetParentPositions(0);
		const TArrayView<const FVector2f> Parents1 = PatchField.GetParentPositions(1);

		ASSERT_THAT(IsTrue(Parents0.Num() > 0 && Parents1.Num() > 0,
			TEXT("Both species must scatter patch parents")));

		// STATISTICAL independence, not just "not element-wise identical" (which a single differing
		// parent would satisfy). If the two species shared one RNG lattice their parents would be
		// co-located; per-species seed offsets make them land independently, so only a SMALL fraction
		// of species-0 parents fall within one PatchRadiusSigma of ANY species-1 parent — roughly the
		// chance-coincidence rate for two independent point sets at this density.
		const float Sigma = Def->Species[0].PatchRadiusSigma; // 300 cm (equal for both species)
		const float SigmaSq = Sigma * Sigma;
		int32 NearCoincident = 0;
		for (int32 I = 0; I < Parents0.Num(); ++I)
		{
			for (int32 J = 0; J < Parents1.Num(); ++J)
			{
				if ((Parents0[I] - Parents1[J]).SizeSquared() <= SigmaSq)
				{
					++NearCoincident;
					break; // this species-0 parent has a species-1 neighbour; stop
				}
			}
		}
		const float CoincidentFraction = static_cast<float>(NearCoincident) / static_cast<float>(Parents0.Num());

		UE_LOGF(LogArcForestPatchTest, Display,"[Decorrelation] parents0=%d parents1=%d nearCoincident=%d fraction=%.4f",
			Parents0.Num(), Parents1.Num(), NearCoincident, CoincidentFraction);

		// Reference fraction = 0.6250 (the chance-coincidence rate for two INDEPENDENT 48-parent sets
		// at this density/sigma). Co-located (bugged, shared-RNG) lattices would push every species-0
		// parent onto its species-1 twin -> fraction -> 1.0, so the 0.85 threshold discriminates
		// independence from co-location.
		ASSERT_THAT(IsTrue(CoincidentFraction < 0.85f,
			TEXT("Species parent lattices must be independent: near-coincident fraction < 0.85 (reference 0.6250)")));
	}
};
