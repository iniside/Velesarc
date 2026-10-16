// Copyright Lukasz Baran. All Rights Reserved.

// ArcForestTerrainFieldsTests.cpp
//
// CLOSED-FORM coverage of ArcForestTerrainFields::Derive (Forest/ArcForestTerrainFields.{h,cpp},
// landed f247caabe8f3, no-data semantics doc-corrected 79af41e7778f) and of
// ArcForestField::SampleFieldBilinear (Forest/ArcForestField.{h,cpp}), which ships unused until
// Step 7 and is otherwise only exercised indirectly.
//
// Every expected value below is derived from the header's documented convention plus closed-form
// trigonometry/algebra (FMath::Atan/Atan2/Cos are generic primitives, not the code under test) —
// NEVER by running Derive()/SampleFieldBilinear() and pinning what came out. A RED here is reported
// as a possible production defect, not silently re-baselined.
//
// No RNG, no simulation, no PCG, no fixture asset: height grids and masks are plain in-test arrays,
// and Derive()/SampleFieldBilinear() are called directly — this is what keeps the suite fast and exact.

#include "CQTest.h"

#include "Forest/ArcForestField.h"
#include "Forest/ArcForestTerrainFields.h"

namespace ArcForestTerrainFieldsTestLocal
{
	/** atan(Gradient) in degrees, normalised by MaxSlopeDegrees — the same formula Derive() applies. */
	float AnalyticNormalizedSlope(float GradientMagnitude, float MaxSlopeDegrees)
	{
		const float SlopeDegrees = FMath::RadiansToDegrees(FMath::Atan(GradientMagnitude));
		return FMath::Clamp(SlopeDegrees / MaxSlopeDegrees, 0.0f, 1.0f);
	}

	/** 0.5*(1+cos(Aspect-Azimuth)) for the steepest-DESCENT aspect atan2(-dhdy,-dhdx). */
	float AnalyticExposure(float DhDx, float DhDy, float AzimuthDegrees)
	{
		const float Aspect = FMath::Atan2(-DhDy, -DhDx);
		const float AzimuthRadians = FMath::DegreesToRadians(AzimuthDegrees);
		return FMath::Clamp(0.5f * (1.0f + FMath::Cos(Aspect - AzimuthRadians)), 0.0f, 1.0f);
	}

	/** Builds a row-major height grid where every row is identical: h(col) = Slope*col. */
	TArray<float> MakeXRamp(int32 DimX, int32 DimY, float Slope)
	{
		TArray<float> Heights;
		Heights.SetNumUninitialized(DimX * DimY);
		for (int32 Row = 0; Row < DimY; ++Row)
		{
			for (int32 Col = 0; Col < DimX; ++Col)
			{
				Heights[(Row * DimX) + Col] = Slope * static_cast<float>(Col);
			}
		}
		return Heights;
	}

	/** All-true mask of the given cell count (equivalent to an empty HitGrid, but explicit). */
	TArray<bool> MakeAllValidMask(int32 CellCount)
	{
		TArray<bool> Mask;
		Mask.Init(true, CellCount);
		return Mask;
	}
}

