// Copyright Lukasz Baran. All Rights Reserved.

// ArcForestTransmittanceTests.cpp
// Step 10 of docs/plans/2026-07-25-1133-arcpcgextension-habitat-fields-and-recruitment-plan.md —
// coverage of the QUERY-HEIGHT TRANSMITTANCE RASTER landed by Step 9 (commit 8fa645d32dcf):
// FArcForestLightModel::ComputeIncidentLight's optional OutTransmittance / QueryHeightCm / Stride /
// QueryBand parameters (ArcForestLightModel.h:183-195, ArcForestLightModel.cpp:398-511).
//
// THE CLAIM UNDER TEST, and it is the plan's named open risk: the raster is NOT a post-sweep sum over
// the finished band grids. The per-plant read is STRICTLY-TALLER-only (two-phase group split,
// ArcForestLightModel.cpp:461-497), while the finished grids contain every plant. Step 9 therefore
// snapshots the accumulators MID-SWEEP, in two capture cases:
//   1. GROUP BOUNDARY (:448-459) — emitted immediately before Phase A of the first equal-height group
//      whose GroupHeight <= QueryHeightCm.
//   2. POST-SWEEP FALLBACK (:502-510) — if no group is ever at or below the query height, the finished
//      accumulators ARE the correct snapshot; without this case a REUSED buffer would keep the previous
//      iteration's raster.
//
// TOPOLOGY: standalone, in-process, no world, no PIE, no RHI. The unit is a pure CPU function over
// plain arrays (FArcForestLightModel's PURITY contract, ArcForestLightModel.h:104-106) — no UObject, no
// replicated state, no actor, and the dedicated-server / client paths execute the exact same pure
// function on the exact same inputs. A PIE-network fixture would add a world and change nothing any
// assertion below reads, so standalone IS the at-risk topology in full.
//
// PRODUCTION PATH, not a proxy: every case hand-builds the stand into a real FArcForestSolveState
// (State.Add + bSeeded, the ArcForestBandModelTests.cpp technique — ZERO RNG, closed-form expectations)
// and calls the shipped FArcForestLightModel::ComputeIncidentLight with exactly the argument shape the
// solver uses (ArcForestSimulator.cpp:902-906): State.Pos / State.Height / State.CrownRadius, a uniform
// CrownLAIDensity array, a Bands array built by the shipped LayerForHeight. Nothing is re-implemented.
//
// WHY NOT END-TO-END THROUGH StepIterations: FArcForestSimParams::bEnableRecruitment gates the emit at
// the simulator call site (ArcForestSimulator.cpp:873-874) but is not yet surfaced as a
// UPCGForestSimSettings UPROPERTY (that is Step 11), and StepIterations exposes no raster on
// FArcForestSolveState, so a solver-level test cannot observe the raster at all. Direct calls are the
// only way to reach the emit path today.
//
// FIXTURE GEOMETRY — three constraints, all of them load-bearing:
//   * DIMS <= 256 PER AXIS. At Stride = 1 the EFFECTIVE stride still grows when a light-grid dim
//     exceeds MaxTransmittanceDim: EffStride = Max(Requested, DivideAndRoundUp(Dim, 256))
//     (ArcForestLightModel.cpp:151-152). Every Stride = 1 fixture below uses a 3200 cm footprint at
//     100 cm cells = 32x32, asserted, so "Stride = 1" really is 1.
//   * PLANTS AT CELL CENTRES. The sweep resolves a cell as floor((Pos-Origin)/CellSize)
//     (:55-56) while FArcForestScalarField's samplers use floor((Pos-Min)/Size*Dims)
//     (ArcForestField.cpp:21-25) — algebraically the same, two different float expressions, so a plant
//     exactly ON a cell boundary can resolve differently. CellCentre() below places every plant at the
//     exact centre of its cell.
//   * QueryBand IS ALWAYS PASSED EXPLICITLY. The parameter defaults to EArcForestLayer::Field, and with
//     the shipped IntraLayerShading (Field-only ON) an omitted argument would include all four band
//     grids for a Shrub/Understory/Canopy query height — a silently too-dark raster.
//
// CROWN RADIUS CONVENTION: 40 cm splats EXACTLY ONE cell (a neighbour centre is 100 cm away, the plant's
// own centre 0), and 450 cm splats EXACTLY 69 cells — the integer lattice points with i^2+j^2 <= 20.25,
// and 20.25 is unreachable by a sum of two squares so no cell sits exactly on the disc boundary and the
// count cannot flip on a float tie. Both counts are closed-form, not measured-and-pinned.
//
// KNOWN GAPS — recorded, not silently skipped:
//   * The ResolveBounds-failure early-out (ArcForestLightModel.cpp:349-357) is UNREACHABLE as shipped
//     and is therefore not covered. ResolveBounds only returns false for an EMPTY position array
//     (:44-49 builds a valid AABB from any single point, and it does not require a positive extent),
//     but an empty position array is already consumed by the NumPlants == 0 early-out at :326. Its unity
//     fill is the same FillUnityTransmittance call the other two early-outs make, which IS covered.
//   * Effective-stride GROWTH above 256 dims is covered only on the FillUnityTransmittance path
//     (Transmittance_EmptyStandIsUnity, arm D), not on the live sweep — a live 1000x1000-cell sweep is a
//     million-cell fixture for a layout claim that is computed by the same ResolveRasterLayout call in
//     both paths.
//   * SampleBilinear is not exercised; only SampleNearest. Step 11 samples the raster bilinearly, so its
//     own tests must cover that.

#include "CQTest.h"

#include "Forest/ArcForestField.h"
#include "Forest/ArcForestLightModel.h"
#include "Forest/ArcForestSimTypes.h"
#include "Forest/ArcForestSimulator.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcForestTransmittanceTest, Log, All);

namespace ArcForestTransmittanceTestsLocal
{
	// The cm-scaled band thresholds the SOLVER passes to LayerForHeight (ArcForestSimulator.cpp:
	// ShrubMinHeightCm / UnderstoryMinHeightCm / CanopyMinHeightCm = 1.5 m / 5 m / 15 m in cm).
	constexpr float ShrubMinCm = 150.0f;
	constexpr float UnderstoryMinCm = 500.0f;
	constexpr float CanopyMinCm = 1500.0f;

	/** The solver's light-grid resolution (ArcForestSimulator.cpp LightGridCellSizeCm). */
	constexpr float CellSizeCm = 100.0f;

	/** The solver's uniform per-plant crown LAI proxy (ArcForestSimulator.cpp CrownLeafAreaIndex). */
	constexpr float CrownLai = 2.0f;

	/** Beer-Lambert k; FArcForestSimParams::CanopyExtinction's own default, restated for the closed forms. */
	constexpr float ExtinctionK = 0.5f;

	/** Crown radius that splats EXACTLY the plant's own cell (see the header comment). */
	constexpr float SingleCellRadiusCm = 40.0f;

	/** Crown radius that splats EXACTLY 69 cells at 100 cm cells (see the header comment). */
	constexpr float WideCrownRadiusCm = 450.0f;

	/** Cells covered by WideCrownRadiusCm: integer (i,j) with i^2+j^2 <= 20.25. Closed form, no ties. */
	constexpr int32 WideCrownCellCount = 69;

	/** Live band of a height under the solver's own cm thresholds — the shipped classifier, not a copy. */
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

	/** A square XY footprint anchored at the ORIGIN, so cell centres are exact multiples of 50 cm. */
	FBox2f OriginBounds(float SizeCm)
	{
		return FBox2f(FVector2f(0.0f, 0.0f), FVector2f(SizeCm, SizeCm));
	}

