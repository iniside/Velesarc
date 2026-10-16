// Copyright Lukasz Baran. All Rights Reserved.

// ArcForestSimCoreTests.cpp
// TIER 1 pure-core CQTest coverage for the ArcPCGExtension forest solver (Forest/ArcForestSimulator,
// Forest/ArcForestLightModel). No PCG, no editor, no RHI: every case builds an in-memory
// UArcForestSpeciesDefinition and drives the pure array-in/array-out solver directly.
//
// The reference numbers pinned in each test come from an actual run on this branch at the stated
// seed/params (see the "REFERENCE RUN" comment on each case). They are regression anchors — a change
// to a tuning constant or the RNG order will trip them, which is the point.
//
// DETERMINISM CASE drives the COMPLETION overload Simulate(Params, Species, Grid, Dims) WITHOUT a
// ShouldYield functor: the yield path is an early-abort that is timing-dependent and would make an
// element-wise identity assertion flaky. This is a deliberate hostile-review / proof-auditor
// requirement, restated here so a future edit does not "helpfully" switch to the yield overload.

#include "CQTest.h"

#include "ArcForestTestHelpers.h"

#include "Forest/ArcForestSimTypes.h"
#include "Forest/ArcForestSimulator.h"
#include "Forest/ArcForestSpeciesDefinition.h"
#include "Forest/ArcForestLightModel.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcForestTest, Log, All);

namespace ArcForestSimCoreTestsLocal
{
	using namespace ArcForestTestHelpers;

	// cm thresholds the solver itself uses (ArcForestSimulator.cpp): 1.5 m / 5 m / 15 m in cm.
	constexpr float ShrubMinCm = 150.0f;
	constexpr float UnderstoryMinCm = 500.0f;
	constexpr float CanopyMinCm = 1500.0f;

	/** All arrays of two results equal element-wise (the determinism contract). */
	bool ResultsIdentical(const FArcForestResult& A, const FArcForestResult& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A.PosXY[Index] != B.PosXY[Index]) { return false; }
			if (A.Species[Index] != B.Species[Index]) { return false; }
			if (A.MatureLayer[Index] != B.MatureLayer[Index]) { return false; }
			if (A.CrownRadius[Index] != B.CrownRadius[Index]) { return false; }
			if (A.Height[Index] != B.Height[Index]) { return false; }
			if (A.Age[Index] != B.Age[Index]) { return false; }
			if (A.Vigor[Index] != B.Vigor[Index]) { return false; }
			if (A.PatchId[Index] != B.PatchId[Index]) { return false; }
		}
		return true;
	}

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

	/** Live competition band of a survivor from its CURRENT height (the same rule ComputeBands uses). */
	EArcForestLayer BandOfHeight(float HeightCm)
	{
		return FArcForestLightModel::LayerForHeight(HeightCm, ShrubMinCm, UnderstoryMinCm, CanopyMinCm);
	}

	/**
	 * Largest crown-overlap fraction (r_i+r_j-dist)/(r_i+r_j) over all SAME-BAND survivor pairs, in
	 * [0,1] (0 = no same-band pair overlaps). This is the spacing the self-thinning claim is about:
	 * within a competition band, surviving crowns must not sit deeply inside one another.
	 */
	float SameBandMaxOverlapFraction(const FArcForestResult& Result)
	{
		float MaxOverlap = 0.0f;
		const int32 Count = Result.Num();
		for (int32 I = 0; I < Count; ++I)
		{
			const EArcForestLayer BandI = BandOfHeight(Result.Height[I]);
			for (int32 J = I + 1; J < Count; ++J)
			{
				if (BandOfHeight(Result.Height[J]) != BandI)
				{
					continue;
				}
				const float SumRadii = Result.CrownRadius[I] + Result.CrownRadius[J];
				if (SumRadii <= 0.0f)
				{
					continue;
				}
				const float Dist = (Result.PosXY[I] - Result.PosXY[J]).Size();
				if (Dist < SumRadii)
				{
					MaxOverlap = FMath::Max(MaxOverlap, (SumRadii - Dist) / SumRadii);
				}
			}
		}
		return MaxOverlap;
	}

	/** A three-species mixed forest (canopy / understory / field) used by several cases. */
	TStrongObjectPtr<UArcForestSpeciesDefinition> MakeMixedForest()
	{
		TArray<FArcForestSpeciesEntry> Table;
		// Canopy pioneer: tall, wide, fast, light-demanding (high LightNeeded, low tolerance).
		FArcForestSpeciesEntry Oak = MakeSpecies(TEXT("Oak"), EArcForestLayer::Canopy, 2400.0f, 600.0f, 0.14f,
			0.02f, 350.0f, 0.05f); // TargetDensity 0.02 = sparse canopy (wide crown)
		Oak.LightNeeded = 0.6f;
		Oak.ShadeTolerance = 0.25f;
		Table.Add(MoveTemp(Oak));
		// Understory shade-tolerant: medium, slower, survives shade (low LightNeeded, high tolerance).
		FArcForestSpeciesEntry Hazel = MakeSpecies(TEXT("Hazel"), EArcForestLayer::Understory, 900.0f, 300.0f, 0.10f,
			0.1f, 250.0f, 0.08f); // TargetDensity 0.1 = mid-dense understory
		Hazel.LightNeeded = 0.3f;
		Hazel.ShadeTolerance = 0.8f;
		Table.Add(MoveTemp(Hazel));
		// Field ground cover: short, small crown, shade-tolerant fern.
		FArcForestSpeciesEntry Fern = MakeSpecies(TEXT("Fern"), EArcForestLayer::Field, 120.0f, 90.0f, 0.20f,
			0.5f, 200.0f, 0.12f); // TargetDensity 0.5 = dense ground cover
		Fern.LightNeeded = 0.25f;
		Fern.ShadeTolerance = 0.9f;
		Table.Add(MoveTemp(Fern));
		return MakeDefinition(MoveTemp(Table));
	}
}

