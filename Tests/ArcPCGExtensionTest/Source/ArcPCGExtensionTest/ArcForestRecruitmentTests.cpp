// Copyright Lukasz Baran. All Rights Reserved.

// ArcForestRecruitmentTests.cpp
// Step 12 of docs/plans/2026-07-25-1133-arcpcgextension-habitat-fields-and-recruitment-plan.md —
// contract coverage of the RECRUITMENT STAGE (stage h) landed by Step 11 (commit 48384f9cb719) and its
// review fixes (commit e0397ac72380): ArcForestSimulator.cpp RecruitPlants / ResolvePopulationCeiling /
// MakeRecruitStream / the bRecruitmentActive gate in StepIterations.
//
// THE SHIPPED CONTRACT UNDER TEST (verified against the landed code, not restated from the plan):
//   * bEnableRecruitment defaults FALSE and gates the whole stage AND the query-height raster.
//   * Attempts = Min(Round(RecruitmentRate * Ceiling), Max(0, Ceiling - live)); EXACTLY that many darts,
//     three unconditional draws each (x, y, accept), NO retry loop.
//   * Acceptance is the THREE-factor product
//         Clamp(SampleFieldNearest(dart, Bounds, SuitabilityGrid, GridDims)
//               * EvaluateHabitatResponse(...) * EvaluateShadeResponse(...), 0, 1)
//     and a dart is accepted when DrawAccept < AcceptProb. FRandomStream::FRand() is in [0,1), so an
//     AcceptProb of exactly 1.0 accepts unconditionally and an AcceptProb of exactly 0.0 rejects
//     unconditionally. EVERY case below exploits that: with step-shaped curves and a 0/1 suitability
//     mask the acceptance decision is a HARD SPATIAL MASK, so the assertions are exact set-membership
//     claims rather than drifting count inequalities.
//   * Spacing: Max(RecruitMinSpacingCm, SpacingFraction * (SeedlingCrownRadiusCm + neighbour's LIVE
//     CrownRadius)) against an intrusive-chain hash that same-iteration recruits enter as accepted.
//   * Recruits: Height = ArcForestSim::SeedlingHeightCm, CrownRadius = ArcForestSim::SeedlingCrownRadiusCm,
//     Age = 0, Vigor = 1, PatchId = INDEX_NONE. Asserted against the NAMED constants, never 10/5 literals.
//   * The per-iteration stream is MakeRecruitStream(Seed, State.Iteration + 1) — no per-call cursor, so it
//     is chunk-invariant.
//
// TOPOLOGY: standalone, in-process, no world, no PIE, no RHI. Class 3 genuinely does not apply — the unit
// is a pure CPU solver over plain arrays with no UObject, no replication and no authority split, so a
// FPIENetworkComponent/UWorldPartition fixture would add a world no assertion here reads.
// NOT covered, and it is this plugin's real topology risk: production flattens the curves on the GAME
// THREAD (PCGForestSimSettings.cpp) before UE::Tasks::Launch, which is the whole reason the shade LUT
// exists instead of a worker-read FRichCurve a designer can drag mid-solve. Every fixture below flattens
// inline on the same thread, so an edit that moved the flatten INSIDE the task would keep all of these
// green. See KNOWN GAPS.
// The unit is the pure array-in/array-out solver (FArcForestSimulator's "PURITY (MC3)" contract,
// ArcForestSimulator.h): no UObject mutation, no replicated state, no actor, no authority split. The
// server, a client and a dedicated server all execute the identical pure function on identical inputs;
// the only caller is a PCG element that runs the solve on a UE::Tasks worker in the editor. A
// FPIENetworkComponent/UWorldPartition fixture would add a world and change nothing any assertion here
// reads, so a network fixture would be cost without discrimination.
//
// PRODUCTION PATH, NOT A PROXY. Two shapes, both shipped entry points:
//   * SEEDED SOLVES call Simulate / SeedInitial + StepIterations + Emit exactly as the PCG element does.
//   * HAND-BUILT STANDS fill a real FArcForestSolveState via State.Add + bSeeded (the
//     ArcForestTransmittanceTests.cpp / ArcForestBandModelTests.cpp technique) and then call the shipped
//     StepIterations. RecruitPlants is file-local to the solver .cpp and is deliberately NOT reached by
//     any test-only hook: every case below drives it through StepIterations, which is the only way
//     production reaches it. Nothing is re-implemented; the raster the shade case checks against is
//     produced by the same FArcForestLightModel::ComputeIncidentLight call the solver makes at stage (c).
//
// MUTATION DISCIPLINE. Every threshold below has BOTH SIDES MEASURED, and — this is the part the earlier
// steps of this rollout got wrong (a "Nurse > 8" assert that passed on the very regression it named) —
// the mutant side is measured BY THE TEST ITSELF, in the same run, wherever the mutant's output is
// reachable through a fixture change rather than a production edit:
//   * "delete the suitability factor" == running the same fixture with a UNIFORM suitability grid
//     (SampleFieldNearest on an all-ones grid returns exactly 1.0, which is what deleting the factor does).
//   * "delete the shade factor" == running the same fixture with UNAUTHORED ShadeResponseCurves
//     (EvaluateShadeResponse returns exactly 1.0 for bAuthored == false).
//   * "drop the RecruitMinSpacingCm floor" == running the same fixture with RecruitMinSpacingCm = 0.
//   * "ignore MaturityHeightCm" == running the same fixture with MaturityHeightCm = 0.
//   * "gate the share split on > 0.0f instead of UE_SMALL_NUMBER" == running the same fixture with
//     RecruitmentWeight = 1.0: the share RATIO Attempts*W/WeightSum is SCALE-INVARIANT, so a mutant that
//     let a denormal WeightSum through would produce exactly the (1.0, 1.0) run's output.
//   * "delete the bEnableRecruitment gate" == the enabled run, compared against the disabled one.
// Only three mutants are argued rather than executed (no production edit is permitted here, and their
// outputs are not reachable from any fixture): the per-iteration stream replaced by a carried one, the
// dart loop turned into accept-until-quota, and the per-axis bounds gate relaxed back to the area
// product. Each is named on its assertion with the exact reason its assertion flips. See KNOWN GAPS.
//
// KNOWN GAPS — recorded, not silently skipped:
//   * Ceiling DERIVATION (PopulationCeiling == 0 -> the per-species TargetDensity * SeedingDensity * area
//     sum) is exercised only implicitly by the seeded solves, which use it; every hand-built case pins
//     PopulationCeiling explicitly so Attempts is a closed form. A dedicated derivation test would be
//     asserting arithmetic already covered by ArcForestSeedingContractTests' candidate-budget cases.
//   * The RecruitStride / RecruitQueryHeightCm plumbing into the raster is Step 10's subject
//     (ArcForestTransmittanceTests.cpp); here it is only exercised through the shade-preference case.
//   * "Recruitment cannot revive a dead stand" (the NumPlants == 0 break above stage h) is not covered:
//     the break sits in the iteration loop above recruitment and killing a whole stand deterministically
//     needs a mortality fixture, not a recruitment one.
//   * The GT-flatten-before-UE::Tasks::Launch ordering is not exercised (see TOPOLOGY above). Every case
//     here flattens inline on the same thread, so moving the flatten inside the task keeps all of them
//     green even though it reintroduces the editor-mutation race the LUT design exists to prevent.
//   * bEnableRecruitment gating the query-height RASTER (as distinct from the recruitment stage) is not
//     provable behaviourally: suppressing the raster has no observable output, so a mutant that always
//     passes a non-null OutTransmittance stays green. Only the stage half of that gate is pinned.
//   * RecruitQueryHeightCm is never VARIED in a recruitment-enabled run — a mutant that ignored it and
//     hardcoded the seedling height stays green. The node-side copy of it is likewise unpinned; see
//     Element_RecruitmentSettingsReachTheSolver in ArcForestPCGSmokeTests.cpp, which pins
//     bEnableRecruitment, PopulationCeiling and RecruitMinSpacingCm but not the other three.
//   * BRITTLENESS, stated so it is not discovered later: roughly twenty exact anchors here sit downstream
//     of the seeding stream, the light model, growth/mortality, ArcForestRecruitSalt,
//     MinCompetitionCellCm, ResponseLutSize and FRand. Any legitimate change to those forces a wholesale
//     re-baseline of this file. Mitigation, and it must be PRESERVED on any such re-baseline: every
//     mutant-side pin is paired with an independent inequality, so re-pinning the numbers cannot silently
//     erase the discrimination.

#include "CQTest.h"

#include "ArcForestTestHelpers.h"

#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"
#include "UObject/StrongObjectPtr.h"

#include "Forest/ArcForestField.h"
#include "Forest/ArcForestLightModel.h"
#include "Forest/ArcForestSimTypes.h"
#include "Forest/ArcForestSimulator.h"
#include "Forest/ArcForestSpeciesDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcForestRecruitTest, Log, All);

namespace ArcForestRecruitmentTestsLocal
{
	using namespace ArcForestTestHelpers;

	/** The cm-scaled band thresholds the SOLVER passes to LayerForHeight (ArcForestSimulator.cpp). */
	constexpr float ShrubMinCm = 150.0f;
	constexpr float UnderstoryMinCm = 500.0f;
	constexpr float CanopyMinCm = 1500.0f;

	/** The solver's light-grid resolution (ArcForestSimulator.cpp LightGridCellSizeCm). */
	constexpr float LightCellSizeCm = 100.0f;

	/** The solver's uniform per-plant crown LAI proxy (ArcForestSimulator.cpp CrownLeafAreaIndex). */
	constexpr float CrownLaiProxy = 2.0f;

	/** The node's suitability-grid resolution and dimension cap (PCGForestSimSettings). */
	constexpr float SuitabilityCellSizeCm = 200.0f;
	constexpr int32 MaxGridDim = 256;

	/**
	 * A stand hand-built into a REAL FArcForestSolveState — no seeding, no RNG, closed-form geometry.
	 * bSeeded is set because the resumable-driver contract requires it even though SeedInitial is
	 * deliberately never called (the same technique as ArcForestTransmittanceTests.cpp).
	 */
	struct FHandBuiltStand
	{
		FArcForestSolveState State;

		void Add(FVector2f Pos, int32 SpeciesIndex, float HeightCm, float CrownRadiusCm)
		{
			State.Add(Pos, SpeciesIndex, CrownRadiusCm, HeightCm, /*PatchId=*/INDEX_NONE);
			State.bSeeded = true;
			State.Iteration = 0;
		}

		int32 Num() const
		{
			return State.Num();
		}
	};

	/**
	 * Recruits of the MOST RECENT iteration, identified by Age == 0.
	 *
	 * Age is an exact discriminator here and not a heuristic: stage (g) increments every survivor's Age
	 * immediately BEFORE stage (h) appends recruits at Age 0, so after one iteration every pre-existing
	 * plant is at Age >= 1 and every recruit of that iteration is at exactly 0. Each case that uses this
	 * asserts the pre-existing plants' Age separately, so a regression that stopped ageing survivors
	 * cannot make this predicate silently over-count.
	 */
	void CollectRecruits(
		const FArcForestSolveState& State,
		TArray<FVector2f>& OutPos,
		TArray<int32>& OutSpecies)
	{
		OutPos.Reset();
		OutSpecies.Reset();
		for (int32 Index = 0; Index < State.Num(); ++Index)
		{
			if (State.Age[Index] == 0)
			{
				OutPos.Add(State.Pos[Index]);
				OutSpecies.Add(State.Species[Index]);
			}
		}
	}

	/** Count of Age == 0 entries — the recruit count of the most recent iteration. */
	int32 CountRecruits(const FArcForestSolveState& State)
	{
		int32 Count = 0;
		for (int32 Index = 0; Index < State.Num(); ++Index)
		{
			Count += (State.Age[Index] == 0) ? 1 : 0;
		}
		return Count;
	}

	/** Count of Age == 0 entries of one species. */
	int32 CountRecruitsOfSpecies(const FArcForestSolveState& State, int32 SpeciesIndex)
	{
		int32 Count = 0;
		for (int32 Index = 0; Index < State.Num(); ++Index)
		{
			Count += (State.Age[Index] == 0 && State.Species[Index] == SpeciesIndex) ? 1 : 0;
		}
		return Count;
	}