	/**
	 * World XY centre of light-grid cell (CellX, CellY) for a footprint anchored at the origin. The sweep
	 * derives its cell as floor((Pos - Origin) / CellSize), so a plant placed here can never straddle a
	 * cell boundary (see the geometry note in the file header).
	 */
	FVector2f CellCentre(int32 CellX, int32 CellY)
	{
		return FVector2f(
			(static_cast<float>(CellX) * CellSizeCm) + (0.5f * CellSizeCm),
			(static_cast<float>(CellY) * CellSizeCm) + (0.5f * CellSizeCm));
	}

	/** Row-major flat index into a Dims raster, X fastest — the grid convention in ArcForestField.h:22. */
	int32 FlatIndex(FIntPoint Dims, int32 X, int32 Y)
	{
		return (Y * Dims.X) + X;
	}

	/**
	 * One hand-built stand: a real FArcForestSolveState plus the two derived arrays the solver builds per
	 * iteration (the uniform CrownLAIDensity proxy and the live Bands array). bSeeded is set so the state
	 * honours the resumable-driver contract even though SeedInitial is deliberately never called — that
	 * is the whole point: zero RNG, zero dart draws, closed-form expectations.
	 */
	struct FHandBuiltStand
	{
		FArcForestSolveState State;
		TArray<float> CrownLaiDensity;
		TArray<EArcForestLayer> Bands;

		void Add(FVector2f Pos, float HeightCm, float CrownRadiusCm)
		{
			State.Add(Pos, /*Species=*/0, CrownRadiusCm, HeightCm, /*PatchId=*/INDEX_NONE);
			CrownLaiDensity.Add(CrownLai);
			Bands.Add(BandOfHeight(HeightCm));
			State.bSeeded = true;
		}

		int32 Num() const
		{
			return State.Num();
		}
	};

	/** Number of raster cells strictly below full light — the shaded-footprint statistic. */
	int32 CountShadedCells(const FArcForestScalarField& Field)
	{
		int32 Count = 0;
		for (int32 Index = 0; Index < Field.Values.Num(); ++Index)
		{
			if (Field.Values[Index] < 1.0f)
			{
				++Count;
			}
		}
		return Count;
	}

	/** Smallest raster value, or -1 for an empty raster (so a vacuous field is visible, not silent). */
	float MinValue(const FArcForestScalarField& Field)
	{
		if (Field.Values.Num() == 0)
		{
			return -1.0f;
		}
		float Result = Field.Values[0];
		for (int32 Index = 1; Index < Field.Values.Num(); ++Index)
		{
			Result = FMath::Min(Result, Field.Values[Index]);
		}
		return Result;
	}

