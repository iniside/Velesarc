// Copyright Lukasz Baran. All Rights Reserved.

// ArcForestBandModelTests.cpp
// MECHANISM-TIED coverage of the MC1 LIVE-BAND rule: a plant's competition/light band comes from its
// CURRENT HEIGHT (ArcForestSimulator.cpp, ComputeBands -> FArcForestLightModel::LayerForHeight), NEVER
// from its species' MatureLayer. The historical bug (H1) was species-fixed banding.
//
// WHY THIS FILE EXISTS — the only guard on that rule today is
// ArcForestSimCoreTests.cpp::RegenerationKeystone_SeedlingBandFromHeightNotSpecies part (3), which
// asserts a SURVIVOR-COUNT INEQUALITY (`Nurse > 11`, measured 13 correct vs 9 reverted) over a FULLY
// SEEDED, RNG-driven stand. That artifact is unsound as a proof: the correct side has already drifted
// 12 -> 11 -> 13 across seeding changes, so a future seeding change of the same class can land on 11
// (`11 > 11` is false) and turn the test RED with no bug present — the +/-2 discrimination band is
// NARROWER than the demonstrated drift of both endpoints.
//
// EVERY TEST BELOW CONTAINS NO RNG. FArcForestSolveState is a plain public SoA and
// SeedInitial/StepIterations/Emit are all ARCPCGEXTENSION_API, so these cases HAND-BUILD the stand with
// State.Add(...) + State.bSeeded = true and SKIP SeedInitial entirely. No FRandomStream is touched, no
// patch field is built, no dart is thrown: the assertions cannot drift with the seeding stream, the
// dart-draw count, WPatch, or SpacingFraction. They still drive the PRODUCTION solver
// (StepIterations -> ComputeBands / ComputeIncidentLight / GrowPlants / UpdateVigor /
// ResolveCompetition / ApplyMortalityAndCompact), never a proxy.

#include "CQTest.h"

#include "ArcForestTestHelpers.h"

#include "Forest/ArcForestLightModel.h"
#include "Forest/ArcForestSimTypes.h"
#include "Forest/ArcForestSimulator.h"
#include "Forest/ArcForestSpeciesDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcForestBandTest, Log, All);

namespace ArcForestBandModelTestsLocal
{
	using namespace ArcForestTestHelpers;

	// The cm-scaled thresholds the SOLVER passes to LayerForHeight (ArcForestSimulator.cpp:
	// ShrubMinHeightCm / UnderstoryMinHeightCm / CanopyMinHeightCm = 1.5 m / 5 m / 15 m in cm).
	constexpr float ShrubMinCm = 150.0f;
	constexpr float UnderstoryMinCm = 500.0f;
	constexpr float CanopyMinCm = 1500.0f;

	/** One row of the boundary table: a height and the band the IMPLEMENTATION must return for it. */
	struct FBoundaryCase
	{
		float HeightCm = 0.0f;
		EArcForestLayer Expected = EArcForestLayer::Field;
		const TCHAR* Label = nullptr;
	};

	/** Live band of a height under the solver's own cm thresholds (the rule ComputeBands applies). */
	EArcForestLayer BandOfHeight(float HeightCm)
	{
		return FArcForestLightModel::LayerForHeight(HeightCm, ShrubMinCm, UnderstoryMinCm, CanopyMinCm);
	}

	/** Human-readable band name for the diagnostic logs. */
	const TCHAR* BandLabel(EArcForestLayer Band)
	{
		switch (Band)
		{
		case EArcForestLayer::Field:      return TEXT("Field");
		case EArcForestLayer::Shrub:      return TEXT("Shrub");
		case EArcForestLayer::Understory: return TEXT("Understory");
		case EArcForestLayer::Canopy:     return TEXT("Canopy");
		default:                          return TEXT("<invalid>");
		}
	}

	/** Number of survivors carrying a given species id. */
	int32 CountSpecies(const FArcForestResult& Result, int32 SpeciesId)
	{
		int32 Count = 0;
		for (int32 Index = 0; Index < Result.Num(); ++Index)
		{
			if (Result.Species[Index] == SpeciesId)
			{
				++Count;
			}
		}
		return Count;
	}