	/** Positions in a set whose X is at or above a bound — the masked/open-region membership counter. */
	int32 CountAtOrRightOf(TArrayView<const FVector2f> Positions, float MinX)
	{
		int32 Count = 0;
		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			Count += (Positions[Index].X >= MinX) ? 1 : 0;
		}
		return Count;
	}

	/** Positions in a set whose X is at or below a bound. */
	int32 CountAtOrLeftOf(TArrayView<const FVector2f> Positions, float MaxX)
	{
		int32 Count = 0;
		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			Count += (Positions[Index].X <= MaxX) ? 1 : 0;
		}
		return Count;
	}

	/** Positions of one species out of a recruit slice. */
	void FilterBySpecies(
		TArrayView<const FVector2f> Pos,
		TArrayView<const int32> Species,
		int32 SpeciesIndex,
		TArray<FVector2f>& OutPos)
	{
		OutPos.Reset();
		for (int32 Index = 0; Index < Pos.Num(); ++Index)
		{
			if (Species[Index] == SpeciesIndex)
			{
				OutPos.Add(Pos[Index]);
			}
		}
	}

	/** Smallest pairwise distance in a set, or -1.0 for fewer than two points (a vacuous set is visible). */
	float MinPairwiseDistance(TArrayView<const FVector2f> Positions)
	{
		if (Positions.Num() < 2)
		{
			return -1.0f;
		}
		float BestSq = TNumericLimits<float>::Max();
		for (int32 I = 0; I < Positions.Num(); ++I)
		{
			for (int32 J = I + 1; J < Positions.Num(); ++J)
			{
				BestSq = FMath::Min(BestSq, (Positions[I] - Positions[J]).SizeSquared());
			}
		}
		return FMath::Sqrt(BestSq);
	}

	/** Number of points within Radius of Centre (inclusive) — the spacing-rule annulus counter. */
	int32 CountWithinRadius(TArrayView<const FVector2f> Positions, FVector2f Centre, float Radius)
	{
		int32 Count = 0;
		const float RadiusSq = FMath::Square(Radius);
		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			Count += ((Positions[Index] - Centre).SizeSquared() <= RadiusSq) ? 1 : 0;
		}
		return Count;
	}

	/** Smallest distance from Centre to any point, or -1.0 for an empty set. */
	float MinDistanceTo(TArrayView<const FVector2f> Positions, FVector2f Centre)
	{
		if (Positions.Num() == 0)
		{
			return -1.0f;
		}
		float BestSq = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			BestSq = FMath::Min(BestSq, (Positions[Index] - Centre).SizeSquared());
		}
		return FMath::Sqrt(BestSq);
	}

	/**
	 * ELEMENT-WISE identity across every parallel array of an emitted result — not a count or a
	 * statistic. This is what "reproduces the baseline exactly" has to mean: a recruitment stage that
	 * consumed one RNG draw while disabled, or emitted a raster that perturbed nothing but timing, would
	 * still produce the same COUNT and sail through any statistical comparison.
	 */
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

	/** Element-wise identity of two solve states, including the solver-only StarvationCounter + cursor. */
	bool StatesIdentical(const FArcForestSolveState& A, const FArcForestSolveState& B)
	{
		if (A.Num() != B.Num() || A.Iteration != B.Iteration)
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A.Pos[Index] != B.Pos[Index]) { return false; }
			if (A.Species[Index] != B.Species[Index]) { return false; }
			if (A.CrownRadius[Index] != B.CrownRadius[Index]) { return false; }
			if (A.Height[Index] != B.Height[Index]) { return false; }
			if (A.Age[Index] != B.Age[Index]) { return false; }
			if (A.Vigor[Index] != B.Vigor[Index]) { return false; }
			if (A.PatchId[Index] != B.PatchId[Index]) { return false; }
			if (A.StarvationCounter[Index] != B.StarvationCounter[Index]) { return false; }
		}
		return true;
	}

	/**
	 * Author a curve as PIECEWISE-LINEAR keys. Interp mode is forced to RCIM_Linear because
	 * FRichCurveKey defaults to RCIM_Cubic with auto tangents, which would ease the curve and blur the
	 * STEP shape the hard-mask acceptance below depends on.
	 *
	 * Deliberately a local copy of the same three lines in ArcForestHabitatTestsLocal rather than a new
	 * entry in the shared ArcForestTestHelpers.h: adding an identically-named function to that header
	 * would sit in scope of every existing forest test's `using namespace ArcForestTestHelpers`.
	 */
	void AuthorLinearCurve(FRuntimeFloatCurve& Curve, TArrayView<const FVector2f> Keys)
	{
		FRichCurve* Rich = Curve.GetRichCurve();
		Rich->Reset();
		for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
		{
			const FKeyHandle Handle = Rich->AddKey(Keys[KeyIndex].X, Keys[KeyIndex].Y);
			Rich->SetKeyInterpMode(Handle, RCIM_Linear);
		}
	}

	/**
	 * A two-species table carrying NOTHING but ecological scalars: no FieldResponses and no
	 * ShadeResponseCurve, which is the SHIPPING AUTHORING DEFAULT and therefore the case where both
	 * EvaluateHabitatResponse and EvaluateShadeResponse return exactly 1.0. That is exactly why it is
	 * the right table for the suitability-gating case: with those two terms pinned at 1.0 the ONLY
	 * factor left in the acceptance product is the suitability sample, so the assertion is about that
	 * factor and nothing else.
	 *
	 * Both rows are mature from the first iteration (MaturityHeightCm defaults to 0) and carry the
	 * default RecruitmentWeight of 1.0, so the budget splits evenly.
	 */
	TStrongObjectPtr<UArcForestSpeciesDefinition> MakeDefaultPair()
	{
		TArray<FArcForestSpeciesEntry> Table;
		Table.Add(MakeSpecies(TEXT("PlainA"), EArcForestLayer::Canopy,
			/*MaxHeight=*/2000.0f, /*MaxCrownRadius=*/400.0f, /*GrowthRate=*/0.10f,
			/*TargetDensity=*/0.02f, /*PatchRadiusSigma=*/300.0f, /*PatchDensity=*/0.05f));
		Table.Add(MakeSpecies(TEXT("PlainB"), EArcForestLayer::Shrub,
			/*MaxHeight=*/400.0f, /*MaxCrownRadius=*/120.0f, /*GrowthRate=*/0.12f,
			/*TargetDensity=*/0.05f, /*PatchRadiusSigma=*/300.0f, /*PatchDensity=*/0.05f));
		return MakeDefinition(MoveTemp(Table));
	}

	/** A single-species table with the same "no authored curves" shape, for the seeded-solve cases. */
	TStrongObjectPtr<UArcForestSpeciesDefinition> MakeSingleSpecies()
	{
		TArray<FArcForestSpeciesEntry> Table;
		FArcForestSpeciesEntry Oak = MakeSpecies(TEXT("Oak"), EArcForestLayer::Canopy,
			/*MaxHeight=*/2000.0f, /*MaxCrownRadius=*/450.0f, /*GrowthRate=*/0.12f,
			/*TargetDensity=*/0.05f, /*PatchRadiusSigma=*/400.0f, /*PatchDensity=*/0.04f);
		Oak.LightNeeded = 0.4f;
		Oak.ShadeTolerance = 0.6f;
		Table.Add(MoveTemp(Oak));
		return MakeDefinition(MoveTemp(Table));
	}

	/** Base params for a hand-built recruitment case: recruitment ON with an explicit ceiling. */
	FArcForestSimParams MakeRecruitParams(float HalfSizeCm, int32 Ceiling, float Rate)
	{
		FArcForestSimParams Params;
		Params.Seed = 987654;
		Params.Iterations = 1;
		Params.Bounds2D = CentredBounds(HalfSizeCm);
		Params.bEnableRecruitment = true;
		Params.PopulationCeiling = Ceiling;
		Params.RecruitmentRate = Rate;
		return Params;
	}

	/** The shipped Attempts formula, restated so each case can assert its dart count is non-zero. */
	int32 ExpectedAttempts(const FArcForestSimParams& Params, int32 LiveCount)
	{
		const int32 Budget = FMath::Max(0,
			FMath::RoundToInt(Params.RecruitmentRate * static_cast<float>(Params.PopulationCeiling)));
		return FMath::Min(Budget, FMath::Max(0, Params.PopulationCeiling - LiveCount));
	}

	/** A uniform all-ones suitability grid — the "suitability factor deleted" control. */
	void BuildUniformSuitability(FIntPoint Dims, TArray<float>& OutGrid)
	{
		OutGrid.Init(1.0f, Dims.X * Dims.Y);
	}

	/**
	 * A suitability grid that is 1.0 on the LEFT half of Bounds and EXACTLY 0.0 on the right, the cells
	 * split on the sign of their CENTRE X. With centred bounds and a 200 cm cell the grid boundary falls
	 * exactly on X = 0, so "masked" is precisely the cells spanning X > 0 and SampleFieldNearest answers
	 * 0.0 for every dart with X >= 0 (FloorToInt puts X == 0 in the first masked cell).
	 *
	 * This is the shape a real Density mask or a ProjectPoint miss produces upstream, reduced to a hole
	 * whose membership test is exact.
	 */
	void BuildRightHalfMaskedSuitability(const FBox2f& Bounds, FIntPoint Dims, TArray<float>& OutGrid)
	{
		OutGrid.SetNumUninitialized(Dims.X * Dims.Y);
		for (int32 Row = 0; Row < Dims.Y; ++Row)
		{
			for (int32 Col = 0; Col < Dims.X; ++Col)
			{
				const FVector2f Centre = ArcForestField::CellCentre(Bounds, Dims, Col, Row);
				OutGrid[(Row * Dims.X) + Col] = (Centre.X > 0.0f) ? 0.0f : 1.0f;
			}
		}
	}

	/**
	 * Reproduce the query-height transmittance raster the solver emits at stage (c), by calling the SAME
	 * FArcForestLightModel::ComputeIncidentLight with the SAME argument shape (ArcForestSimulator.cpp:
	 * State arrays, a uniform CrownLAIDensity proxy, Bands from the shipped LayerForHeight, the solver's
	 * 100 cm light cells, and QueryBand passed EXPLICITLY).
	 *
	 * Used only by the shade case, and only to make its claim mechanism-tied: the raster is evaluated at
	 * the state the solver's stage (c) sees (the hand-built stand, before growth), so
	 * EvaluateShadeResponse(Habitat, species, Raster.SampleBilinear(pos)) is the very factor the
	 * acceptance test used.
	 */
	void BuildQueryHeightRaster(
		const FArcForestSolveState& State,
		const FArcForestSimParams& Params,
		FArcForestScalarField& OutRaster)
	{
		TArray<float> CrownLai;
		CrownLai.Init(CrownLaiProxy, State.Num());

		TArray<EArcForestLayer> Bands;
		Bands.Reserve(State.Num());
		for (int32 Index = 0; Index < State.Num(); ++Index)
		{
			Bands.Add(FArcForestLightModel::LayerForHeight(
				State.Height[Index], ShrubMinCm, UnderstoryMinCm, CanopyMinCm));
		}

		const EArcForestLayer QueryBand = FArcForestLightModel::LayerForHeight(
			Params.RecruitQueryHeightCm, ShrubMinCm, UnderstoryMinCm, CanopyMinCm);

		TArray<float> IncidentLight;
		const FArcForestLightModel LightModel;
		LightModel.ComputeIncidentLight(
			State.Pos, State.Height, State.CrownRadius, CrownLai, Bands, Params,
			LightCellSizeCm, IncidentLight,
			&OutRaster, Params.RecruitQueryHeightCm, Params.RecruitStride, QueryBand);
	}
}