	/** Index of the first raster cell that is not EXACTLY 1.0, or INDEX_NONE. */
	int32 FindFirstNonUnity(const FArcForestScalarField& Field)
	{
		for (int32 Index = 0; Index < Field.Values.Num(); ++Index)
		{
			if (Field.Values[Index] != 1.0f)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** Beer-Lambert transmittance for a known LAI — the closed form every expectation below is built from. */
	float Transmittance(float Lai)
	{
		return FMath::Exp(-ExtinctionK * Lai);
	}
}

TEST_CLASS(ArcForestTransmittance, "ArcPCGExtension.ForestSim.Transmittance")
{
	// ============================================================================================
	// 1. THE CORRECTION ITSELF — the raster at a plant's own height EQUALS that plant's own
	//    OutIncidentLight, at Stride = 1.
	//
	// GUARDS: CAPTURE CASE 1, the group-boundary snapshot at ArcForestLightModel.cpp:448-459.
	//
	// THE STAND (all four plants at the SAME cell centre, cell (16,16) of a 32x32 grid, crown radius
	// 40 cm so each splats exactly that one cell):
	//     idx0  h=2000 cm  band Canopy       — STRICTLY TALLER than the query
	//     idx1  h= 500 cm  band Understory   — EXACTLY AT the query height
	//     idx2  h= 500 cm  band Understory   — EXACTLY AT the query height
	//     idx3  h= 100 cm  band Field        — SHORTER than the query
	// QueryHeightCm = 500, QueryBand = Understory, Stride = 1.
	//
	// The two 500 cm plants form a group at EXACTLY the query height, which is what makes the
	// strictly-taller rule load-bearing: without such a group the boundary construction and a
	// post-sweep sum would be indistinguishable and this test would prove nothing.
	//
	// IntraLayerShading[Understory] is switched ON deliberately, and the reason is NOT that the
	// post-sweep mutant survives without it — it does not. With the shipped default (OFF) the
	// Understory grid is excluded from both the per-plant read and the raster, but idx3's crown sits in
	// the Field band, which is never gated, so the post-sweep mutant would read 2.0 + 2.0 = 4.0 ->
	// exp(-2) = 0.1353353 against the correct 0.3678794 and still go RED.
	//
	// ON is load-bearing for a different, sharper reason: with it OFF the DISCRIMINATOR would be the
	// SHORTER plant (idx3), so the test would only prove "plants below the query height are excluded"
	// and would leave the EQUAL-HEIGHT tie — the strictly-taller rule that is the actual subject of
	// this step — completely untested. ON puts the tie group's LAI in an included band and makes the
	// tie itself what separates the two constructions.
	//
	// Concretely, ON is what kills the `<` instead of `<=` boundary mutant at ArcForestLightModel.cpp:454
	// (capture slid one group too far): with ON that mutant reads 2.0 + 4.0 = 6.0 -> exp(-3) = 0.0497871
	// and flips; with OFF it reads 2.0 -> 0.3678794 and SURVIVES. That is the mutant the plan's open
	// risk (line 617) was really about.
	//
	// CLOSED FORM at cell (16,16):
	//     correct (taller-only)  = exp(-0.5 * 2.0) = 0.36787944  <- Canopy crown only
	//     post-sweep (all four)  = exp(-0.5 * 8.0) = 0.01831564  <- 2.0 + 2*2.0 + 2.0
	// MEASURED both sides: 0.3678794 (asserted) vs 0.0183156 (the mutant's value, asserted AGAINST with
	// a floor of 0.1 — the real separation is 0.3495638, so the threshold sits nowhere near either side).
	//
	// MUTANT THIS TURNS RED — THE POST-SWEEP MUTANT: delete capture case 1 and let case 2 emit
	// unconditionally after the sweep. The raster at the query cell becomes 0.0183 while idx1's own
	// OutIncidentLight stays 0.3679, so the equality assertion flips. This is the test the plan calls
	// "the test that proves the correction landed".
	//
	// BIT-IDENTICAL, NOT MERELY CLOSE: at Stride = 1 the raster sums IncludedBandGrids in the same
	// ascending band order the per-plant read uses (:419-427 vs :474-481), BlockCells == 1 so the mean is
	// an exact division by 1.0f, and the exp argument is the same float expression. Asserted with ==.
	TEST_METHOD(Transmittance_MatchesPerPlantLightAtSameHeight)
	{
		using namespace ArcForestTransmittanceTestsLocal;

		constexpr float QueryHeightCm = 500.0f;
		const EArcForestLayer QueryBand = BandOfHeight(QueryHeightCm);
		ASSERT_THAT(IsTrue(QueryBand == EArcForestLayer::Understory,
			TEXT("Fixture precondition: a 500 cm query height must classify as Understory under the "
				"solver's cm thresholds, or the self-band gate under test is the wrong one")));

		FHandBuiltStand Stand;
		const FVector2f QueryPos = CellCentre(16, 16);
		Stand.Add(QueryPos, /*HeightCm=*/2000.0f, SingleCellRadiusCm); // idx0 — taller
		Stand.Add(QueryPos, QueryHeightCm, SingleCellRadiusCm);        // idx1 — exactly at query height
		Stand.Add(QueryPos, QueryHeightCm, SingleCellRadiusCm);        // idx2 — exactly at query height
		Stand.Add(QueryPos, /*HeightCm=*/100.0f, SingleCellRadiusCm);  // idx3 — shorter

		ASSERT_THAT(IsTrue(Stand.Bands[1] == QueryBand && Stand.Bands[2] == QueryBand,
			TEXT("Fixture precondition: both equal-height plants must sit in the QUERY BAND, otherwise "
				"their LAI lands in a grid the self-band gate treats differently from the query")));
		ASSERT_THAT(IsTrue(Stand.Bands[0] == EArcForestLayer::Canopy && Stand.Bands[3] == EArcForestLayer::Field,
			TEXT("Fixture precondition: the taller plant is Canopy and the shorter is Field")));

		FArcForestSimParams Params;
		Params.CanopyExtinction = ExtinctionK;
		Params.Bounds2D = OriginBounds(3200.0f);
		// See the test comment: ON puts the EQUAL-HEIGHT group's LAI in an included band, so the tie
		// itself is the discriminator. With OFF the shorter plant would discriminate instead and the
		// strictly-taller tie rule would go untested (and the `<` vs `<=` boundary mutant would survive).
		Params.IntraLayerShading[static_cast<int32>(EArcForestLayer::Understory)] = true;

		const FArcForestLightModel LightModel;
		TArray<float> IncidentLight;
		FArcForestScalarField Raster;
		LightModel.ComputeIncidentLight(
			Stand.State.Pos, Stand.State.Height, Stand.State.CrownRadius, Stand.CrownLaiDensity, Stand.Bands,
			Params, CellSizeCm, IncidentLight,
			&Raster, QueryHeightCm, /*Stride=*/1, QueryBand);

		// Hazard: at Stride = 1 the EFFECTIVE stride still grows if a light-grid dim exceeds 256
		// (:151-152). 3200 cm at 100 cm cells is 32x32, so Stride = 1 really is 1 — asserted, not assumed.
		ASSERT_THAT(IsTrue(Raster.Dims.X == 32 && Raster.Dims.Y == 32,
			FString::Printf(TEXT("Fixture precondition: the raster must be 32x32 (a 3200 cm footprint at "
				"100 cm cells, well under MaxTransmittanceDim = %d) so Stride = 1 is not silently grown; "
				"got %dx%d"), ArcForestLight::MaxTransmittanceDim, Raster.Dims.X, Raster.Dims.Y)));
		ASSERT_THAT(IsTrue(Raster.Values.Num() == Raster.Dims.X * Raster.Dims.Y && Raster.Values.Num() > 0,
			TEXT("Non-vacuity: the raster must be non-empty and exactly Dims.X*Dims.Y long before any "
				"value is read out of it")));
		ASSERT_THAT(IsTrue(IncidentLight.Num() == Stand.Num(),
			TEXT("Non-vacuity: OutIncidentLight must be parallel to the stand")));

		// Values[] directly, NOT SampleNearest: the samplers clamp their return to [0,1]
		// (ArcForestField.cpp:28) while OutIncidentLight is unclamped, so a clamped read could hide a
		// divergence. The sampler path is covered by Transmittance_UnderCanopyIsDarker instead.
		const int32 QueryCell = FlatIndex(Raster.Dims, 16, 16);
		const float RasterAtQuery = Raster.Values[QueryCell];
		const float TallerOnly = Transmittance(CrownLai);                    // 0.36787944
		const float PostSweepAll = Transmittance(CrownLai * 4.0f);           // 0.01831564
		const float ShorterPlantLight = IncidentLight[3];

		UE_LOGF(LogArcForestTransmittanceTest, Display,"[MatchesPerPlant] raster[16,16]=%.7f | idx1 light=%.7f idx2 light=%.7f | taller-only=%.7f "
				"post-sweep=%.7f | shorter plant (%ls) light=%.7f | separation=%.7f",
			RasterAtQuery, IncidentLight[1], IncidentLight[2], TallerOnly, PostSweepAll,
			BandLabel(Stand.Bands[3]), ShorterPlantLight, FMath::Abs(RasterAtQuery - PostSweepAll));

		// THE DISCRIMINATOR — bit-identical equality against the production per-plant read.
		ASSERT_THAT(IsTrue(RasterAtQuery == IncidentLight[1],
			FString::Printf(TEXT("The raster at a plant's own cell must EQUAL that plant's own incident "
				"light BIT-FOR-BIT at Stride = 1 (same ascending band order, BlockCells == 1, same exp "
				"argument). Got raster=%.9g vs plant=%.9g. A post-sweep raster would report %.9g here, "
				"because it includes the two EQUAL-HEIGHT crowns and the SHORTER crown that the "
				"strictly-taller rule excludes"), RasterAtQuery, IncidentLight[1], PostSweepAll)));
		ASSERT_THAT(IsTrue(RasterAtQuery == IncidentLight[2],
			TEXT("The same equality must hold for the second plant of the equal-height group — the tie "
				"rule makes both reads identical, so a raster matching only one of them is a coincidence")));

		// Closed form: the value is the Canopy crown alone, and NOT the post-sweep sum. Both sides of the
		// separation are measured above; the 0.1 floor is ~3.5x below the real gap of 0.3495638.
		ASSERT_THAT(IsTrue(FMath::Abs(RasterAtQuery - TallerOnly) <= 1.0e-6f,
			FString::Printf(TEXT("Closed form: only the 2000 cm Canopy crown is strictly taller than the "
				"500 cm query, so the raster must be exp(-0.5*2.0) = %.7f; got %.7f"),
				TallerOnly, RasterAtQuery)));
		ASSERT_THAT(IsTrue(FMath::Abs(RasterAtQuery - PostSweepAll) > 0.1f,
			FString::Printf(TEXT("The raster must be MATERIALLY different from the post-sweep sum over all "
				"four crowns (exp(-0.5*8.0) = %.7f). Measured separation %.7f against a 0.1 floor — if this "
				"is RED the mid-sweep capture is gone"), PostSweepAll, FMath::Abs(RasterAtQuery - PostSweepAll))));

		// Non-vacuity on the other side: the sweep really did produce differentiated values, so the
		// equality above is not "everything is 1.0".
		ASSERT_THAT(IsTrue(RasterAtQuery < 1.0f,
			TEXT("Non-vacuity: the query cell must be genuinely SHADED (a taller crown covers it), not the "
				"full-light default")));
		ASSERT_THAT(IsTrue(ShorterPlantLight < RasterAtQuery - 0.1f,
			FString::Printf(TEXT("Non-vacuity: the 100 cm plant sees the query-height crowns too and must "
				"read MUCH darker than the query-height raster (measured %.7f vs %.7f). If these are equal "
				"the sweep is not height-ordered at all"), ShorterPlantLight, RasterAtQuery)));
		ASSERT_THAT(IsTrue(Raster.Values[FlatIndex(Raster.Dims, 0, 0)] == 1.0f,
			TEXT("Non-vacuity: a cell far from every crown must be EXACTLY full light — the raster is not "
				"uniformly dark")));
	}

	// ============================================================================================
	// 2. THE PRODUCTION CASE — every plant above the query height, so the raster is the post-sweep
	//    state, and it must NOT be the previous call's raster.
	//
	// GUARDS: CAPTURE CASE 2, the post-sweep fallback at ArcForestLightModel.cpp:502-510.
	//
	// The failure this must catch is the specific one the plan names: the raster buffer is REUSED
	// scratch (ArcForestSimulator.cpp:873 hoists it out of the iteration loop), so a boundary-only
	// implementation leaves it holding the PREVIOUS iteration's values. A single call cannot observe
	// that — so this test makes TWO calls through ONE FArcForestScalarField, exactly as the solver does
	// across iterations:
	//   CALL 1 — one plant at h = 10 cm, crown 450 cm, query height 10 cm. Its group height (10) is
	//            <= the query, so capture case 1 fires BEFORE any Phase B write: the raster is 1.0
	//            everywhere. (This arm is itself a second kill for the post-sweep mutant, which would
	//            put exp(-1) into 69 cells here.)
	//   CALL 2 — one plant at h = 2000 cm, crown 450 cm, same query height 10 cm. No group is ever at
	//            or below 10, so ONLY capture case 2 can produce anything: 69 cells at exp(-1).
	//
	// MEASURED both sides at the crown cell: call 1 = 1.0000000, call 2 = 0.3678794. Shaded-cell counts:
	// call 1 = 0, call 2 = 69 (closed form: i^2+j^2 <= 20.25).
	//
	// MUTANT THIS TURNS RED — THE BOUNDARY-ONLY MUTANT: delete capture case 2. Nothing emits on call 2
	// (Dims/Bounds are still assigned at :411-412, and the buffer length is unchanged, so the stale
	// values survive verbatim), the crown cell reads call 1's 1.0, and BOTH the value assertion and the
	// 69-cell count assertion flip RED.
	TEST_METHOD(Transmittance_QueryBelowAllPlantsUsesFullStand)
	{
		using namespace ArcForestTransmittanceTestsLocal;

		constexpr float QueryHeightCm = 10.0f; // the solver's SeedlingHeightCm
		const EArcForestLayer QueryBand = BandOfHeight(QueryHeightCm);
		ASSERT_THAT(IsTrue(QueryBand == EArcForestLayer::Field,
			TEXT("Fixture precondition: a 10 cm seedling query height is in the Field band")));

		FArcForestSimParams Params;
		Params.CanopyExtinction = ExtinctionK;
		Params.Bounds2D = OriginBounds(3200.0f);

		const FArcForestLightModel LightModel;
		const FVector2f CrownPos = CellCentre(16, 16);
		TArray<float> IncidentLight;

		// ONE field across both calls — this IS the reused-scratch topology under test.
		FArcForestScalarField Raster;

		// --- CALL 1: a group AT the query height -> boundary capture before any write -> all 1.0 ------
		FHandBuiltStand AtQueryHeight;
		AtQueryHeight.Add(CrownPos, QueryHeightCm, WideCrownRadiusCm);
		LightModel.ComputeIncidentLight(
			AtQueryHeight.State.Pos, AtQueryHeight.State.Height, AtQueryHeight.State.CrownRadius,
			AtQueryHeight.CrownLaiDensity, AtQueryHeight.Bands, Params, CellSizeCm, IncidentLight,
			&Raster, QueryHeightCm, /*Stride=*/1, QueryBand);

		const FIntPoint DimsAfterCall1 = Raster.Dims;
		const int32 CrownCell = FlatIndex(DimsAfterCall1, 16, 16);
		ASSERT_THAT(IsTrue(DimsAfterCall1.X == 32 && DimsAfterCall1.Y == 32,
			FString::Printf(TEXT("Fixture precondition: 32x32 raster (Stride = 1 not silently grown); got "
				"%dx%d"), DimsAfterCall1.X, DimsAfterCall1.Y)));
		ASSERT_THAT(IsTrue(Raster.Values.Num() == 32 * 32,
			TEXT("Non-vacuity: the call-1 raster must be fully sized before anything is read")));

		const int32 Call1NonUnity = FindFirstNonUnity(Raster);
		const float Call1AtCrown = Raster.Values[CrownCell];
		// Counted, not assumed: the log line below reports this as a MEASURED number, so it must be
		// measured off the call-1 raster BEFORE call 2 overwrites the shared buffer.
		const int32 Call1Shaded = CountShadedCells(Raster);
		ASSERT_THAT(IsTrue(Call1NonUnity == INDEX_NONE,
			FString::Printf(TEXT("A stand whose ONLY group sits exactly AT the query height must produce a "
				"raster of exactly 1.0 everywhere — the boundary capture fires before that group's Phase B, "
				"so no crown has been splatted yet. First non-unity cell index %d (value %.7f). A "
				"post-sweep raster would darken %d cells to %.7f here"),
				Call1NonUnity, (Call1NonUnity != INDEX_NONE) ? Raster.Values[Call1NonUnity] : 1.0f,
				WideCrownCellCount, Transmittance(CrownLai))));

		// --- CALL 2: every plant ABOVE the query height -> only capture case 2 can emit ---------------
		FHandBuiltStand AllTaller;
		AllTaller.Add(CrownPos, /*HeightCm=*/2000.0f, WideCrownRadiusCm);
		ASSERT_THAT(IsTrue(AllTaller.State.Height[0] > QueryHeightCm,
			TEXT("Fixture precondition: NO group may be at or below the query height, or capture case 1 "
				"would fire and this test would not exercise the post-sweep fallback at all")));

		LightModel.ComputeIncidentLight(
			AllTaller.State.Pos, AllTaller.State.Height, AllTaller.State.CrownRadius,
			AllTaller.CrownLaiDensity, AllTaller.Bands, Params, CellSizeCm, IncidentLight,
			&Raster, QueryHeightCm, /*Stride=*/1, QueryBand);

		const float Call2AtCrown = Raster.Values[CrownCell];
		const int32 Call2Shaded = CountShadedCells(Raster);
		const float Expected = Transmittance(CrownLai);

		UE_LOGF(LogArcForestTransmittanceTest, Display,"[QueryBelowAll] call1 crown=%.7f shaded=%d | call2 crown=%.7f shaded=%d | expected=%.7f",
			Call1AtCrown, Call1Shaded, Call2AtCrown, Call2Shaded, Expected);

		ASSERT_THAT(IsTrue(Raster.Dims == DimsAfterCall1 && Raster.Values.Num() == 32 * 32,
			TEXT("The second call must reuse the same footprint and buffer length — that is what makes a "
				"missing post-sweep emit STALE rather than merely absent")));
		ASSERT_THAT(IsTrue(FMath::Abs(Call2AtCrown - Expected) <= 1.0e-6f,
			FString::Printf(TEXT("With every plant strictly taller than the query height the finished "
				"accumulators ARE the correct snapshot: the crown cell must read exp(-0.5*2.0) = %.7f, got "
				"%.7f. Without capture case 2 this cell still holds the PREVIOUS call's %.7f"),
				Expected, Call2AtCrown, Call1AtCrown)));
		ASSERT_THAT(IsTrue(Call2Shaded == WideCrownCellCount,
			FString::Printf(TEXT("The 450 cm crown must shade exactly %d raster cells (integer lattice "
				"points with i^2+j^2 <= 20.25); got %d. A boundary-only implementation leaves the stale "
				"all-unity buffer from call 1 -> 0 shaded cells"), WideCrownCellCount, Call2Shaded)));
		ASSERT_THAT(IsTrue(Call2AtCrown != Call1AtCrown,
			TEXT("Non-vacuity: the two calls must genuinely differ at the crown cell, or the stale-buffer "
				"failure mode would be invisible to this fixture")));
	}

	// ============================================================================================
	// 3. EVERY EARLY-OUT IS FULL LIGHT.
	//
	// GUARDS: FillUnityTransmittance (ArcForestLightModel.cpp:172-200) at each REACHABLE early-out —
	// NumPlants == 0 (:326-334) and the parallel-array mismatch (:338-346) — plus its degenerate-bounds
	// branch (:179-185) and the effective-stride cap (:151-152).
	//
	// ARMS A, B AND C RUN ON A PRE-DIRTIED FIELD: the same FArcForestScalarField is first filled by a real
	// shaded sweep, so "the early-out did not touch the buffer" cannot pass as green. That is the mutant
	// this kills — dropping the FillUnityTransmittance call from an early-out leaves the field holding
	// another stand's raster (measured below: 69 shaded cells at 0.3678794 before each of those arms).
	//
	// ARM D IS NOT PRE-DIRTIED, deliberately: arm C leaves the field EMPTY, and arm D runs straight after
	// with no re-priming. It stays non-vacuous by a different route — an untouched empty field fails its
	// Dims == 250 and Values.Num() == 62500 asserts — so the layout claim it carries is still proved.
	//
	// ARMS:
	//   A. zero plants, valid bounds       -> 32x32, every cell EXACTLY 1.0
	//   B. malformed parallel arrays        -> 32x32, every cell EXACTLY 1.0
	//   C. zero plants, INVALID bounds      -> field left EMPTY (Dims 0, Values 0) and SampleNearest 1.0
	//   D. zero plants, 100000 cm bounds at Stride = 1 -> effective stride GROWS to 4, dims 250 <= 256
	//
	// The third early-out (ResolveBounds failure, :349-357) is UNREACHABLE as shipped — see the KNOWN
	// GAPS note in the file header.
	TEST_METHOD(Transmittance_EmptyStandIsUnity)
	{
		using namespace ArcForestTransmittanceTestsLocal;

		constexpr float QueryHeightCm = 10.0f;
		const EArcForestLayer QueryBand = BandOfHeight(QueryHeightCm);

		FArcForestSimParams Params;
		Params.CanopyExtinction = ExtinctionK;
		Params.Bounds2D = OriginBounds(3200.0f);

		const FArcForestLightModel LightModel;
		TArray<float> IncidentLight;
		FArcForestScalarField Raster;

		// A shaded stand used ONLY to dirty the buffer before each early-out arm.
		FHandBuiltStand Shaded;
		Shaded.Add(CellCentre(16, 16), /*HeightCm=*/2000.0f, WideCrownRadiusCm);

		const TArray<EArcForestLayer> NoBands;
		const TArray<float> NoFloats;
		const TArray<FVector2f> NoPositions;

		// --- A. zero plants, valid bounds -------------------------------------------------------------
		LightModel.ComputeIncidentLight(
			Shaded.State.Pos, Shaded.State.Height, Shaded.State.CrownRadius, Shaded.CrownLaiDensity,
			Shaded.Bands, Params, CellSizeCm, IncidentLight, &Raster, QueryHeightCm, /*Stride=*/1, QueryBand);
		const int32 DirtyShadedCells = CountShadedCells(Raster);
		const float DirtyMin = MinValue(Raster);
		ASSERT_THAT(IsTrue(DirtyShadedCells == WideCrownCellCount,
			FString::Printf(TEXT("Precondition for every arm below: the priming sweep must leave the buffer "
				"genuinely DIRTY (%d shaded cells expected, got %d, min %.7f). Without this an early-out "
				"that never touches the buffer would pass"),
				WideCrownCellCount, DirtyShadedCells, DirtyMin)));

		LightModel.ComputeIncidentLight(
			NoPositions, NoFloats, NoFloats, NoFloats, NoBands, Params, CellSizeCm, IncidentLight,
			&Raster, QueryHeightCm, /*Stride=*/1, QueryBand);

		ASSERT_THAT(IsTrue(Raster.Dims.X == 32 && Raster.Dims.Y == 32 && Raster.Values.Num() == 32 * 32,
			FString::Printf(TEXT("An EMPTY stand must still size the raster from Params.Bounds2D and the "
				"stride: expected 32x32/1024, got %dx%d/%d"),
				Raster.Dims.X, Raster.Dims.Y, Raster.Values.Num())));
		ASSERT_THAT(IsTrue(FindFirstNonUnity(Raster) == INDEX_NONE,
			FString::Printf(TEXT("Empty stand -> every cell EXACTLY 1.0 (mirroring the OutIncidentLight "
				"prefill). Min value %.7f, %d cells still shaded from the priming sweep"),
				MinValue(Raster), CountShadedCells(Raster))));
		ASSERT_THAT(IsTrue(IncidentLight.Num() == 0,
			TEXT("An empty stand yields an empty per-plant light array")));

		// --- B. malformed parallel arrays -------------------------------------------------------------
		LightModel.ComputeIncidentLight(
			Shaded.State.Pos, Shaded.State.Height, Shaded.State.CrownRadius, Shaded.CrownLaiDensity,
			Shaded.Bands, Params, CellSizeCm, IncidentLight, &Raster, QueryHeightCm, /*Stride=*/1, QueryBand);
		ASSERT_THAT(IsTrue(CountShadedCells(Raster) == WideCrownCellCount,
			TEXT("Precondition: re-dirty the buffer before the malformed-array arm")));

		TArray<FVector2f> TwoPositions;
		TwoPositions.Add(CellCentre(4, 4));
		TwoPositions.Add(CellCentre(5, 5));
		TArray<float> OneHeight;
		OneHeight.Add(2000.0f);
		LightModel.ComputeIncidentLight(
			TwoPositions, OneHeight, OneHeight, OneHeight, NoBands, Params, CellSizeCm, IncidentLight,
			&Raster, QueryHeightCm, /*Stride=*/1, QueryBand);

		ASSERT_THAT(IsTrue(Raster.Dims.X == 32 && Raster.Dims.Y == 32 && Raster.Values.Num() == 32 * 32,
			TEXT("A parallel-array length mismatch must still size the raster from Params.Bounds2D")));
		ASSERT_THAT(IsTrue(FindFirstNonUnity(Raster) == INDEX_NONE,
			FString::Printf(TEXT("Malformed parallel arrays -> every cell EXACTLY 1.0 (degrade to no "
				"shading, never stale data). Min value %.7f"), MinValue(Raster))));
		ASSERT_THAT(IsTrue(IncidentLight.Num() == TwoPositions.Num() &&
			IncidentLight[0] == 1.0f && IncidentLight[1] == 1.0f,
			TEXT("The per-plant array keeps its full-light prefill on the malformed-array early-out")));

		// --- C. zero plants, INVALID bounds -> empty field, sampler answers 1.0 -----------------------
		LightModel.ComputeIncidentLight(
			Shaded.State.Pos, Shaded.State.Height, Shaded.State.CrownRadius, Shaded.CrownLaiDensity,
			Shaded.Bands, Params, CellSizeCm, IncidentLight, &Raster, QueryHeightCm, /*Stride=*/1, QueryBand);
		ASSERT_THAT(IsTrue(CountShadedCells(Raster) == WideCrownCellCount,
			TEXT("Precondition: re-dirty the buffer before the invalid-bounds arm")));

		FArcForestSimParams InvalidBoundsParams = Params;
		InvalidBoundsParams.Bounds2D = FBox2f(ForceInit);
		LightModel.ComputeIncidentLight(
			NoPositions, NoFloats, NoFloats, NoFloats, NoBands, InvalidBoundsParams, CellSizeCm,
			IncidentLight, &Raster, QueryHeightCm, /*Stride=*/1, QueryBand);

		ASSERT_THAT(IsTrue(Raster.Values.Num() == 0 && Raster.Dims == FIntPoint::ZeroValue,
			FString::Printf(TEXT("With no footprint to size from, the field must be left EMPTY (documented "
				"contract) rather than carrying the previous stand's %d values"), Raster.Values.Num())));
		ASSERT_THAT(IsTrue(Raster.SampleNearest(FVector2f(1650.0f, 1650.0f)) == 1.0f,
			TEXT("An empty field must sample as exactly 1.0 — that is what makes the empty answer 'full "
				"light' and not 'blackout'")));

		// --- D. the effective-stride cap ---------------------------------------------------------------
		FArcForestSimParams HugeBoundsParams = Params;
		HugeBoundsParams.Bounds2D = OriginBounds(100000.0f); // 1000 x 1000 light cells
		LightModel.ComputeIncidentLight(
			NoPositions, NoFloats, NoFloats, NoFloats, NoBands, HugeBoundsParams, CellSizeCm,
			IncidentLight, &Raster, QueryHeightCm, /*Stride=*/1, QueryBand);

		UE_LOGF(LogArcForestTransmittanceTest, Display,"[EmptyStand] cap arm dims=%dx%d values=%d min=%.7f",
			Raster.Dims.X, Raster.Dims.Y, Raster.Values.Num(), MinValue(Raster));

		// 1000 cells / EffStride 4 = 250 raster cells per axis. Requested Stride was 1, so the CAP — not
		// the request — decided this: Max(1, DivideAndRoundUp(1000, 256)) = 4.
		ASSERT_THAT(IsTrue(Raster.Dims.X == 250 && Raster.Dims.Y == 250,
			FString::Printf(TEXT("A 1000x1000-cell footprint at Stride = 1 must GROW the effective stride "
				"to 4 and produce 250x250 (never 1000x1000, never a truncated 256x256); got %dx%d"),
				Raster.Dims.X, Raster.Dims.Y)));
		ASSERT_THAT(IsTrue(Raster.Dims.X <= ArcForestLight::MaxTransmittanceDim &&
			Raster.Dims.Y <= ArcForestLight::MaxTransmittanceDim,
			TEXT("Raster dims must never exceed MaxTransmittanceDim per axis")));
		ASSERT_THAT(IsTrue(Raster.Values.Num() == 250 * 250 && FindFirstNonUnity(Raster) == INDEX_NONE,
			TEXT("The capped early-out raster is fully sized and uniformly 1.0")));
	}

	// ============================================================================================
	// 4. SHADE GEOMETRY — under the crown is materially darker than the open, and matches exp(-k*LAI)
	//    for the known crown.
	//
	// GUARDS: the emit path's exp/extinction arithmetic and the raster's FOOTPRINT
	// (ResolveRasterLayout, :146-161), which is what makes FArcForestScalarField's samplers agree with
	// the block index of a position's LAI cell.
	//
	// THE STAND: one 2000 cm Canopy plant, crown 450 cm, LAI 2.0, at cell (16,16) of a 32x32 grid;
	// query height 10 cm (Field band), Stride = 1.
	//
	// MEASURED both sides: crown cell 0.3678794 vs open cell 1.0000000, separation 0.6321206 against an
	// asserted floor of 0.3 (the floor sits ~2x below the measured gap, and ~0.37 above the crown value).
	// Shaded-cell count 69 (closed form), so the assertion is not "everything is dark".
	//
	// MUTANTS THIS TURNS RED: dropping the exp (value 2.0 or -1.0, not 0.3679); the wrong extinction k;
	// a raster footprint sized to the LAI grid extent instead of Dims*Stride*CellSize (the sampler
	// agreement assertion shears by up to a block); and any mapping error that puts the shade in the
	// wrong cell (the 69-cell count is centred on the crown cell, which is asserted by value).
	TEST_METHOD(Transmittance_UnderCanopyIsDarker)
	{
		using namespace ArcForestTransmittanceTestsLocal;

		constexpr float QueryHeightCm = 10.0f;
		const EArcForestLayer QueryBand = BandOfHeight(QueryHeightCm);

		FHandBuiltStand Stand;
		const FVector2f CrownPos = CellCentre(16, 16);
		Stand.Add(CrownPos, /*HeightCm=*/2000.0f, WideCrownRadiusCm);
		ASSERT_THAT(IsTrue(Stand.Bands[0] == EArcForestLayer::Canopy,
			TEXT("Fixture precondition: the shade source is a Canopy-band plant, so cross-band shade "
				"(never gated) is what reaches the Field-band query height")));

		FArcForestSimParams Params;
		Params.CanopyExtinction = ExtinctionK;
		Params.Bounds2D = OriginBounds(3200.0f);

		const FArcForestLightModel LightModel;
		TArray<float> IncidentLight;
		FArcForestScalarField Raster;
		LightModel.ComputeIncidentLight(
			Stand.State.Pos, Stand.State.Height, Stand.State.CrownRadius, Stand.CrownLaiDensity,
			Stand.Bands, Params, CellSizeCm, IncidentLight, &Raster, QueryHeightCm, /*Stride=*/1, QueryBand);

		ASSERT_THAT(IsTrue(Raster.Dims.X == 32 && Raster.Dims.Y == 32 && Raster.Values.Num() == 32 * 32,
			TEXT("Fixture precondition: a fully sized 32x32 raster (Stride = 1 not silently grown)")));

		const FVector2f OpenPos = CellCentre(0, 0);
		const float UnderCrown = Raster.Values[FlatIndex(Raster.Dims, 16, 16)];
		const float InTheOpen = Raster.Values[FlatIndex(Raster.Dims, 0, 0)];
		const int32 ShadedCells = CountShadedCells(Raster);
		const float Expected = Transmittance(CrownLai);

		UE_LOGF(LogArcForestTransmittanceTest, Display,"[UnderCanopy] underCrown=%.7f open=%.7f separation=%.7f shadedCells=%d expected=%.7f | "
				"sampleNearest(crown)=%.7f sampleNearest(open)=%.7f",
			UnderCrown, InTheOpen, InTheOpen - UnderCrown, ShadedCells, Expected,
			Raster.SampleNearest(CrownPos), Raster.SampleNearest(OpenPos));

		ASSERT_THAT(IsTrue(FMath::Abs(UnderCrown - Expected) <= 1.0e-6f,
			FString::Printf(TEXT("Under a single known crown the raster must be exp(-0.5*2.0) = %.7f; got "
				"%.7f"), Expected, UnderCrown)));
		ASSERT_THAT(IsTrue(InTheOpen == 1.0f,
			FString::Printf(TEXT("A cell far outside the crown must be EXACTLY full light; got %.7f"),
				InTheOpen)));
		ASSERT_THAT(IsTrue(InTheOpen - UnderCrown > 0.3f,
			FString::Printf(TEXT("Under-crown must be MATERIALLY darker than the open: measured separation "
				"%.7f against a 0.3 floor"), InTheOpen - UnderCrown)));
		ASSERT_THAT(IsTrue(ShadedCells == WideCrownCellCount,
			FString::Printf(TEXT("The 450 cm crown must darken exactly %d cells — the disc of lattice "
				"points with i^2+j^2 <= 20.25, no boundary ties; got %d"),
				WideCrownCellCount, ShadedCells)));
		ASSERT_THAT(IsTrue(ShadedCells < Raster.Values.Num(),
			TEXT("Non-vacuity: the raster must NOT be uniformly shaded, or 'darker under the crown' would "
				"be unfalsifiable")));

		// THE SAMPLER PATH (kept out of the bit-exact equality test because the samplers clamp to [0,1]).
		// Every value here is already inside [0,1], so the clamp is a no-op and the sampler's block
		// mapping is what is actually being asserted.
		ASSERT_THAT(IsTrue(Raster.SampleNearest(CrownPos) == UnderCrown,
			TEXT("SampleNearest at the crown position must resolve to the SAME cell the sweep splatted "
				"into — the raster's Bounds are sized Dims*Stride*CellSize precisely so the two cell "
				"derivations agree")));
		ASSERT_THAT(IsTrue(Raster.SampleNearest(OpenPos) == InTheOpen,
			TEXT("SampleNearest in the open must resolve to the open cell")));
	}

	// ============================================================================================
	// 5. THE SELF-BAND FLAG — flipping IntraLayerShading for the QUERY band moves the raster in the
	//    documented direction (ON adds the query band's own LAI, so the value can only get DARKER).
	//
	// GUARDS: the raster's self-band gate at ArcForestLightModel.cpp:414-427, which must mirror the
	// per-plant read's gate at :469-481.
	//
	// THE STAND (both plants at the same cell centre, crown 40 cm = one cell each):
	//     idx0  h=2000 cm  band Canopy      — CROSS-band shade, never gated
	//     idx1  h=1000 cm  band Understory  — SAME band as the query height, so the gate decides it
	// QueryHeightCm = 500 -> Understory. Both plants are strictly taller than the query, so both crowns
	// are in the accumulators either way and the ONLY variable is the flag.
	//
	// MEASURED both sides at the query cell:
	//     flag OFF (shipped default) = exp(-0.5*2.0) = 0.3678794  <- Canopy only
	//     flag ON                    = exp(-0.5*4.0) = 0.1353353  <- Canopy + Understory
	// Directional gap 0.2325441, asserted with a strict `<` plus closed-form equality on BOTH arms.
	// Neither arm is 1.0, so neither is a vacuous "no shade at all" reading.
	//
	// MUTANT THIS TURNS RED — THE SELF-BAND MUTANT: ignore IntraLayerShading[QueryBandIndex] and always
	// include the query band. Both arms collapse to 0.1353353: the OFF closed-form assertion flips RED
	// and so does the strict inequality. A mutant that inverted the gate instead would collapse both to
	// 0.3678794 and flip the ON assertion — the direction is pinned, not merely the change.
	TEST_METHOD(Transmittance_SelfBandFlagRespected)
	{
		using namespace ArcForestTransmittanceTestsLocal;

		constexpr float QueryHeightCm = 500.0f;
		const EArcForestLayer QueryBand = BandOfHeight(QueryHeightCm);
		ASSERT_THAT(IsTrue(QueryBand == EArcForestLayer::Understory,
			TEXT("Fixture precondition: the query height must be in the Understory band")));

		FHandBuiltStand Stand;
		const FVector2f QueryPos = CellCentre(16, 16);
		Stand.Add(QueryPos, /*HeightCm=*/2000.0f, SingleCellRadiusCm); // Canopy — cross-band, never gated
		Stand.Add(QueryPos, /*HeightCm=*/1000.0f, SingleCellRadiusCm); // Understory — the gated band
		ASSERT_THAT(IsTrue(Stand.Bands[0] == EArcForestLayer::Canopy && Stand.Bands[1] == QueryBand,
			TEXT("Fixture precondition: exactly ONE plant shares the query band, and one does not — that "
				"is what makes the gate's effect separable from 'all shade disappeared'")));
		ASSERT_THAT(IsTrue(Stand.State.Height[1] > QueryHeightCm,
			TEXT("Fixture precondition: the same-band plant must be STRICTLY TALLER than the query height, "
				"or its crown would not be in the accumulators at capture time in either arm")));

		FArcForestSimParams FlagOff;
		FlagOff.CanopyExtinction = ExtinctionK;
		FlagOff.Bounds2D = OriginBounds(3200.0f);
		ASSERT_THAT(IsTrue(!FlagOff.IntraLayerShading[static_cast<int32>(EArcForestLayer::Understory)],
			TEXT("Fixture precondition: the SHIPPED default for the Understory band is OFF (Field-only "
				"flags), so this arm is the production configuration")));

		FArcForestSimParams FlagOn = FlagOff;
		FlagOn.IntraLayerShading[static_cast<int32>(EArcForestLayer::Understory)] = true;

		const FArcForestLightModel LightModel;
		TArray<float> IncidentLight;

		FArcForestScalarField RasterOff;
		LightModel.ComputeIncidentLight(
			Stand.State.Pos, Stand.State.Height, Stand.State.CrownRadius, Stand.CrownLaiDensity,
			Stand.Bands, FlagOff, CellSizeCm, IncidentLight, &RasterOff, QueryHeightCm, /*Stride=*/1, QueryBand);

		FArcForestScalarField RasterOn;
		LightModel.ComputeIncidentLight(
			Stand.State.Pos, Stand.State.Height, Stand.State.CrownRadius, Stand.CrownLaiDensity,
			Stand.Bands, FlagOn, CellSizeCm, IncidentLight, &RasterOn, QueryHeightCm, /*Stride=*/1, QueryBand);

		ASSERT_THAT(IsTrue(RasterOff.Values.Num() == 32 * 32 && RasterOn.Values.Num() == 32 * 32,
			TEXT("Fixture precondition: both rasters fully sized 32x32 before any value is read")));

		const int32 QueryCell = FlatIndex(RasterOff.Dims, 16, 16);
		const float ValueOff = RasterOff.Values[QueryCell];
		const float ValueOn = RasterOn.Values[QueryCell];
		const float ExpectedOff = Transmittance(CrownLai);          // Canopy only
		const float ExpectedOn = Transmittance(CrownLai * 2.0f);    // Canopy + Understory

		UE_LOGF(LogArcForestTransmittanceTest, Display,"[SelfBandFlag] off=%.7f (expected %.7f) | on=%.7f (expected %.7f) | gap=%.7f",
			ValueOff, ExpectedOff, ValueOn, ExpectedOn, ValueOff - ValueOn);

		ASSERT_THAT(IsTrue(FMath::Abs(ValueOff - ExpectedOff) <= 1.0e-6f,
			FString::Printf(TEXT("IntraLayerShading[Understory] OFF must EXCLUDE the same-band crown: only "
				"the Canopy crown shades, exp(-0.5*2.0) = %.7f; got %.7f"), ExpectedOff, ValueOff)));
		ASSERT_THAT(IsTrue(FMath::Abs(ValueOn - ExpectedOn) <= 1.0e-6f,
			FString::Printf(TEXT("IntraLayerShading[Understory] ON must INCLUDE it: exp(-0.5*4.0) = %.7f; "
				"got %.7f"), ExpectedOn, ValueOn)));
		ASSERT_THAT(IsTrue(ValueOn < ValueOff,
			FString::Printf(TEXT("DIRECTION: enabling intra-layer self-shading can only ADD LAI, so the "
				"raster must get DARKER, never brighter or merely different. Measured on=%.7f off=%.7f"),
				ValueOn, ValueOff)));
		ASSERT_THAT(IsTrue(ValueOff < 1.0f && ValueOn > 0.0f,
			TEXT("Non-vacuity: cross-band shade applies in BOTH arms (neither is full light), and neither "
				"arm is a degenerate zero")));
	}

	// ============================================================================================
	// 6. STRIDE AGGREGATION — a block's value is exp(-k * MEAN LAI over the block's REAL cell count),
	//    not a point sample, not the mean of per-cell transmittances, and not a Stride^2 division.
	//
	// GUARDS: EmitTransmittance's two passes (ArcForestLightModel.cpp:229-264) — the block sum, the
	// BlockCells divisor built from FMath::Min against the grid dims (:254-259), and the single exp per
	// raster cell (:262).
	//
	// THE STAND — footprint 3000 cm at 100 cm cells = 30x30 light cells, Stride = 4, so raster dims are
	// ceil(30/4) = 8x8 and the LAST block of each axis is PARTIAL (cells 28-29 only = 2x2 = 4 cells, not
	// 16). Two plants, each crown 40 cm so each splats EXACTLY ONE light cell, LAI 2.0, both at 2000 cm:
	//     interior block (0,0): plant at cell (0,0)   -> 1 of 16 cells carries LAI 2.0
	//     trailing block (7,7): plant at cell (28,28) -> 1 of  4 cells carries LAI 2.0
	// LAI therefore VARIES inside both blocks, which is what makes the averaging observable at all.
	//
	// MEASURED, all four numbers on this branch:
	//     interior  LAI-averaged  exp(-0.5 * 2.0/16) = exp(-0.0625) = 0.9394131  <- asserted
	//     interior  mean-after-exp (15 + exp(-1))/16              = 0.9604925  <- Jensen mutant, gap 0.0210794
	//     trailing  real cell count exp(-0.5 * 2.0/4) = exp(-0.25) = 0.7788008  <- asserted
	//     trailing  Stride^2 divisor exp(-0.5 * 2.0/16)           = 0.9394131  <- mutant, gap 0.1606123
	// Each asserted value carries a 1e-6 tolerance and each mutant value is asserted AGAINST with a
	// floor 20x-160x below the measured gap.
	//
	// MUTANTS THIS TURNS RED:
	//   * THE STRIDE^2 MUTANT — divide by Stride*Stride instead of the real BlockCells. Only the
	//     trailing block moves (interior blocks have 16 real cells anyway), and it moves by 0.16.
	//   * THE MEAN-AFTER-EXP MUTANT — mean(exp(-k*LAI)) instead of exp(-k*mean(LAI)). Jensen guarantees
	//     a difference for non-constant LAI, and both blocks here have non-constant LAI.
	//   * A POINT SAMPLE of the block's corner cell: 1.0 for the interior block (the plant sits in cell
	//     (0,0), which IS the corner, so a corner sample would read exp(-1) = 0.3679 instead) — either
	//     way both closed forms flip.
	TEST_METHOD(Transmittance_StrideAveragesLaiNotPointSamples)
	{
		using namespace ArcForestTransmittanceTestsLocal;

		constexpr float QueryHeightCm = 10.0f;
		constexpr int32 Stride = 4;
		constexpr int32 GridDim = 30;      // 3000 cm / 100 cm
		constexpr int32 RasterDim = 8;     // ceil(30 / 4)
		constexpr int32 InteriorBlockCells = Stride * Stride; // 16
		constexpr int32 TrailingBlockCells = 4;               // cells 28,29 on each axis
		const EArcForestLayer QueryBand = BandOfHeight(QueryHeightCm);

		FHandBuiltStand Stand;
		const FVector2f InteriorPos = CellCentre(0, 0);
		const FVector2f TrailingPos = CellCentre(28, 28);
		Stand.Add(InteriorPos, /*HeightCm=*/2000.0f, SingleCellRadiusCm);
		Stand.Add(TrailingPos, /*HeightCm=*/2000.0f, SingleCellRadiusCm);

		FArcForestSimParams Params;
		Params.CanopyExtinction = ExtinctionK;
		Params.Bounds2D = OriginBounds(static_cast<float>(GridDim) * CellSizeCm);

		const FArcForestLightModel LightModel;
		TArray<float> IncidentLight;
		FArcForestScalarField Raster;
		LightModel.ComputeIncidentLight(
			Stand.State.Pos, Stand.State.Height, Stand.State.CrownRadius, Stand.CrownLaiDensity,
			Stand.Bands, Params, CellSizeCm, IncidentLight, &Raster, QueryHeightCm, Stride, QueryBand);

		// The whole test rests on the trailing block being PARTIAL, so pin the layout first.
		ASSERT_THAT(IsTrue(Raster.Dims.X == RasterDim && Raster.Dims.Y == RasterDim,
			FString::Printf(TEXT("Fixture precondition: %d light cells at stride %d must give a %dx%d "
				"raster (so block %d is PARTIAL — 2 cells per axis); got %dx%d"),
				GridDim, Stride, RasterDim, RasterDim, RasterDim - 1, Raster.Dims.X, Raster.Dims.Y)));
		ASSERT_THAT(IsTrue(Raster.Values.Num() == RasterDim * RasterDim && Raster.Values.Num() > 0,
			TEXT("Non-vacuity: the raster must be fully sized before any block value is read")));
		ASSERT_THAT(IsTrue(GridDim % Stride != 0,
			TEXT("Fixture precondition: the grid dim must NOT be a multiple of the stride, or there would "
				"be no partial trailing block and the Stride^2 mutant would be invisible")));

		const float InteriorValue = Raster.Values[FlatIndex(Raster.Dims, 0, 0)];
		const float TrailingValue = Raster.Values[FlatIndex(Raster.Dims, RasterDim - 1, RasterDim - 1)];

		const float InteriorLaiAveraged = Transmittance(CrownLai / static_cast<float>(InteriorBlockCells));
		const float InteriorMeanAfterExp =
			(static_cast<float>(InteriorBlockCells - 1) + Transmittance(CrownLai)) /
			static_cast<float>(InteriorBlockCells);
		const float TrailingRealCount = Transmittance(CrownLai / static_cast<float>(TrailingBlockCells));
		const float TrailingStrideSquared = Transmittance(CrownLai / static_cast<float>(InteriorBlockCells));

		UE_LOGF(LogArcForestTransmittanceTest, Display,"[StrideAverages] interior=%.7f (lai-avg %.7f, mean-after-exp %.7f, gap %.7f) | "
				"trailing=%.7f (real-count %.7f, stride^2 %.7f, gap %.7f) | sampleNearest interior=%.7f "
				"trailing=%.7f",
			InteriorValue, InteriorLaiAveraged, InteriorMeanAfterExp,
			FMath::Abs(InteriorLaiAveraged - InteriorMeanAfterExp),
			TrailingValue, TrailingRealCount, TrailingStrideSquared,
			FMath::Abs(TrailingRealCount - TrailingStrideSquared),
			Raster.SampleNearest(InteriorPos), Raster.SampleNearest(TrailingPos));

		// --- the interior block: averaged BEFORE the exp -----------------------------------------------
		ASSERT_THAT(IsTrue(FMath::Abs(InteriorValue - InteriorLaiAveraged) <= 1.0e-6f,
			FString::Printf(TEXT("A 4x4 block with LAI 2.0 in ONE of its 16 cells must read "
				"exp(-0.5 * 2.0/16) = %.7f (mean LAI, then ONE exp); got %.7f"),
				InteriorLaiAveraged, InteriorValue)));
		ASSERT_THAT(IsTrue(FMath::Abs(InteriorValue - InteriorMeanAfterExp) > 1.0e-3f,
			FString::Printf(TEXT("It must NOT be the mean of per-cell transmittances (%.7f) — Jensen "
				"separates the two forms for non-constant LAI. Measured gap %.7f against a 1e-3 floor"),
				InteriorMeanAfterExp, FMath::Abs(InteriorValue - InteriorMeanAfterExp))));
		ASSERT_THAT(IsTrue(InteriorValue < 1.0f && InteriorValue > Transmittance(CrownLai),
			FString::Printf(TEXT("Non-vacuity + not a point sample: the block must be dimmer than open sky "
				"(1.0) yet BRIGHTER than the single shaded cell's own exp(-1) = %.7f; got %.7f"),
				Transmittance(CrownLai), InteriorValue)));

		// --- the trailing block: divided by its REAL cell count, not Stride^2 -------------------------
		ASSERT_THAT(IsTrue(FMath::Abs(TrailingValue - TrailingRealCount) <= 1.0e-6f,
			FString::Printf(TEXT("The PARTIAL trailing block holds only 4 real cells, so LAI 2.0 in one of "
				"them means exp(-0.5 * 2.0/4) = %.7f; got %.7f"), TrailingRealCount, TrailingValue)));
		ASSERT_THAT(IsTrue(FMath::Abs(TrailingValue - TrailingStrideSquared) > 1.0e-2f,
			FString::Printf(TEXT("Dividing the partial block by Stride^2 = 16 would DILUTE the edge to "
				"%.7f. Measured gap %.7f against a 1e-2 floor"),
				TrailingStrideSquared, FMath::Abs(TrailingValue - TrailingStrideSquared))));
		ASSERT_THAT(IsTrue(TrailingValue < InteriorValue,
			FString::Printf(TEXT("Non-vacuity: the same single shaded cell averaged over 4 cells must be "
				"darker than averaged over 16 (measured %.7f vs %.7f) — if these matched, the divisor "
				"would not be the block's real cell count"), TrailingValue, InteriorValue)));

		// The sampler must land in the same blocks the assertions read by index (all values are inside
		// [0,1] here, so the samplers' clamp is a no-op).
		ASSERT_THAT(IsTrue(Raster.SampleNearest(InteriorPos) == InteriorValue,
			TEXT("SampleNearest at the interior plant must resolve into ITS block: the raster's Bounds are "
				"Dims*Stride*CellSize precisely so floor(floor(x/CellSize)/Stride) == the sampler's block")));
		ASSERT_THAT(IsTrue(Raster.SampleNearest(TrailingPos) == TrailingValue,
			TEXT("SampleNearest at the trailing plant must resolve into the PARTIAL trailing block — "
				"sizing the raster Bounds to the LAI grid extent instead would shear this by a block")));
	}
};