	/** Index of the first survivor of a species, or INDEX_NONE. */
	int32 FindFirstOfSpecies(const FArcForestResult& Result, int32 SpeciesId)
	{
		for (int32 Index = 0; Index < Result.Num(); ++Index)
		{
			if (Result.Species[Index] == SpeciesId)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/**
	 * Two-species table for the band-MEMBERSHIP case.
	 *  sp0 MATURE — already at its mature form (2000 cm / 600 cm crown), so it never grows: a static
	 *      Canopy-band shade + competition source. TWO of these are placed at the SAME XY, which is
	 *      load-bearing (see the test comment): 2 x CrownLeafAreaIndex 2.0 = LAI 4.0 of shade AND
	 *      2 x CompetitionPenaltyPerPair 0.15 = 0.30 vigour penalty per iteration for a same-band
	 *      weaker neighbour. One mature would produce only 0.15, which the 0.34 recovery term absorbs
	 *      (steady state 0.559, above the 0.2 mortality floor) — i.e. the control would not die and the
	 *      case would not discriminate.
	 *  sp1 SEEDLING — MatureLayer = Canopy (this is what a species-fixed regression would band it by),
	 *      but only 10 cm tall. Very shade tolerant (St = 1.0) so EffectiveNeed = 0.2*(1-0.8) = 0.04,
	 *      comfortably below the exp(-0.5*4.0) = 0.135 it actually receives under the two crowns.
	 *      GrowthRate 0.01 keeps it well below CanopyMinHeightCm for the whole run.
	 */
	TStrongObjectPtr<UArcForestSpeciesDefinition> MakeMembershipTable()
	{
		TArray<FArcForestSpeciesEntry> Table;
		FArcForestSpeciesEntry MatureCanopy = MakeSpecies(TEXT("MatureCanopy"), EArcForestLayer::Canopy, 2000.0f, 600.0f, 0.10f,
			0.0f, 300.0f, 0.0f); // densities unused: nothing is seeded
		MatureCanopy.LightNeeded = 0.5f;
		MatureCanopy.ShadeTolerance = 0.5f;
		Table.Add(MoveTemp(MatureCanopy));
		FArcForestSpeciesEntry ShadeSeedling = MakeSpecies(TEXT("ShadeSeedling"), EArcForestLayer::Canopy, 2400.0f, 600.0f, 0.01f,
			0.0f, 300.0f, 0.0f); // MatureLayer Canopy = what the H1 bug banded by
		ShadeSeedling.LightNeeded = 0.2f;
		ShadeSeedling.ShadeTolerance = 1.0f;
		Table.Add(MoveTemp(ShadeSeedling));
		return MakeDefinition(MoveTemp(Table));
	}

	/**
	 * Hand-build the membership stand with NO seeding: two coincident mature canopy plants plus one
	 * small sp1 plant at the SAME XY at SmallHeightCm / 5 cm crown. bSeeded is set so the state honours
	 * the resumable-driver contract (SeedInitial is deliberately never called — that is the whole point:
	 * zero RNG).
	 */
	void BuildMembershipStand(FArcForestSolveState& State, float SmallHeightCm)
	{
		State = FArcForestSolveState();
		const FVector2f Origin(0.0f, 0.0f);
		State.Add(Origin, /*Species=*/0, /*Radius=*/600.0f, /*Height=*/2000.0f, /*PatchId=*/INDEX_NONE);
		State.Add(Origin, /*Species=*/0, /*Radius=*/600.0f, /*Height=*/2000.0f, /*PatchId=*/INDEX_NONE);
		State.Add(Origin, /*Species=*/1, /*Radius=*/5.0f, /*Height=*/SmallHeightCm, /*PatchId=*/INDEX_NONE);
		State.Iteration = 0;
		State.bSeeded = true;
	}
}

TEST_CLASS(ArcForestBandModel, "ArcPCGExtension.ForestSim.BandModel")
{
	// BAND THRESHOLDS — table-driven pin on FArcForestLightModel::LayerForHeight
	// (ArcForestLightModel.cpp) at the EXACT boundaries the solver uses (150 / 500 / 1500 cm).
	//
	// GUARDS: the three comparison branches `Height < ShrubMinHeight`, `Height < UnderstoryMinHeight`,
	// `Height < CanopyMinHeight`. The implementation uses STRICT `<` with lower-bound semantics, so a
	// height exactly ON a boundary belongs to the HIGHER band (150 -> Shrub, 500 -> Understory,
	// 1500 -> Canopy). The expected column below was READ OFF the implementation, not assumed.
	//
	// MUTANTS THIS TURNS RED: flipping any `<` to `<=` (moves the on-boundary height down a band),
	// reordering the cascade, or renumbering a threshold constant. Nothing currently probes any
	// boundary at all — the existing coverage samples only 10 cm and 2000 cm, both a full band away
	// from every edge, so a `<`/`<=` flip is invisible to it (and utterly invisible to a
	// survivor-count artifact, which averages over a whole stand).
	TEST_METHOD(BandThresholds_LayerForHeightBoundaries)
	{
		using namespace ArcForestBandModelTestsLocal;

		const FBoundaryCase Cases[] = {
			{ 0.0f,      EArcForestLayer::Field,      TEXT("ground zero") },
			{ 149.99f,   EArcForestLayer::Field,      TEXT("just below the Field->Shrub edge") },
			{ 150.0f,    EArcForestLayer::Shrub,      TEXT("exactly ON the Field->Shrub edge") },
			{ 499.99f,   EArcForestLayer::Shrub,      TEXT("just below the Shrub->Understory edge") },
			{ 500.0f,    EArcForestLayer::Understory, TEXT("exactly ON the Shrub->Understory edge") },
			{ 1499.99f,  EArcForestLayer::Understory, TEXT("just below the Understory->Canopy edge") },
			{ 1500.0f,   EArcForestLayer::Canopy,     TEXT("exactly ON the Understory->Canopy edge") },
			{ 100000.0f, EArcForestLayer::Canopy,     TEXT("far above every threshold") }
		};

		for (int32 CaseIndex = 0; CaseIndex < UE_ARRAY_COUNT(Cases); ++CaseIndex)
		{
			const FBoundaryCase& Case = Cases[CaseIndex];
			const EArcForestLayer Actual = BandOfHeight(Case.HeightCm);

			UE_LOGF(LogArcForestBandTest, Display,"[BandThresholds] h=%.2f cm -> %ls (expected %ls) — %ls",
				Case.HeightCm, BandLabel(Actual), BandLabel(Case.Expected), Case.Label);

			ASSERT_THAT(IsTrue(Actual == Case.Expected,
				FString::Printf(
					TEXT("LayerForHeight(%.2f cm) must be %s (%s) — boundaries are STRICT lower bounds: "
						"a height exactly on a boundary belongs to the HIGHER band"),
					Case.HeightCm, BandLabel(Case.Expected), Case.Label)));
		}

		// The thresholds are lower bounds of the band ABOVE, i.e. the bands must be contiguous with no
		// height owned by two bands: restate that as a monotonicity check across the whole cascade.
		ASSERT_THAT(IsTrue(
			static_cast<int32>(BandOfHeight(149.99f)) < static_cast<int32>(BandOfHeight(150.0f)) &&
			static_cast<int32>(BandOfHeight(499.99f)) < static_cast<int32>(BandOfHeight(500.0f)) &&
			static_cast<int32>(BandOfHeight(1499.99f)) < static_cast<int32>(BandOfHeight(1500.0f)),
			TEXT("Each threshold must step the band up by exactly one at the boundary")));
	}

	// BAND MEMBERSHIP — THE REPLACEMENT DISCRIMINATOR for the H1 (species-fixed banding) regression,
	// and the most important test in this file. NO SEEDING, NO RNG: the stand is hand-built into
	// FArcForestSolveState and driven through the production StepIterations/Emit path.
	//
	// GUARDS: ArcForestSimulator.cpp, ComputeBands ->
	//     OutBands[Index] = FArcForestLightModel::LayerForHeight(Pop.Height[Index], …)
	// consumed by ResolveCompetition (which pairs plants ONLY within a band) and by
	// ComputeIncidentLight's per-band self-shading gate.
	//
	// THE STAND (all three plants at the SAME XY, so crown overlap is exactly 1.0):
	//     2 x sp0 mature canopy (2000 cm, 600 cm crown, static)  +  1 x sp1 (MatureLayer = Canopy).
	//
	//  A. LIVE-BAND STAND — sp1 starts at 10 cm.
	//     CORRECT (height-derived): 10 cm -> Field. It is the ONLY Field plant, so ResolveCompetition
	//     skips its band entirely (BandGlobal.Num() < 2) and it takes ZERO crown-overlap penalty.
	//     Cross-band shade is always on, so it sits at exp(-0.5 * 4.0) = 0.135 light, above its
	//     EffectiveNeed of 0.04 -> vigour recovers -> SHADED BUT ALIVE.
	//
	//  B. CONTROL STAND — bit-identical except sp1 starts at 1600 cm, which is genuinely IN the Canopy
	//     band. This reproduces the SPECIES-FIXED MUTANT'S EXACT ARITHMETIC through the unmodified
	//     production path: same band as the matures -> paired every iteration at overlap 1.0 -> weaker
	//     of pair (lower vigour, or on the first iteration the vigour tie broken by "shorter loses")
	//     -> -0.30 vigour/iteration, and its own-band shade is gated OFF (IntraLayerShading[Canopy] ==
	//     false) so it reads light 1.0. Recovery cannot keep up (steady state 0.118 < the 0.2
	//     VigorMortalityFloor) -> it crosses the floor and StarvationLimit = 4 kills it.
	//
	// MUTANT THAT TURNS THIS RED: reverting ComputeBands to `Table[Pop.Species[i]].MatureLayer`. sp1's
	// MatureLayer IS Canopy, so stand A collapses into stand B — the seedling dies and assertion A
	// flips RED. Stand B is the MEASURED control proving A is not vacuous: it shows this fixture DOES
	// kill a plant once it shares the matures' band, so A's survival is caused by band membership and
	// by nothing else. The mutant's arithmetic is therefore measured WITHOUT ever editing production.
	//
	// WHY THE `Nurse > 11` SURVIVOR-COUNT ARTIFACT DOES NOT COVER THIS: it is a count inequality over
	// an RNG-seeded stand whose correct side has drifted 12 -> 11 -> 13 across seeding changes, with a
	// discrimination band of only +/-2. This case is BINARY (survive/die), has no counts of a seeded
	// population, and touches no random stream — it cannot drift with the seeding fixture at all.
	TEST_METHOD(BandMembership_SeedlingUnderCanopySurvivesOnLiveBand)
	{
		using namespace ArcForestBandModelTestsLocal;

		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeMembershipTable();
		ASSERT_THAT(IsTrue(Def->Species[1].MatureLayer == EArcForestLayer::Canopy,
			TEXT("Fixture precondition: the seedling species' MatureLayer must be Canopy, otherwise a "
				"species-fixed banding regression would not mis-bucket it and this case proves nothing")));

		// The two banding models genuinely diverge for this plant: live band Field, mature band Canopy.
		ASSERT_THAT(IsTrue(BandOfHeight(10.0f) == EArcForestLayer::Field,
			TEXT("A 10 cm plant's LIVE band is Field (MC1)")));
		ASSERT_THAT(IsTrue(BandOfHeight(2000.0f) == EArcForestLayer::Canopy,
			TEXT("A 2000 cm mature's live band is Canopy")));

		FArcForestSimParams Params;
		Params.Iterations = 18;
		Params.CanopyExtinction = 0.5f;
		Params.Bounds2D = CentredBounds(1000.0f); // 20 m x 20 m; no seeding happens inside it

		const TArray<float> EmptySuitability; // empty -> uniform suitability 1.0 (never sampled: no seeding)
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		// --- A. live-band stand: the small plant starts in the Field band -------------------------
		FArcForestSolveState LiveBandState;
		BuildMembershipStand(LiveBandState, /*SmallHeightCm=*/10.0f);
		Simulator.StepIterations(LiveBandState, Params, *Def, EmptySuitability, Dims, /*MaxIters=*/0);
		const FArcForestResult LiveBand = Simulator.Emit(LiveBandState, *Def);

		const int32 LiveSmallCount = CountSpecies(LiveBand, /*SpeciesId=*/1);
		const int32 LiveSmallIndex = FindFirstOfSpecies(LiveBand, /*SpeciesId=*/1);

		// --- B. control stand: identical, but the small plant starts IN the Canopy band ------------
		FArcForestSolveState ControlState;
		BuildMembershipStand(ControlState, /*SmallHeightCm=*/1600.0f);
		Simulator.StepIterations(ControlState, Params, *Def, EmptySuitability, Dims, /*MaxIters=*/0);
		const FArcForestResult Control = Simulator.Emit(ControlState, *Def);

		const int32 ControlSmallCount = CountSpecies(Control, /*SpeciesId=*/1);

		UE_LOGF(LogArcForestBandTest, Display,"[BandMembership] liveBand survivors=%d (small=%d, h=%.1f, vigor=%.3f) | control survivors=%d (small=%d)",
			LiveBand.Num(), LiveSmallCount,
			(LiveSmallIndex != INDEX_NONE) ? LiveBand.Height[LiveSmallIndex] : -1.0f,
			(LiveSmallIndex != INDEX_NONE) ? LiveBand.Vigor[LiveSmallIndex] : -1.0f,
			Control.Num(), ControlSmallCount);

		// THE DISCRIMINATOR: the seedling lives because its LIVE band differs from the matures'.
		ASSERT_THAT(IsTrue(LiveSmallCount == 1,
			TEXT("A 10 cm seedling of a CANOPY-mature species under two mature crowns must SURVIVE: its "
				"live band is Field, so ResolveCompetition never pairs it with the Canopy matures. Under "
				"species-fixed banding (H1) it shares their band, is the weaker of every pair at overlap "
				"1.0, and starves out")));
		ASSERT_THAT(IsTrue(LiveSmallIndex != INDEX_NONE && LiveBand.Height[LiveSmallIndex] < CanopyMinCm,
			TEXT("The surviving seedling must still be BELOW CanopyMinHeightCm — it survived as a "
				"sub-canopy plant, it did not merely outgrow the matures")));

		// The control proves the fixture is lethal ONCE the small plant shares the matures' band, so the
		// assertion above is caused by band membership and not by a harmless stand.
		ASSERT_THAT(IsTrue(ControlSmallCount == 0,
			TEXT("CONTROL (non-vacuity): the SAME stand with the small plant starting at 1600 cm — i.e. "
				"genuinely in the Canopy band, which is exactly what species-fixed banding would assign "
				"it — must KILL that plant. If this is green the fixture cannot kill anything and the "
				"survival assertion above proves nothing")));

		// Both mature crowns survive in both stands, so neither result is a died-out degenerate stand.
		ASSERT_THAT(IsTrue(CountSpecies(LiveBand, /*SpeciesId=*/0) == 2,
			TEXT("Both mature canopy plants must survive the live-band stand")));
		ASSERT_THAT(IsTrue(CountSpecies(Control, /*SpeciesId=*/0) == 2,
			TEXT("Both mature canopy plants must survive the control stand")));
	}

	// BAND GRADUATION — pins the MC1 "plants graduate up as they grow" claim, which NOTHING currently
	// tests. NO SEEDING, NO RNG: two hand-built plants, driven one iteration at a time so the live-band
	// trajectory can be sampled per iteration.
	//
	// GUARDS: the placement of the ComputeBands call INSIDE StepIterations' per-iteration loop
	// (ArcForestSimulator.cpp, StepIterations step "a. Live bands from CURRENT height … recomputed
	// every iteration"), not merely the classifier.
	//
	// THE STAND: a static mature (2000 cm / 600 cm crown, Canopy) plus a GROWER at the same XY starting
	// at 10 cm (Field) with GrowthRate 0.25 and MaxHeight 2400. Under shade (exp(-0.5*2.0) = 0.368) its
	// growth response is ~0.80, so it climbs Field -> Shrub -> Understory -> Canopy within a handful of
	// iterations, and ONCE IN THE CANOPY BAND it starts sharing the mature's competition domain: at
	// overlap 1.0 it is the weaker of the pair (lower vigour) and takes -0.15 vigour per iteration
	// against a 0.34 recovery term -> its vigour settles near 0.56, far below the 1.0 of an uncontested
	// plant.
	//
	// TWO MUTANTS THIS TURNS RED:
	//  1. Bands frozen at seed time (ComputeBands HOISTED OUT of the loop, computed once from the
	//     initial heights): the grower would be classified Field forever, never join the mature's
	//     Canopy band, never be paired, and finish at vigour 1.0 -> the vigour assertion flips RED.
	//     Its band-from-height still rises, which is precisely why the band-trajectory assertion alone
	//     is not enough and the vigour consequence is asserted too.
	//  2. Growth gated so plants cannot leave their birth band (or a graduation regression that pins a
	//     plant to its first band): the strictly-increasing band trajectory flips RED.
	// The `Nurse > 11` survivor-count artifact leaves BOTH green — it is a single end-of-run count and
	// observes no per-iteration band trajectory at all.
	TEST_METHOD(BandGraduation_PlantsChangeBandAcrossIterations)
	{
		using namespace ArcForestBandModelTestsLocal;

		TArray<FArcForestSpeciesEntry> Table;
		// sp0 MATURE — already at its mature form, so it is a static shade + competition source.
		FArcForestSpeciesEntry MatureCanopy = MakeSpecies(TEXT("MatureCanopy"), EArcForestLayer::Canopy, 2000.0f, 600.0f, 0.10f,
			0.0f, 300.0f, 0.0f); // densities unused: nothing is seeded
		MatureCanopy.LightNeeded = 0.5f;
		MatureCanopy.ShadeTolerance = 0.5f;
		Table.Add(MoveTemp(MatureCanopy));
		// sp1 GROWER — starts as a 10 cm seedling and climbs every band inside the iteration budget.
		FArcForestSpeciesEntry Grower = MakeSpecies(TEXT("Grower"), EArcForestLayer::Canopy, 2400.0f, 600.0f, 0.25f,
			0.0f, 300.0f, 0.0f);
		Grower.LightNeeded = 0.35f;
		Grower.ShadeTolerance = 0.85f;
		Table.Add(MoveTemp(Grower));
		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));

		FArcForestSimParams Params;
		Params.Iterations = 20;
		Params.CanopyExtinction = 0.5f;
		Params.Bounds2D = CentredBounds(1000.0f);

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		// Hand-built stand, NO seeding: mature at index 0, grower at index 1, same XY.
		FArcForestSolveState State;
		const FVector2f Origin(0.0f, 0.0f);
		State.Add(Origin, /*Species=*/0, /*Radius=*/600.0f, /*Height=*/2000.0f, /*PatchId=*/INDEX_NONE);
		State.Add(Origin, /*Species=*/1, /*Radius=*/5.0f, /*Height=*/10.0f, /*PatchId=*/INDEX_NONE);
		State.Iteration = 0;
		State.bSeeded = true;

		constexpr int32 GrowerIndex = 1;
		const EArcForestLayer InitialBand = BandOfHeight(State.Height[GrowerIndex]);
		ASSERT_THAT(IsTrue(InitialBand == EArcForestLayer::Field,
			TEXT("Fixture precondition: the grower must START in the Field band")));

		// Step ONE iteration at a time and recompute the live-band histogram from the live heights,
		// exactly as ComputeBands does. Nothing dies in this stand, so plant indices stay stable — the
		// per-iteration guard below makes that explicit rather than assumed.
		int32 BandHistogram[GArcForestLayerCount] = { 0, 0, 0, 0 };
		EArcForestLayer PreviousBand = InitialBand;
		EArcForestLayer HighestBand = InitialBand;
		int32 StrictIncreases = 0;
		bool bBandEverDecreased = false;
		bool bPopulationStable = true;

		bool bDone = false;
		while (!bDone)
		{
			bDone = Simulator.StepIterations(State, Params, *Def, EmptySuitability, Dims, /*MaxIters=*/1);
			if (State.Num() != 2)
			{
				bPopulationStable = false;
				break;
			}

			const EArcForestLayer Band = BandOfHeight(State.Height[GrowerIndex]);
			++BandHistogram[static_cast<int32>(Band)];
			if (static_cast<int32>(Band) > static_cast<int32>(PreviousBand))
			{
				++StrictIncreases;
			}
			else if (static_cast<int32>(Band) < static_cast<int32>(PreviousBand))
			{
				bBandEverDecreased = true;
			}
			if (static_cast<int32>(Band) > static_cast<int32>(HighestBand))
			{
				HighestBand = Band;
			}

			UE_LOGF(LogArcForestBandTest, Display,"[BandGraduation] iter %d — grower h=%.1f cm band=%ls vigor=%.3f",
				State.Iteration, State.Height[GrowerIndex], BandLabel(Band), State.Vigor[GrowerIndex]);

			PreviousBand = Band;
		}

		ASSERT_THAT(IsTrue(bPopulationStable,
			TEXT("Fixture precondition: nothing may die in this stand (plant indices must stay stable)")));

		const FArcForestResult Result = Simulator.Emit(State, *Def);
		const int32 GrowerResultIndex = FindFirstOfSpecies(Result, /*SpeciesId=*/1);

		UE_LOGF(LogArcForestBandTest, Display,"[BandGraduation] histogram Field=%d Shrub=%d Understory=%d Canopy=%d; strictIncreases=%d "
				"highest=%ls finalVigor=%.3f",
			BandHistogram[0], BandHistogram[1], BandHistogram[2], BandHistogram[3],
			StrictIncreases, BandLabel(HighestBand),
			(GrowerResultIndex != INDEX_NONE) ? Result.Vigor[GrowerResultIndex] : -1.0f);

		// (1) SUCCESSION: the grower's live band must strictly increase during the run — it starts in
		// Field and must be found in a higher band later. A regression that pins a plant to its birth
		// band (or stalls growth entirely) leaves StrictIncreases at 0.
		ASSERT_THAT(IsTrue(StrictIncreases > 0,
			TEXT("A growing plant's live band must STRICTLY INCREASE across iterations (MC1 succession: "
				"plants graduate upward as they grow)")));
		ASSERT_THAT(IsTrue(!bBandEverDecreased,
			TEXT("A growing plant's live band must never step DOWN (growth is monotonic here)")));
		ASSERT_THAT(IsTrue(HighestBand == EArcForestLayer::Canopy,
			TEXT("The grower must reach the CANOPY band from a 10 cm Field start within the iteration "
				"budget — Field -> Shrub -> Understory -> Canopy")));
		ASSERT_THAT(IsTrue(BandHistogram[static_cast<int32>(EArcForestLayer::Field)] +
			BandHistogram[static_cast<int32>(EArcForestLayer::Shrub)] +
			BandHistogram[static_cast<int32>(EArcForestLayer::Understory)] > 0,
			TEXT("The grower must spend part of the run BELOW the canopy — otherwise it never graduated, "
				"it was simply born mature")));

		// (2) THE SOLVER-OBSERVABLE CONSEQUENCE that kills the hoisted-ComputeBands mutant: once the
		// grower graduates into the mature's Canopy band it must actually START COMPETING with it and
		// carry the suppression. With bands frozen at seed time the grower stays Field, is never paired,
		// and finishes at vigour 1.0.
		ASSERT_THAT(IsTrue(GrowerResultIndex != INDEX_NONE,
			TEXT("The grower must survive to be observable")));
		ASSERT_THAT(IsTrue(Result.Vigor[GrowerResultIndex] < 0.9f,
			TEXT("After graduating into the mature's Canopy band the grower must carry same-band crown "
				"suppression (vigour well below 1.0). If ComputeBands is hoisted out of the per-iteration "
				"loop the grower stays banded Field forever, is never paired with the mature, and ends at "
				"vigour 1.0 -> RED")));
		ASSERT_THAT(IsTrue(Result.Vigor[GrowerResultIndex] > 0.0f,
			TEXT("Suppressed, not deleted: the graduated grower is still alive")));
	}
};