TEST_CLASS(ArcForestRecruitment, "ArcPCGExtension.ForestSim.Recruitment")
{
	// ============================================================================================
	// 1. DISABLED REPRODUCES THE BASELINE — bEnableRecruitment == false makes the whole stage, AND the
	//    query-height raster it needs, consume NOTHING: not one RNG draw, not one appended plant.
	//
	// GUARDS: the bRecruitmentActive gate (ArcForestSimulator.cpp StepIterations) and the
	// TransmittancePtr null-selection one line above it.
	//
	// THREE RUNS, and the third is what stops this being a tautology:
	//   A  disabled, every recruit knob at its default.
	//   B  disabled, every recruit knob PERTURBED (rate 0.9, ceiling 100000, stride 1, query height
	//      500 cm, min spacing 7 cm). Element-wise identical to A.
	//   C  A with the single flag flipped ON. NOT identical to A.
	//
	// MUTANT THIS TURNS RED — DELETE THE bEnableRecruitment GATING (recruit unconditionally): A recruits
	// at rate 0.1 and B at rate 0.9, so A != B and the identity assertion flips. A test that only
	// compared A against itself would survive that mutant.
	// MEASURED BOTH SIDES: survivors A = 14, B = 14 (element-wise identical), C = 64. The enabled run is
	// 4.6x the disabled one, so the fixture demonstrably CAN show a difference when the flag is on — the
	// identity above is not holding because nothing was there to change. All three counts are PINNED.
	TEST_METHOD(Recruitment_DisabledReproducesBaseline)
	{
		using namespace ArcForestRecruitmentTestsLocal;

		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeSingleSpecies();

		FArcForestSimParams Disabled;
		Disabled.Seed = 20260726;
		Disabled.SeedingDensity = 2.0f;
		Disabled.Iterations = 8;
		Disabled.Bounds2D = CentredBounds(1500.0f); // 30 m x 30 m

		// Same solve, recruitment still OFF, but every recruitment knob moved off its default. If the
		// disabled path touched the stream or the raster at all, these two would diverge.
		FArcForestSimParams DisabledPerturbed = Disabled;
		DisabledPerturbed.RecruitmentRate = 0.9f;
		DisabledPerturbed.PopulationCeiling = 100000;
		DisabledPerturbed.RecruitStride = 1;
		DisabledPerturbed.RecruitQueryHeightCm = 500.0f;
		DisabledPerturbed.RecruitMinSpacingCm = 7.0f;

		FArcForestSimParams Enabled = Disabled;
		Enabled.bEnableRecruitment = true;

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		const FArcForestResult ResultA = Simulator.Simulate(Disabled, *Def, EmptySuitability, Dims);
		const FArcForestResult ResultB = Simulator.Simulate(DisabledPerturbed, *Def, EmptySuitability, Dims);
		const FArcForestResult ResultC = Simulator.Simulate(Enabled, *Def, EmptySuitability, Dims);

		UE_LOGF(LogArcForestRecruitTest, Display,"[DisabledBaseline] MEASURED survivors: A(off,defaults)=%d B(off,perturbed)=%d C(on)=%d",
			ResultA.Num(), ResultB.Num(), ResultC.Num());

		ASSERT_THAT(IsTrue(ResultA.Num() > 0,
			TEXT("Fixture must produce a stand, else every identity below is vacuous")));
		// EXACT ANCHORS (deterministic: one seed, no yield functor). Pinned rather than left as ">= 1"
		// because a `> 0`-style parent assert leaves the value that actually matters unpinned.
		ASSERT_THAT(IsTrue(ResultA.Num() == 14,
			TEXT("Reference disabled survivor count for this seed/params is 14")));
		ASSERT_THAT(IsTrue(ResultC.Num() == 64,
			TEXT("Reference ENABLED survivor count for the same seed/params is 64 — the recruitment "
				"stream's regression anchor")));
		ASSERT_THAT(IsTrue(ResultsIdentical(ResultA, ResultB),
			TEXT("bEnableRecruitment == false must be an EXACT no-op: moving RecruitmentRate, "
				"PopulationCeiling, RecruitStride, RecruitQueryHeightCm and RecruitMinSpacingCm must not "
				"change one element. RED if the gate is deleted (the two rates then recruit differently)")));
		ASSERT_THAT(IsTrue(!ResultsIdentical(ResultA, ResultC),
			TEXT("Non-vacuity: with the flag ON the same fixture MUST differ from the disabled baseline, "
				"otherwise the identity above would hold no matter what the gate did")));
	}

	// ============================================================================================
	// 2. RECRUITMENT GROWS A SEEDED STAND — enabled, the same seed carries MORE survivors than the
	//    disabled baseline, and the extra plants are the seedling cohort of the final iteration.
	//
	// GUARDS: the append path in RecruitPlants (Pop.Add at seedling height/radius) reached through
	// StepIterations stage (h).
	//
	// Both counts are PINNED EXACTLY (they are deterministic: one seed, one species table, no yield
	// functor), because the Step 8 audit found that a `> 50`-style parent assert leaves the interesting
	// value effectively unpinned. The inequality is asserted too, since that is the claim; the exact
	// pins are what make a silent drift in the dart stream visible.
	//
	// MUTANT THIS TURNS RED — anything that stops recruits being appended (the gate, a zeroed Attempts,
	// an acceptance term stuck at 0): the enabled count collapses onto the disabled one and both the
	// inequality and the exact pin flip.
	// MEASURED BOTH SIDES: survivors 14 (off) vs 64 (on) — a 50-plant gap; final-iteration Age-0 cohort
	// 0 (off) vs 3 (on). THREE of the four are pinned exactly; the on-side final-iteration cohort is
	// asserted only as > 0, because it rides the recruit stream rather than the seeding stream.
	TEST_METHOD(Recruitment_PopulationGrowsFromSeededStand)
	{
		using namespace ArcForestRecruitmentTestsLocal;

		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeSingleSpecies();

		FArcForestSimParams Disabled;
		Disabled.Seed = 20260726;
		Disabled.SeedingDensity = 2.0f;
		Disabled.Iterations = 8;
		Disabled.Bounds2D = CentredBounds(1500.0f);

		FArcForestSimParams Enabled = Disabled;
		Enabled.bEnableRecruitment = true;

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		const FArcForestResult Without = Simulator.Simulate(Disabled, *Def, EmptySuitability, Dims);
		const FArcForestResult With = Simulator.Simulate(Enabled, *Def, EmptySuitability, Dims);

		int32 SeedlingCohortWith = 0;
		int32 SeedlingCohortWithout = 0;
		for (int32 Index = 0; Index < With.Num(); ++Index)
		{
			SeedlingCohortWith += (With.Age[Index] == 0) ? 1 : 0;
		}
		for (int32 Index = 0; Index < Without.Num(); ++Index)
		{
			SeedlingCohortWithout += (Without.Age[Index] == 0) ? 1 : 0;
		}

		UE_LOGF(LogArcForestRecruitTest, Display,"[PopulationGrows] MEASURED survivors off=%d on=%d; finalIterationCohort off=%d on=%d",
			Without.Num(), With.Num(), SeedlingCohortWithout, SeedlingCohortWith);

		ASSERT_THAT(IsTrue(Without.Num() > 0, TEXT("Baseline stand must be non-empty")));
		// EXACT ANCHORS for both sides of the inequality below.
		ASSERT_THAT(IsTrue(Without.Num() == 14 && With.Num() == 64,
			TEXT("Reference survivor counts for this seed/params are 14 (off) and 64 (on)")));
		ASSERT_THAT(IsTrue(With.Num() > Without.Num(),
			TEXT("Recruitment must ADD plants: enabled survivors > disabled survivors at the same seed")));
		ASSERT_THAT(IsTrue(SeedlingCohortWithout == 0,
			TEXT("With recruitment off NOTHING may carry Age 0 after 8 iterations — every seeded plant "
				"has been aged eight times, so a non-zero cohort would mean the gate leaked")));
		ASSERT_THAT(IsTrue(SeedlingCohortWith > 0,
			TEXT("With recruitment on the final iteration must leave an Age-0 seedling cohort")));
	}

	// ============================================================================================
	// 3. THE POPULATION CEILING IS NEVER EXCEEDED — driven one iteration at a time so the count is
	//    inspected after EVERY iteration, not just at the end.
	//
	// GUARDS: Attempts = Min(Budget, Max(0, Ceiling - NumLive)) in RecruitPlants. An aggressive rate
	// (1.0) makes Budget == Ceiling every iteration, so the Min is the ONLY thing holding the line.
	//
	// MUTANT THIS TURNS RED — DROP THE CLAMP (Attempts = Budget, i.e. "return Ceiling unclamped"): with
	// rate 1.0 the solver would throw Ceiling darts every iteration on top of a stand already at the
	// ceiling, so the population overshoots on the very first iteration where live > 0. The per-iteration
	// max asserted below flips.
	// MEASURED BOTH SIDES: seeded 12, and the per-iteration standing counts are
	// 267, 288, 296, 295, 282, 232, 185, 152, 125, 112 against a ceiling of 300 — so the run presses to
	// within 4 plants of the ceiling (296) and never crosses it. Both the maximum and the distance to the
	// ceiling are therefore real: the unclamped mutant throws 300 darts at a stand of 267 and the count
	// leaves the [0, 300] band on iteration 2, while the shipped clamp allows only 300 - 267 = 33.
	// The 296 maximum is PINNED, so a clamp that started allowing even a few extra recruits is visible
	// even before the population would exceed the ceiling.
	TEST_METHOD(Recruitment_RespectsPopulationCeiling)
	{
		using namespace ArcForestRecruitmentTestsLocal;

		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeSingleSpecies();

		constexpr int32 Ceiling = 300;

		FArcForestSimParams Params;
		Params.Seed = 20260726;
		Params.SeedingDensity = 1.0f;
		Params.Iterations = 10;
		Params.Bounds2D = CentredBounds(1500.0f);
		Params.bEnableRecruitment = true;
		Params.PopulationCeiling = Ceiling;
		Params.RecruitmentRate = 1.0f;   // Budget == Ceiling every iteration
		Params.RecruitMinSpacingCm = 40.0f; // loose enough that darts are rarely spacing-rejected

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		FArcForestSolveState State;
		Simulator.SeedInitial(State, Params, *Def, EmptySuitability, Dims);
		const int32 SeededCount = State.Num();

		int32 MaxObserved = SeededCount;
		bool bDone = false;
		while (!bDone)
		{
			bDone = Simulator.StepIterations(State, Params, *Def, EmptySuitability, Dims, /*MaxIters=*/1);
			MaxObserved = FMath::Max(MaxObserved, State.Num());
			UE_LOGF(LogArcForestRecruitTest, Display,"[Ceiling] MEASURED after iteration %d: standing=%d (ceiling=%d)",
				State.Iteration, State.Num(), Ceiling);
		}

		UE_LOGF(LogArcForestRecruitTest, Display,"[Ceiling] MEASURED seeded=%d maxObserved=%d final=%d ceiling=%d",
			SeededCount, MaxObserved, State.Num(), Ceiling);

		ASSERT_THAT(IsTrue(SeededCount > 0 && SeededCount < Ceiling,
			TEXT("Fixture precondition: the seeded stand must start BELOW the ceiling, else Attempts is "
				"0 from the first iteration and the clamp is never exercised")));
		ASSERT_THAT(IsTrue(MaxObserved <= Ceiling,
			TEXT("The live population must NEVER exceed PopulationCeiling at any iteration boundary. RED "
				"if Attempts stops being clamped by (Ceiling - live)")));
		ASSERT_THAT(IsTrue(MaxObserved > SeededCount,
			TEXT("Non-vacuity: recruitment must actually have pushed the stand up toward the ceiling, "
				"otherwise 'never exceeds' would hold trivially")));
		// EXACT ANCHORS. 296 of 300 is the measured high-water mark: close enough that the clamp is
		// genuinely the binding constraint, and pinned so a clamp that loosened by a few plants shows up
		// here rather than only once the population actually overshot.
		ASSERT_THAT(IsTrue(SeededCount == 12,
			TEXT("Reference seeded count for this seed/params is 12")));
		ASSERT_THAT(IsTrue(MaxObserved == 296,
			TEXT("Reference high-water mark is 296 of a 300 ceiling — the clamp's regression anchor")));
	}

	// ============================================================================================
	// 4. ATTEMPTS ARE BOUNDED — A SATURATED STAND PRODUCES ZERO RECRUITS AND STILL RETURNS.
	//
	// GUARDS: the "exactly Attempts darts, no retry loop" contract in RecruitPlants.
	//
	// THE FIXTURE: one live plant at the centre of a 20 m x 20 m footprint and
	// RecruitMinSpacingCm = 100000 cm, which is larger than the whole footprint's diagonal. Every dart
	// is therefore spacing-rejected — the stand is saturated by construction — while Attempts is
	// provably non-zero (asserted from the shipped formula).
	//
	// BOUNDEDNESS IS PROVEN BY COUNTING, NOT BY TIMING, and that is deliberate: the accept-until-quota
	// mutant does not fail slowly, it HANGS (it can never fill a quota on a stand where every position
	// is rejected), so a timing assertion would be a deadlock rather than a red test. The countable
	// consequence is what is asserted: with Attempts > 0 darts thrown and every one rejected, the recruit
	// count must be EXACTLY 0 and the population EXACTLY unchanged. A quota loop cannot produce that
	// pair — it either hangs or, if bounded by some retry cap, produces recruits closer to the quota.
	// NOTE the dart count below is DERIVED from the shipped formula, not measured — ExpectedAttempts is a
	// test-local restatement of it, so asserting Attempts == 199 cannot flip if production dropped its
	// clamp. What actually proves darts were thrown is the control arm: 157 recruits on the identical
	// stream and identical live count is a measured lower bound of 157 real darts.
	// MEASURED BOTH SIDES: 0 recruits at the 100000 cm floor against 157 recruits at a
	// 50 cm floor on the identical fixture and the identical dart stream. 0 vs 157 out of 199 — the
	// darts were genuinely thrown and genuinely acceptable, so the zero is spacing rejection, not an
	// absent stage. All three numbers are pinned.
	TEST_METHOD(Recruitment_AttemptsAreBoundedNoRetryLoop)
	{
		using namespace ArcForestRecruitmentTestsLocal;

		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefaultPair();

		FArcForestSimParams Saturated = MakeRecruitParams(/*HalfSizeCm=*/1000.0f, /*Ceiling=*/200, /*Rate=*/1.0f);
		Saturated.RecruitMinSpacingCm = 100000.0f;

		FHandBuiltStand Stand;
		Stand.Add(FVector2f(0.0f, 0.0f), /*SpeciesIndex=*/0, /*HeightCm=*/1800.0f, /*CrownRadiusCm=*/300.0f);

		const int32 Attempts = ExpectedAttempts(Saturated, Stand.Num());

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		FArcForestSolveState SaturatedState = Stand.State;
		Simulator.StepIterations(SaturatedState, Saturated, *Def, EmptySuitability, Dims, /*MaxIters=*/1);
		const int32 SaturatedRecruits = CountRecruits(SaturatedState);

		// Control arm: the identical fixture with a workable spacing floor. This is what proves the
		// zero above is spacing rejection of REAL darts rather than an absent stage.
		FArcForestSimParams Loose = Saturated;
		Loose.RecruitMinSpacingCm = 50.0f;
		FArcForestSolveState LooseState = Stand.State;
		Simulator.StepIterations(LooseState, Loose, *Def, EmptySuitability, Dims, /*MaxIters=*/1);
		const int32 LooseRecruits = CountRecruits(LooseState);

		UE_LOGF(LogArcForestRecruitTest, Display,"[Bounded] MEASURED attempts=%d saturatedRecruits=%d (spacing 100000 cm) "
				"looseRecruits=%d (spacing 50 cm); standing saturated=%d loose=%d",
			Attempts, SaturatedRecruits, LooseRecruits, SaturatedState.Num(), LooseState.Num());

		ASSERT_THAT(IsTrue(Attempts > 0,
			TEXT("Fixture precondition: the shipped Attempts formula must yield a non-zero dart count, "
				"else 'zero recruits' would prove nothing about the loop")));
		ASSERT_THAT(IsTrue(SaturatedRecruits == 0,
			TEXT("A saturated stand must produce EXACTLY zero recruits from a non-zero dart count: the "
				"loop throws Attempts darts and stops. RED under an accept-until-quota loop, which cannot "
				"reach its quota here at all")));
		ASSERT_THAT(IsTrue(SaturatedState.Num() == Stand.Num(),
			TEXT("The saturated population must be EXACTLY unchanged (no partial appends)")));
		ASSERT_THAT(IsTrue(LooseRecruits > 0,
			TEXT("Control arm: with a workable spacing floor the same darts DO recruit, proving the zero "
				"above is spacing rejection and not a stage that never ran")));
		// EXACT ANCHORS: the dart count from the shipped formula, and the control arm's realised count.
		ASSERT_THAT(IsTrue(Attempts == 199,
			TEXT("Closed form: ceiling 200 at rate 1.0 with 1 live plant gives "
				"Min(Round(1.0*200), 200-1) = 199 darts")));
		ASSERT_THAT(IsTrue(LooseRecruits == 157,
			TEXT("Reference control-arm recruit count is 157 of 199 darts")));
	}

	// ============================================================================================
	// 5. CHUNK-INVARIANCE — THE LOAD-BEARING DETERMINISM CASE.
	//
	// GUARDS: MakeRecruitStream(Params.Seed, State.Iteration + 1). The stream is derived only from
	// values that live in Params/State, never from a per-call cursor, which is the ONLY reason iteration
	// N is handed the same dart sequence whether it ran alone, in a chunk of four, or in one run to
	// completion.
	//
	// This mirrors ResumableEquivalence_StepIterationsEqualsAllAtOnce (ArcForestSimCoreTests.cpp) rather
	// than duplicating it: same three-way drive (all-at-once / MaxIters=1 / MaxIters=4), same
	// element-wise comparison, but with recruitment ON so the recruit stream is inside the compared
	// state. That existing case cannot cover this — bEnableRecruitment defaults false, so it exercises
	// no recruit draw at all.
	//
	// MUTANT THIS TURNS RED — REPLACE THE PER-ITERATION STREAM WITH ONE CARRIED ACROSS ITERATIONS (a
	// stream constructed once per StepIterations CALL and reused for every iteration in that call): the
	// MaxIters=1 drive constructs a fresh stream per iteration and stays equal to production, while the
	// MaxIters=4 and MaxIters=0 drives consume iteration N's darts from a stream already advanced by
	// iterations 1..N-1 within the same call. Those two identity assertions then flip.
	//
	// NOTE which arm carries the kill. Arm A (`Simulate()`) is NOT a distinct chunk size: the completion
	// overload is itself `while (!bDone) bDone = StepIterations(..., MaxIters=1, ...)`, so it is
	// code-path identical to arm B and stays GREEN under the carried-stream mutant. Arm D below is the
	// one that drives production's actual shape — PCGForestSimSettings.cpp calls StepIterations with
	// MaxIters=0, i.e. the whole solve in ONE call, which is the only shape where the per-call
	// FRecruitScratch and RecruitCeiling are reused across all eight iterations. So the mutant is
	// discriminated by arms C (MaxIters=4) and D (MaxIters=0), not by A.
	//
	// This mutant is
	// ARGUED, not executed — no production edit is permitted in this step, and a carried stream is not
	// reachable from any fixture. What the test measures instead is the PRECONDITION that makes the
	// mutant observable: MULTIPLE iterations must each recruit a non-zero count (logged and asserted),
	// because a carried stream is indistinguishable from a per-iteration one when only one iteration
	// ever draws.
	// MEASURED: seeded 11 -> 181 survivors, identical across all three drives, with per-iteration recruit
	// counts 163, 115, 38, 12, 1, 1, 0, 3 — SEVEN of the eight iterations recruit, and iterations 2..8
	// draw from a stream a carried-cursor mutant would have advanced by 163+ draws before reaching them.
	// The three survivor counts and the seven-recruiting-iterations figure are pinned.
	TEST_METHOD(Recruitment_ChunkedEqualsAllAtOnce)
	{
		using namespace ArcForestRecruitmentTestsLocal;

		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeSingleSpecies();

		FArcForestSimParams Params;
		Params.Seed = 5150;
		Params.SeedingDensity = 1.0f;
		Params.Iterations = 8;
		Params.Bounds2D = CentredBounds(1500.0f);
		Params.bEnableRecruitment = true;
		Params.PopulationCeiling = 400;
		Params.RecruitmentRate = 0.5f;
		Params.RecruitMinSpacingCm = 80.0f;

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		// A: the completion overload (no ShouldYield, so nothing is timing-dependent). NOT a distinct
		// chunk size — it loops StepIterations(MaxIters=1) internally, so it duplicates arm B's path.
		// Kept because it is the API most callers reach for, and it must agree with the rest.
		const FArcForestResult AllAtOnce = Simulator.Simulate(Params, *Def, EmptySuitability, Dims);

		// B: one iteration per call, recording the per-iteration recruit count as it goes.
		FArcForestSolveState StateOne;
		Simulator.SeedInitial(StateOne, Params, *Def, EmptySuitability, Dims);
		const int32 SeededCount = StateOne.Num();
		int32 IterationsThatRecruited = 0;
		bool bDoneOne = false;
		while (!bDoneOne)
		{
			bDoneOne = Simulator.StepIterations(StateOne, Params, *Def, EmptySuitability, Dims, /*MaxIters=*/1);
			const int32 Recruited = CountRecruits(StateOne);
			IterationsThatRecruited += (Recruited > 0) ? 1 : 0;
			UE_LOGF(LogArcForestRecruitTest, Display,"[ChunkInvariance] MEASURED iteration %d recruited=%d standing=%d",
				StateOne.Iteration, Recruited, StateOne.Num());
		}
		const FArcForestResult ChunkOfOne = Simulator.Emit(StateOne, *Def);

		// C: four iterations per call.
		FArcForestSolveState StateFour;
		Simulator.SeedInitial(StateFour, Params, *Def, EmptySuitability, Dims);
		bool bDoneFour = false;
		while (!bDoneFour)
		{
			bDoneFour = Simulator.StepIterations(StateFour, Params, *Def, EmptySuitability, Dims, /*MaxIters=*/4);
		}
		const FArcForestResult ChunkOfFour = Simulator.Emit(StateFour, *Def);

		// D: MaxIters=0 — the ENTIRE solve in one StepIterations call. This is what
		// PCGForestSimSettings.cpp drives, and the only shape in which the per-call FRecruitScratch and
		// RecruitCeiling are reused across all eight iterations. Without this arm the identity chain
		// never covers production's own invocation.
		FArcForestSolveState StateAll;
		Simulator.SeedInitial(StateAll, Params, *Def, EmptySuitability, Dims);
		const bool bDoneAll =
			Simulator.StepIterations(StateAll, Params, *Def, EmptySuitability, Dims, /*MaxIters=*/0);
		const FArcForestResult AllInOneCall = Simulator.Emit(StateAll, *Def);

		UE_LOGF(LogArcForestRecruitTest, Display,"[ChunkInvariance] MEASURED seeded=%d allAtOnce=%d chunkOf1=%d chunkOf4=%d "
				"oneCall=%d iterationsThatRecruited=%d",
			SeededCount, AllAtOnce.Num(), ChunkOfOne.Num(), ChunkOfFour.Num(), AllInOneCall.Num(),
			IterationsThatRecruited);

		ASSERT_THAT(IsTrue(AllAtOnce.Num() > SeededCount,
			TEXT("Non-vacuity: recruitment must have grown the stand past its seeded size, else the "
				"identities below would hold for a solve that never drew a recruit dart")));
		ASSERT_THAT(IsTrue(IterationsThatRecruited >= 2,
			TEXT("Non-vacuity: at least TWO iterations must each have recruited, which is what makes a "
				"cross-iteration (carried) stream observable at all")));
		// EXACT ANCHORS: the seeded size, the survivor count all three drives must agree on, and how many
		// iterations actually drew recruit darts (7 of 8 in the reference run).
		ASSERT_THAT(IsTrue(SeededCount == 11 && AllAtOnce.Num() == 181,
			TEXT("Reference run: 11 seeded, 181 survivors after 8 recruiting iterations")));
		ASSERT_THAT(IsTrue(IterationsThatRecruited == 7,
			TEXT("Reference run: 7 of the 8 iterations recruited (163, 115, 38, 12, 1, 1, 0, 3)")));
		ASSERT_THAT(IsTrue(ResultsIdentical(AllAtOnce, ChunkOfOne),
			TEXT("Seed + StepIterations(MaxIters=1)* + Emit must be element-wise identical to Simulate() "
				"WITH RECRUITMENT ON: the recruit stream is (Seed, salt, Iteration+1) and holds no cursor")));
		ASSERT_THAT(IsTrue(ResultsIdentical(AllAtOnce, ChunkOfFour),
			TEXT("Chunk-size independence: MaxIters=4 must also be element-wise identical. RED if the "
				"per-iteration stream is replaced by one carried across iterations within a call")));
		ASSERT_THAT(IsTrue(bDoneAll,
			TEXT("MaxIters=0 must complete the whole solve in ONE call, else arm D is not the shape "
				"PCGForestSimSettings.cpp drives and proves nothing about it")));
		ASSERT_THAT(IsTrue(AllInOneCall.Num() == AllAtOnce.Num(),
			TEXT("Arm D non-vacuity: the single-call drive must reach the same survivor count before its "
				"element-wise identity is read")));
		ASSERT_THAT(IsTrue(ResultsIdentical(AllAtOnce, AllInOneCall),
			TEXT("PRODUCTION'S OWN SHAPE: StepIterations(MaxIters=0) — the entire solve in one call, as "
				"PCGForestSimSettings.cpp drives it — must be element-wise identical. This is the arm that "
				"kills the carried-stream mutant, since arm A is itself a MaxIters=1 loop")));
	}

	// ============================================================================================
	// 6. THE SHADE CURVE DRIVES PLACEMENT — mechanism-tied and RNG-free, per the plan's (c).
	//
	// GUARDS: the EvaluateShadeResponse factor of the acceptance product, and the bilinear read of the
	// stage-(c) query-height raster feeding it.
	//
	// THE STAND IS HAND-BUILT, not seeded, and it is built around ONE hard numeric constraint that any
	// "obvious" canopy fixture violates: the spacing rule rejects a dart within
	// SpacingFraction * (SeedlingCrownRadiusCm + neighbour's LIVE radius) = 0.8*R + 4 of a standing
	// plant, while the shade it casts only reaches R. With ordinary 450 cm crowns the rejection radius is
	// 364 cm, leaving only a ~22%-of-disc admissible ring per tree — and in a closed lattice the
	// neighbours' rejection discs overlap that ring, so such a fixture admits few or no understory darts
	// and the assertions below would be at risk of vacuity. The fixture therefore uses ONE
	// very wide crown cluster — THREE co-located plants at (-2500, 0), each 2500 cm radius — so that:
	//     dark zone (LAI 3*2 = 6, t = exp(-0.5*6) = 0.0498)   reaches out to 2500 cm
	//     spacing rejection (0.8 * (5 + 2500))                reaches out to 2004 cm
	// leaving a wide DARK AND ADMISSIBLE annulus at 2004..~2450 cm, while everything beyond 2500 cm sits
	// at transmittance EXACTLY 1.0. RecruitStride is 1 so the raster is cell-exact (no 4 m aggregation
	// smearing the disc edge).
	//
	// THE CURVES ARE STEPS, and that is what makes this RNG-free. DarkLover's preference is 1.0 for
	// transmittance <= 0.15 and 0.0 from 0.25 up; OpenLover's is 0.0 up to 0.75 and 1.0 from 0.85 up.
	// Habitat and suitability are both exactly 1.0 here, so AcceptProb is exactly 1.0 or exactly 0.0
	// across almost the whole footprint, and FRand() in [0,1) turns that into "accept every dart" or
	// "accept none". In the narrow transition band each species' preference is strictly between 0 and 1,
	// so acceptance THERE is probabilistic rather than masked — no assertion depends on that band, since
	// both bounds below sit outside it on the exactly-0.0 side. The claim is therefore an exact
	// SET-MEMBERSHIP claim about transmittance, not a count inequality that could drift.
	//
	// THE ASSERTION IS ON TRANSMITTANCE, NOT ON A GEOMETRIC REGION, deliberately: transmittance is the
	// curve's actual input, so "recruits preferentially under an existing canopy" is stated as "every
	// DarkLover recruit sits where the light is low" with no dependence on how the fixture's discs
	// happen to rasterise. The raster is reproduced from the same stand through the same
	// FArcForestLightModel::ComputeIncidentLight call the solver makes at stage (c), so the sampled
	// value IS the factor the acceptance test used. The positional claim is asserted too, as the mean-X
	// ordering, since that is the ecological statement a reader cares about.
	//
	// THRESHOLD SLACK, and why it is not fudge: the flattened LUT samples the curve at multiples of
	// 1/63 = 0.0159 and interpolates linearly, so the effective zero-crossing of each step can sit up to
	// one sample off the authored knee (0.25 -> at most ~0.254; 0.75 -> at least ~0.746). The asserted
	// bounds are 0.30 and 0.70, one further sample clear of that.
	//
	// MEASURED, BOTH SIDES, IN THE REFERENCE RUN:
	//   authored  dark = 23, open = 237; WORST dark transmittance = 0.1428 (bound 0.30), WORST open
	//             transmittance = 1.0000 (bound 0.70); recruits on the wrong side of their knee: 0 and 0;
	//             mean X: dark -2497.8 (the crown sits at -2500), open +940.7.
	//   mutant    dark = 253, open = 111; recruits on the wrong side of their knee: 230 and 13.
	// So the shipped side is 0 and 0 against a mutant side of 230 and 13, and the worst measured
	// transmittance is a full 0.157 clear of its bound. There is no threshold here that the mutant
	// could pass.
	//
	// MUTANT THIS TURNS RED — DELETE THE SHADE FACTOR (or ignore the curve): AcceptProb becomes 1.0
	// everywhere and both species scatter over the whole footprint, so DarkLover lands in full light and
	// OpenLover lands in the dark annulus.
	// MEASURED BOTH SIDES, IN THIS RUN: the control arm re-runs the identical stand with UNAUTHORED
	// curves, which is exactly what deleting the factor does (EvaluateShadeResponse returns exactly 1.0
	// for bAuthored == false). Its counts of recruits on the WRONG side of each knee are logged and
	// asserted non-zero, while the authored run's are asserted to be exactly zero.
	TEST_METHOD(Recruitment_ShadeCurveDrivesPlacement)
	{
		using namespace ArcForestRecruitmentTestsLocal;

		// sp0 DARKLOVER — seedlings establish only in deep shade. sp1 OPENLOVER — only in the open.
		TArray<FArcForestSpeciesEntry> Table;
		FArcForestSpeciesEntry DarkLover = MakeSpecies(TEXT("DarkLover"), EArcForestLayer::Understory,
			/*MaxHeight=*/900.0f, /*MaxCrownRadius=*/250.0f, /*GrowthRate=*/0.08f,
			/*TargetDensity=*/0.05f, /*PatchRadiusSigma=*/300.0f, /*PatchDensity=*/0.05f);
		const FVector2f DarkKeys[] = {
			FVector2f(0.0f, 1.0f), FVector2f(0.15f, 1.0f), FVector2f(0.25f, 0.0f), FVector2f(1.0f, 0.0f) };
		AuthorLinearCurve(DarkLover.ShadeResponseCurve, DarkKeys);
		Table.Add(MoveTemp(DarkLover));

		FArcForestSpeciesEntry OpenLover = MakeSpecies(TEXT("OpenLover"), EArcForestLayer::Canopy,
			/*MaxHeight=*/3000.0f, /*MaxCrownRadius=*/2500.0f, /*GrowthRate=*/0.14f,
			/*TargetDensity=*/0.05f, /*PatchRadiusSigma=*/300.0f, /*PatchDensity=*/0.05f);
		const FVector2f OpenKeys[] = {
			FVector2f(0.0f, 0.0f), FVector2f(0.75f, 0.0f), FVector2f(0.85f, 1.0f), FVector2f(1.0f, 1.0f) };
		AuthorLinearCurve(OpenLover.ShadeResponseCurve, OpenKeys);
		Table.Add(MoveTemp(OpenLover));

		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));

		// The same table with the curves stripped — the "shade factor deleted" control.
		TArray<FArcForestSpeciesEntry> PlainTable = Def->Species;
		PlainTable[0].ShadeResponseCurve.GetRichCurve()->Reset();
		PlainTable[1].ShadeResponseCurve.GetRichCurve()->Reset();
		TStrongObjectPtr<UArcForestSpeciesDefinition> PlainDef = MakeDefinition(MoveTemp(PlainTable));

		FArcForestSimParams Params = MakeRecruitParams(/*HalfSizeCm=*/5000.0f, /*Ceiling=*/1000, /*Rate=*/1.0f);
		Params.RecruitMinSpacingCm = 300.0f;
		Params.RecruitStride = 1; // cell-exact raster: no 4 m aggregation across the crown edge

		// THREE co-located wide crowns: LAI 6 inside 2500 cm -> t = exp(-3) = 0.0498, and the shade
		// reaches 496 cm beyond the 2004 cm spacing-rejection radius (see the header note).
		const FVector2f CanopyPos(-2500.0f, 0.0f);
		FHandBuiltStand Stand;
		for (int32 Layer = 0; Layer < 3; ++Layer)
		{
			Stand.Add(CanopyPos, /*SpeciesIndex=*/1, /*HeightCm=*/2000.0f, /*CrownRadiusCm=*/2500.0f);
		}

		// Flatten the authored curves exactly as the game thread does before the solve launches.
		FArcForestHabitatInputs Habitat;
		Habitat.MoistureNoise.Frequency = 0.0f; // silence the live default; no FieldResponses read it anyway
		ArcForestSpeciesHabitat::FlattenFieldResponses(*Def, Habitat);

		FArcForestHabitatInputs PlainHabitat;
		PlainHabitat.MoistureNoise.Frequency = 0.0f;
		ArcForestSpeciesHabitat::FlattenFieldResponses(*PlainDef, PlainHabitat);

		ASSERT_THAT(IsTrue(Habitat.SpeciesShadeResponses.Num() == 2
			&& Habitat.SpeciesShadeResponses[0].bAuthored
			&& Habitat.SpeciesShadeResponses[1].bAuthored,
			TEXT("Fixture precondition: both shade curves must flatten as AUTHORED, else the acceptance "
				"product silently loses its shade factor and this test proves nothing")));
		ASSERT_THAT(IsTrue(PlainHabitat.SpeciesShadeResponses.Num() == 2
			&& !PlainHabitat.SpeciesShadeResponses[0].bAuthored
			&& !PlainHabitat.SpeciesShadeResponses[1].bAuthored,
			TEXT("Control precondition: the stripped table must flatten as UNAUTHORED, which is what "
				"makes it the faithful 'shade factor deleted' mutant")));

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		FArcForestSolveState State = Stand.State;
		Simulator.StepIterations(State, Params, *Def, EmptySuitability, Dims, /*MaxIters=*/1, Habitat);

		FArcForestSolveState PlainState = Stand.State;
		Simulator.StepIterations(PlainState, Params, *PlainDef, EmptySuitability, Dims, /*MaxIters=*/1, PlainHabitat);

		// Reproduce the raster the solver read at stage (c) — the same call, on the same pre-growth stand.
		FArcForestScalarField Raster;
		BuildQueryHeightRaster(Stand.State, Params, Raster);

		TArray<FVector2f> RecruitPos;
		TArray<int32> RecruitSpecies;
		CollectRecruits(State, RecruitPos, RecruitSpecies);

		TArray<FVector2f> PlainRecruitPos;
		TArray<int32> PlainRecruitSpecies;
		CollectRecruits(PlainState, PlainRecruitPos, PlainRecruitSpecies);

		// The two knees, each pushed one further LUT sample clear of the authored value (see the header).
		constexpr float DarkKneeBound = 0.30f;
		constexpr float OpenKneeBound = 0.70f;

		int32 DarkCount = 0;
		int32 OpenCount = 0;
		int32 DarkAboveKnee = 0;   // DarkLover recruits in light its curve forbids
		int32 OpenBelowKnee = 0;   // OpenLover recruits in shade its curve forbids
		int32 ZeroPreference = 0;
		float MaxDarkTransmittance = -1.0f;
		float MinOpenTransmittance = TNumericLimits<float>::Max();
		double DarkSumX = 0.0;
		double OpenSumX = 0.0;
		for (int32 Index = 0; Index < RecruitPos.Num(); ++Index)
		{
			const float Transmittance = Raster.SampleBilinear(RecruitPos[Index]);
			const float Preference = ArcForestField::EvaluateShadeResponse(
				Habitat, RecruitSpecies[Index], Transmittance);
			ZeroPreference += (Preference <= 0.0f) ? 1 : 0;
			if (RecruitSpecies[Index] == 0)
			{
				++DarkCount;
				DarkSumX += static_cast<double>(RecruitPos[Index].X);
				MaxDarkTransmittance = FMath::Max(MaxDarkTransmittance, Transmittance);
				DarkAboveKnee += (Transmittance > DarkKneeBound) ? 1 : 0;
			}
			else
			{
				++OpenCount;
				OpenSumX += static_cast<double>(RecruitPos[Index].X);
				MinOpenTransmittance = FMath::Min(MinOpenTransmittance, Transmittance);
				OpenBelowKnee += (Transmittance < OpenKneeBound) ? 1 : 0;
			}
		}

		int32 PlainDarkCount = 0;
		int32 PlainOpenCount = 0;
		int32 PlainDarkAboveKnee = 0;
		int32 PlainOpenBelowKnee = 0;
		for (int32 Index = 0; Index < PlainRecruitPos.Num(); ++Index)
		{
			const float Transmittance = Raster.SampleBilinear(PlainRecruitPos[Index]);
			if (PlainRecruitSpecies[Index] == 0)
			{
				++PlainDarkCount;
				PlainDarkAboveKnee += (Transmittance > DarkKneeBound) ? 1 : 0;
			}
			else
			{
				++PlainOpenCount;
				PlainOpenBelowKnee += (Transmittance < OpenKneeBound) ? 1 : 0;
			}
		}

		const float DarkMeanX = (DarkCount > 0) ? static_cast<float>(DarkSumX / DarkCount) : 0.0f;
		const float OpenMeanX = (OpenCount > 0) ? static_cast<float>(OpenSumX / OpenCount) : 0.0f;

		UE_LOGF(LogArcForestRecruitTest, Display,"[ShadeCurve] MEASURED authored: dark=%d open=%d darkAbove%.2f=%d openBelow%.2f=%d "
				"zeroPreference=%d maxDarkT=%.4f minOpenT=%.4f meanX dark=%.1f open=%.1f",
			DarkCount, OpenCount, DarkKneeBound, DarkAboveKnee, OpenKneeBound, OpenBelowKnee,
			ZeroPreference, MaxDarkTransmittance, MinOpenTransmittance, DarkMeanX, OpenMeanX);
		UE_LOGF(LogArcForestRecruitTest, Display,"[ShadeCurve] MEASURED control (curves stripped == shade factor deleted): dark=%d "
				"open=%d darkAbove%.2f=%d openBelow%.2f=%d",
			PlainDarkCount, PlainOpenCount, DarkKneeBound, PlainDarkAboveKnee,
			OpenKneeBound, PlainOpenBelowKnee);

		ASSERT_THAT(IsTrue(DarkCount > 0 && OpenCount > 0,
			TEXT("Non-vacuity: BOTH species must recruit, else a zero count would satisfy every "
				"membership assertion below for free")));
		ASSERT_THAT(IsTrue(DarkAboveKnee == 0,
			TEXT("A species whose shade preference is 0 above transmittance 0.25 must not place ONE "
				"seedling in light above 0.30 — the shade factor is the only term that can reject it")));
		ASSERT_THAT(IsTrue(OpenBelowKnee == 0,
			TEXT("A species whose preference is 0 below transmittance 0.75 must not place ONE seedling "
				"in shade below 0.70")));
		ASSERT_THAT(IsTrue(ZeroPreference == 0,
			TEXT("Mechanism tie: every recruit's own EvaluateShadeResponse at its own position, read "
				"from the same stage-(c) raster, must be strictly positive — acceptance is "
				"DrawAccept < AcceptProb and FRand() is never negative")));
		ASSERT_THAT(IsTrue(DarkMeanX < OpenMeanX,
			TEXT("The ecological statement: with the canopy on the left, the shade-preferring species' "
				"seedlings sit further left than the gap-preferring species'")));
		// EXACT ANCHORS for both cohorts and for the mutant side's wrong-region counts.
		ASSERT_THAT(IsTrue(DarkCount == 23 && OpenCount == 237,
			TEXT("Reference cohorts: 23 shade-preferring and 237 gap-preferring recruits")));
		ASSERT_THAT(IsTrue(PlainDarkAboveKnee == 230 && PlainOpenBelowKnee == 13,
			TEXT("Reference mutant side: 230 shade-preferring recruits in light above 0.30 and 13 "
				"gap-preferring recruits in shade below 0.70")));
		ASSERT_THAT(IsTrue(PlainDarkAboveKnee > 0 && PlainOpenBelowKnee > 0,
			TEXT("Measured mutant side: with the curves stripped (exactly what deleting the shade factor "
				"does) BOTH species DO land on the wrong side of their own knee, so the two zero "
				"assertions above are discriminating, not vacuous")));
	}

	// ============================================================================================
	// 7. THE MINIMUM SPACING FLOOR PREVENTS CLOTS — recruit against recruit, within one iteration.
	//
	// GUARDS: the MinSpacingFloor term of Max(RecruitMinSpacingCm, SpacingFraction * (SeedlingCrownRadius
	// + neighbour's LIVE CrownRadius)) and the insertion of same-iteration recruits into the hash as they
	// are accepted (without that insertion recruits are only tested against the standing stand and can
	// stack on each other).
	//
	// Recruit-vs-recruit the radius term is only 0.8 * (5 + 5) = 8 cm, so the floor is the entire rule
	// here: at RecruitMinSpacingCm = 300 the closest two recruits must be >= 300 cm apart.
	//
	// MUTANT THIS TURNS RED — DROP THE FLOOR (or fail to insert same-iteration recruits): the minimum
	// pairwise distance collapses to the 8 cm radius rule (or to ~0 without the insertion).
	// MEASURED BOTH SIDES: at a 300 cm floor the run places 89 recruits whose closest pair is
	// 300.219 cm apart; with the floor at 0 the identical fixture and identical dart stream place 395
	// recruits whose closest pair is 9.833 cm apart (the 0.8*(5+5) = 8 cm radius rule plus the two
	// darts' own separation). 300.219 vs 9.833 is a factor of 30 — the assertion is nowhere near either
	// side — and 89 vs 395 is the clot itself. All four numbers are pinned.
	TEST_METHOD(Recruitment_MinSpacingPreventsClots)
	{
		using namespace ArcForestRecruitmentTestsLocal;

		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefaultPair();

		constexpr float MinSpacingCm = 300.0f;

		FArcForestSimParams Params = MakeRecruitParams(/*HalfSizeCm=*/2000.0f, /*Ceiling=*/400, /*Rate=*/1.0f);
		Params.RecruitMinSpacingCm = MinSpacingCm;

		// One small standing plant, far from the centre of mass of the darts, so the pairwise statistic
		// below is dominated by recruit-vs-recruit pairs (the case the floor exists for).
		FHandBuiltStand Stand;
		Stand.Add(FVector2f(-1900.0f, -1900.0f), /*SpeciesIndex=*/0, /*HeightCm=*/1800.0f, /*CrownRadiusCm=*/20.0f);

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		FArcForestSolveState State = Stand.State;
		Simulator.StepIterations(State, Params, *Def, EmptySuitability, Dims, /*MaxIters=*/1);

		TArray<FVector2f> RecruitPos;
		TArray<int32> RecruitSpecies;
		CollectRecruits(State, RecruitPos, RecruitSpecies);
		const float MinDistance = MinPairwiseDistance(RecruitPos);

		// Control arm: the same fixture with the floor removed — the measured mutant side.
		FArcForestSimParams NoFloor = Params;
		NoFloor.RecruitMinSpacingCm = 0.0f;
		FArcForestSolveState NoFloorState = Stand.State;
		Simulator.StepIterations(NoFloorState, NoFloor, *Def, EmptySuitability, Dims, /*MaxIters=*/1);
		TArray<FVector2f> NoFloorPos;
		TArray<int32> NoFloorSpecies;
		CollectRecruits(NoFloorState, NoFloorPos, NoFloorSpecies);
		const float NoFloorMinDistance = MinPairwiseDistance(NoFloorPos);

		UE_LOGF(LogArcForestRecruitTest, Display,"[MinSpacing] MEASURED floor=%.1f recruits=%d minPairDist=%.3f | control floor=0: "
				"recruits=%d minPairDist=%.3f",
			MinSpacingCm, RecruitPos.Num(), MinDistance, NoFloorPos.Num(), NoFloorMinDistance);

		ASSERT_THAT(IsTrue(RecruitPos.Num() >= 2,
			TEXT("Non-vacuity: at least two recruits are needed for a pairwise-distance claim")));
		ASSERT_THAT(IsTrue(NoFloorPos.Num() >= 2,
			TEXT("Non-vacuity: the control arm needs at least two recruits too")));
		// The rule rejects on SizeSquared() < Square(MinDist), so an accepted pair is exactly at or above
		// MinDist. Compared with a 1e-3 relative slack purely against float rounding in the sqrt above.
		ASSERT_THAT(IsTrue(MinDistance >= MinSpacingCm * (1.0f - 1e-3f),
			TEXT("No two same-iteration recruits may sit closer than RecruitMinSpacingCm. RED if the "
				"floor is dropped (the rule falls back to 0.8*(5+5) = 8 cm) or if same-iteration recruits "
				"are not inserted into the spacing hash")));
		ASSERT_THAT(IsTrue(NoFloorMinDistance < MinSpacingCm,
			TEXT("Measured mutant side: with the floor at 0 the closest recruit pair IS closer than the "
				"floor, so the assertion above is discriminating")));
		ASSERT_THAT(IsTrue(NoFloorPos.Num() > RecruitPos.Num(),
			TEXT("Measured mutant side: dropping the floor lets more recruits pack into the same area — "
				"the clot the floor exists to prevent")));
		// EXACT ANCHORS for both cohorts (the distances stay inequalities — see the note above).
		ASSERT_THAT(IsTrue(RecruitPos.Num() == 89 && NoFloorPos.Num() == 395,
			TEXT("Reference counts: 89 recruits at a 300 cm floor, 395 with the floor dropped")));
	}

	// ============================================================================================
	// 8. EMITTED SEEDLINGS ARE SANE — the cohort a caller now finds in every enabled output.
	//
	// GUARDS: the Pop.Add call in RecruitPlants
	// (DartPos, SpeciesIndex, SeedlingCrownRadiusCm, SeedlingHeightCm, INDEX_NONE) and stage (h)'s
	// position AFTER the ageing loop, which is what makes a final-iteration recruit Age 0 having faced
	// zero mortality.
	//
	// Asserted against the NAMED constants ArcForestSim::SeedlingHeightCm / SeedlingCrownRadiusCm
	// (ArcForestSimTypes.h), never against 10.0f / 5.0f literals: those constants are now the single
	// authority shared with FArcForestSimParams::RecruitQueryHeightCm's default and the spacing rule, so
    // a literal here would silently stop tracking the thing under test.
	//
	// MUTANT THIS TURNS RED — recruits born at a species' mature height/radius, or with an inherited
	// PatchId, or appended BEFORE the ageing loop (which would give them Age 1): each of the four
	// exact-equality assertions below flips independently.
	// MEASURED BOTH SIDES: 2 standing plants and 160 recruits; badHeight/badRadius/badPatchId/badVigor
	// all 0, and on the other side of the same predicate standingWrongAge = 0 and
	// standingAtSeedlingHeight = 0. The pre-existing plants therefore satisfy NONE of the four seedling
	// equalities, so those equalities are a genuine discriminator rather than something every plant in
	// the state happens to meet.
	TEST_METHOD(Recruitment_EmittedSeedlingsAreSane)
	{
		using namespace ArcForestRecruitmentTestsLocal;

		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefaultPair();

		FArcForestSimParams Params = MakeRecruitParams(/*HalfSizeCm=*/2000.0f, /*Ceiling=*/200, /*Rate=*/1.0f);

		FHandBuiltStand Stand;
		Stand.Add(FVector2f(-1500.0f, -1500.0f), /*SpeciesIndex=*/0, /*HeightCm=*/1800.0f, /*CrownRadiusCm=*/300.0f);
		Stand.Add(FVector2f(1500.0f, 1500.0f), /*SpeciesIndex=*/1, /*HeightCm=*/380.0f, /*CrownRadiusCm=*/100.0f);
		const int32 StandingCount = Stand.Num();

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		FArcForestSolveState State = Stand.State;
		Simulator.StepIterations(State, Params, *Def, EmptySuitability, Dims, /*MaxIters=*/1);
		const FArcForestResult Result = Simulator.Emit(State, *Def);

		int32 RecruitCount = 0;
		int32 BadHeight = 0;
		int32 BadRadius = 0;
		int32 BadPatchId = 0;
		int32 BadVigor = 0;
		int32 StandingWithWrongAge = 0;
		int32 StandingAtSeedlingHeight = 0;
		for (int32 Index = 0; Index < Result.Num(); ++Index)
		{
			if (Result.Age[Index] == 0)
			{
				++RecruitCount;
				BadHeight += (Result.Height[Index] != ArcForestSim::SeedlingHeightCm) ? 1 : 0;
				BadRadius += (Result.CrownRadius[Index] != ArcForestSim::SeedlingCrownRadiusCm) ? 1 : 0;
				BadPatchId += (Result.PatchId[Index] != INDEX_NONE) ? 1 : 0;
				BadVigor += (Result.Vigor[Index] != 1.0f) ? 1 : 0;
			}
			else
			{
				StandingWithWrongAge += (Result.Age[Index] != 1) ? 1 : 0;
				StandingAtSeedlingHeight +=
					(Result.Height[Index] <= ArcForestSim::SeedlingHeightCm) ? 1 : 0;
			}
		}

		UE_LOGF(LogArcForestRecruitTest, Display,"[SeedlingsSane] MEASURED standing=%d recruits=%d badHeight=%d badRadius=%d "
				"badPatchId=%d badVigor=%d standingWrongAge=%d standingAtSeedlingHeight=%d "
				"(seedling h=%.2f r=%.2f)",
			StandingCount, RecruitCount, BadHeight, BadRadius, BadPatchId, BadVigor,
			StandingWithWrongAge, StandingAtSeedlingHeight,
			ArcForestSim::SeedlingHeightCm, ArcForestSim::SeedlingCrownRadiusCm);

		ASSERT_THAT(IsTrue(RecruitCount > 0, TEXT("Non-vacuity: the iteration must have recruited")));
		ASSERT_THAT(IsTrue(RecruitCount == 160,
			TEXT("Reference recruit count for this fixture is 160 (of 198 darts)")));
		ASSERT_THAT(IsTrue(Result.Num() == StandingCount + RecruitCount,
			TEXT("Nothing may die in a single iteration of this fixture, so the emitted count is exactly "
				"the standing stand plus the recruits")));
		ASSERT_THAT(IsTrue(BadHeight == 0,
			TEXT("Every recruit's Height must equal ArcForestSim::SeedlingHeightCm EXACTLY")));
		ASSERT_THAT(IsTrue(BadRadius == 0,
			TEXT("Every recruit's CrownRadius must equal ArcForestSim::SeedlingCrownRadiusCm EXACTLY")));
		ASSERT_THAT(IsTrue(BadPatchId == 0,
			TEXT("Every recruit's PatchId must be INDEX_NONE — a recruit cannot mint or inherit a "
				"conspecific patch id")));
		ASSERT_THAT(IsTrue(BadVigor == 0,
			TEXT("Every recruit's Vigor must be exactly 1.0: stage (h) runs after mortality and after "
				"UpdateVigor, so a recruit faces neither in the iteration it is born in")));
		ASSERT_THAT(IsTrue(StandingWithWrongAge == 0,
			TEXT("Discriminator check: every pre-existing plant must be at Age 1 after one iteration, "
				"which is what makes Age == 0 an exact recruit predicate")));
		ASSERT_THAT(IsTrue(StandingAtSeedlingHeight == 0,
			TEXT("Discriminator check: no pre-existing plant may sit at or below the seedling height, "
				"else the height assertion above could pass on the standing stand")));
	}

	// ============================================================================================
	// 9. SUITABILITY GATES RECRUITS — THE TEST THAT PINS COMMIT e0397ac72380.
	//
	// GUARDS: the SampleFieldNearest(DartPos, Bounds, SuitabilityGrid, GridDims) factor of the
	// acceptance product. That factor was ABSENT as first landed (48384f9cb719) and its absence was the
	// blocking defect: EvaluateHabitatResponse returns exactly 1.0 for a species with no FieldResponses
	// and EvaluateShadeResponse returns exactly 1.0 for an unauthored ShadeResponseCurve, and BOTH are
	// the shipping authoring defaults — so with the DEFAULT species table used here the acceptance
	// product was exactly 1.0 at every dart and every dart was accepted uniformly over Bounds2D,
	// including Density-masked cells and every ProjectPoint miss. Nothing downstream filters it: Emit is
	// a straight SoA copy, the PCG emit loop filters nothing, mortality never reads position, and a
	// recruit added on the final iteration faces no mortality at all.
	//
	// THE FIXTURE: a suitability grid that is 1.0 for X < 0 and EXACTLY 0.0 for X > 0 over a
	// 100 m x 100 m footprint, at the node's own 200 cm cell size (so the grid boundary falls exactly on
	// X = 0). SampleFieldNearest therefore answers 0.0 for every dart with X >= 0, AcceptProb is exactly
	// 0.0 there, and DrawAccept >= 0 always — a hard mask, no probability involved. The species table is
	// the DEFAULT one on purpose: it is the unconditional case, the one the missing factor made
	// catastrophic.
	//
	// MUTANT THIS TURNS RED — DELETE THE SUITABILITY FACTOR (i.e. revert e0397ac72380): AcceptProb
	// returns to exactly 1.0 across the whole footprint and recruits appear in the masked half.
	// MEASURED BOTH SIDES: with the mask, 257 recruits and EXACTLY 0 of them in the masked half. With the
	// identical fixture and a UNIFORM all-ones grid — precisely what deleting the factor computes, since
	// SampleFieldNearest on an all-ones grid returns 1.0 — 539 recruits of which 282 land in that same
	// half. 0 vs 282 out of 598 darts — Budget = RoundToInt(1.0*600) = 600, Attempts = Min(600, 600-2) =
	// 598. Both recruit counts are pinned; the dart count is derived from the formula, not measured.
	TEST_METHOD(Recruitment_SuitabilityMaskBlocksRecruits)
	{
		using namespace ArcForestRecruitmentTestsLocal;

		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefaultPair();

		FArcForestSimParams Params = MakeRecruitParams(/*HalfSizeCm=*/5000.0f, /*Ceiling=*/600, /*Rate=*/1.0f);

		const FIntPoint GridDims =
			ArcForestField::ComputeGridDims(Params.Bounds2D, SuitabilityCellSizeCm, MaxGridDim);

		TArray<float> MaskedGrid;
		BuildRightHalfMaskedSuitability(Params.Bounds2D, GridDims, MaskedGrid);
		TArray<float> UniformGrid;
		BuildUniformSuitability(GridDims, UniformGrid);

		// Two standing plants, both inside the SUITABLE half so they cannot be mistaken for recruits and
		// their spacing rejection cannot bias the masked-half count.
		FHandBuiltStand Stand;
		Stand.Add(FVector2f(-4000.0f, -4000.0f), /*SpeciesIndex=*/0, /*HeightCm=*/1800.0f, /*CrownRadiusCm=*/300.0f);
		Stand.Add(FVector2f(-4000.0f, 4000.0f), /*SpeciesIndex=*/1, /*HeightCm=*/380.0f, /*CrownRadiusCm=*/100.0f);

		const FArcForestSimulator Simulator;

		FArcForestSolveState MaskedState = Stand.State;
		Simulator.StepIterations(MaskedState, Params, *Def, MaskedGrid, GridDims, /*MaxIters=*/1);
		TArray<FVector2f> MaskedRecruits;
		TArray<int32> MaskedSpecies;
		CollectRecruits(MaskedState, MaskedRecruits, MaskedSpecies);

		FArcForestSolveState UniformState = Stand.State;
		Simulator.StepIterations(UniformState, Params, *Def, UniformGrid, GridDims, /*MaxIters=*/1);
		TArray<FVector2f> UniformRecruits;
		TArray<int32> UniformSpecies;
		CollectRecruits(UniformState, UniformRecruits, UniformSpecies);

		const int32 MaskedInHole = CountAtOrRightOf(MaskedRecruits, 0.0f);
		const int32 UniformInHole = CountAtOrRightOf(UniformRecruits, 0.0f);

		UE_LOGF(LogArcForestRecruitTest, Display,"[SuitabilityGate] MEASURED dims=%dx%d masked: recruits=%d inMaskedHalf=%d | "
				"control (uniform grid == factor deleted): recruits=%d inMaskedHalf=%d",
			GridDims.X, GridDims.Y, MaskedRecruits.Num(), MaskedInHole,
			UniformRecruits.Num(), UniformInHole);

		ASSERT_THAT(IsTrue(GridDims.X == 50 && GridDims.Y == 50,
			TEXT("Fixture precondition: a 10000 cm footprint at 200 cm cells must give 50x50 cells, so "
				"the mask boundary falls exactly on X = 0 and 'masked' is an exact membership test")));
		ASSERT_THAT(IsTrue(MaskedRecruits.Num() > 0,
			TEXT("Non-vacuity: the suitable half must still recruit, else 'nothing in the hole' would "
				"hold because nothing recruited at all")));
		ASSERT_THAT(IsTrue(MaskedInHole == 0,
			TEXT("NOT ONE recruit may land where suitability is 0. RED if the suitability factor is "
				"removed from the acceptance product (commit e0397ac72380 reverted) — with the default "
				"species table habitat and shade are both exactly 1.0, so suitability is the only term "
				"that can reject anything here")));
		ASSERT_THAT(IsTrue(UniformInHole > 0,
			TEXT("Measured mutant side: with a uniform grid — exactly what deleting the factor computes "
				"— recruits DO land in the same region, so the zero above is discriminating")));
		// EXACT ANCHORS for both sides.
		ASSERT_THAT(IsTrue(MaskedRecruits.Num() == 257,
			TEXT("Reference masked-run recruit count is 257, all of them in the suitable half")));
		ASSERT_THAT(IsTrue(UniformRecruits.Num() == 539 && UniformInHole == 282,
			TEXT("Reference mutant side: 539 recruits, 282 of them inside the masked half")));
	}

	// ============================================================================================
	// 10. A DOUBLY-INVERTED FBox2f LEAVES RECRUITMENT INACTIVE.
	//
	// GUARDS: the per-axis bRecruitmentActive test
	// (bEnableRecruitment && bIsValid && Size.X > 0 && Size.Y > 0) in StepIterations.
	//
	// FBox2f(Min, Max) sets bIsValid = 1 and validates NOTHING, so a box with Max.X < Min.X AND
	// Max.Y < Min.Y is "valid" with two negative extents — and therefore a POSITIVE area product. The
	// original gate tested that product: it would have called recruitment ACTIVE while the light model
	// rejected the same box and derived the raster from the plant AABB instead, putting the raster and
	// the darts in two different frames (both samplers clamp, so it would read as plausible edge values
	// rather than as an error).
	//
	// MUTANT THIS TURNS RED — GATE ON THE AREA PRODUCT instead of per-axis: recruitment becomes active
	// on this box and the two identity assertions below flip. The mutant is ARGUED, not executed (no
	// production edit here), but its arithmetic is asserted directly in this test: the box's area product
	// is measured and asserted POSITIVE while its per-axis extents are asserted NEGATIVE, which is
	// exactly the disagreement between the two gates.
	// MEASURED BOTH SIDES: the inverted box reports bIsValid = 1, size (-4000, -4000) and an area product
	// of +16,000,000 (POSITIVE — the mutant gate's input), and recruits 0. The VALID box of the same
	// magnitude, same fixture, same stream recruits 228. So "zero" is the inverted box, not an inert
	// fixture, and the arithmetic that would fool an area-product gate is asserted directly.
	TEST_METHOD(Recruitment_DoublyInvertedBoundsStaysInactive)
	{
		using namespace ArcForestRecruitmentTestsLocal;

		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefaultPair();

		constexpr float HalfSizeCm = 2000.0f;

		FArcForestSimParams Inverted = MakeRecruitParams(HalfSizeCm, /*Ceiling=*/300, /*Rate=*/1.0f);
		Inverted.Bounds2D = FBox2f(
			FVector2f(HalfSizeCm, HalfSizeCm), FVector2f(-HalfSizeCm, -HalfSizeCm)); // Max < Min on BOTH axes

		const FVector2f InvertedSize = Inverted.Bounds2D.GetSize();
		const float InvertedArea = InvertedSize.X * InvertedSize.Y;

		FArcForestSimParams Valid = Inverted;
		Valid.Bounds2D = CentredBounds(HalfSizeCm);

		FArcForestSimParams InvertedDisabled = Inverted;
		InvertedDisabled.bEnableRecruitment = false;

		FHandBuiltStand Stand;
		Stand.Add(FVector2f(-1000.0f, -1000.0f), /*SpeciesIndex=*/0, /*HeightCm=*/1800.0f, /*CrownRadiusCm=*/300.0f);
		Stand.Add(FVector2f(1000.0f, 1000.0f), /*SpeciesIndex=*/1, /*HeightCm=*/380.0f, /*CrownRadiusCm=*/100.0f);

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		FArcForestSolveState InvertedState = Stand.State;
		Simulator.StepIterations(InvertedState, Inverted, *Def, EmptySuitability, Dims, /*MaxIters=*/1);

		FArcForestSolveState DisabledState = Stand.State;
		Simulator.StepIterations(DisabledState, InvertedDisabled, *Def, EmptySuitability, Dims, /*MaxIters=*/1);

		FArcForestSolveState ValidState = Stand.State;
		Simulator.StepIterations(ValidState, Valid, *Def, EmptySuitability, Dims, /*MaxIters=*/1);

		const int32 InvertedRecruits = CountRecruits(InvertedState);
		const int32 ValidRecruits = CountRecruits(ValidState);

		UE_LOGF(LogArcForestRecruitTest, Display,"[InvertedBounds] MEASURED bIsValid=%d size=(%.1f, %.1f) areaProduct=%.1f "
				"invertedRecruits=%d validBoxRecruits=%d",
			static_cast<int32>(Inverted.Bounds2D.bIsValid), InvertedSize.X, InvertedSize.Y,
			InvertedArea, InvertedRecruits, ValidRecruits);

		ASSERT_THAT(IsTrue(Inverted.Bounds2D.bIsValid != 0,
			TEXT("Fixture precondition: FBox2f(Min, Max) must mark the inverted box VALID — that is what "
				"makes bIsValid alone insufficient")));
		ASSERT_THAT(IsTrue(InvertedSize.X < 0.0f && InvertedSize.Y < 0.0f,
			TEXT("Fixture precondition: both extents must be negative")));
		ASSERT_THAT(IsTrue(InvertedArea > 0.0f,
			TEXT("The disagreement itself: the AREA PRODUCT of a doubly-inverted box is POSITIVE, so an "
				"area-product gate would call recruitment active on a box the light model rejects")));
		ASSERT_THAT(IsTrue(InvertedRecruits == 0,
			TEXT("Recruitment must stay INACTIVE on a doubly-inverted box. RED if the gate is relaxed "
				"back to the area product")));
		ASSERT_THAT(IsTrue(StatesIdentical(InvertedState, DisabledState),
			TEXT("Inactive must mean EXACTLY inactive: the inverted-box run must be element-wise "
				"identical to the same run with bEnableRecruitment == false")));
		ASSERT_THAT(IsTrue(ValidRecruits > 0,
			TEXT("Measured control side: the VALID box of the same magnitude recruits, so the zero above "
				"is the inverted box and not an inert fixture")));
		// EXACT ANCHOR on the control side only. The mutant's own count on the INVERTED box is NOT claimed
		// to be 228: FRandRange(Min, Max) with Max < Min still spans the same interval, so it would draw
		// a comparable but differently-ordered dart set. What is asserted is the shipped zero, which no
		// non-zero mutant count can satisfy.
		ASSERT_THAT(IsTrue(ValidRecruits == 228,
			TEXT("Reference control count on the valid box is 228 recruits")));
	}

	// ============================================================================================
	// 11. A DENORMAL RecruitmentWeight SUM RECRUITS NOTHING.
	//
	// GUARDS: the WeightSum <= UE_SMALL_NUMBER early return in RecruitPlants (it was `<= 0.0f`).
	//
	// RecruitmentWeight is only ClampMin = "0.0", so a table of denormal positive weights clears a
	// `> 0.0f` test and then divides the share ratio by ~1e-40. The shipped answer is to recruit nothing
	// and consume not one draw.
	//
	// MUTANT THIS TURNS RED — GATE ON `> 0.0f`: the split proceeds and the fixture recruits.
	// MEASURED BOTH SIDES, IN THIS RUN, EXACTLY: the share ratio Attempts * Weight / WeightSum is
	// SCALE-INVARIANT, so a mutant that let the denormal sum through would compute the very same shares
	// as the control arm below, which runs the identical fixture at RecruitmentWeight = 1.0. Its recruit
	// count is therefore not merely "the other side" — it is the mutant's own output, logged and
	// asserted non-zero.
	// MEASURED BOTH SIDES: weight sum 2.0e-40, denormal run 0 recruits (and element-wise identical to the
	// disabled run), unit-weight run 228 recruits. 0 vs 228 of 298 darts. Both are pinned.
	TEST_METHOD(Recruitment_DenormalWeightSumRecruitsNothing)
	{
		using namespace ArcForestRecruitmentTestsLocal;

		// Denormal-scale weights: the SUM (2e-40) is far below UE_SMALL_NUMBER (1e-8) but strictly > 0.
		TArray<FArcForestSpeciesEntry> DenormalTable = MakeDefaultPair()->Species;
		DenormalTable[0].RecruitmentWeight = 1e-40f;
		DenormalTable[1].RecruitmentWeight = 1e-40f;
		TStrongObjectPtr<UArcForestSpeciesDefinition> DenormalDef = MakeDefinition(MoveTemp(DenormalTable));

		TStrongObjectPtr<UArcForestSpeciesDefinition> UnitDef = MakeDefaultPair();

		FArcForestSimParams Params = MakeRecruitParams(/*HalfSizeCm=*/2000.0f, /*Ceiling=*/300, /*Rate=*/1.0f);

		FArcForestSimParams Disabled = Params;
		Disabled.bEnableRecruitment = false;

		FHandBuiltStand Stand;
		Stand.Add(FVector2f(-1000.0f, -1000.0f), /*SpeciesIndex=*/0, /*HeightCm=*/1800.0f, /*CrownRadiusCm=*/300.0f);
		Stand.Add(FVector2f(1000.0f, 1000.0f), /*SpeciesIndex=*/1, /*HeightCm=*/380.0f, /*CrownRadiusCm=*/100.0f);

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		FArcForestSolveState DenormalState = Stand.State;
		Simulator.StepIterations(DenormalState, Params, *DenormalDef, EmptySuitability, Dims, /*MaxIters=*/1);

		FArcForestSolveState DisabledState = Stand.State;
		Simulator.StepIterations(DisabledState, Disabled, *DenormalDef, EmptySuitability, Dims, /*MaxIters=*/1);

		FArcForestSolveState UnitState = Stand.State;
		Simulator.StepIterations(UnitState, Params, *UnitDef, EmptySuitability, Dims, /*MaxIters=*/1);

		const int32 DenormalRecruits = CountRecruits(DenormalState);
		const int32 UnitRecruits = CountRecruits(UnitState);

		UE_LOGF(LogArcForestRecruitTest, Display,"[DenormalWeights] MEASURED weightSum=%.3e denormalRecruits=%d unitWeightRecruits=%d "
				"(the mutant's own output, by scale-invariance of the share ratio)",
			static_cast<double>(DenormalDef->Species[0].RecruitmentWeight
				+ DenormalDef->Species[1].RecruitmentWeight),
			DenormalRecruits, UnitRecruits);

		ASSERT_THAT(IsTrue(DenormalDef->Species[0].RecruitmentWeight > 0.0f,
			TEXT("Fixture precondition: the weights must be strictly positive, so only the "
				"UE_SMALL_NUMBER threshold — not a zero test — can reject them")));
		ASSERT_THAT(IsTrue(DenormalRecruits == 0,
			TEXT("A denormal RecruitmentWeight sum must recruit NOTHING. RED if the gate is `> 0.0f`")));
		ASSERT_THAT(IsTrue(StatesIdentical(DenormalState, DisabledState),
			TEXT("And it must consume nothing at all: element-wise identical to the disabled run")));
		ASSERT_THAT(IsTrue(UnitRecruits > 0,
			TEXT("Measured mutant side: the share ratio is scale-invariant, so a `> 0.0f` gate would "
				"produce exactly this unit-weight run's recruits from the denormal table")));
		ASSERT_THAT(IsTrue(UnitRecruits == 228,
			TEXT("Reference mutant-side count is 228 recruits — what a `> 0.0f` gate would place from the "
				"denormal table, since the share ratio is scale-invariant")));
	}

	// ============================================================================================
	// 12. A HUGE RecruitmentWeight DOES NOT INVERT THE SPLIT.
	//
	// GUARDS: the ShareRatio cap in RecruitPlants —
	//   CappedShare = (ShareRatio < Attempts) ? FloorToInt(ShareRatio) : Attempts.
	//
	// RecruitmentWeight has no authored upper bound, so Attempts * Weight overflows to +inf, and casting
	// a non-finite float inside FMath::FloorToInt is UNDEFINED BEHAVIOUR that lands on INT_MIN in
	// practice. Unguarded, the HEAVIEST species is clamped to a 0 share and the remainder rule hands its
	// entire budget to the FIRST mature species — precisely the inverse of the intended split.
	//
	// THE FIXTURE puts the huge weight on species 1 (NOT species 0), which is what makes the two
	// behaviours distinguishable: with the weight on species 0 the correct answer and the UB answer both
	// give species 0 everything, and the test would prove nothing.
	//
	// MUTANT THIS TURNS RED — DROP THE CAP (plain FloorToInt(ShareRatio)): every dart goes to species 0
	// instead of species 1.
	// MEASURED BOTH SIDES, IN THIS RUN, EXACTLY: acceptance here is species-INDEPENDENT (default table,
	// no grid: AcceptProb is exactly 1.0 for both species), and the dart positions come from one stream
	// in dart order, so the mutant accepts the SAME positions and only relabels them species 0. Its
	// output is therefore the measured total recruit count with species 0 == total, species 1 == 0 —
	// asserted here as the exact mirror image.
	// MEASURED: 298 darts, Attempts * Weight = inf (non-finite, asserted directly), 228 recruits of which
	// species 1 = 228 and species 0 = 0. The mutant's output on this fixture is the mirror image:
	// species 0 = 228, species 1 = 0. Every number here is pinned or asserted exactly.
	TEST_METHOD(Recruitment_HugeWeightDoesNotInvertSplit)
	{
		using namespace ArcForestRecruitmentTestsLocal;

		TArray<FArcForestSpeciesEntry> Table = MakeDefaultPair()->Species;
		Table[0].RecruitmentWeight = 1.0f;
		Table[1].RecruitmentWeight = 1e38f; // Attempts * this overflows float
		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));

		FArcForestSimParams Params = MakeRecruitParams(/*HalfSizeCm=*/2000.0f, /*Ceiling=*/300, /*Rate=*/1.0f);

		FHandBuiltStand Stand;
		Stand.Add(FVector2f(-1000.0f, -1000.0f), /*SpeciesIndex=*/0, /*HeightCm=*/1800.0f, /*CrownRadiusCm=*/300.0f);
		Stand.Add(FVector2f(1000.0f, 1000.0f), /*SpeciesIndex=*/1, /*HeightCm=*/380.0f, /*CrownRadiusCm=*/100.0f);

		const int32 Attempts = ExpectedAttempts(Params, Stand.Num());
		const float OverflowedProduct = static_cast<float>(Attempts) * Def->Species[1].RecruitmentWeight;

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		FArcForestSolveState State = Stand.State;
		Simulator.StepIterations(State, Params, *Def, EmptySuitability, Dims, /*MaxIters=*/1);

		const int32 Total = CountRecruits(State);
		const int32 OfSpeciesZero = CountRecruitsOfSpecies(State, 0);
		const int32 OfSpeciesOne = CountRecruitsOfSpecies(State, 1);

		UE_LOGF(LogArcForestRecruitTest, Display,"[HugeWeight] MEASURED attempts=%d attempts*weight=%.3e (finite=%d) recruits total=%d "
				"species0=%d species1=%d",
			Attempts, static_cast<double>(OverflowedProduct),
			static_cast<int32>(FMath::IsFinite(OverflowedProduct)), Total, OfSpeciesZero, OfSpeciesOne);

		ASSERT_THAT(IsTrue(!FMath::IsFinite(OverflowedProduct),
			TEXT("Fixture precondition: Attempts * Weight must actually overflow to a non-finite float, "
				"else the cap under test is never reached")));
		ASSERT_THAT(IsTrue(Total > 0, TEXT("Non-vacuity: the iteration must have recruited")));
		ASSERT_THAT(IsTrue(Attempts == 298 && Total == 228,
			TEXT("Reference run: 298 darts, 228 recruits")));
		ASSERT_THAT(IsTrue(OfSpeciesOne == Total,
			TEXT("The overwhelmingly heaviest species must receive the ENTIRE budget: every recruit is "
				"species 1. RED without the ratio cap, which lands FloorToInt on INT_MIN and hands the "
				"whole budget to species 0 via the remainder rule")));
		ASSERT_THAT(IsTrue(OfSpeciesZero == 0,
			TEXT("The mirror half of the same claim, stated exactly: species 0 gets NOTHING. This is the "
				"measured mutant output (species0 == total) inverted")));
	}

	// ============================================================================================
	// 13. AN IMMATURE SPECIES DOES NOT RECRUIT.
	//
	// GUARDS: the MaturityHeightCm gate in RecruitPlants — a species contributes only when its tallest
	// LIVE plant reaches its MaturityHeightCm (or the gate is <= 0). A species with NO live plants has
	// SpeciesMaxHeight == -FLT_MAX and therefore recruits only if its gate is <= 0, which is the
    // "an extinct species does not come back from nothing" half of the rule.
	//
	// THE FIXTURE: species 0 stands 1800 cm tall but needs 100000 cm to reproduce; species 1 has NO live
	// plants and a gate of 0. So the whole budget must go to species 1.
	//
	// MUTANT THIS TURNS RED — IGNORE MaturityHeightCm (treat every species as mature): the budget splits
	// 50/50 by weight and species 0 recruits.
	// MEASURED BOTH SIDES: gated run 228 recruits, species 0 = 0 and species 1 = 228. The control arm —
	// the identical fixture with species 0's MaturityHeightCm set to 0, which is exactly what ignoring
	// the gate computes — places 126 species-0 recruits. 0 vs 126. Both are pinned.
	TEST_METHOD(Recruitment_ImmatureSpeciesDoesNotRecruit)
	{
		using namespace ArcForestRecruitmentTestsLocal;

		TArray<FArcForestSpeciesEntry> GatedTable = MakeDefaultPair()->Species;
		GatedTable[0].MaturityHeightCm = 100000.0f; // unreachable by the standing plant
		GatedTable[1].MaturityHeightCm = 0.0f;      // recruits from the first iteration
		TStrongObjectPtr<UArcForestSpeciesDefinition> GatedDef = MakeDefinition(MoveTemp(GatedTable));

		TArray<FArcForestSpeciesEntry> OpenTable = MakeDefaultPair()->Species;
		OpenTable[0].MaturityHeightCm = 0.0f; // the "gate ignored" control
		OpenTable[1].MaturityHeightCm = 0.0f;
		TStrongObjectPtr<UArcForestSpeciesDefinition> OpenDef = MakeDefinition(MoveTemp(OpenTable));

		FArcForestSimParams Params = MakeRecruitParams(/*HalfSizeCm=*/2000.0f, /*Ceiling=*/300, /*Rate=*/1.0f);

		// ONLY species 0 stands here, and it is far below its gate.
		FHandBuiltStand Stand;
		Stand.Add(FVector2f(-1000.0f, -1000.0f), /*SpeciesIndex=*/0, /*HeightCm=*/1800.0f, /*CrownRadiusCm=*/300.0f);

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		FArcForestSolveState GatedState = Stand.State;
		Simulator.StepIterations(GatedState, Params, *GatedDef, EmptySuitability, Dims, /*MaxIters=*/1);

		FArcForestSolveState OpenState = Stand.State;
		Simulator.StepIterations(OpenState, Params, *OpenDef, EmptySuitability, Dims, /*MaxIters=*/1);

		const int32 GatedTotal = CountRecruits(GatedState);
		const int32 GatedSpeciesZero = CountRecruitsOfSpecies(GatedState, 0);
		const int32 GatedSpeciesOne = CountRecruitsOfSpecies(GatedState, 1);
		const int32 OpenSpeciesZero = CountRecruitsOfSpecies(OpenState, 0);

		UE_LOGF(LogArcForestRecruitTest, Display,"[MaturityGate] MEASURED gated: total=%d species0=%d species1=%d | control (gate 0 == "
				"gate ignored): species0=%d",
			GatedTotal, GatedSpeciesZero, GatedSpeciesOne, OpenSpeciesZero);

		ASSERT_THAT(IsTrue(GatedTotal > 0, TEXT("Non-vacuity: the mature species must have recruited")));
		ASSERT_THAT(IsTrue(GatedSpeciesZero == 0,
			TEXT("A species whose tallest live plant is below its MaturityHeightCm must contribute NOT "
				"ONE recruit. RED if the maturity gate is ignored")));
		ASSERT_THAT(IsTrue(GatedSpeciesOne == GatedTotal,
			TEXT("The whole budget must go to the one mature species (stated exactly, not as a majority)")));
		ASSERT_THAT(IsTrue(OpenSpeciesZero > 0,
			TEXT("Measured mutant side: with the gate at 0 — exactly what ignoring it computes — species "
				"0 DOES recruit, so the zero above is discriminating")));
		ASSERT_THAT(IsTrue(GatedTotal == 228 && OpenSpeciesZero == 126,
			TEXT("Reference run: 228 recruits all from the mature species, against 126 species-0 recruits "
				"on the gate-ignored side")));
	}

	// ============================================================================================
	// 14. SPACING USES THE NEIGHBOUR'S LIVE CROWN RADIUS, NOT ITS SPECIES' MATURE ONE.
	//
	// GUARDS: MinDist = Max(RecruitMinSpacingCm, SpacingFraction * (SeedlingCrownRadiusCm +
	// Scratch.Radius[NeighbourIndex])), where Scratch.Radius holds Pop.CrownRadius — the LIVE radius.
	//
	// Seeding rejects on MATURE MaxCrownRadius because it is laying out a stand that will grow into
	// itself; a recruit is a 5 cm plant slotting into an existing stand, so a mature-radius rejection
	// would forbid all understory. This fixture makes the two rules numerically far apart: one standing
	// plant with a LIVE crown radius of 10 cm whose species' MaxCrownRadius is 600 cm, at
	// SpacingFraction 0.8:
	//     live rule    = Max(150, 0.8 * (5 + 10))  = 150 cm
	//     mature rule  =        0.8 * (5 + 600)    = 484 cm
	// So the annulus 150 cm <= d < 484 cm from that plant is EXACTLY the set of positions the live rule
	// admits and the mature rule forbids.
	//
	// MUTANT THIS TURNS RED — USE THE MATURE CROWN RADIUS: every recruit in that annulus disappears, so
	// the count asserted non-zero below goes to zero.
	// MEASURED BOTH SIDES: 88 recruits, 13 of them inside the mature rule (i.e. in the 150..484 cm
	// annulus, since 0 are inside the live rule) and the closest recruit sits at 185.91 cm — above the
	// 150 cm live rule and well below the 484 cm mature rule. The mutant's count for that annulus is
	// exactly 0 by construction: it is the mutant's rejection set. Nothing here is a proxy statistic.
	TEST_METHOD(Recruitment_SpacingUsesLiveNotMatureCrownRadius)
	{
		using namespace ArcForestRecruitmentTestsLocal;

		constexpr float LiveRadiusCm = 10.0f;
		constexpr float MatureRadiusCm = 600.0f;
		constexpr float MinSpacingCm = 150.0f;

		TArray<FArcForestSpeciesEntry> Table;
		Table.Add(MakeSpecies(TEXT("WideMature"), EArcForestLayer::Canopy,
			/*MaxHeight=*/2000.0f, MatureRadiusCm, /*GrowthRate=*/0.10f,
			/*TargetDensity=*/0.02f, /*PatchRadiusSigma=*/300.0f, /*PatchDensity=*/0.05f));
		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));

		FArcForestSimParams Params = MakeRecruitParams(/*HalfSizeCm=*/1000.0f, /*Ceiling=*/400, /*Rate=*/1.0f);
		Params.RecruitMinSpacingCm = MinSpacingCm;

		const FVector2f PlantPos(0.0f, 0.0f);
		FHandBuiltStand Stand;
		Stand.Add(PlantPos, /*SpeciesIndex=*/0, /*HeightCm=*/1800.0f, LiveRadiusCm);

		const float LiveRule = FMath::Max(MinSpacingCm,
			Params.SpacingFraction * (ArcForestSim::SeedlingCrownRadiusCm + LiveRadiusCm));
		const float MatureRule =
			Params.SpacingFraction * (ArcForestSim::SeedlingCrownRadiusCm + MatureRadiusCm);

		const TArray<float> EmptySuitability;
		const FIntPoint Dims(0, 0);
		const FArcForestSimulator Simulator;

		FArcForestSolveState State = Stand.State;
		Simulator.StepIterations(State, Params, *Def, EmptySuitability, Dims, /*MaxIters=*/1);

		TArray<FVector2f> RecruitPos;
		TArray<int32> RecruitSpecies;
		CollectRecruits(State, RecruitPos, RecruitSpecies);

		const int32 WithinMatureRule = CountWithinRadius(RecruitPos, PlantPos, MatureRule);
		const int32 WithinLiveRule = CountWithinRadius(RecruitPos, PlantPos, LiveRule * (1.0f - 1e-3f));
		const float ClosestToPlant = MinDistanceTo(RecruitPos, PlantPos);

		UE_LOGF(LogArcForestRecruitTest, Display,"[LiveRadiusSpacing] MEASURED liveRule=%.1f matureRule=%.1f recruits=%d "
				"insideMatureRule=%d insideLiveRule=%d closestToPlant=%.2f",
			LiveRule, MatureRule, RecruitPos.Num(), WithinMatureRule, WithinLiveRule, ClosestToPlant);

		ASSERT_THAT(IsTrue(RecruitPos.Num() > 0, TEXT("Non-vacuity: the iteration must have recruited")));
		ASSERT_THAT(IsTrue(MatureRule > LiveRule,
			TEXT("Fixture precondition: the mature rule must be strictly wider than the live rule, else "
				"the two are indistinguishable")));
		ASSERT_THAT(IsTrue(WithinLiveRule == 0,
			TEXT("No recruit may sit inside the LIVE spacing rule around the standing plant")));
		ASSERT_THAT(IsTrue(WithinMatureRule > 0,
			TEXT("Recruits MUST exist in the annulus the live rule admits and the mature rule forbids "
				"(150 cm <= d < 484 cm). RED if spacing reads the species' MaxCrownRadius instead of the "
				"neighbour's live CrownRadius — that mutant's rejection set is exactly this annulus, so "
				"the count would be 0")));
		ASSERT_THAT(IsTrue(RecruitPos.Num() == 88 && WithinMatureRule == 13,
			TEXT("Reference run: 88 recruits, 13 of them in the annulus the mature-radius mutant would "
				"have rejected")));
		ASSERT_THAT(IsTrue(ClosestToPlant >= LiveRule * (1.0f - 1e-3f) && ClosestToPlant < MatureRule,
			TEXT("The closest recruit to the standing plant must sit at or above the live rule AND "
				"strictly below the mature rule — both sides of the discrimination, in one number")));
	}
};