TEST_CLASS(ArcForestSimCore, "ArcPCGExtension.ForestSim.Core")
{
	// DETERMINISM — the same (Params, Species, SuitabilityGrid, Dims) MUST yield element-wise
	// identical survivors. Driven through the completion overload (NO ShouldYield) so the result is
	// fully determined, never timing-dependent.
	//
	// REFERENCE RUN (seed 20250720, 2000x2000 cm, density 2.0, 24 iterations, uniform suitability,
	// per-species Poisson-disk seeding at SpacingFraction 0.8 default):
	//   survivors = 116 (identity holds regardless of the count; the exact count is a regression anchor).
	// RE-BASELINED (was 107) for GAUSSIAN-AROUND-PARENTS seeding, commits fa74bcadc0e2 + 628946456873:
	// the per-dart draw block went 3 -> 7 variates and dart positions are now drawn around clump parents,
	// so the realized seed set — and therefore the survivor set — is a different (deterministic) one.
	// The IDENTITY assertion below is unchanged and is the real contract; the count is the anchor that
	// pins the fixed-7-draw stream order (any change to the draw count/order moves it).
	TEST_METHOD(Determinism_SameInputsIdenticalSurvivors)
	{
		using namespace ArcForestSimCoreTestsLocal;

		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeMixedForest();

		FArcForestSimParams Params;
		Params.Seed = 20250720;
		Params.SeedingDensity = 2.0f;
		Params.Iterations = 24;
		Params.Bounds2D = CentredBounds(1000.0f); // 20 m x 20 m

		const TArray<float> EmptySuitability; // empty -> uniform suitability 1.0
		const FIntPoint Dims(0, 0);

		const FArcForestSimulator Simulator;
		const FArcForestResult First = Simulator.Simulate(Params, *Def, EmptySuitability, Dims);
		const FArcForestResult Second = Simulator.Simulate(Params, *Def, EmptySuitability, Dims);

		UE_LOGF(LogArcForestTest, Display,"[Determinism] survivors=%d", First.Num());

		ASSERT_THAT(IsTrue(First.Num() > 0, TEXT("Determinism run must produce at least one survivor")));
		ASSERT_THAT(IsTrue(First.Num() == 116,
			TEXT("Reference survivor count for this fixed seed/params is 116 (Gaussian-around-parents "
				"seeding regression anchor)")));
		ASSERT_THAT(IsTrue(ResultsIdentical(First, Second),
			TEXT("Identical inputs must produce element-wise identical survivor arrays")));
	}

	// SELF-THINNING — under forced same-band competition the survivor count drops below the seeded
	// count and the survivors SPREAD OUT (mean nearest-neighbour distance rises): the Yoda/Reineke
	// self-thinning signature. A single wide-crowned canopy species at high density guarantees crown
	// overlap so the weaker-of-two suppression bites.
	//
	// REFERENCE RUN (seed 4242, 3000x3000 cm, density 4.0, 40 iterations, single species r_max=650,
	// per-species Poisson-disk seeding at SpacingFraction 0.25 — see the fixture comment):
	//   seeded = 67, survivors = 20, meanNN seeded = 345.52 cm, meanNN survivors = 535.66 cm,
	//   worst same-band overlap = 0.7328.
	// RE-BASELINED (was seeded 63 / survivors 18 / meanNN0 347.70 / meanNNK 621.79 / overlap 0.6811) for
	// GAUSSIAN-AROUND-PARENTS seeding, commits fa74bcadc0e2 + 628946456873. The counts move because the
	// dart stream changed (3 -> 7 draws per dart) and darts now land as Gaussian offspring of clump
	// parents instead of uniformly, so a different deterministic seed set survives the Poisson-disk floor.
	// The RELATIVE invariants below were re-verified against this run and still hold with margin:
	//   survivors 20 < seeded 67, and meanNN survivors 535.66 > meanNN seeded 345.52.
	TEST_METHOD(SelfThinning_CountDropsAndSpreadsOut)
	{
		using namespace ArcForestSimCoreTestsLocal;

		TArray<FArcForestSpeciesEntry> Table;
		FArcForestSpeciesEntry DenseOak = MakeSpecies(TEXT("DenseOak"), EArcForestLayer::Canopy, 2000.0f, 650.0f, 0.25f,
			2.0f, 400.0f, 0.02f); // TargetDensity 2.0 = far above spacing saturation; mature crowns overlap -> competition thins
		DenseOak.LightNeeded = 0.5f;
		DenseOak.ShadeTolerance = 0.3f;
		Table.Add(MoveTemp(DenseOak));
		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));

		FArcForestSimParams Params;
		Params.Seed = 4242;
		Params.SeedingDensity = 4.0f;
		// Tight seed spacing (0.25 of summed mature crown radii) so seedlings pack in and their MATURE
		// crowns overlap heavily -> same-band competition actually bites and the stand self-thins. At the
		// 0.8 default the Poisson-disk pre-disperses the seed set so far that no crown overlaps and nothing
		// dies (seeded == survivors) -> this test would be vacuous. This value forces real mortality.
		Params.SpacingFraction = 0.25f;
		Params.Bounds2D = CentredBounds(1500.0f); // 30 m x 30 m

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		// Seeded population (0 iterations = pure seeding, no mortality yet).
		FArcForestSimParams SeedOnly = Params;
		SeedOnly.Iterations = 0;
		const FArcForestResult Seeded = Simulator.Simulate(SeedOnly, *Def, EmptySuitability, Dims);

		// Matured population after competition thins it.
		Params.Iterations = 40;
		const FArcForestResult Survivors = Simulator.Simulate(Params, *Def, EmptySuitability, Dims);

		const float NN0 = MeanNearestNeighbour(Seeded.PosXY);
		const float NNK = MeanNearestNeighbour(Survivors.PosXY);
		const float MaxOverlap = SameBandMaxOverlapFraction(Survivors);

		UE_LOGF(LogArcForestTest, Display,"[SelfThinning] seeded=%d survivors=%d meanNN0=%.2f meanNNK=%.2f maxSameBandOverlap=%.4f",
			Seeded.Num(), Survivors.Num(), NN0, NNK, MaxOverlap);

		ASSERT_THAT(IsTrue(Seeded.Num() == 67,
			TEXT("Reference seeded count is 67 (Gaussian-around-parents seeding at SpacingFraction 0.25 "
				"regression anchor)")));
		ASSERT_THAT(IsTrue(Survivors.Num() == 20,
			TEXT("Reference survivor count is 20 (Gaussian-around-parents seeding regression anchor)")));
		ASSERT_THAT(IsTrue(Survivors.Num() < Seeded.Num(),
			TEXT("Competition must thin the stand: survivors < seeded")));
		ASSERT_THAT(IsTrue(Survivors.Num() > 0, TEXT("Self-thinning must not wipe the whole stand")));
		ASSERT_THAT(IsTrue(NNK > NN0,
			TEXT("Survivors must spread out: mean nearest-neighbour distance rises vs seeded")));

		// Crown-spacing claim (documented tolerance). This solver thins by MORTALITY, not by hard
		// radius separation: it penalises overlapping same-band crowns each iteration until the weaker
		// dies, so survivors may still PARTIALLY overlap — they are not driven fully apart. Under
		// Gaussian-around-parents seeding the reference run's worst same-band overlap is 0.7328 (was
		// 0.6811 under uniform placement — offspring start closer together inside a clump, so the closest
		// surviving pair is tighter), i.e. the closest surviving pair still keeps
		// dist >= (r_i+r_j)*(1-0.7328). The 0.85 tolerance is UNCHANGED and still discriminates: it
		// documents the permitted partial overlap while failing a degenerate regression where competition
		// collapsed and survivors stacked (overlap -> 1.0, crowns near-coincident).
		ASSERT_THAT(IsTrue(MaxOverlap < 0.85f,
			TEXT("Same-band survivors must not stack: dist >= (r_i+r_j)*(1-0.85) (reference max 0.7328)")));
	}

	// LIGHT ATTENUATION — a short plant directly under a tall dense crown receives strictly less
	// incident light than an open-grown short plant. Tests FArcForestLightModel::ComputeIncidentLight
	// directly with a hand-built 2-plant set. With CanopyExtinction k=0.5 and one overlapping crown of
	// LAI=2.0, the shaded plant gets exp(-0.5*2.0) = exp(-1) ~= 0.36788; the open plant gets 1.0.
	TEST_METHOD(LightAttenuation_ShadedBelowTallCrown)
	{
		using namespace ArcForestSimCoreTestsLocal;

		FArcForestSimParams Params;
		Params.CanopyExtinction = 0.5f;
		Params.Bounds2D = CentredBounds(1000.0f);

		// Shaded scene: tall dense crown at origin + short plant at origin.
		const TArray<FVector2f> Positions = { FVector2f(0.0f, 0.0f), FVector2f(0.0f, 0.0f) };
		const TArray<float> Heights = { 2000.0f, 50.0f };
		const TArray<float> CrownRadii = { 500.0f, 100.0f };
		const TArray<float> CrownLAI = { 2.0f, 2.0f };
		const TArray<EArcForestLayer> Bands = { EArcForestLayer::Canopy, EArcForestLayer::Field };

		const FArcForestLightModel LightModel;
		TArray<float> ShadedLight;
		LightModel.ComputeIncidentLight(
			Positions, Heights, CrownRadii, CrownLAI, Bands, Params, 100.0f, ShadedLight);

		// Open scene: the same short plant, alone.
		const TArray<FVector2f> OpenPos = { FVector2f(0.0f, 0.0f) };
		const TArray<float> OpenHeights = { 50.0f };
		const TArray<float> OpenRadii = { 100.0f };
		const TArray<float> OpenLAI = { 2.0f };
		const TArray<EArcForestLayer> OpenBands = { EArcForestLayer::Field };
		TArray<float> OpenLight;
		LightModel.ComputeIncidentLight(
			OpenPos, OpenHeights, OpenRadii, OpenLAI, OpenBands, Params, 100.0f, OpenLight);

		UE_LOGF(LogArcForestTest, Display,"[Light] shadedShort=%.5f openShort=%.5f tall=%.5f",
			ShadedLight[1], OpenLight[0], ShadedLight[0]);

		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(OpenLight[0], 1.0f, 1e-4f),
			TEXT("Open-grown short plant must receive full light (1.0)")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ShadedLight[0], 1.0f, 1e-4f),
			TEXT("Tallest plant is shaded by nothing above it -> full light")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ShadedLight[1], FMath::Exp(-1.0f), 1e-3f),
			TEXT("Short plant under one LAI=2 crown at k=0.5 must get exp(-1)")));
		ASSERT_THAT(IsTrue(ShadedLight[1] < OpenLight[0],
			TEXT("Shaded short plant must receive less light than the open-grown one")));
	}

	// REGENERATION KEYSTONE (MC1 — the flagship; H1's exact bug). A canopy-SPECIES seedling under a
	// mature canopy must be SHADE-GATED but NOT bucketed into the canopy competition band against the
	// mature crowns. The live competition/light band is derived from a plant's CURRENT height, never
	// from its species MatureLayer. This test asserts that invariant at its locus and FAILS if anyone
	// reverts to species-fixed bands.
	//
	//  - A 10 cm canopy-species seedling classifies to Field; a 2000 cm mature classifies to Canopy.
	//    They are DIFFERENT bands, so ResolveCompetition (which only pairs within a band) never lets
	//    the mature crown cull the seedling. Under species-fixed bands BOTH would be Canopy -> the
	//    seedling would be the weaker-of-two every iteration and get thinned out (the H1 bug).
	//  - Cross-band (vertical) shading is always on, so the seedling under the mature dense crown is
	//    shaded (low incident light) yet alive (light > 0): "shade-gated, not deleted."
	TEST_METHOD(RegenerationKeystone_SeedlingBandFromHeightNotSpecies)
	{
		using namespace ArcForestSimCoreTestsLocal;

		// (1) Band is derived from CURRENT height (MC1), not the species mature form.
		const EArcForestLayer SeedlingBand =
			FArcForestLightModel::LayerForHeight(10.0f, ShrubMinCm, UnderstoryMinCm, CanopyMinCm);
		const EArcForestLayer MatureBand =
			FArcForestLightModel::LayerForHeight(2000.0f, ShrubMinCm, UnderstoryMinCm, CanopyMinCm);

		ASSERT_THAT(IsTrue(SeedlingBand == EArcForestLayer::Field,
			TEXT("A 10 cm canopy-species seedling must live in the Field band (MC1), not the Canopy")));
		ASSERT_THAT(IsTrue(MatureBand == EArcForestLayer::Canopy,
			TEXT("A 2000 cm mature tree must live in the Canopy band")));
		ASSERT_THAT(IsTrue(SeedlingBand != MatureBand,
			TEXT("Seedling and mature must occupy DIFFERENT competition bands -> never same-band culled")));

		// The species mature form IS Canopy — proving the two classifications genuinely diverge. Under
		// the reverted (buggy) species-fixed model the seedling would share MatureBand and be culled.
		TArray<FArcForestSpeciesEntry> Table;
		FArcForestSpeciesEntry ShadeOak = MakeSpecies(TEXT("ShadeOak"), EArcForestLayer::Canopy, 2400.0f, 600.0f, 0.12f,
			0.05f, 350.0f, 0.05f); // TargetDensity irrelevant here (MatureLayer-only assertion)
		ShadeOak.LightNeeded = 0.35f;
		ShadeOak.ShadeTolerance = 0.85f;
		Table.Add(MoveTemp(ShadeOak));
		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));
		ASSERT_THAT(IsTrue(Def->Species[0].MatureLayer == EArcForestLayer::Canopy,
			TEXT("Species MatureLayer is Canopy -> species-fixed bands would mis-bucket the seedling")));

		// (2) The seedling under a mature dense crown is shaded (< full light) but alive (> 0).
		FArcForestSimParams Params;
		Params.CanopyExtinction = 0.5f;
		Params.Bounds2D = CentredBounds(1000.0f);

		const TArray<FVector2f> Positions = { FVector2f(0.0f, 0.0f), FVector2f(0.0f, 0.0f) };
		const TArray<float> Heights = { 2000.0f, 10.0f };
		const TArray<float> CrownRadii = { 600.0f, 5.0f };
		const TArray<float> CrownLAI = { 2.0f, 2.0f };
		const TArray<EArcForestLayer> Bands = { EArcForestLayer::Canopy, EArcForestLayer::Field };

		const FArcForestLightModel LightModel;
		TArray<float> Light;
		LightModel.ComputeIncidentLight(
			Positions, Heights, CrownRadii, CrownLAI, Bands, Params, 100.0f, Light);

		UE_LOGF(LogArcForestTest, Display,"[Regen] seedlingLight=%.5f", Light[1]);

		ASSERT_THAT(IsTrue(Light[1] < 0.9f,
			TEXT("Seedling under the mature canopy must be shade-gated (well below full light)")));
		ASSERT_THAT(IsTrue(Light[1] > 0.0f,
			TEXT("Shade-gated seedling must still receive some light -> alive, not deleted")));

		// (3) THE DISCRIMINATING FULL-SOLVER ASSERTION — routed through ArcForestSimulator::ComputeBands
		// (the H1 bug locus, ArcForestSimulator.cpp (ComputeBands)). A single, dense, shade-tolerant canopy
		// species is seeded so EVERY survivor is a canopy-mature plant; the stand then self-thins over
		// the growth iterations. The surviving COUNT is the observable that separates the two banding
		// models:
		//
		//  - CORRECT (height-derived bands): plants stratify by their CURRENT height — the tall
		//    early-winners occupy the Canopy band while the many shaded juveniles sit in the
		//    Field/Shrub/Understory bands, where they compete only with each other, NOT the mature
		//    crowns above them. Most of the juvenile bank therefore survives -> a HIGH survivor count.
		//  - REVERTED (species-fixed bands): every plant is forced into the Canopy band from seed time
		//    regardless of height, so each shaded juvenile is pitted weaker-of-two against the mature
		//    crowns and culled. The stand collapses to the handful of dominants -> a LOW survivor count.
		//
		// The threshold sits ABOVE the reverted count and BELOW the correct count, so the assertion
		// FLIPS RED the moment ComputeBands is reverted (confirmed by the temporary line-303 revert in
		// the Step 8 review — reverted count << threshold << correct count).
		//
		// FULL-SOLVER GUARD that the banding rule is actually wired through ComputeBands
		// (ArcForestSimulator.cpp (ComputeBands)). Parts (1)+(2) prove the FORWARD mechanism directly and
		// deterministically (band from live height; cross-band shade without deletion). This part
		// proves the solver USES that rule end to end, by pinning a survivor-composition signature that
		// provably CHANGES when ComputeBands is reverted to species-fixed bands.
		//
		// Scenario: a fast, wide, light-demanding canopy pioneer (NURSE) interleaved with a slow,
		// short-staying, shade-tolerant canopy-mature species (SLOWOAK). The observable is the count of
		// ESTABLISHED canopy pioneers (Nurse survivors):
		//  - CORRECT (height-derived bands): the mature Nurses occupy the Canopy band and compete only
		//    with each other, so the canopy establishes normally -> 12 Nurse survivors (PRE-SEEDING-
		//    REWRITE measurement; the current Gaussian-around-parents value is 13, see below).
		//  - REVERTED (species-fixed bands): every plant collapses into its MatureLayer band, so the
		//    mature Nurses are forced to share the Canopy band with the whole short SlowOak population;
		//    the changed competition domains + per-band self-shading gating over-thin the canopy -> 9 Nurse
		//    survivors under the CURRENT seeding (measured in Step 3; it was 5 pre-seeding-rewrite).
		//    (The intended "short canopy bank" direction is NOT usable as the
		//    observable here: in this model crown radius scales with height, so a permanently-short
		//    species has a tiny crown that is never culled, and the per-band self-shading flag flips its
		//    light the other way — SlowOak survival actually RISES under the revert. The Nurse-count
		//    signature is the clean, deterministic flip.)
		//
		// The threshold sits strictly between the two counts, so the test FLIPS RED under the revert.
		// GOLD-STANDARD VERIFICATION (Step 8 review): temporarily reverting ComputeBands to
		//   OutBands[i] = Table[Pop.Species[i]].MatureLayer  drove Nurse survivors 12 -> 5 and turned
		// this test RED; restoring the height-based form returned it to GREEN.
		//
		// REFERENCE RUN (seed 31337, 2000x2000 cm, density 5.0, 18 iterations, WPatch 5.0, per-species
		// Poisson-disk seeding at SpacingFraction 0.3 — see the SimParams comment):
		//   CORRECT (height-derived bands) survivors = 33 (Nurse 13, SlowOak 20).
		// RE-BASELINED for GAUSSIAN-AROUND-PARENTS seeding (commits fa74bcadc0e2 + 628946456873): the
		// measured CORRECT Nurse count is now 13 (was 11 under uniform-placement Poisson-disk seeding,
		// and 12 pre-seeding-rewrite). WPatch 5.0 is no longer an affinity weight — it is the
		// clumped-fraction dial p = 5/6, so 5 of every 6 darts land as Gaussian offspring of a clump
		// parent; the mixed Nurse/SlowOak canopy stratum is now genuinely patchy rather than scattered.
		// PROOF GAP — CLOSED (Step 3 of the clump-seeding plan). The reverted (species-fixed band) Nurse
		// count HAS now been measured under this seeding: it is 9, against the correct 13. The old
		// threshold of 8 was therefore GREEN-ON-FAILURE — 9 clears it — and has been moved to 11. See the
		// MEASURED DISCRIMINATION BAND block on the assertion below for both numbers, the procedure, and
		// the margin caveat. Do not re-run the production revert to "close the gap"; it is closed.
		TArray<FArcForestSpeciesEntry> RegenTable;
		// sp0 NURSE — fast wide light-demanding canopy pioneer (establishes the overhead canopy).
		FArcForestSpeciesEntry Nurse = MakeSpecies(TEXT("Nurse"), EArcForestLayer::Canopy, 2000.0f, 600.0f, 0.30f,
			0.3f, 400.0f, 0.05f); // TargetDensity 0.3 = enough canopy pioneers to establish
		Nurse.LightNeeded = 0.55f;
		Nurse.ShadeTolerance = 0.2f;
		RegenTable.Add(MoveTemp(Nurse));
		// sp1 SLOWOAK — canopy-mature but crawling growth (stays short) + very shade-tolerant.
		FArcForestSpeciesEntry SlowOak = MakeSpecies(TEXT("SlowOak"), EArcForestLayer::Canopy, 2400.0f, 300.0f, 0.006f,
			0.5f, 300.0f, 0.08f); // TargetDensity 0.5 = dense shade-tolerant juvenile bank
		SlowOak.LightNeeded = 0.2f;
		SlowOak.ShadeTolerance = 0.97f;
		RegenTable.Add(MoveTemp(SlowOak));
		TStrongObjectPtr<UArcForestSpeciesDefinition> Regen = MakeDefinition(MoveTemp(RegenTable));

		FArcForestSimParams SimParams;
		SimParams.Seed = 31337;
		SimParams.SeedingDensity = 5.0f;
		SimParams.Iterations = 18;
		SimParams.WPatch = 5.0f; // strong affinity so both species actually seed (interleaved patches)
		// Tight seed spacing so the canopy stratum seeds a dense mixed stand (Nurse + SlowOak share the
		// Canopy stratum's placed-set) — at the 0.8 default only a handful seed and the band-model
		// discrimination has no juvenile bank to stress. This restores a stand the height-derived band
		// rule can actually stratify.
		SimParams.SpacingFraction = 0.3f;
		SimParams.Bounds2D = CentredBounds(1000.0f);

		const TArray<float> EmptySuitability;
		const FArcForestSimulator Simulator;
		const FArcForestResult Result =
			Simulator.Simulate(SimParams, *Regen, EmptySuitability, FIntPoint(0, 0));

		int32 NurseSurvivors = 0;
		int32 SlowOakSurvivors = 0;
		for (int32 Index = 0; Index < Result.Num(); ++Index)
		{
			if (Result.Species[Index] == 0) { ++NurseSurvivors; }
			else if (Result.Species[Index] == 1) { ++SlowOakSurvivors; }
		}

		UE_LOGF(LogArcForestTest, Display,"[Regen] survivors=%d nurseSurvivors=%d slowOakSurvivors=%d",
			Result.Num(), NurseSurvivors, SlowOakSurvivors);

		// THRESHOLD 11 — set from a MEASURED discrimination band under Gaussian-around-parents seeding,
		// not inherited. Both sides were run on this exact fixture (seed 31337, SeedingDensity 5.0,
		// Iterations 18, WPatch 5.0, SpacingFraction 0.3, 20x20 m):
		//     correct (height-derived ComputeBands):        Nurse = 13   -> GREEN
		//     reverted (Table[Species[i]].MatureLayer):      Nurse =  9   -> RED
		// The measurement was taken by temporarily reverting ComputeBands in production, running ONLY this
		// test, then restoring and rebuilding green; the revert itself was never committed.
		//
		// WHY THE OLD THRESHOLD WAS BROKEN — it was 8, inherited from the pre-rewrite calibration
		// (reverted 5 << 8 << correct 12). Under the new seeding the reverted count rose to 9, which
		// CLEARS 8: the assertion was GREEN-ON-FAILURE and would have passed on the very regression it
		// names. Re-baselining the correct side alone (12 -> 13) would not have revealed that; only
		// measuring the reverted side did.
		//
		// MARGIN IS THIN — 11 sits 2 above the reverted 9 and 2 below the correct 13. That is a real but
		// narrow band, so this assertion is sensitive to any change that shifts the Nurse count by ~2:
		// a seeding-stream change, a fixture edit, or a mortality/vigour tuning pass. If it moves, RE-MEASURE
		// BOTH SIDES the same way — do NOT re-baseline the correct side alone, which is exactly how the
		// threshold went stale the first time.
		ASSERT_THAT(IsTrue(NurseSurvivors > 11,
			TEXT("Height-derived bands must let the canopy establish (Nurse survivors > 11; measured 13 "
				"correct vs 9 under the species-fixed revert). The revert over-thins the canopy by banding "
				"short juveniles with the CANOPY band — their species MatureLayer — instead of by their "
				"live height, so they compete weaker-of-pair against the mature crowns (H1)")));
	}

	// RESUMABLE EQUIVALENCE (Step 2 — the resumable Seed/StepIterations/Emit API introduced in
	// FArcForestSimulator). Exercises the previously-untested branch: the CHUNKED loop inside
	// StepIterations (ArcForestSimulator.cpp, the resume-cursor path advancing State.Iteration by
	// MaxIters per call and re-declaring only local scratch) must produce a result BIT-IDENTICAL to
	// the all-at-once completion overload, REGARDLESS of the chunk size used to drive it. This is the
	// property Step 3 (resumable element) and Step 5 (worker) both rely on: chunking the same solve
	// differently must never change the outcome. A regression that leaks state between StepIterations
	// calls (e.g. re-seeding scratch that should persist, or an off-by-one on the resume cursor) would
	// flip this red while leaving the completion-only Determinism/SelfThinning cases green.
	//
	// Uses the same forced-competition shape as SelfThinning_CountDropsAndSpreadsOut (one wide-crowned
	// canopy species at high density) so the run actually exercises mortality/compaction, not just
	// seeding.
	TEST_METHOD(ResumableEquivalence_StepIterationsEqualsAllAtOnce)
	{
		using namespace ArcForestSimCoreTestsLocal;

		TArray<FArcForestSpeciesEntry> Table;
		FArcForestSpeciesEntry DenseOak = MakeSpecies(TEXT("DenseOak"), EArcForestLayer::Canopy, 2000.0f, 650.0f, 0.25f,
			2.0f, 400.0f, 0.02f); // TargetDensity 2.0 = far above spacing saturation; mature crowns overlap -> competition thins
		DenseOak.LightNeeded = 0.5f;
		DenseOak.ShadeTolerance = 0.3f;
		Table.Add(MoveTemp(DenseOak));
		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));

		FArcForestSimParams Params;
		Params.Seed = 4242;
		Params.SeedingDensity = 4.0f;
		Params.Iterations = 16;
		Params.SpacingFraction = 0.25f; // tight seed spacing so mature crowns overlap and competition runs (else no mortality to chunk)
		Params.Bounds2D = CentredBounds(1500.0f); // 30 m x 30 m

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		// A: the all-at-once completion overload (thin wrapper over Seed+Step(<=0)+Emit).
		const FArcForestResult AllAtOnce = Simulator.Simulate(Params, *Def, EmptySuitability, Dims);

		// B: hand-driven resumable API, chunked one iteration at a time.
		FArcForestSolveState StateChunk1;
		Simulator.SeedInitial(StateChunk1, Params, *Def, EmptySuitability, Dims);
		bool bDoneChunk1 = false;
		while (!bDoneChunk1)
		{
			bDoneChunk1 = Simulator.StepIterations(StateChunk1, Params, *Def, EmptySuitability, Dims, /*MaxIters=*/1);
		}
		const FArcForestResult ChunkOfOne = Simulator.Emit(StateChunk1, *Def);

		// C: hand-driven resumable API, chunked four iterations at a time (proves chunk-size
		// independence, not just "resumable == completion" for one particular chunking).
		FArcForestSolveState StateChunk4;
		Simulator.SeedInitial(StateChunk4, Params, *Def, EmptySuitability, Dims);
		bool bDoneChunk4 = false;
		while (!bDoneChunk4)
		{
			bDoneChunk4 = Simulator.StepIterations(StateChunk4, Params, *Def, EmptySuitability, Dims, /*MaxIters=*/4);
		}
		const FArcForestResult ChunkOfFour = Simulator.Emit(StateChunk4, *Def);

		UE_LOGF(LogArcForestTest, Display,"[ResumableEquivalence] allAtOnce=%d chunkOf1=%d chunkOf4=%d",
			AllAtOnce.Num(), ChunkOfOne.Num(), ChunkOfFour.Num());

		ASSERT_THAT(IsTrue(AllAtOnce.Num() > 0, TEXT("Reference run must produce at least one survivor")));
		ASSERT_THAT(IsTrue(ResultsIdentical(AllAtOnce, ChunkOfOne),
			TEXT("Seed+StepIterations(MaxIters=1)*+Emit must be element-wise identical to Simulate()")));
		ASSERT_THAT(IsTrue(ResultsIdentical(AllAtOnce, ChunkOfFour),
			TEXT("Seed+StepIterations(MaxIters=4)*+Emit must be element-wise identical to Simulate() "
				"(chunk-size independence: the resume cursor must not change the outcome)")));
	}

	// PARALLEL EQUIVALENCE (Step 7 — commit 7b5e30781c51). The gather-reformulated solver
	// (ResolveCompetition + GrowPlants + UpdateVigor + ComputeBands + mortality-mark, all driven by
	// ParallelForWithTaskContext and toggled by FArcForestSimParams::bParallel) MUST produce a result
	// element-wise IDENTICAL whether it runs serial (bParallel=false) or parallel (bParallel=true) for
	// the same seed/species/grid. This is the exact-equality guard that goes RED if the gather
	// reintroduces a SCATTER-WRITE RACE — parallel threads accumulating into a shared per-neighbour
	// penalty array (torn/lost writes) instead of the per-plant sole-writer design, so the parallel
	// path diverges from serial. That scatter shape was the previously-wrong form before Step 6's
	// per-plant incoming-penalty gather landed.
	//
	// NOTE (verified by hostile-reviewer + proof-auditor of commit 388a89a8b48a): this test does NOT
	// guard the sort-before-sum in ResolveCompetition. Per-plant summation order is already deterministic
	// (FArcForestSpatialHash::QueryRadius scans cells in a fixed integer loop, appending each bucket in
	// insertion order — independent of thread scheduling), and each plant is the sole writer of its own
	// Penalty[]. So serial and parallel sum each plant's terms in the SAME order with or without the sort;
	// removing the sort would NOT make this test (or run-to-run determinism) go RED. The sort is a
	// cross-platform bit-reproducibility measure, not a single-binary-observable invariant.
	//
	// Uses the dense forced-competition fixture from SelfThinning_CountDropsAndSpreadsOut (single
	// wide-crowned canopy oak, high density, enough iterations to thin) so ResolveCompetition's gather
	// does real per-plant work every iteration — the parallel path is genuinely exercised.
	TEST_METHOD(ParallelEquivalence_SameSeedParallelEqualsSerial)
	{
		using namespace ArcForestSimCoreTestsLocal;

		TArray<FArcForestSpeciesEntry> Table;
		FArcForestSpeciesEntry DenseOak = MakeSpecies(TEXT("DenseOak"), EArcForestLayer::Canopy, 2000.0f, 650.0f, 0.25f,
			2.0f, 400.0f, 0.02f); // TargetDensity 2.0 = far above spacing saturation; mature crowns overlap -> competition thins
		DenseOak.LightNeeded = 0.5f;
		DenseOak.ShadeTolerance = 0.3f;
		Table.Add(MoveTemp(DenseOak));
		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));

		FArcForestSimParams Params;
		Params.Seed = 4242;
		Params.SeedingDensity = 4.0f;
		Params.Iterations = 40;
		Params.SpacingFraction = 0.25f; // tight seed spacing so mature crowns overlap and the gather does real per-plant work (else no thinning)
		Params.Bounds2D = CentredBounds(1500.0f); // 30 m x 30 m

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		// Seeded population size (0 iterations = pure seeding) — used to prove the fixture actually thins,
		// so the equality below can't silently pass on a degenerate all-survive/empty population.
		FArcForestSimParams SeedOnly = Params;
		SeedOnly.Iterations = 0;
		SeedOnly.bParallel = false;
		const int32 Seeded = Simulator.Simulate(SeedOnly, *Def, EmptySuitability, Dims).Num();

		// A: serial. B: parallel. Identical seed/species/grid; ONLY bParallel differs.
		FArcForestSimParams SerialParams = Params;
		SerialParams.bParallel = false;
		const FArcForestResult SerialResult = Simulator.Simulate(SerialParams, *Def, EmptySuitability, Dims);

		FArcForestSimParams ParallelParams = Params;
		ParallelParams.bParallel = true;
		const FArcForestResult ParallelResult = Simulator.Simulate(ParallelParams, *Def, EmptySuitability, Dims);

		UE_LOGF(LogArcForestTest, Display,"[ParallelEquivalence] seeded=%d serial=%d parallel=%d",
			Seeded, SerialResult.Num(), ParallelResult.Num());

		// Fixture must genuinely thin (dense competition ran), else the equality is vacuous.
		ASSERT_THAT(IsTrue(SerialResult.Num() > 0, TEXT("Serial run must produce at least one survivor")));
		ASSERT_THAT(IsTrue(SerialResult.Num() < Seeded,
			TEXT("Fixture must thin (survivors < seeded) so the parallel path does real gather work")));
		ASSERT_THAT(IsTrue(ResultsIdentical(SerialResult, ParallelResult),
			TEXT("Parallel gather must equal serial element-wise: RED if a scatter-write race is "
				"reintroduced (shared cross-thread penalty writes -> torn/lost writes -> parallel diverges)")));
	}

	// PARALLEL RUN-TO-RUN DETERMINISM (Step 7 — commit 7b5e30781c51). Repeated PARALLEL runs of the same
	// (Params, Species, Grid, Dims) must be element-wise identical to one another. Complements
	// ParallelEquivalence: that pins parallel==serial for one run; this pins the parallel path against
	// itself across ParallelFor task decomposition / thread scheduling. It goes RED if a scheduling-
	// dependent nondeterminism is introduced into the parallel path — e.g. a scatter-write race that
	// happens to match serial on one run but varies across runs, or any accumulation that becomes order-
	// dependent on thread timing.
	//
	// NOTE (per the 388a89a8b48a review): the current gather is already deterministic run-to-run without
	// the sort (per-plant sole-writer over a fixed QueryRadius neighbour order), so this test does NOT
	// discriminate a sort-before-sum removal — the sort is cross-platform-reproducibility defensive code,
	// not something a single-binary test can falsify. This guards run-to-run stability of the parallel path.
	//
	// Same dense forced-competition fixture; N=5 parallel runs, each compared to the first.
	TEST_METHOD(ParallelDeterminism_RepeatedRunsIdentical)
	{
		using namespace ArcForestSimCoreTestsLocal;

		TArray<FArcForestSpeciesEntry> Table;
		FArcForestSpeciesEntry DenseOak = MakeSpecies(TEXT("DenseOak"), EArcForestLayer::Canopy, 2000.0f, 650.0f, 0.25f,
			2.0f, 400.0f, 0.02f); // TargetDensity 2.0 = far above spacing saturation; mature crowns overlap -> competition thins
		DenseOak.LightNeeded = 0.5f;
		DenseOak.ShadeTolerance = 0.3f;
		Table.Add(MoveTemp(DenseOak));
		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));

		FArcForestSimParams Params;
		Params.Seed = 4242;
		Params.SeedingDensity = 4.0f;
		Params.Iterations = 40;
		Params.bParallel = true;
		Params.SpacingFraction = 0.25f; // tight seed spacing so mature crowns overlap and the parallel gather thins the stand (else no mortality)
		Params.Bounds2D = CentredBounds(1500.0f); // 30 m x 30 m

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		// Seeded population size to prove the run thins (guards against a degenerate all-survive/empty
		// population where run-to-run equality would be trivially true).
		FArcForestSimParams SeedOnly = Params;
		SeedOnly.Iterations = 0;
		const int32 Seeded = Simulator.Simulate(SeedOnly, *Def, EmptySuitability, Dims).Num();

		const FArcForestResult FirstRun = Simulator.Simulate(Params, *Def, EmptySuitability, Dims);

		UE_LOGF(LogArcForestTest, Display,"[SortedSumDeterminism] seeded=%d firstRunSurvivors=%d", Seeded, FirstRun.Num());

		ASSERT_THAT(IsTrue(FirstRun.Num() > 0, TEXT("Parallel run must produce at least one survivor")));
		ASSERT_THAT(IsTrue(FirstRun.Num() < Seeded,
			TEXT("Fixture must thin (survivors < seeded) so the gather sum order actually matters")));

		constexpr int32 RepeatCount = 5;
		for (int32 Run = 1; Run < RepeatCount; ++Run)
		{
			const FArcForestResult NextRun = Simulator.Simulate(Params, *Def, EmptySuitability, Dims);
			UE_LOGF(LogArcForestTest, Display,"[SortedSumDeterminism] run %d survivors=%d", Run, NextRun.Num());
			ASSERT_THAT(IsTrue(ResultsIdentical(FirstRun, NextRun),
				TEXT("Repeated parallel runs must be element-wise identical: RED if scheduling-dependent "
					"nondeterminism (e.g. a scatter-write race varying across runs) enters the parallel path")));
		}
	}
};
