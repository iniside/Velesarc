// Copyright Lukasz Baran. All Rights Reserved.

// ArcForestSeedingContractTests.cpp
// TIER 1 pure-core CQTest coverage for the Gaussian-around-parents SeedPopulation contract
// (Forest/ArcForestSimulator.cpp, SeedPopulation) introduced by the clump-aware seeding rework
// (docs/plans/2026-07-25-0533-arcpcgextension-clump-aware-seeding-plan.md, Step 1). Every case here
// runs at Iterations = 0 to isolate seed-time behaviour from growth/competition/mortality.
//
// The reference numbers pinned below come from an actual run on this branch at the stated
// seed/params (see the per-test "REFERENCE RUN" comment). They are regression anchors.

#include "CQTest.h"

#include "ArcForestTestHelpers.h"

#include "Forest/ArcForestSimTypes.h"
#include "Forest/ArcForestSimulator.h"
#include "Forest/ArcForestSpeciesDefinition.h"
#include "Forest/ArcForestLightModel.h"
#include "Forest/ArcForestPatchField.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcForestSeedingContractTest, Log, All);

namespace ArcForestSeedingContractTestsLocal
{
	using namespace ArcForestTestHelpers;

	// cm thresholds the solver itself uses (ArcForestSimulator.cpp): 1.5 m / 5 m / 15 m in cm.
	constexpr float ShrubMinCm = 150.0f;
	constexpr float UnderstoryMinCm = 500.0f;
	constexpr float CanopyMinCm = 1500.0f;

	/**
	 * SEEDING stratum of a SPECIES — SeedPopulation groups species by their MATURE height
	 * (LayerForHeight(Entry.MaxHeight, …), ArcForestSimulator.cpp), NOT by a plant's current height and
	 * NOT by the authored MatureLayer (which is mesh-only metadata). Min spacing is enforced only WITHIN
	 * such a stratum, so any test reasoning about seed-time spacing or traversal order must use THIS rule.
	 */
	EArcForestLayer StratumOfMatureHeight(float MaxHeightCm)
	{
		return FArcForestLightModel::LayerForHeight(MaxHeightCm, ShrubMinCm, UnderstoryMinCm, CanopyMinCm);
	}

	/** True if Id appears in Ids (TArrayView has no Contains here without pulling in Algo — trivial loop). */
	bool ContainsId(TArrayView<const int32> Ids, int32 Id)
	{
		for (int32 Index = 0; Index < Ids.Num(); ++Index)
		{
			if (Ids[Index] == Id)
			{
				return true;
			}
		}
		return false;
	}

	/** Pull one species' placed darts out of a result, preserving placement order. */
	void ExtractSpeciesSlice(
		const FArcForestResult& Result,
		int32 SpeciesId,
		TArray<FVector2f>& OutPos,
		TArray<int32>& OutPatchId)
	{
		OutPos.Reset();
		OutPatchId.Reset();
		for (int32 Index = 0; Index < Result.Num(); ++Index)
		{
			if (Result.Species[Index] == SpeciesId)
			{
				OutPos.Add(Result.PosXY[Index]);
				OutPatchId.Add(Result.PatchId[Index]);
			}
		}
	}

	/** Element-wise equality of two extracted species slices (position AND inherited patch id). */
	bool SlicesIdentical(
		TArrayView<const FVector2f> PosA,
		TArrayView<const int32> PatchIdA,
		TArrayView<const FVector2f> PosB,
		TArrayView<const int32> PatchIdB)
	{
		if (PosA.Num() != PosB.Num() || PatchIdA.Num() != PatchIdB.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < PosA.Num(); ++Index)
		{
			if (PosA[Index] != PosB[Index]) { return false; }
			if (PatchIdA[Index] != PatchIdB[Index]) { return false; }
		}
		return true;
	}

	/** Smallest id in a non-empty view (caller asserts non-emptiness first). */
	int32 MinId(TArrayView<const int32> Ids)
	{
		int32 Result = TNumericLimits<int32>::Max();
		for (int32 Index = 0; Index < Ids.Num(); ++Index)
		{
			Result = FMath::Min(Result, Ids[Index]);
		}
		return Result;
	}

	/** Largest id in a non-empty view (caller asserts non-emptiness first). */
	int32 MaxId(TArrayView<const int32> Ids)
	{
		int32 Result = TNumericLimits<int32>::Lowest();
		for (int32 Index = 0; Index < Ids.Num(); ++Index)
		{
			Result = FMath::Max(Result, Ids[Index]);
		}
		return Result;
	}
}