TEST_CLASS(ArcForestTerrainFieldsAndBilinear, "ArcPCGExtension.ForestSim.TerrainFields")
{
	FArcForestTerrainFieldParams DefaultParams;

	TEST_METHOD(Slope_ConstantRampMatchesAnalyticGradient)
	{
		using namespace ArcForestTerrainFieldsTestLocal;

		// 5x5 grid, square 100cm cells (Bounds 500x500), height rises 20 per column, flat per row.
		// Because the ramp is perfectly LINEAR, the one-sided border difference and the central
		// interior difference produce the IDENTICAL analytic gradient (0.2). A halved-border bug
		// (clamped-neighbour central difference over 2*Spacing) would read 0.1 at every border cell
		// instead, so asserting the border is exactly what discriminates it.
		constexpr int32 DimX = 5;
		constexpr int32 DimY = 5;
		constexpr float RampSlope = 20.0f; // height units per column
		const TArray<float> Heights = MakeXRamp(DimX, DimY, RampSlope);
		const TArray<bool> HitGrid = MakeAllValidMask(DimX * DimY);
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(500.0f, 500.0f));
		const float Dx = 500.0f / static_cast<float>(DimX); // 100
		const float AnalyticGradient = RampSlope / Dx; // 0.2

		FArcForestTerrainFieldParams Params = DefaultParams;
		Params.MaxSlopeDegrees = 45.0f;
		FArcForestScalarField Slope;
		FArcForestScalarField Exposure;
		FArcForestScalarField Elevation;
		ArcForestTerrainFields::Derive(Heights, HitGrid, FIntPoint(DimX, DimY), Bounds, Params, Slope, Exposure, Elevation);

		const float Expected = AnalyticNormalizedSlope(AnalyticGradient, Params.MaxSlopeDegrees);
		for (int32 Row = 0; Row < DimY; ++Row)
		{
			for (int32 Col = 0; Col < DimX; ++Col)
			{
				const int32 Index = (Row * DimX) + Col;
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Slope.Values[Index], Expected, 1e-4f),
					*FString::Printf(TEXT("Row %d Col %d: expected %f got %f (border must match interior, one-sided rule)"),
						Row, Col, Expected, Slope.Values[Index])));
			}
		}
	}

	TEST_METHOD(Slope_AnisotropicCellsUseSeparateDxDy)
	{
		using namespace ArcForestTerrainFieldsTestLocal;

		// Non-square Bounds2D: X span 300 (Dx=100), Y span 600 (Dy=200) over a 3x3 grid. Ramp
		// along the SHORT axis (X) only, so dh/dy = 0 everywhere and the correct gradient is
		// RampSlope/Dx = 40/100 = 0.4. If the implementation mistakenly derived a single "cell
		// size" or swapped axes (using Dy=200 for the X gradient), it would read 40/200 = 0.2
		// instead -- atan(0.4)=21.80deg vs atan(0.2)=11.31deg, a visibly different number.
		constexpr int32 DimX = 3;
		constexpr int32 DimY = 3;
		constexpr float RampSlope = 40.0f;
		const TArray<float> Heights = MakeXRamp(DimX, DimY, RampSlope);
		const TArray<bool> HitGrid = MakeAllValidMask(DimX * DimY);
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(300.0f, 600.0f));
		const float Dx = 300.0f / static_cast<float>(DimX); // 100
		const float CorrectGradient = RampSlope / Dx; // 0.4
		const float SquareCellAssumptionGradient = RampSlope / (600.0f / static_cast<float>(DimY)); // 0.2, WRONG

		FArcForestTerrainFieldParams Params = DefaultParams;
		Params.MaxSlopeDegrees = 45.0f;
		FArcForestScalarField Slope;
		FArcForestScalarField Exposure;
		FArcForestScalarField Elevation;
		ArcForestTerrainFields::Derive(Heights, HitGrid, FIntPoint(DimX, DimY), Bounds, Params, Slope, Exposure, Elevation);

		const float Expected = AnalyticNormalizedSlope(CorrectGradient, Params.MaxSlopeDegrees);
		const float WrongIfSquareAssumed = AnalyticNormalizedSlope(SquareCellAssumptionGradient, Params.MaxSlopeDegrees);
		ASSERT_THAT(IsTrue(!FMath::IsNearlyEqual(Expected, WrongIfSquareAssumed, 1e-3f),
			TEXT("Test setup error: the correct and square-cell-assumption answers must differ")));

		// Middle cell is fully interior on both axes -- the cleanest read.
		const int32 CentreIndex = (1 * DimX) + 1;
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Slope.Values[CentreIndex], Expected, 1e-4f),
			*FString::Printf(TEXT("Centre cell: expected %f (Dx=100) got %f (would be %f if Dy=200 were used for X)"),
				Expected, Slope.Values[CentreIndex], WrongIfSquareAssumed)));
	}

	TEST_METHOD(Slope_MaxSlopeDegreesClampAndBinaryArmAreExercised)
	{
		using namespace ArcForestTerrainFieldsTestLocal;

		// GAP 7: every OTHER slope fixture in this file uses MaxSlopeDegrees=45, so a hardcoded-45
		// mutant survives the whole suite, the [0,1] UPPER clamp is never reached (atan of every
		// fixture's gradient stays under 45 degrees), and the documented "MaxSlopeDegrees <= 0
		// degenerates to a binary field" arm is dead code in this file.
		constexpr int32 DimX = 3;
		constexpr int32 DimY = 3;
		const TArray<float> RampHeights = MakeXRamp(DimX, DimY, 40.0f); // RampSlope/Dx = 40/100 = 0.4
		const TArray<bool> HitGrid = MakeAllValidMask(DimX * DimY);
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(300.0f, 300.0f));
		const int32 CentreIndex = (1 * DimX) + 1;

		// (a) MaxSlopeDegrees=15: atan(0.4)=21.8014 degrees / 15 = 1.4534 -> clamps to 1.0. A
		// clamp-removed mutant would read 1.4534 (fails the [0,1] contract outright); a
		// hardcoded-45 mutant would read atan(0.4)/45 = 0.4845. Both differ from 1.0.
		{
			FArcForestTerrainFieldParams Params;
			Params.MaxSlopeDegrees = 15.0f;
			FArcForestScalarField Slope, Exposure, Elevation;
			ArcForestTerrainFields::Derive(RampHeights, HitGrid, FIntPoint(DimX, DimY), Bounds, Params, Slope, Exposure, Elevation);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Slope.Values[CentreIndex], 1.0f, 1e-5f),
				*FString::Printf(TEXT("MaxSlopeDegrees=15: expected clamp to 1.0 (atan(0.4)/15=1.4534), got %f"), Slope.Values[CentreIndex])));
		}

		// (b) MaxSlopeDegrees=0: degenerates to a binary field -- any non-zero gradient -> 1.0,
		// exactly flat -> 0.0. This is the ONLY way to execute that documented branch in this file.
		{
			FArcForestTerrainFieldParams Params;
			Params.MaxSlopeDegrees = 0.0f;
			FArcForestScalarField Slope, Exposure, Elevation;
			ArcForestTerrainFields::Derive(RampHeights, HitGrid, FIntPoint(DimX, DimY), Bounds, Params, Slope, Exposure, Elevation);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Slope.Values[CentreIndex], 1.0f, 1e-6f),
				*FString::Printf(TEXT("MaxSlopeDegrees=0, ramp: expected binary 1.0, got %f"), Slope.Values[CentreIndex])));

			TArray<float> FlatHeights;
			FlatHeights.Init(250.0f, DimX * DimY);
			FArcForestScalarField SlopeFlat, ExposureFlat, ElevationFlat;
			ArcForestTerrainFields::Derive(FlatHeights, HitGrid, FIntPoint(DimX, DimY), Bounds, Params, SlopeFlat, ExposureFlat, ElevationFlat);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(SlopeFlat.Values[CentreIndex], 0.0f, 1e-6f),
				*FString::Printf(TEXT("MaxSlopeDegrees=0, flat: expected binary 0.0, got %f"), SlopeFlat.Values[CentreIndex])));
		}
	}

	TEST_METHOD(Slope_NonLinearProfileKillsOneSidedStencilMutant)
	{
		using namespace ArcForestTerrainFieldsTestLocal;

		// GAP 8: every height fixture elsewhere in this file is AFFINE (constant gradient), so a
		// mutant that replaces the interior central difference with a forward one-sided difference
		// returns IDENTICAL numbers everywhere on an affine ramp and survives the whole suite, while
		// introducing a real half-cell phase shift on curved terrain in production.
		//
		// h(col) = 100*col^2 on DimX=5 (Bounds.X=500, Dx=100), single row (DimY=1, Y degenerate):
		// {0,100,400,900,1600}. Documented rule: interior cells use the central difference over
		// 2*Dx, border cells use the one-sided difference over Dx.
		//   col0 (border):   (100-0)/100     = 1.0
		//   col1 (interior): (400-0)/200      = 2.0
		//   col2 (interior): (900-100)/200    = 4.0
		//   col3 (interior): (1600-400)/200   = 6.0
		//   col4 (border):   (1600-900)/100   = 7.0
		// A forward-one-sided-everywhere mutant (h[i+1]-h[i])/Dx would read col1 as (400-100)/100=3.0
		// instead of 2.0; a clamped-neighbour-central mutant would read col0 as (100-0)/200=0.5
		// instead of 1.0. This asserts the STATED production formula's result directly
		// (pre-normalization gradient, not raw), via AnalyticNormalizedSlope, at a MaxSlopeDegrees=90
		// chosen so no value clamps to 1.0 and all five stay numerically distinguishable (atan
		// approaches but never reaches 90 degrees).
		constexpr int32 DimX = 5;
		constexpr int32 DimY = 1;
		const TArray<float> Heights = { 0.0f, 100.0f, 400.0f, 900.0f, 1600.0f };
		const TArray<bool> HitGrid = MakeAllValidMask(DimX * DimY);
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(500.0f, 100.0f)); // Dx=100, Y degenerate

		FArcForestTerrainFieldParams Params;
		Params.MaxSlopeDegrees = 90.0f;
		FArcForestScalarField Slope, Exposure, Elevation;
		ArcForestTerrainFields::Derive(Heights, HitGrid, FIntPoint(DimX, DimY), Bounds, Params, Slope, Exposure, Elevation);

		const float ExpectedGradients[DimX] = { 1.0f, 2.0f, 4.0f, 6.0f, 7.0f };
		for (int32 Col = 0; Col < DimX; ++Col)
		{
			const float Expected = AnalyticNormalizedSlope(ExpectedGradients[Col], Params.MaxSlopeDegrees);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Slope.Values[Col], Expected, 1e-4f),
				*FString::Printf(TEXT("Quadratic col %d: expected gradient %f (normalised %f) got %f"),
					Col, ExpectedGradients[Col], Expected, Slope.Values[Col])));
		}
	}

	TEST_METHOD(Slope_DiagonalRampNeedsBothGradientTermsLive)
	{
		using namespace ArcForestTerrainFieldsTestLocal;

		// GAP 5: every OTHER slope fixture in this file is single-axis, so Sqrt(DhDx^2+DhDy^2) never
		// has both terms non-zero at once -- |dx|+|dy|, Max(|dx|,|dy|), or a dropped term all
		// reproduce the single-axis fixtures' numbers. This is a diagonal ramp so both axes carry a
		// DIFFERENT non-zero gradient.
		//
		// h(col,row) = 40*col + 40*row, non-square Bounds 300x600 over 3x3 (Dx=100, Dy=200).
		// Centre cell (1,1) is interior on both axes: DhDx = 40/100 = 0.4, DhDy = 40/200 = 0.2.
		// Correct magnitude = Sqrt(0.4^2+0.2^2) = Sqrt(0.20) = 0.4472136.
		// Dx-for-both mutant: Sqrt(0.4^2+0.4^2) = 0.5657. Dy-for-both: Sqrt(0.2^2+0.2^2) = 0.2828.
		// |dx|+|dy|: 0.6. Max(|dx|,|dy|): 0.4. All four differ from 0.4472136.
		constexpr int32 DimX = 3;
		constexpr int32 DimY = 3;
		TArray<float> Heights;
		Heights.SetNumUninitialized(DimX * DimY);
		for (int32 Row = 0; Row < DimY; ++Row)
		{
			for (int32 Col = 0; Col < DimX; ++Col)
			{
				Heights[(Row * DimX) + Col] = (40.0f * static_cast<float>(Col)) + (40.0f * static_cast<float>(Row));
			}
		}
		const TArray<bool> HitGrid = MakeAllValidMask(DimX * DimY);
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(300.0f, 600.0f));
		const float DhDx = 40.0f / (300.0f / static_cast<float>(DimX)); // 0.4
		const float DhDy = 40.0f / (600.0f / static_cast<float>(DimY)); // 0.2
		const float CorrectMagnitude = FMath::Sqrt((DhDx * DhDx) + (DhDy * DhDy)); // 0.4472136

		FArcForestTerrainFieldParams Params;
		Params.MaxSlopeDegrees = 45.0f;
		Params.SunAzimuthDegrees = 0.0f;
		FArcForestScalarField Slope;
		FArcForestScalarField Exposure;
		FArcForestScalarField Elevation;
		ArcForestTerrainFields::Derive(Heights, HitGrid, FIntPoint(DimX, DimY), Bounds, Params, Slope, Exposure, Elevation);

		const int32 CentreIndex = (1 * DimX) + 1;
		const float ExpectedSlope = AnalyticNormalizedSlope(CorrectMagnitude, Params.MaxSlopeDegrees);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Slope.Values[CentreIndex], ExpectedSlope, 1e-4f),
			*FString::Printf(TEXT("Diagonal centre slope: expected %f (magnitude 0.4472136) got %f"), ExpectedSlope, Slope.Values[CentreIndex])));

		// Aspect/exposure at azimuth 0: Aspect = Atan2(-DhDy,-DhDx) = Atan2(-0.2,-0.4). This is what
		// discriminates a DhDx<->DhDy SWAP inside atan2 (magnitude alone is symmetric under swap, so
		// it can't catch this): AnalyticExposure() below applies the exact same closed-form the
		// production code documents (aspect of steepest DESCENT).
		const float ExpectedExposure = AnalyticExposure(DhDx, DhDy, 0.0f);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Exposure.Values[CentreIndex], ExpectedExposure, 1e-4f),
			*FString::Printf(TEXT("Diagonal centre exposure @ azimuth 0: expected %f got %f"), ExpectedExposure, Exposure.Values[CentreIndex])));
	}

	TEST_METHOD(Aspect_DescentDirectionNotAscent)
	{
		using namespace ArcForestTerrainFieldsTestLocal;

		// Height rises along +X (h = 30*col), flat along Y. Steepest DESCENT is therefore -X, i.e.
		// Aspect = atan2(-dh/dy,-dh/dx) = atan2(0,-positive) = PI (180deg, facing -X). At azimuth 0
		// (world +X) that reads exposure 0.0; at azimuth 180 it reads 1.0. This matches the header's
		// worked example verbatim (h(col)=100*col -> "exposure 0.0 at azimuth 0").
		constexpr int32 DimX = 3;
		constexpr int32 DimY = 3;
		constexpr float RampSlope = 30.0f;
		const TArray<float> HeightsRising = MakeXRamp(DimX, DimY, RampSlope);
		const TArray<float> HeightsFalling = MakeXRamp(DimX, DimY, -RampSlope);
		const TArray<bool> HitGrid = MakeAllValidMask(DimX * DimY);
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(300.0f, 300.0f));
		const int32 CentreIndex = (1 * DimX) + 1;

		TFunction<float(const TArray<float>&, float)> ExposureAtAzimuth = [&](const TArray<float>& Heights, float AzimuthDegrees) -> float
		{
			FArcForestTerrainFieldParams Params;
			Params.SunAzimuthDegrees = AzimuthDegrees;
			FArcForestScalarField Slope;
			FArcForestScalarField Exposure;
			FArcForestScalarField Elevation;
			ArcForestTerrainFields::Derive(Heights, HitGrid, FIntPoint(DimX, DimY), Bounds, Params, Slope, Exposure, Elevation);
			return Exposure.Values[CentreIndex];
		};

		// Rising ramp: downhill faces -X. Analytic: Aspect=PI exactly (DhDy==0), independent of
		// RampSlope's magnitude, since only its SIGN matters to atan2 here.
		const float RisingAt0 = ExposureAtAzimuth(HeightsRising, 0.0f);
		const float RisingAt180 = ExposureAtAzimuth(HeightsRising, 180.0f);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(RisingAt0, AnalyticExposure(RampSlope / 100.0f, 0.0f, 0.0f), 1e-4f),
			*FString::Printf(TEXT("Rising ramp @ azimuth 0: expected 0.0 (descent faces -X), got %f"), RisingAt0)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(RisingAt180, AnalyticExposure(RampSlope / 100.0f, 0.0f, 180.0f), 1e-4f),
			*FString::Printf(TEXT("Rising ramp @ azimuth 180: expected 1.0 (descent faces -X, sun faces -X), got %f"), RisingAt180)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(RisingAt0, 0.0f, 1e-4f), TEXT("Analytic sanity: 0.0")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(RisingAt180, 1.0f, 1e-4f), TEXT("Analytic sanity: 1.0")));

		// Rotating the ramp 180 degrees (falling along +X, downhill now faces +X) must INVERT the
		// exposure at each fixed azimuth -- this is the test that catches an ascent/descent sign error.
		const float FallingAt0 = ExposureAtAzimuth(HeightsFalling, 0.0f);
		const float FallingAt180 = ExposureAtAzimuth(HeightsFalling, 180.0f);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FallingAt0, 1.0f, 1e-4f),
			*FString::Printf(TEXT("Falling ramp @ azimuth 0: expected 1.0 (descent now faces +X), got %f"), FallingAt0)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FallingAt180, 0.0f, 1e-4f),
			*FString::Printf(TEXT("Falling ramp @ azimuth 180: expected 0.0, got %f"), FallingAt180)));
	}

	TEST_METHOD(Aspect_AzimuthConventionDirectionAndYSignArePinned)
	{
		using namespace ArcForestTerrainFieldsTestLocal;

		// GAP 6: cos is EVEN and cos(A-pi) == cos(A+pi), so an azimuth-0/180 pair (as used by the
		// existing X-ramp test) cannot distinguish "+X toward +Y" from a clockwise convention, nor a
		// Y-sign error -- both alternate conventions still land on 0.0/1.0 at 0/180. A Y-only ramp
		// probed at azimuth 90 breaks that degeneracy.
		//
		// h(row) = -30*row (downhill faces +Y), flat along X, 3x3, Bounds 300x300 (Dy=100). At the
		// interior row (row=1): DhDy = (h(row2)-h(row0))/(2*100) = (-60-0)/200 = -0.3, DhDx = 0.
		// Aspect = Atan2(-DhDy,-DhDx) = Atan2(0.3,0) = +pi/2 (90 degrees, facing +Y). At azimuth 90:
		// cos(Aspect-Azimuth) = cos(0) = 1 -> exposure = 0.5*(1+1) = 1.0.
		//
		// A CLOCKWISE convention (angles increasing +X toward -Y) would compute Aspect = -pi/2, so
		// cos(-pi/2 - pi/2) = cos(-pi) = -1 -> exposure 0.0. A Y-SIGN error in the aspect formula
		// (Atan2(+DhDy,-DhDx) instead of Atan2(-DhDy,-DhDx)) gives Aspect = Atan2(-0.3,0) = -pi/2,
		// the same wrong 0.0. Both mutants are caught by this single assertion.
		constexpr int32 DimX = 3;
		constexpr int32 DimY = 3;
		TArray<float> Heights;
		Heights.SetNumUninitialized(DimX * DimY);
		for (int32 Row = 0; Row < DimY; ++Row)
		{
			for (int32 Col = 0; Col < DimX; ++Col)
			{
				Heights[(Row * DimX) + Col] = -30.0f * static_cast<float>(Row);
			}
		}
		const TArray<bool> HitGrid = MakeAllValidMask(DimX * DimY);
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(300.0f, 300.0f));

		FArcForestTerrainFieldParams Params;
		Params.SunAzimuthDegrees = 90.0f;
		FArcForestScalarField Slope;
		FArcForestScalarField Exposure;
		FArcForestScalarField Elevation;
		ArcForestTerrainFields::Derive(Heights, HitGrid, FIntPoint(DimX, DimY), Bounds, Params, Slope, Exposure, Elevation);

		const int32 CentreIndex = (1 * DimX) + 1;
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Exposure.Values[CentreIndex], 1.0f, 1e-4f),
			*FString::Printf(TEXT("Y-ramp @ azimuth 90: expected 1.0 (+X toward +Y convention), got %f"), Exposure.Values[CentreIndex])));
	}

	TEST_METHOD(Aspect_FlatGroundIsNeutral)
	{
		using namespace ArcForestTerrainFieldsTestLocal;

		// A perfectly flat grid has GradientMagnitude == 0, which trips the explicit guard rather
		// than letting Atan2(0,0) produce an azimuth-dependent answer -- exposure must be exactly
		// 0.5 no matter what SunAzimuthDegrees is.
		constexpr int32 DimX = 3;
		constexpr int32 DimY = 3;
		TArray<float> FlatHeights;
		FlatHeights.Init(250.0f, DimX * DimY);
		const TArray<bool> HitGrid = MakeAllValidMask(DimX * DimY);
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(300.0f, 300.0f));

		for (const float AzimuthDegrees : { 0.0f, 90.0f, 180.0f, 270.0f })
		{
			FArcForestTerrainFieldParams Params;
			Params.SunAzimuthDegrees = AzimuthDegrees;
			FArcForestScalarField Slope;
			FArcForestScalarField Exposure;
			FArcForestScalarField Elevation;
			ArcForestTerrainFields::Derive(FlatHeights, HitGrid, FIntPoint(DimX, DimY), Bounds, Params, Slope, Exposure, Elevation);

			for (const float Value : Exposure.Values)
			{
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Value, 0.5f, 1e-6f),
					*FString::Printf(TEXT("Flat ground @ azimuth %f: expected 0.5, got %f"), AzimuthDegrees, Value)));
			}
		}
	}

	TEST_METHOD(Elevation_UsesAbsoluteRangeNotGridExtrema)
	{
		using namespace ArcForestTerrainFieldsTestLocal;

		// Two grids with the SAME shape (a 50-unit ramp per column) but different absolute height
		// offsets, normalised against the SAME authored ElevationRangeCm. If the implementation ever
		// fell back to the grid's own observed min/max, both grids would produce IDENTICAL fields
		// (each grid re-normalising its own span to [0,1]) -- the partition-seam bug this guards.
		constexpr int32 DimX = 3;
		constexpr int32 DimY = 1;
		const TArray<float> HeightsLow = { 100.0f, 150.0f, 200.0f };
		const TArray<float> HeightsHigh = { 1000.0f, 1050.0f, 1100.0f };
		const TArray<bool> HitGrid = MakeAllValidMask(DimX * DimY);
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(300.0f, 100.0f));

		FArcForestTerrainFieldParams Params;
		Params.ElevationMinCm = 0.0f;
		Params.ElevationMaxCm = 2000.0f;

		FArcForestScalarField SlopeLow, ExposureLow, ElevationLow;
		ArcForestTerrainFields::Derive(HeightsLow, HitGrid, FIntPoint(DimX, DimY), Bounds, Params, SlopeLow, ExposureLow, ElevationLow);
		FArcForestScalarField SlopeHigh, ExposureHigh, ElevationHigh;
		ArcForestTerrainFields::Derive(HeightsHigh, HitGrid, FIntPoint(DimX, DimY), Bounds, Params, SlopeHigh, ExposureHigh, ElevationHigh);

		for (int32 Col = 0; Col < DimX; ++Col)
		{
			const float ExpectedLow = (HeightsLow[Col] - Params.ElevationMinCm) / (Params.ElevationMaxCm - Params.ElevationMinCm);
			const float ExpectedHigh = (HeightsHigh[Col] - Params.ElevationMinCm) / (Params.ElevationMaxCm - Params.ElevationMinCm);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ElevationLow.Values[Col], ExpectedLow, 1e-5f),
				*FString::Printf(TEXT("Low grid col %d: expected %f got %f"), Col, ExpectedLow, ElevationLow.Values[Col])));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ElevationHigh.Values[Col], ExpectedHigh, 1e-5f),
				*FString::Printf(TEXT("High grid col %d: expected %f got %f"), Col, ExpectedHigh, ElevationHigh.Values[Col])));
			ASSERT_THAT(IsTrue(!FMath::IsNearlyEqual(ElevationLow.Values[Col], ElevationHigh.Values[Col], 1e-3f),
				TEXT("Same-shape grids at different absolute heights must NOT normalise identically")));
		}
	}

	TEST_METHOD(Elevation_FlatRangeIsConstant)
	{
		using namespace ArcForestTerrainFieldsTestLocal;

		constexpr int32 DimX = 3;
		constexpr int32 DimY = 3;
		const TArray<float> Heights = MakeXRamp(DimX, DimY, 77.0f);
		const TArray<bool> HitGrid = MakeAllValidMask(DimX * DimY);
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(300.0f, 300.0f));

		FArcForestTerrainFieldParams Params;
		Params.ElevationMinCm = 500.0f;
		Params.ElevationMaxCm = 500.0f; // Min == Max

		FArcForestScalarField Slope, Exposure, Elevation;
		ArcForestTerrainFields::Derive(Heights, HitGrid, FIntPoint(DimX, DimY), Bounds, Params, Slope, Exposure, Elevation);

		for (const float Value : Elevation.Values)
		{
			ASSERT_THAT(IsTrue(!FMath::IsNaN(Value) && FMath::IsFinite(Value), TEXT("Flat range must not produce NaN/Inf")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Value, 0.5f, 1e-6f),
				*FString::Printf(TEXT("Min==Max elevation range: expected constant 0.5, got %f"), Value)));
		}
	}

	TEST_METHOD(Contract_OutputFieldsStampDimsAndBoundsAndRoundTripBilinear)
	{
		using namespace ArcForestTerrainFieldsTestLocal;

		// GAP 4: every other assertion in this file indexes Field.Values[i] directly, so nothing
		// pins the Bounds/Dims PrepareField stamps onto the three outputs. A transposed stamp
		// (FIntPoint(Dims.Y, Dims.X)) is invisible on a SQUARE grid, which is why every other test
		// in the file uses one -- this fixture is deliberately NON-square (4x2) so a transpose is
		// detectable, and it round-trips one output through SampleBilinear against its own Values.
		constexpr int32 DimX = 4;
		constexpr int32 DimY = 2;
		const TArray<float> Heights = MakeXRamp(DimX, DimY, 50.0f); // h(col) = 50*col, identical per row
		const TArray<bool> HitGrid = MakeAllValidMask(DimX * DimY);
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(400.0f, 200.0f)); // Dx=Dy=100

		FArcForestTerrainFieldParams Params;
		Params.MaxSlopeDegrees = 45.0f;
		Params.ElevationMinCm = 0.0f;
		Params.ElevationMaxCm = 200.0f;

		FArcForestScalarField Slope, Exposure, Elevation;
		ArcForestTerrainFields::Derive(Heights, HitGrid, FIntPoint(DimX, DimY), Bounds, Params, Slope, Exposure, Elevation);

		ASSERT_THAT(IsTrue(Slope.Dims == FIntPoint(DimX, DimY) && Exposure.Dims == FIntPoint(DimX, DimY) && Elevation.Dims == FIntPoint(DimX, DimY),
			*FString::Printf(TEXT("Dims must be stamped exactly (%d,%d), not transposed: Slope=(%d,%d) Exposure=(%d,%d) Elevation=(%d,%d)"),
				DimX, DimY, Slope.Dims.X, Slope.Dims.Y, Exposure.Dims.X, Exposure.Dims.Y, Elevation.Dims.X, Elevation.Dims.Y)));
		ASSERT_THAT(IsTrue(Slope.Bounds.Min.Equals(Bounds.Min) && Slope.Bounds.Max.Equals(Bounds.Max)
				&& Exposure.Bounds.Min.Equals(Bounds.Min) && Exposure.Bounds.Max.Equals(Bounds.Max)
				&& Elevation.Bounds.Min.Equals(Bounds.Min) && Elevation.Bounds.Max.Equals(Bounds.Max),
			TEXT("Bounds must be stamped exactly onto all three outputs, not left ForceInit-default")));

		// Round trip: cell (col=2,row=1) centre sits at world (250,150) -- (2.5*100, 1.5*100) -- which
		// SampleBilinear resolves to Fx=2.0,Fy=1.0 exactly (Tx=Ty=0, no interpolation, exact tap).
		// Elevation there = (h(col2)-0)/(200-0) = 100/200 = 0.5, a value distinct from every other
		// column's elevation on this ramp, so a transposed Dims fed into the sampler (reading col/row
		// swapped) would read a different column's height and fail this exact-equality check.
		const int32 RoundTripIndex = (1 * DimX) + 2; // row1, col2
		const float SampledElevation = Elevation.SampleBilinear(FVector2f(250.0f, 150.0f));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(SampledElevation, Elevation.Values[RoundTripIndex], 1e-5f),
			*FString::Printf(TEXT("Round-trip cell (2,1): SampleBilinear=%f must equal Values[%d]=%f"),
				SampledElevation, RoundTripIndex, Elevation.Values[RoundTripIndex])));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Elevation.Values[RoundTripIndex], 0.5f, 1e-5f),
			*FString::Printf(TEXT("Cell (2,1) elevation: expected 0.5 (h=100 in [0,200]) got %f"), Elevation.Values[RoundTripIndex])));
	}

	// GAP-B (round-2 review): every other fixture in this file has Bounds.Min == (0,0), so a mutant
	// that drops the `Pos - Bounds.Min` origin shift in the sampler (ArcForestField.cpp, FracX =
	// (Pos.X - Bounds.Min.X)/Size.X), or that stamps FBox2f(ZeroVector, Bounds.Max) in PrepareField,
	// survives the ENTIRE suite -- at the origin the Min.Equals half of the Bounds assertion is
	// vacuous and every probe position is already origin-relative.
	//
	// This is exactly the at-risk topology Step 7 runs in: the bake footprint is the PCG execution
	// bounds, i.e. the PARTITION CELL, whose Min is NOT the origin (see ArcForestTerrainFields.h on
	// per-partition bounds). A Min-dropping regression would therefore be invisible here and would
	// misplace every habitat lookup in a partitioned level. Same 4x2 ramp, offset far from origin.
	TEST_METHOD(Contract_OffsetBoundsOriginShiftIsHonoured)
	{
		using namespace ArcForestTerrainFieldsTestLocal;

		constexpr int32 DimX = 4;
		constexpr int32 DimY = 2;
		const TArray<float> Heights = MakeXRamp(DimX, DimY, 50.0f); // h(col) = 50*col
		const TArray<bool> HitGrid = MakeAllValidMask(DimX * DimY);
		// Min far from the origin, same 400x200 span => Dx=Dy=100, identical cell layout to the
		// origin-based contract test above, so every expected value carries over unchanged.
		const FVector2f Origin(10000.0f, 20000.0f);
		const FBox2f Bounds(Origin, Origin + FVector2f(400.0f, 200.0f));

		FArcForestTerrainFieldParams Params;
		Params.MaxSlopeDegrees = 45.0f;
		Params.ElevationMinCm = 0.0f;
		Params.ElevationMaxCm = 200.0f;

		FArcForestScalarField Slope, Exposure, Elevation;
		ArcForestTerrainFields::Derive(Heights, HitGrid, FIntPoint(DimX, DimY), Bounds, Params, Slope, Exposure, Elevation);

		// The Min half of the stamp is now load-bearing: FBox2f(ZeroVector, Bounds.Max) fails here.
		ASSERT_THAT(IsTrue(Slope.Bounds.Min.Equals(Origin) && Exposure.Bounds.Min.Equals(Origin) && Elevation.Bounds.Min.Equals(Origin),
			*FString::Printf(TEXT("Bounds.Min must be stamped as the offset origin (%f,%f), not zeroed: got Slope=(%f,%f)"),
				Origin.X, Origin.Y, Slope.Bounds.Min.X, Slope.Bounds.Min.Y)));

		// Round-trip at the OFFSET cell centre for (col=2,row=1): Origin + (2.5*100, 1.5*100).
		// Correct code subtracts Bounds.Min, giving Fx=2.0/Fy=1.0 exactly -> the exact tap, elevation
		// 0.5. A mutant that omits the origin shift computes Fx = 10250/400*4 - 0.5 = 101.0, clamps
		// to the last column, and reads h(col3)=150 -> 0.75. Distinct at 1e-5.
		const int32 RoundTripIndex = (1 * DimX) + 2;
		const float SampledElevation = Elevation.SampleBilinear(Origin + FVector2f(250.0f, 150.0f));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(SampledElevation, 0.5f, 1e-5f),
			*FString::Printf(TEXT("Offset-bounds round trip: expected 0.5 at the (2,1) cell centre, got %f (a value of 0.75 means the Bounds.Min origin shift was dropped)"), SampledElevation)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(SampledElevation, Elevation.Values[RoundTripIndex], 1e-5f),
			*FString::Printf(TEXT("Offset-bounds round trip must equal Values[%d]=%f, got %f"),
				RoundTripIndex, Elevation.Values[RoundTripIndex], SampledElevation)));

		// Slope must be unaffected by the offset -- gradient 0.5 (50/100) as in the origin fixture.
		const float ExpectedSlope = AnalyticNormalizedSlope(0.5f, 45.0f);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Slope.Values[RoundTripIndex], ExpectedSlope, 1e-4f),
			*FString::Printf(TEXT("Offset bounds must not change slope: expected %f got %f"), ExpectedSlope, Slope.Values[RoundTripIndex])));
	}

	TEST_METHOD(NoData_HoleInheritsNeighbourMorphologyNoCliff)
	{
		using namespace ArcForestTerrainFieldsTestLocal;

		// The header's own worked example: 3x3, Dx=Dy=200, h(col)=100*col identical on EVERY row
		// (so which neighbour row the hole inherits from is irrelevant -- both give the same
		// values), middle row entirely invalid. Replication must reproduce the SAME heights as the
		// fully-valid control, so slope/exposure/elevation must come out ELEMENT-WISE IDENTICAL to
		// a control run with no holes at all -- the strongest possible "no cliff" assertion.
		//
		// MUST-FIX 1: the hole cells must hold a real miss sentinel, or a build that deletes
		// ReplicateFromNearestValid entirely (i.e. reads HeightGrid raw) is byte-identical to the
		// correct BFS-replicated run and this whole test stays green under that mutant. The CONTROL
		// run stays on the unmodified ramp; only the HOLE run's invalid row is poisoned.
		constexpr int32 DimX = 3;
		constexpr int32 DimY = 3;
		const TArray<float> HeightsControl = MakeXRamp(DimX, DimY, 100.0f);
		TArray<float> HeightsWithHoleSentinel = HeightsControl;
		HeightsWithHoleSentinel[3] = -100000.0f; // row1, col0 -- miss sentinel, not a measurement
		HeightsWithHoleSentinel[4] = -100000.0f; // row1, col1
		HeightsWithHoleSentinel[5] = -100000.0f; // row1, col2
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(600.0f, 600.0f)); // Dx=Dy=200

		TArray<bool> HitGridWithHole = MakeAllValidMask(DimX * DimY);
		HitGridWithHole[3] = false; // row1, col0
		HitGridWithHole[4] = false; // row1, col1
		HitGridWithHole[5] = false; // row1, col2
		const TArray<bool> HitGridControl = MakeAllValidMask(DimX * DimY);

		FArcForestTerrainFieldParams Params;
		Params.MaxSlopeDegrees = 45.0f;
		Params.SunAzimuthDegrees = 0.0f;

		// Correct code BFS-replicates row1 from row0 (scan-order tiebreak: row0 cells are queued and
		// resolved before row2's), giving row1 = {0,100,200} -- identical to the control's row1, so
		// every value below stays element-wise identical to the control, exactly as before. A build
		// that skips replication instead leaves the -100000 sentinel live at row1 and it leaks into
		// its NEIGHBOURS' one-sided border derivative (row0/row2 DhDy jump to roughly +-500, slope
		// clamps to 1.0), which the per-cell loop below catches at indices 0,1,2,6,7,8.
		FArcForestScalarField SlopeHole, ExposureHole, ElevationHole;
		ArcForestTerrainFields::Derive(HeightsWithHoleSentinel, HitGridWithHole, FIntPoint(DimX, DimY), Bounds, Params, SlopeHole, ExposureHole, ElevationHole);
		FArcForestScalarField SlopeControl, ExposureControl, ElevationControl;
		ArcForestTerrainFields::Derive(HeightsControl, HitGridControl, FIntPoint(DimX, DimY), Bounds, Params, SlopeControl, ExposureControl, ElevationControl);

		for (int32 Index = 0; Index < (DimX * DimY); ++Index)
		{
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(SlopeHole.Values[Index], SlopeControl.Values[Index], 1e-5f),
				*FString::Printf(TEXT("Slope at %d must match the no-hole control (no cliff): hole=%f control=%f"),
					Index, SlopeHole.Values[Index], SlopeControl.Values[Index])));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ExposureHole.Values[Index], ExposureControl.Values[Index], 1e-5f),
				*FString::Printf(TEXT("Exposure at %d must match the no-hole control: hole=%f control=%f"),
					Index, ExposureHole.Values[Index], ExposureControl.Values[Index])));
		}

		// The header's worked numbers for the hole's centre cell (row1,col1): dh/dx=0.5 ->
		// atan(0.5)=26.57deg -> 0.59 at MaxSlopeDegrees=45, exposure 0.0 at azimuth 0. NOT flat.
		constexpr int32 CentreOfHole = (1 * DimX) + 1;
		const float ExpectedSlope = AnalyticNormalizedSlope(0.5f, Params.MaxSlopeDegrees);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(SlopeHole.Values[CentreOfHole], ExpectedSlope, 1e-3f),
			*FString::Printf(TEXT("Hole centre slope: expected %f (atan(0.5) normalised) got %f"),
				ExpectedSlope, SlopeHole.Values[CentreOfHole])));
		ASSERT_THAT(IsTrue(!FMath::IsNearlyEqual(SlopeHole.Values[CentreOfHole], 0.0f, 1e-3f),
			TEXT("A replicated hole must inherit its source row's gradient, NOT read as flat ground")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ExposureHole.Values[CentreOfHole], 0.0f, 1e-4f),
			*FString::Printf(TEXT("Hole centre exposure: expected 0.0 got %f"), ExposureHole.Values[CentreOfHole])));
		ASSERT_THAT(IsTrue(!FMath::IsNearlyEqual(ExposureHole.Values[CentreOfHole], 0.5f, 1e-3f),
			TEXT("A replicated hole must NOT read as the flat-ground neutral 0.5")));
	}

	TEST_METHOD(NoData_DeriveIsBitIdenticalAcrossRepeatedCalls)
	{
		using namespace ArcForestTerrainFieldsTestLocal;

		// GAP 9: the header claims bit-identity for the BFS replication (Step 7 seeds off these
		// fields under a bit-identity contract) but nothing in this file checks for it -- every other
		// assertion uses IsNearlyEqual, which would pass even if the BFS's queue/tie-break introduced
		// run-to-run float jitter. Re-uses the hole fixture (the one place BFS replication actually
		// runs) and requires EXACT (not nearly-equal) equality across three independent calls.
		constexpr int32 DimX = 3;
		constexpr int32 DimY = 3;
		const TArray<float> HeightsControl = MakeXRamp(DimX, DimY, 100.0f);
		TArray<float> HeightsWithHoleSentinel = HeightsControl;
		HeightsWithHoleSentinel[3] = -100000.0f;
		HeightsWithHoleSentinel[4] = -100000.0f;
		HeightsWithHoleSentinel[5] = -100000.0f;
		TArray<bool> HitGridWithHole = MakeAllValidMask(DimX * DimY);
		HitGridWithHole[3] = false;
		HitGridWithHole[4] = false;
		HitGridWithHole[5] = false;
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(600.0f, 600.0f));

		FArcForestTerrainFieldParams Params;
		Params.MaxSlopeDegrees = 45.0f;
		// GAP-A (round-2 review): an authored elevation range is REQUIRED here. Left at the 0/0
		// default, bHaveElevationRange is false and Elevation is a constant 0.5 array, so the
		// Elevation bit-identity arm below could not fail for any implementation — it would not
		// witness replication jitter at all. With a live range it carries the replicated heights.
		Params.ElevationMinCm = 0.0f;
		Params.ElevationMaxCm = 1000.0f;

		FArcForestScalarField SlopeA, ExposureA, ElevationA;
		FArcForestScalarField SlopeB, ExposureB, ElevationB;
		FArcForestScalarField SlopeC, ExposureC, ElevationC;
		ArcForestTerrainFields::Derive(HeightsWithHoleSentinel, HitGridWithHole, FIntPoint(DimX, DimY), Bounds, Params, SlopeA, ExposureA, ElevationA);
		ArcForestTerrainFields::Derive(HeightsWithHoleSentinel, HitGridWithHole, FIntPoint(DimX, DimY), Bounds, Params, SlopeB, ExposureB, ElevationB);
		ArcForestTerrainFields::Derive(HeightsWithHoleSentinel, HitGridWithHole, FIntPoint(DimX, DimY), Bounds, Params, SlopeC, ExposureC, ElevationC);

		const bool bSlopeBitEqual = (SlopeA.Values.Num() == SlopeB.Values.Num()) && (SlopeA.Values.Num() == SlopeC.Values.Num())
			&& (FMemory::Memcmp(SlopeA.Values.GetData(), SlopeB.Values.GetData(), SlopeA.Values.Num() * sizeof(float)) == 0)
			&& (FMemory::Memcmp(SlopeA.Values.GetData(), SlopeC.Values.GetData(), SlopeA.Values.Num() * sizeof(float)) == 0);
		ASSERT_THAT(IsTrue(bSlopeBitEqual, TEXT("Slope.Values must be bit-identical across repeated Derive() calls on the same input")));

		const bool bExposureBitEqual = (ExposureA.Values.Num() == ExposureB.Values.Num()) && (ExposureA.Values.Num() == ExposureC.Values.Num())
			&& (FMemory::Memcmp(ExposureA.Values.GetData(), ExposureB.Values.GetData(), ExposureA.Values.Num() * sizeof(float)) == 0)
			&& (FMemory::Memcmp(ExposureA.Values.GetData(), ExposureC.Values.GetData(), ExposureA.Values.Num() * sizeof(float)) == 0);
		ASSERT_THAT(IsTrue(bExposureBitEqual, TEXT("Exposure.Values must be bit-identical across repeated Derive() calls on the same input")));

		const bool bElevationBitEqual = (ElevationA.Values.Num() == ElevationB.Values.Num()) && (ElevationA.Values.Num() == ElevationC.Values.Num())
			&& (FMemory::Memcmp(ElevationA.Values.GetData(), ElevationB.Values.GetData(), ElevationA.Values.Num() * sizeof(float)) == 0)
			&& (FMemory::Memcmp(ElevationA.Values.GetData(), ElevationC.Values.GetData(), ElevationA.Values.Num() * sizeof(float)) == 0);
		ASSERT_THAT(IsTrue(bElevationBitEqual, TEXT("Elevation.Values must be bit-identical across repeated Derive() calls on the same input")));
	}

	TEST_METHOD(NoData_BfsTiebreakInheritsLowerScanOrderRow)
	{
		using namespace ArcForestTerrainFieldsTestLocal;

		// GAP 9 (tie-break pin): 3x3, Dx=Dy=200, row0 flat at h=0, row2 flat at h=1000, row1 entirely
		// invalid -- Manhattan-equidistant (distance 1) from BOTH row0 and row2. The header documents
		// ties resolved by "scan/insertion order": row0's cells are enqueued (indices 0,1,2) BEFORE
		// row2's (indices 6,7,8), so the BFS resolves row1 from row0 -- NOT the nearer-in-value row2
		// -- during the processing of heads 0-2, before row2 is even dequeued. With
		// ElevationMin=0/Max=1000, row1 must therefore read Elevation 0.0 exactly (row0's height),
		// not 1.0 (row2's).
		constexpr int32 DimX = 3;
		constexpr int32 DimY = 3;
		TArray<float> Heights;
		Heights.SetNumUninitialized(DimX * DimY);
		for (int32 Row = 0; Row < DimY; ++Row)
		{
			for (int32 Col = 0; Col < DimX; ++Col)
			{
				Heights[(Row * DimX) + Col] = (Row == 2) ? 1000.0f : 0.0f; // row0=0, row1=(invalid, irrelevant), row2=1000
			}
		}
		TArray<bool> HitGrid = MakeAllValidMask(DimX * DimY);
		HitGrid[3] = false; // row1, col0
		HitGrid[4] = false; // row1, col1
		HitGrid[5] = false; // row1, col2
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(600.0f, 600.0f)); // Dx=Dy=200

		FArcForestTerrainFieldParams Params;
		Params.ElevationMinCm = 0.0f;
		Params.ElevationMaxCm = 1000.0f;

		FArcForestScalarField Slope, Exposure, Elevation;
		ArcForestTerrainFields::Derive(Heights, HitGrid, FIntPoint(DimX, DimY), Bounds, Params, Slope, Exposure, Elevation);

		for (int32 Col = 0; Col < DimX; ++Col)
		{
			const int32 Row1Index = (1 * DimX) + Col;
			ASSERT_THAT(IsTrue(Elevation.Values[Row1Index] == 0.0f,
				*FString::Printf(TEXT("Row1 col %d must tie-break to row0's height (elevation 0.0, scan order), got %f"),
					Col, Elevation.Values[Row1Index])));
		}
	}

	TEST_METHOD(NoData_NoValidCellsGivesFlatTriple)
	{
		using namespace ArcForestTerrainFieldsTestLocal;

		// The flat triple (slope 0, exposure 0.5, elevation 0.5) belongs to exactly ONE case per the
		// header: no cell in the grid is valid at all.
		constexpr int32 DimX = 3;
		constexpr int32 DimY = 3;
		const TArray<float> Heights = MakeXRamp(DimX, DimY, 100.0f);
		TArray<bool> HitGridAllInvalid;
		HitGridAllInvalid.Init(false, DimX * DimY);
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(600.0f, 600.0f));

		FArcForestTerrainFieldParams Params;
		Params.ElevationMinCm = 0.0f;
		Params.ElevationMaxCm = 1000.0f;

		FArcForestScalarField Slope, Exposure, Elevation;
		ArcForestTerrainFields::Derive(Heights, HitGridAllInvalid, FIntPoint(DimX, DimY), Bounds, Params, Slope, Exposure, Elevation);

		for (int32 Index = 0; Index < (DimX * DimY); ++Index)
		{
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Slope.Values[Index], 0.0f, 1e-6f),
				*FString::Printf(TEXT("No-valid-data slope at %d: expected 0.0 got %f"), Index, Slope.Values[Index])));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Exposure.Values[Index], 0.5f, 1e-6f),
				*FString::Printf(TEXT("No-valid-data exposure at %d: expected 0.5 got %f"), Index, Exposure.Values[Index])));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Elevation.Values[Index], 0.5f, 1e-6f),
				*FString::Printf(TEXT("No-valid-data elevation at %d: expected 0.5 got %f"), Index, Elevation.Values[Index])));
		}
	}

	TEST_METHOD(Degenerate_SingleCellAndSingleAxisGridsAreSafe)
	{
		using namespace ArcForestTerrainFieldsTestLocal;

		FArcForestTerrainFieldParams Params;
		Params.MaxSlopeDegrees = 45.0f;
		Params.ElevationMinCm = 0.0f;
		Params.ElevationMaxCm = 1000.0f;

		// (a) Dims == (1,1): a single cell has no neighbour on either axis -> zero gradient, no crash.
		{
			const TArray<float> Heights = { 500.0f };
			const TArray<bool> HitGrid = { true };
			const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(100.0f, 100.0f));
			FArcForestScalarField Slope, Exposure, Elevation;
			ArcForestTerrainFields::Derive(Heights, HitGrid, FIntPoint(1, 1), Bounds, Params, Slope, Exposure, Elevation);
			ASSERT_THAT(IsTrue(Slope.Values.Num() == 1 && FMath::IsNearlyEqual(Slope.Values[0], 0.0f, 1e-6f),
				TEXT("1x1 grid: slope must be exactly 0")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Exposure.Values[0], 0.5f, 1e-6f),
				TEXT("1x1 grid: exposure must be the flat neutral 0.5")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Elevation.Values[0], 0.5f, 1e-6f),
				TEXT("1x1 grid: elevation of 500 in [0,1000] must be 0.5")));
		}

		// (b) Dims == (1,3): X axis is degenerate (DhDx must be exactly 0); Y axis carries the ramp.
		{
			const TArray<float> Heights = { 0.0f, 50.0f, 100.0f }; // h(row) = 50*row
			const TArray<bool> HitGrid = MakeAllValidMask(3);
			const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(100.0f, 300.0f)); // Dy = 100
			FArcForestScalarField Slope, Exposure, Elevation;
			ArcForestTerrainFields::Derive(Heights, HitGrid, FIntPoint(1, 3), Bounds, Params, Slope, Exposure, Elevation);
			// Interior row (row 1): DhDy = (100-0)/(2*100) = 0.5, DhDx = 0 (degenerate axis).
			const float ExpectedSlope = AnalyticNormalizedSlope(0.5f, Params.MaxSlopeDegrees);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Slope.Values[1], ExpectedSlope, 1e-4f),
				*FString::Printf(TEXT("1x3 grid interior row: expected slope %f got %f"), ExpectedSlope, Slope.Values[1])));
			for (const float Value : Slope.Values)
			{
				ASSERT_THAT(IsTrue(!FMath::IsNaN(Value) && FMath::IsFinite(Value), TEXT("1x3 grid: no NaN/Inf")));
			}
		}

		// (c) Dims == (3,1): mirror of (b), Y axis degenerate, X carries the ramp.
		{
			const TArray<float> Heights = { 0.0f, 50.0f, 100.0f }; // h(col) = 50*col
			const TArray<bool> HitGrid = MakeAllValidMask(3);
			const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(300.0f, 100.0f)); // Dx = 100
			FArcForestScalarField Slope, Exposure, Elevation;
			ArcForestTerrainFields::Derive(Heights, HitGrid, FIntPoint(3, 1), Bounds, Params, Slope, Exposure, Elevation);
			const float ExpectedSlope = AnalyticNormalizedSlope(0.5f, Params.MaxSlopeDegrees);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Slope.Values[1], ExpectedSlope, 1e-4f),
				*FString::Printf(TEXT("3x1 grid interior col: expected slope %f got %f"), ExpectedSlope, Slope.Values[1])));
			for (const float Value : Slope.Values)
			{
				ASSERT_THAT(IsTrue(!FMath::IsNaN(Value) && FMath::IsFinite(Value), TEXT("3x1 grid: no NaN/Inf")));
			}
		}

		// (d) Empty/mis-sized grid: three EMPTY fields, which sample as the neutral 1.0.
		{
			const TArray<float> Heights; // Num() == 0, mismatched against Dims (2,2) -> CellCount 4
			const TArray<bool> HitGrid;
			const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(100.0f, 100.0f));
			FArcForestScalarField Slope, Exposure, Elevation;
			ArcForestTerrainFields::Derive(Heights, HitGrid, FIntPoint(2, 2), Bounds, Params, Slope, Exposure, Elevation);
			ASSERT_THAT(IsTrue(Slope.Values.Num() == 0 && Exposure.Values.Num() == 0 && Elevation.Values.Num() == 0,
				TEXT("Mismatched grid: all three fields must come back empty")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Slope.SampleNearest(FVector2f(1.0f, 1.0f)), 1.0f, 1e-6f),
				TEXT("Empty field samples as the neutral 1.0 per ArcForestField's contract")));
		}

		// (e) Zero-span bounds: both axes degenerate via Spacing<=0, even though Dims > 1.
		{
			const TArray<float> Heights = MakeXRamp(3, 3, 100.0f);
			const TArray<bool> HitGrid = MakeAllValidMask(9);
			const FBox2f Bounds(FVector2f(50.0f, 50.0f), FVector2f(50.0f, 50.0f)); // zero size
			FArcForestScalarField Slope, Exposure, Elevation;
			ArcForestTerrainFields::Derive(Heights, HitGrid, FIntPoint(3, 3), Bounds, Params, Slope, Exposure, Elevation);
			for (int32 Index = 0; Index < 9; ++Index)
			{
				ASSERT_THAT(IsTrue(!FMath::IsNaN(Slope.Values[Index]) && FMath::IsFinite(Slope.Values[Index]),
					TEXT("Zero-span bounds: slope must not be NaN/Inf")));
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Slope.Values[Index], 0.0f, 1e-6f),
					TEXT("Zero-span bounds: Spacing<=0 forces zero gradient on both axes")));
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Exposure.Values[Index], 0.5f, 1e-6f),
					TEXT("Zero-span bounds: zero gradient reads as the flat neutral 0.5")));
			}
		}
	}

	TEST_METHOD(Bilinear_MidpointOfKnownRamp)
	{
		// v(col,row) = 0.1*col + 0.01*row over a 3x3 grid, Bounds 300x300 (Dx=Dy=100). Bilinear of a
		// linear function is exact everywhere in the interpolation region, so both a half and a
		// non-half fraction must match the closed-form lerp.
		const TArray<float> Values = {
			0.00f, 0.10f, 0.20f, // row0
			0.01f, 0.11f, 0.21f, // row1
			0.02f, 0.12f, 0.22f  // row2
		};
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(300.0f, 300.0f));
		const FIntPoint Dims(3, 3);

		// x=150 sits exactly on col1's centre (Tx=0); y=100 is halfway between row0(centre 50) and
		// row1(centre 150) -> Ty=0.5. Expected = lerp(v(col1,row0), v(col1,row1), 0.5).
		const float Sample1 = ArcForestField::SampleFieldBilinear(FVector2f(150.0f, 100.0f), Bounds, Values, Dims);
		const float Expected1 = FMath::Lerp(0.10f, 0.11f, 0.5f); // 0.105
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Sample1, Expected1, 1e-5f),
			*FString::Printf(TEXT("Half-fraction midpoint: expected %f got %f"), Expected1, Sample1)));

		// x=150 (col1 centre again, Tx=0); y=125 -> Ty=0.75. Expected = lerp(0.10,0.11,0.75).
		const float Sample2 = ArcForestField::SampleFieldBilinear(FVector2f(150.0f, 125.0f), Bounds, Values, Dims);
		const float Expected2 = FMath::Lerp(0.10f, 0.11f, 0.75f); // 0.1075
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Sample2, Expected2, 1e-5f),
			*FString::Printf(TEXT("Non-half fraction: expected %f got %f"), Expected2, Sample2)));
	}

	TEST_METHOD(Bilinear_AllFourTapsLiveKillsAxisDegeneracyAndSwap)
	{
		// GAP 3: every OTHER bilinear test has at least one fraction exactly 0 or a symmetric layout,
		// so Tx==0 (X degrades to nearest), a V10/V01 tap swap, a Tx<->Ty transpose, and a (1-T) sign
		// flip all survive them. This is the one case with BOTH Tx and Ty strictly inside (0,1) and
		// an asymmetric field, so all four mutants produce a different number from the correct one.
		//
		// v(col,row) = 0.1*col + 0.01*row, same field as Bilinear_MidpointOfKnownRamp, 300x300/3x3
		// (Dx=Dy=100). x=175: FracX=175/300=0.58333, Fx=0.58333*3-0.5=1.25 -> X0=1,X1=2, Tx=0.25.
		// y=225: FracY=225/300=0.75, Fy=0.75*3-0.5=1.75 -> Y0=1,Y1=2, Ty=0.75.
		const TArray<float> Values = {
			0.00f, 0.10f, 0.20f, // row0
			0.01f, 0.11f, 0.21f, // row1
			0.02f, 0.12f, 0.22f  // row2
		};
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(300.0f, 300.0f));
		const FIntPoint Dims(3, 3);

		// Taps: V00=v(col1,row1)=0.11, V10=v(col2,row1)=0.21, V01=v(col1,row2)=0.12, V11=v(col2,row2)=0.22.
		// Bottom = Lerp(0.11,0.21,0.25) = 0.135. Top = Lerp(0.12,0.22,0.25) = 0.145.
		// Result = Lerp(0.135,0.145,0.75) = 0.1425.
		const float Sample = ArcForestField::SampleFieldBilinear(FVector2f(175.0f, 225.0f), Bounds, Values, Dims);
		const float Expected = FMath::Lerp(FMath::Lerp(0.11f, 0.21f, 0.25f), FMath::Lerp(0.12f, 0.22f, 0.25f), 0.75f); // 0.1425
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Sample, Expected, 1e-5f),
			*FString::Printf(TEXT("Four-live-taps sample: expected %f got %f"), Expected, Sample)));
	}

	TEST_METHOD(Bilinear_BorderIsConstantExtrapolation)
	{
		// MUST-FIX 2: v(col0,row0) used to be 0.00, which sits exactly on the output's [0,1] clamp
		// floor -- an implementation that linearly extrapolates past the border (no Tx clamp) would
		// land on a NEGATIVE lerp result at both left-border probes, and the final Clamp(...,0,1)
		// would hide the bug by mapping both back to 0.0. Row0 is now {0.5, 0.1, 0.1}: strictly
		// inside (0,1), and DECREASING away from the border, so an unclamped extrapolation lands
		// ABOVE 0.5 where the clamp cannot mask it. Row1/Row2 are untouched, so the RIGHT-border
		// half below (which only ever samples row2, Ty=0 there) is unaffected and stays as before.
		const TArray<float> Values = {
			0.5f, 0.1f, 0.1f, // row0 -- deliberately non-monotonic-with-border-floor
			0.01f, 0.11f, 0.21f,
			0.02f, 0.12f, 0.22f
		};
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(300.0f, 300.0f));
		const FIntPoint Dims(3, 3);

		// y=50 -> FracY=50/300, Fy=0.5-0.5=0 -> Y0=Y1=0 -> Ty=0: both left-border samples read row0
		// only, so V00=0.5 (col0) and V10=0.1 (col1) below.
		//
		// x=0: FracX=0, Fx=0*3-0.5=-0.5. Production clamps X0's tap fraction: Tx=Clamp(Fx-X0,0,1)
		// with X0=Clamp(floor(-0.5),0,2)=0 -> Tx=Clamp(-0.5,0,1)=0 -> Lerp(0.5,0.1,0)=0.5 (expected).
		// An unclamped-Tx mutant would instead compute Lerp(0.5,0.1,-0.5)=0.5+(0.1-0.5)*(-0.5)=0.7,
		// which the final [0,1] clamp does NOT catch (0.7 is already in range).
		const float NearLeft = ArcForestField::SampleFieldBilinear(FVector2f(0.0f, 50.0f), Bounds, Values, Dims);

		// x=-1000: FracX=-1000/300=-3.3333, Fx=-3.3333*3-0.5=-10.5. Production: same X0=0, Tx clamps
		// to 0 -> still 0.5. An unclamped-Tx mutant computes Lerp(0.5,0.1,-10.5)=0.5+(-0.4)*(-10.5)
		// =4.7, and even the OUTER [0,1] clamp then reports 1.0 -- a different wrong answer at a
		// different x, which is exactly why two very different out-of-range x's must still agree.
		const float FarLeft = ArcForestField::SampleFieldBilinear(FVector2f(-1000.0f, 50.0f), Bounds, Values, Dims);

		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(NearLeft, 0.5f, 1e-6f) && FMath::IsNearlyEqual(FarLeft, 0.5f, 1e-6f),
			*FString::Printf(TEXT("Left border: expected constant 0.5 (v(col0,row0)); got NearLeft=%f FarLeft=%f"), NearLeft, FarLeft)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FarLeft, NearLeft, 1e-6f),
			TEXT("Two different out-of-range x's must produce the IDENTICAL value (constant, not linear, extrapolation)")));

		const float FarRight = ArcForestField::SampleFieldBilinear(FVector2f(10000.0f, 250.0f), Bounds, Values, Dims);
		const float NearRight = ArcForestField::SampleFieldBilinear(FVector2f(260.0f, 250.0f), Bounds, Values, Dims);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(FarRight, 0.22f, 1e-6f) && FMath::IsNearlyEqual(NearRight, 0.22f, 1e-6f),
			*FString::Printf(TEXT("Right border: expected constant 0.22 (v(col2,row2)); got FarRight=%f NearRight=%f"), FarRight, NearRight)));
	}

	TEST_METHOD(Bilinear_DegenerateAxisIsExact)
	{
		// Dims.X == 1: no interpolation along X is possible regardless of the x coordinate supplied,
		// including wildly out-of-bounds x's -- if that ever divided by (Dims.X-1)==0 it would crash
		// or NaN instead of returning the exact row value.
		{
			const TArray<float> Values = { 0.1f, 0.4f, 0.7f }; // one column, three rows
			const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(100.0f, 300.0f)); // Dy = 100
			const FIntPoint Dims(1, 3);

			const float AtRow1CentreFarX = ArcForestField::SampleFieldBilinear(FVector2f(99999.0f, 150.0f), Bounds, Values, Dims);
			const float AtRow1CentreNegX = ArcForestField::SampleFieldBilinear(FVector2f(-99999.0f, 150.0f), Bounds, Values, Dims);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(AtRow1CentreFarX, 0.4f, 1e-5f) && FMath::IsNearlyEqual(AtRow1CentreNegX, 0.4f, 1e-5f),
				*FString::Printf(TEXT("Dims.X==1: expected exact row1 value 0.4 regardless of x, got %f / %f"), AtRow1CentreFarX, AtRow1CentreNegX)));

			// Interpolation still happens correctly along the live Y axis: y=225 is Ty=0.75 between
			// row1(centre 150, v=0.4) and row2(centre 250, v=0.7).
			const float Interpolated = ArcForestField::SampleFieldBilinear(FVector2f(0.0f, 225.0f), Bounds, Values, Dims);
			const float Expected = FMath::Lerp(0.4f, 0.7f, 0.75f); // 0.625
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Interpolated, Expected, 1e-4f),
				*FString::Printf(TEXT("Dims.X==1, live Y axis: expected %f got %f"), Expected, Interpolated)));
		}

		// Dims.Y == 1: mirror case, X axis carries the interpolation.
		{
			const TArray<float> Values = { 0.1f, 0.4f, 0.7f }; // one row, three columns
			const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(300.0f, 100.0f)); // Dx = 100
			const FIntPoint Dims(3, 1);

			const float AtCol1CentreFarY = ArcForestField::SampleFieldBilinear(FVector2f(150.0f, 99999.0f), Bounds, Values, Dims);
			const float AtCol1CentreNegY = ArcForestField::SampleFieldBilinear(FVector2f(150.0f, -99999.0f), Bounds, Values, Dims);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(AtCol1CentreFarY, 0.4f, 1e-5f) && FMath::IsNearlyEqual(AtCol1CentreNegY, 0.4f, 1e-5f),
				*FString::Printf(TEXT("Dims.Y==1: expected exact col1 value 0.4 regardless of y, got %f / %f"), AtCol1CentreFarY, AtCol1CentreNegY)));
		}
	}

	TEST_METHOD(Bilinear_ClampsToUnitRange)
	{
		// Deliberately out-of-[0,1] source values with an interpolation midpoint that discriminates
		// clamp-then-lerp from lerp-then-clamp: -2.0 and 2.0 average to 0.0 (already in range) if
		// clamped AFTER the lerp, but would average to 0.5 (clamp(-2)=0, clamp(2)=1, lerp=0.5) if
		// clamped BEFORE. The header states the RESULT is clamped, i.e. after interpolation.
		const TArray<float> Values = { -2.0f, 2.0f };
		const FBox2f Bounds(FVector2f(0.0f, 0.0f), FVector2f(200.0f, 100.0f));
		const FIntPoint Dims(2, 1);

		// Exact centres (Tx=0) must clamp exactly like SampleFieldNearest.
		const float AtCol0 = ArcForestField::SampleFieldBilinear(FVector2f(50.0f, 50.0f), Bounds, Values, Dims);
		const float AtCol1 = ArcForestField::SampleFieldBilinear(FVector2f(150.0f, 50.0f), Bounds, Values, Dims);
		const float NearestCol0 = ArcForestField::SampleFieldNearest(FVector2f(50.0f, 50.0f), Bounds, Values, Dims);
		const float NearestCol1 = ArcForestField::SampleFieldNearest(FVector2f(150.0f, 50.0f), Bounds, Values, Dims);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(AtCol0, 0.0f, 1e-6f) && FMath::IsNearlyEqual(AtCol0, NearestCol0, 1e-6f),
			*FString::Printf(TEXT("col0 centre: bilinear %f must match nearest %f, both clamped from -2.0 to 0.0"), AtCol0, NearestCol0)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(AtCol1, 1.0f, 1e-6f) && FMath::IsNearlyEqual(AtCol1, NearestCol1, 1e-6f),
			*FString::Printf(TEXT("col1 centre: bilinear %f must match nearest %f, both clamped from 2.0 to 1.0"), AtCol1, NearestCol1)));

		// Midpoint proves clamp-after-lerp: raw lerp(-2,2,0.5)=0.0, already in-range, so the result
		// is 0.0 -- NOT 0.5, which is what clamp-before-lerp would give.
		// NOTE: this is exactly why PCGForestSimSettings.cpp's SampleHeightBilinear was deliberately
		// NOT folded into this sampler -- folding would clamp every world-Z height into [0,1] and
		// flatten every terrain height to 1 cm.
		const float Midpoint = ArcForestField::SampleFieldBilinear(FVector2f(100.0f, 50.0f), Bounds, Values, Dims);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Midpoint, 0.0f, 1e-5f),
			*FString::Printf(TEXT("Midpoint of -2.0/2.0: expected 0.0 (clamp AFTER lerp), got %f"), Midpoint)));
	}
};