TEST_CLASS(ArcForestSeedingContract, "ArcPCGExtension.ForestSim.SeedingContract")
{
	// STRATIFICATION — a canopy species and a field species must be able to co-occupy overlapping XY.
	// RED under the old global-argmax species pick (one species winning everywhere at any given XY,
	// which starves the loser species down to near-zero or disjoint footprints): this asserts BOTH
	// species actually seed AND that some canopy plant's mature crown footprint genuinely overlaps a
	// field plant's — real spatial co-occupancy, not just "both exist somewhere in the same bounds".
	// Guards the per-species independent stratum scatter in SeedPopulation (ArcForestSimulator.cpp,
	// the per-stratum `for (MemberIndex...)` loop that gives each species its OWN CandidateCount and
	// its own placed-set, rather than one shared winner-take-all pick per dart).
	TEST_METHOD(Stratification_CanopyAndFieldStackAtOverlappingXY)
	{
		using namespace ArcForestSeedingContractTestsLocal;

		constexpr float CanopyCrownRadius = 200.0f;
		constexpr float FieldCrownRadius = 50.0f;
		constexpr float SpacingFraction = 0.8f;

		TArray<FArcForestSpeciesEntry> Table;
		// Canopy: sparse (5 candidates), wide mature crown (200 cm) — no patches, always uniform.
		FArcForestSpeciesEntry CanopyOak = MakeSpecies(TEXT("CanopyOak"), EArcForestLayer::Canopy, 2000.0f, CanopyCrownRadius, 0.1f,
			0.05f, 0.0f, 0.0f);
		CanopyOak.LightNeeded = 0.5f;
		CanopyOak.ShadeTolerance = 0.3f;
		Table.Add(MoveTemp(CanopyOak));
		// Field: dense (100 candidates), small mature crown (50 cm) — no patches, always uniform.
		FArcForestSpeciesEntry FieldFern = MakeSpecies(TEXT("FieldFern"), EArcForestLayer::Field, 100.0f, FieldCrownRadius, 0.2f,
			1.0f, 0.0f, 0.0f);
		FieldFern.LightNeeded = 0.3f;
		FieldFern.ShadeTolerance = 0.9f;
		Table.Add(MoveTemp(FieldFern));
		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));

		FArcForestSimParams Params;
		Params.Seed = 555;
		Params.SeedingDensity = 1.0f;
		Params.SpacingFraction = SpacingFraction;
		Params.Iterations = 0;
		Params.WPatch = 0.0f; // irrelevant — both species have zero patch parents, always uniform
		Params.Bounds2D = CentredBounds(500.0f); // 10 m x 10 m (100 m^2)

		const TArray<float> EmptySuitability;
		const FArcForestSimulator Simulator;
		const FArcForestResult Result = Simulator.Simulate(Params, *Def, EmptySuitability, FIntPoint(0, 0));

		TArray<FVector2f> CanopyPos;
		TArray<FVector2f> FieldPos;
		for (int32 Index = 0; Index < Result.Num(); ++Index)
		{
			if (Result.Species[Index] == 0) { CanopyPos.Add(Result.PosXY[Index]); }
			else if (Result.Species[Index] == 1) { FieldPos.Add(Result.PosXY[Index]); }
		}

		float MinCrossDist = TNumericLimits<float>::Max();
		for (int32 I = 0; I < CanopyPos.Num(); ++I)
		{
			for (int32 J = 0; J < FieldPos.Num(); ++J)
			{
				MinCrossDist = FMath::Min(MinCrossDist, (CanopyPos[I] - FieldPos[J]).Size());
			}
		}

		// The floor a STRATUM-MERGE regression would have been forced to apply to this very pair: if the
		// canopy and field species shared one stratum, the Poisson-disk loop would reject any cross pair
		// closer than SpacingFraction*(r_canopy + r_field). Derived from the fixture, not hardcoded.
		const float SharedStratumFloor = SpacingFraction * (CanopyCrownRadius + FieldCrownRadius);

		UE_LOGF(LogArcForestSeedingContractTest, Display,"[Stratification] canopy=%d field=%d minCrossDist=%.2f sharedStratumFloor=%.2f",
			CanopyPos.Num(), FieldPos.Num(), MinCrossDist, SharedStratumFloor);

		// REFERENCE RUN (seed 555, 10x10 m, density 1.0), measured on this branch:
		//   CANDIDATE counts (round(TargetDensity*SeedingDensity*Area)): canopy=5, field=100.
		//   REALIZED counts after min-spacing rejection:                 canopy=3, field=53.
		//   minCrossDist=18.92 cm against a sharedStratumFloor of 200.00 cm.
		ASSERT_THAT(IsTrue(CanopyPos.Num() > 0,
			TEXT("Canopy species must seed at all -> RED if a global argmax starves the loser species")));
		ASSERT_THAT(IsTrue(FieldPos.Num() > 0,
			TEXT("Field species must seed at all -> RED if a global argmax starves the loser species")));
		ASSERT_THAT(IsTrue(MinCrossDist < SharedStratumFloor,
			TEXT("Cross-stratum non-interaction: at least one canopy/field pair must sit CLOSER than "
				"SpacingFraction*(r_canopy + r_field) — a distance a shared-stratum implementation could "
				"not have produced, because its Poisson-disk floor would have rejected that dart. Exhibits "
				"a pair that is impossible under a stratum-merge regression, not merely 'both species exist'")));
	}

	// PER-SPECIES DENSITY — with SpacingFraction=0 (the Poisson-disk floor never rejects: MinDist=0 so
	// `dist^2 < 0` is never true), WPatch=0 (every dart uniform, ClumpedProbability=0 so DrawBranch<0 is
	// never true) and uniform suitability (empty grid -> AcceptProb=1.0, and DrawAccept is drawn from
	// FRand() in [0,1) so DrawAccept >= 1.0 is never true), EVERY candidate dart is accepted. This is the
	// one DERIVABLE anchor in the module: realized count must equal the candidate count exactly, computed
	// here from the same formula SeedPopulation uses (CandidateCount = round(TargetDensity *
	// SeedingDensity * AreaMeters2)) applied to the fixture's own values — not a hardcoded magic number.
	// RED if acceptance/spacing ever silently rejects a dart it shouldn't under these degenerate settings.
	TEST_METHOD(PerSpeciesDensity_CountTracksTargetDensity)
	{
		using namespace ArcForestSeedingContractTestsLocal;

		constexpr float TargetDensity = 0.3f;
		constexpr float SeedingDensity = 2.0f;
		constexpr float HalfSizeCm = 700.0f; // 14 m x 14 m (196 m^2)

		TArray<FArcForestSpeciesEntry> Table;
		FArcForestSpeciesEntry Understory = MakeSpecies(TEXT("Understory"), EArcForestLayer::Understory, 800.0f, 150.0f, 0.1f,
			TargetDensity, 0.0f, 0.0f);
		Understory.LightNeeded = 0.4f;
		Understory.ShadeTolerance = 0.6f;
		Table.Add(MoveTemp(Understory));
		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));

		FArcForestSimParams Params;
		Params.Seed = 777;
		Params.SeedingDensity = SeedingDensity;
		Params.SpacingFraction = 0.0f;
		Params.WPatch = 0.0f;
		Params.Iterations = 0;
		Params.Bounds2D = CentredBounds(HalfSizeCm);

		const TArray<float> EmptySuitability;
		const FArcForestSimulator Simulator;
		const FArcForestResult Result = Simulator.Simulate(Params, *Def, EmptySuitability, FIntPoint(0, 0));

		const float SideMeters = (2.0f * HalfSizeCm) / 100.0f;
		const float AreaMeters2 = SideMeters * SideMeters;
		const int32 ExpectedCount = FMath::Max(0, FMath::RoundToInt(TargetDensity * SeedingDensity * AreaMeters2));

		UE_LOGF(LogArcForestSeedingContractTest, Display,"[PerSpeciesDensity] expected=%d realized=%d areaM2=%.2f",
			ExpectedCount, Result.Num(), AreaMeters2);

		ASSERT_THAT(IsTrue(ExpectedCount > 0, TEXT("Fixture must produce a positive candidate count")));
		ASSERT_THAT(IsTrue(Result.Num() == ExpectedCount,
			TEXT("With SpacingFraction=0, WPatch=0 and uniform suitability every candidate dart is accepted: "
				"realized count must equal round(TargetDensity*SeedingDensity*AreaMeters2) exactly")));
	}

	// SEED-TIME MIN SPACING — the Poisson-disk floor must still hold INSIDE a Gaussian clump: no
	// same-stratum pair closer than SpacingFraction*(r_i+r_j), even when offspring are densely packed
	// around a shared parent. Uses a HIGH WPatch (clumped) fixture so the assertion actually exercises
	// within-clump packing (a low-WPatch fixture would mostly test the uniform branch, which
	// SeedTimeMinSpacing already covers implicitly via the SelfThinning fixture in the core tests).
	// RED if the Poisson-disk rejection loop (ArcForestSimulator.cpp, the CellRange scan against
	// PlacedGrid) is ever skipped or mis-scoped for clumped darts specifically.
	//
	// RADIUS SOURCE (load-bearing): the floor is computed from the SPECIES TABLE's MaxCrownRadius, NOT
	// from Result.CrownRadius. At Iterations=0 every plant carries the 5 cm SeedlingCrownRadiusCm, so a
	// result-derived floor would assert only 0.3*(5+5)=3 cm — 16x weaker than the 0.3*(80+80)=48 cm the
	// production loop enforces, and it would stay GREEN under exactly the mutant SeedPopulation's own doc
	// block warns about (swapping Entry.MaxCrownRadius for SeedlingCrownRadiusCm in the MinDist term).
	// Likewise the stratum grouping below uses the species' MATURE height, matching SeedPopulation.
	// MEASURED on this branch: 313 plants, floor=48.00 cm, closest surviving pair=48.04 cm — the bound is
	// tight against real data, so a floor that dropped to the 3 cm seedling value would show a ~45 cm
	// violation. The old result-derived form asserted only 3 cm and could never see it.
	TEST_METHOD(SeedTimeMinSpacing_NoSameStratumOverlap)
	{
		using namespace ArcForestSeedingContractTestsLocal;

		constexpr float MaxCrownRadius = 80.0f;
		constexpr float SpacingFraction = 0.3f;

		TArray<FArcForestSpeciesEntry> Table;
		FArcForestSpeciesEntry ClumpedFern = MakeSpecies(TEXT("ClumpedFern"), EArcForestLayer::Field, 100.0f, MaxCrownRadius, 0.2f,
			2.0f, 150.0f, 0.02f); // dense TargetDensity + real patch parents
		ClumpedFern.LightNeeded = 0.3f;
		ClumpedFern.ShadeTolerance = 0.9f;
		Table.Add(MoveTemp(ClumpedFern));
		// Keep a copy: the definition takes ownership of Table, but the assertion below must read the
		// MATURE radius/height straight out of the species rows the solver itself consumed.
		const TArray<FArcForestSpeciesEntry> SpeciesRows = Table;
		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));

		FArcForestSimParams Params;
		Params.Seed = 909;
		Params.SeedingDensity = 1.0f;
		Params.SpacingFraction = SpacingFraction;
		Params.WPatch = 50.0f; // p = 50/51 ~= 0.98 — almost every dart is a Gaussian offspring
		Params.Iterations = 0;
		Params.Bounds2D = CentredBounds(1000.0f); // 20 m x 20 m (400 m^2)

		const TArray<float> EmptySuitability;
		const FArcForestSimulator Simulator;
		const FArcForestResult Result = Simulator.Simulate(Params, *Def, EmptySuitability, FIntPoint(0, 0));

		UE_LOGF(LogArcForestSeedingContractTest, Display,"[SeedTimeMinSpacing] count=%d", Result.Num());
		ASSERT_THAT(IsTrue(Result.Num() > 10,
			TEXT("Fixture must seed enough plants to genuinely stress within-clump packing")));

		// Accumulated via Max(WorstViolation, MinDist - Dist), so it is never negative: 0 = no violation,
		// positive = the breach magnitude. Real slack is reported separately as SmallestFloor/ClosestPair.
		float WorstViolation = 0.0f;  // negative-or-zero = no violation; positive = a real breach magnitude
		float SmallestFloor = TNumericLimits<float>::Max(); // the weakest floor any pair was held to
		float ClosestPair = TNumericLimits<float>::Max();
		for (int32 I = 0; I < Result.Num(); ++I)
		{
			const FArcForestSpeciesEntry& EntryI = SpeciesRows[Result.Species[I]];
			const EArcForestLayer StratumI = StratumOfMatureHeight(EntryI.MaxHeight);
			for (int32 J = I + 1; J < Result.Num(); ++J)
			{
				const FArcForestSpeciesEntry& EntryJ = SpeciesRows[Result.Species[J]];
				if (StratumOfMatureHeight(EntryJ.MaxHeight) != StratumI)
				{
					continue;
				}
				// MATURE radii, exactly as the production MinDist term uses them.
				const float MinDist = SpacingFraction * (EntryI.MaxCrownRadius + EntryJ.MaxCrownRadius);
				const float Dist = (Result.PosXY[I] - Result.PosXY[J]).Size();
				WorstViolation = FMath::Max(WorstViolation, MinDist - Dist);
				SmallestFloor = FMath::Min(SmallestFloor, MinDist);
				ClosestPair = FMath::Min(ClosestPair, Dist);
			}
		}

		UE_LOGF(LogArcForestSeedingContractTest, Display,"[SeedTimeMinSpacing] worstViolation=%.4f (0 means no breach) floor=%.2f closestPair=%.2f",
			WorstViolation, SmallestFloor, ClosestPair);

		// The floor must be the MATURE one (0.3*(80+80)=48), never the seedling one (0.3*(5+5)=3): this
		// guard fails loudly if the fixture ever drifts into asserting the weaker bound.
		ASSERT_THAT(IsTrue(SmallestFloor >= (SpacingFraction * 2.0f * MaxCrownRadius) - KINDA_SMALL_NUMBER,
			TEXT("The asserted spacing floor must come from MATURE crown radii, not the 5 cm seedling "
				"radius — a seedling-derived floor is 16x weaker and waves the MinDist mutant through")));
		ASSERT_THAT(IsTrue(WorstViolation <= KINDA_SMALL_NUMBER,
			TEXT("No same-stratum pair may be closer than SpacingFraction*(MaxCrownRadius_i + "
				"MaxCrownRadius_j), even for Gaussian offspring packed around a shared clump parent. "
				"RED if the MinDist term ever reads a seedling/current radius instead of the mature one")));
	}

	// FIXED-7-DRAW CONTRACT — THE HIGHEST-VALUE TEST HERE, via a DOWNSTREAM-STRATUM stream offset.
	//
	// SeedPopulation drives every dart of every species off ONE serial FRandomStream, traversing strata
	// Field -> Shrub -> Understory -> Canopy and, within a stratum, species in ascending table index.
	// A species therefore starts reading the stream at exactly `7 * sum(CandidateCount)` of everything
	// traversed before it — IF and ONLY IF the seven per-dart draws are unconditional. Any conditional
	// draw makes an UPSTREAM species' consumption depend on which branch its darts took, which SHIFTS
	// the whole downstream slice of the stream.
	//
	// FIXTURE: species A ("PatchyMoss") is patchy (PatchDensity>0, sigma>0) and mature-short so it lands
	// in the FIELD stratum (traversed first). Species B ("LonelyHazel") is parentless (PatchDensity=0)
	// and mature-tall enough for the SHRUB stratum (traversed second). Different strata matter: min
	// spacing is enforced only WITHIN a stratum, so B's placements are not coupled to A's POSITIONS at
	// all — only to A's RNG CONSUMPTION. The same seed is run at WPatch=0 and WPatch=8, and we assert:
	//   (1) CONTROL — species A's plants DIFFER across the two arms. This proves the high arm genuinely
	//       took the clumped branch. Without it the test could pass vacuously (as the previous
	//       ZeroParentFallback version did: with a single parentless species, WPatch was a provably DEAD
	//       input, so "identical output across two WPatch values" was a tautology, not evidence).
	//   (2) THE CONTRACT — species B's plants are ELEMENT-WISE IDENTICAL across the two arms. Since A
	//       demonstrably took different branches, (2) can hold only if A consumed exactly
	//       7*CandidateCount(A) draws in BOTH arms.
	//
	// MUTANT FAMILY KILLED — scoped to what this fixture actually varies, which is `bClumped`: gating any
	// of the seven draws on `bClumped`; gating DrawParent or U1/U2 on `bClumped`; gating DrawUniformX/Y on
	// the uniform branch; redrawing an out-of-bounds Gaussian offspring instead of clamping it; any extra
	// draw inside the clumped block. Each makes A's draw count differ BETWEEN THE ARMS, which shifts B's
	// slice of the stream and turns (2) RED. Every existing determinism test
	// (Determinism_SameInputsIdenticalSurvivors, Resumable*, Parallel*) runs the SAME params twice and is
	// satisfied by ANY reproducible draw count, variable or not — so those stay GREEN under all of the
	// above. This test is the only one that varies the branch taken while holding the seed fixed.
	//
	// NOT COVERED — draws gated on PARENT AVAILABILITY, e.g. `if (Parents.Num() > 0) { U1 = ...; }`.
	// `FArcForestPatchField::Build` does not read WPatch, so species A has the SAME non-empty parent set in
	// both arms; such a mutant consumes the same (possibly reduced) draw count in both, B's offset is
	// unshifted, and (2) stays GREEN. Do not read this test as covering that family. Closing it needs a
	// THIRD arm that varies parent AVAILABILITY of the upstream species — rerun with species A's
	// PatchDensity = 0 and assert B is still identical. No fixture in this module does that today.
	//
	// It also still exercises the `Parents.Num() == 0` uniform-fallback branch (species B), which no
	// other fixture in this module reaches — but note that reachability alone is NOT what this test
	// proves; the proof is the stream alignment of the DOWNSTREAM stratum.
	//
	// MEASURED on this branch: species A 161 plants at WPatch=0 vs 130 at WPatch=8 (the branch really
	// flipped); species B 94 plants in BOTH arms, element-wise identical.
	TEST_METHOD(DrawCountInvariance_DownstreamStratumStreamAligned)
	{
		using namespace ArcForestSeedingContractTestsLocal;

		constexpr int32 SpeciesA = 0; // patchy, Field stratum, traversed FIRST (the stream producer)
		constexpr int32 SpeciesB = 1; // parentless, Shrub stratum, traversed SECOND (the stream probe)
		constexpr float MatureHeightA = 100.0f;  // < ShrubMinCm (150) -> Field
		constexpr float MatureHeightB = 300.0f;  // [150, 500) -> Shrub

		TArray<FArcForestSpeciesEntry> Table;
		FArcForestSpeciesEntry PatchyMoss = MakeSpecies(TEXT("PatchyMoss"), EArcForestLayer::Field, MatureHeightA, 60.0f, 0.2f,
			0.5f, 200.0f, 0.03f); // real parents -> the clumped branch is live
		PatchyMoss.LightNeeded = 0.3f;
		PatchyMoss.ShadeTolerance = 0.9f;
		Table.Add(MoveTemp(PatchyMoss));
		FArcForestSpeciesEntry LonelyHazel = MakeSpecies(TEXT("LonelyHazel"), EArcForestLayer::Shrub, MatureHeightB, 90.0f, 0.15f,
			0.3f, 200.0f, 0.0f);  // PatchDensity=0 -> Build emits ZERO parents
		LonelyHazel.LightNeeded = 0.4f;
		LonelyHazel.ShadeTolerance = 0.6f;
		Table.Add(MoveTemp(LonelyHazel));
		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));

		// FIXTURE PRECONDITION — the two species MUST land in different strata under SeedPopulation's own
		// grouping rule (LayerForHeight(MaxHeight, ...)). If they shared a stratum, B's positions would be
		// coupled to A's placements through the Poisson-disk floor and the test would be invalid.
		ASSERT_THAT(IsTrue(StratumOfMatureHeight(MatureHeightA) == EArcForestLayer::Field,
			TEXT("Species A must group into the Field stratum (traversed first)")));
		ASSERT_THAT(IsTrue(StratumOfMatureHeight(MatureHeightB) == EArcForestLayer::Shrub,
			TEXT("Species B must group into the Shrub stratum (traversed second) — different stratum from "
				"A, so B shares no Poisson-disk placed-set with A and is coupled to A ONLY via the RNG")));

		FArcForestSimParams Params;
		Params.Seed = 9001;
		Params.SeedingDensity = 1.0f;
		Params.SpacingFraction = 0.5f;
		Params.Iterations = 0;
		Params.Bounds2D = CentredBounds(1000.0f); // 20 m x 20 m (400 m^2)

		const TArray<float> EmptySuitability;

		// FIXTURE PRECONDITION — A really has clump parents and B really has none (the branch asymmetry
		// the whole test rests on), verified against the same patch field SeedInitial builds internally.
		FArcForestPatchField PatchField;
		PatchField.Build(*Def, Params, EmptySuitability, FIntPoint(0, 0));
		ASSERT_THAT(IsTrue(PatchField.GetParentPositions(SpeciesA).Num() > 0,
			TEXT("Species A must have clump parents, else its clumped branch is dead and WPatch is a dead "
				"input — exactly the vacuity this test replaces")));
		ASSERT_THAT(IsTrue(PatchField.GetParentPositions(SpeciesB).Num() == 0,
			TEXT("Species B must be parentless (PatchDensity=0), so it stays on the uniform branch in both "
				"arms and its output can only move if the RNG stream shifted underneath it")));

		const FArcForestSimulator Simulator;

		Params.WPatch = 0.0f;
		const FArcForestResult RunLowWPatch = Simulator.Simulate(Params, *Def, EmptySuitability, FIntPoint(0, 0));

		Params.WPatch = 8.0f;
		const FArcForestResult RunHighWPatch = Simulator.Simulate(Params, *Def, EmptySuitability, FIntPoint(0, 0));

		TArray<FVector2f> LowPosA;
		TArray<int32> LowPatchIdA;
		TArray<FVector2f> HighPosA;
		TArray<int32> HighPatchIdA;
		ExtractSpeciesSlice(RunLowWPatch, SpeciesA, LowPosA, LowPatchIdA);
		ExtractSpeciesSlice(RunHighWPatch, SpeciesA, HighPosA, HighPatchIdA);

		TArray<FVector2f> LowPosB;
		TArray<int32> LowPatchIdB;
		TArray<FVector2f> HighPosB;
		TArray<int32> HighPatchIdB;
		ExtractSpeciesSlice(RunLowWPatch, SpeciesB, LowPosB, LowPatchIdB);
		ExtractSpeciesSlice(RunHighWPatch, SpeciesB, HighPosB, HighPatchIdB);

		UE_LOGF(LogArcForestSeedingContractTest, Display,"[DrawCountInvariance] A: low=%d high=%d | B: low=%d high=%d",
			LowPosA.Num(), HighPosA.Num(), LowPosB.Num(), HighPosB.Num());

		ASSERT_THAT(IsTrue(LowPosA.Num() > 0 && HighPosA.Num() > 0,
			TEXT("Upstream species A must seed in both arms")));
		ASSERT_THAT(IsTrue(LowPosB.Num() > 0 && HighPosB.Num() > 0,
			TEXT("Downstream species B must seed in both arms, else the contract assertion is vacuous")));

		// (1) CONTROL — the high arm must actually have taken the clumped branch.
		ASSERT_THAT(IsFalse(SlicesIdentical(LowPosA, LowPatchIdA, HighPosA, HighPatchIdA),
			TEXT("Species A must DIFFER between WPatch=0 and WPatch=8: this is what proves the clumped "
				"branch was genuinely taken in the high arm. Without it, the identity assertion below "
				"would be a tautology over a dead parameter")));

		// (2) THE CONTRACT — the downstream stratum must be untouched by A's branch choice.
		ASSERT_THAT(IsTrue(SlicesIdentical(LowPosB, LowPatchIdB, HighPosB, HighPatchIdB),
			TEXT("Species B (a DOWNSTREAM stratum) must be ELEMENT-WISE IDENTICAL across WPatch=0 and "
				"WPatch=8. Species A demonstrably took different branches (assertion above), so this can "
				"only hold if A consumed exactly 7*CandidateCount(A) draws in BOTH arms. RED if any of the "
				"seven per-dart draws becomes conditional on bClumped / Parents.Num()>0, or if an "
				"out-of-bounds Gaussian offspring is redrawn instead of clamped")));
	}

	// PATCH ID INHERITANCE — asserts the PatchId VALUE, which ResultsIdentical alone never does (it only
	// compares run-to-run, so a wrong-but-STABLE PatchId value stays green forever). Two arms:
	//   (A) WPatch=0: every plant's PatchId must be INDEX_NONE (no clumping happened).
	//   (B) high WPatch: every plant with PatchId != INDEX_NONE must carry an id that is a genuine MEMBER
	//       of that species' real parent-id set (PatchField.GetParentIds(species)), built directly via
	//       FArcForestPatchField with the SAME seed/params SeedInitial uses internally.
	//
	// THE CONCRETE BUG THIS CATCHES: PatchId = ParentIndex (the per-species LOCAL 0..N-1 index into
	// Parents/ParentIds) instead of PatchId = ParentIds[ParentIndex] (the GLOBALLY unique id, allocated
	// by a single NextPatchId counter across ALL species in Build order). Species 0 takes the low ids
	// (0..N0-1) and species 1's ids start AFTER species 0's — so for species 1 specifically, a wrong
	// local-index value would NOT be a member of species 1's real (globally-offset) id set, unless N0=0.
	// This is why the fixture below uses TWO patchy species and checks membership per-species: checking
	// only species 0 would not catch the bug (species 0's local indices happen to coincide with its own
	// global ids, since it is allocated first). Downstream PCG groups/instances meshes by PatchId, so the
	// shipped symptom of this bug is two different species' patches visually merging into one group.
	TEST_METHOD(PatchIdInheritance_ClumpedDartsCarryParentIds)
	{
		using namespace ArcForestSeedingContractTestsLocal;

		TArray<FArcForestSpeciesEntry> Table;
		FArcForestSpeciesEntry Moss = MakeSpecies(TEXT("Moss"), EArcForestLayer::Field, 100.0f, 80.0f, 0.2f,
			0.1f, 300.0f, 0.03f);
		Moss.LightNeeded = 0.3f;
		Moss.ShadeTolerance = 0.9f;
		Table.Add(MoveTemp(Moss));
		FArcForestSpeciesEntry Bilberry = MakeSpecies(TEXT("Bilberry"), EArcForestLayer::Field, 110.0f, 90.0f, 0.2f,
			0.1f, 300.0f, 0.03f); // species index 1 — catches the local-index bug
		Bilberry.LightNeeded = 0.3f;
		Bilberry.ShadeTolerance = 0.9f;
		Table.Add(MoveTemp(Bilberry));
		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));

		FArcForestSimParams Params;
		Params.Seed = 4242;
		Params.SeedingDensity = 1.0f;
		Params.Iterations = 0;
		Params.Bounds2D = CentredBounds(2000.0f); // 40 m x 40 m

		const TArray<float> EmptySuitability;
		const FArcForestSimulator Simulator;

		// Arm A — WPatch=0: no clumping, every PatchId must be INDEX_NONE.
		Params.WPatch = 0.0f;
		const FArcForestResult UniformResult =
			Simulator.Simulate(Params, *Def, EmptySuitability, FIntPoint(0, 0));

		ASSERT_THAT(IsTrue(UniformResult.Num() > 0, TEXT("Uniform arm must seed at least one plant")));
		bool bAllUniformIndexNone = true;
		for (int32 Index = 0; Index < UniformResult.Num(); ++Index)
		{
			bAllUniformIndexNone &= (UniformResult.PatchId[Index] == INDEX_NONE);
		}
		ASSERT_THAT(IsTrue(bAllUniformIndexNone,
			TEXT("With WPatch=0 every dart is uniform: every plant's PatchId must be INDEX_NONE")));

		// Arm B — high WPatch: build the real per-species parent-id sets the same way SeedInitial does
		// internally, then verify every clumped plant's PatchId is a genuine member of ITS species' set.
		Params.WPatch = 20.0f; // p = 20/21 ~= 0.95 — almost every dart is a Gaussian offspring
		const FArcForestResult ClumpedResult =
			Simulator.Simulate(Params, *Def, EmptySuitability, FIntPoint(0, 0));

		FArcForestPatchField PatchField;
		PatchField.Build(*Def, Params, EmptySuitability, FIntPoint(0, 0));
		ASSERT_THAT(IsTrue(PatchField.GetNumSpecies() == 2, TEXT("Patch field must build one set per species")));

		// FIXTURE PRECONDITIONS the mutant-kill rests on — asserted, not assumed. The local-index-vs-global-id
		// bug is only DISTINGUISHABLE for species 1 if (a) both species actually got parents and (b) species
		// 1's id range is DISJOINT from and above species 0's, so a local index 0..N1-1 cannot accidentally
		// be a member of species 1's real id set.
		const TArrayView<const int32> ParentIds0 = PatchField.GetParentIds(0);
		const TArrayView<const int32> ParentIds1 = PatchField.GetParentIds(1);
		ASSERT_THAT(IsTrue(ParentIds0.Num() > 0,
			TEXT("Species 0 must have parents, else species 1's ids start at 0 and the local-index bug hides")));
		ASSERT_THAT(IsTrue(ParentIds1.Num() > 0,
			TEXT("Species 1 must have parents, else no clumped species-1 dart can exist to discriminate")));
		ASSERT_THAT(IsTrue(MinId(ParentIds1) > MaxId(ParentIds0),
			TEXT("Species 1's globally-allocated parent ids must sit strictly ABOVE species 0's: this "
				"disjointness is what makes a LOCAL ParentIndex value fail the membership check below")));

		int32 ClumpedCountPerSpecies[2] = { 0, 0 };
		bool bAllMembersValid = true;
		for (int32 Index = 0; Index < ClumpedResult.Num(); ++Index)
		{
			const int32 PatchId = ClumpedResult.PatchId[Index];
			if (PatchId == INDEX_NONE)
			{
				continue;
			}
			const int32 SpeciesId = ClumpedResult.Species[Index];
			if (SpeciesId == 0 || SpeciesId == 1)
			{
				++ClumpedCountPerSpecies[SpeciesId];
			}
			const TArrayView<const int32> ParentIds = PatchField.GetParentIds(SpeciesId);
			if (!ContainsId(ParentIds, PatchId))
			{
				bAllMembersValid = false;
			}
		}

		UE_LOGF(LogArcForestSeedingContractTest, Display,"[PatchIdInheritance] uniformCount=%d clumpedCount=%d clumpedSpecies0=%d clumpedSpecies1=%d "
				"parentIds0=[%d..%d] parentIds1=[%d..%d]",
			UniformResult.Num(), ClumpedResult.Num(),
			ClumpedCountPerSpecies[0], ClumpedCountPerSpecies[1],
			MinId(ParentIds0), MaxId(ParentIds0), MinId(ParentIds1), MaxId(ParentIds1));

		// PER-SPECIES, not aggregated: only species 1 discriminates the bug (species 0's local indices
		// coincide with its global ids). An aggregate "> 0" would stay green if fixture drift emptied it.
		ASSERT_THAT(IsTrue(ClumpedCountPerSpecies[1] > 0,
			TEXT("Species 1 specifically must produce at least one clumped (non-INDEX_NONE) plant — it is "
				"the ONLY species whose global ids differ from its local parent indices, so an aggregate "
				"clumped count could stay positive while the discriminating species contributed nothing")));
		ASSERT_THAT(IsTrue(ClumpedCountPerSpecies[0] > 0,
			TEXT("Species 0 must also clump, confirming the high-WPatch arm is broadly on the clumped branch")));
		ASSERT_THAT(IsTrue(bAllMembersValid,
			TEXT("Every clumped plant's PatchId must be a member of ITS species' real parent-id set "
				"(PatchField.GetParentIds(Species[i])): RED if PatchId is the LOCAL ParentIndex instead of "
				"ParentIds[ParentIndex], since species 1's globally-offset ids would then not match")));
	}
};
