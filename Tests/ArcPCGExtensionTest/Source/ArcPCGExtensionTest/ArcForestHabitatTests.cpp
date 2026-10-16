// Copyright Lukasz Baran. All Rights Reserved.

// ArcForestHabitatTests.cpp
// TIER 1 pure-core CQTest coverage for the PER-SPECIES HABITAT RESPONSE landed by
// docs/plans/2026-07-25-1133-arcpcgextension-habitat-fields-and-recruitment-plan.md Step 7
// (commit 0ea88e1fe56b, doc fixes 5ba5353b485d). The unit under test is the combine
//
//     AcceptProb = Suitability(Pos) * Prod_f Pow(Clamp(Lut_f(Value_f(Pos)), 0, 1), Weight_f)
//
// consumed at BOTH sites: FArcForestPatchField::Build (clump PARENT thinning — the site that decides
// where a species' clumps sit at all) and SeedPopulation's dart acceptance (thinning WITHIN a clump).
//
// WHICH TEST CARRIES WHICH SITE — the two sites need SEPARATE mutant kills, because a placement claim
// read off a species that HAS clump parents is carried by Build alone and survives the deletion of the
// dart-acceptance factor:
//   * FArcForestPatchField::Build (ArcForestPatchField.cpp:104-108) — killed by
//     Habitat_ClumpParentsFollowSlopeGradient, Habitat_OppositeCurvesSeparateSpecies and the
//     parent-side statistic of Habitat_MoistureNoiseDrivesPlacement, all of which read PARENT
//     POSITIONS straight out of Build.
//   * SeedPopulation's dart acceptance (ArcForestSimulator.cpp:329-335) — killed by
//     Habitat_DartAcceptanceBiasesParentlessSpecies, whose species is authored with PatchDensity = 0 so
//     that Build provably cannot act on it (it is skipped before Build's candidate loop). Nothing else
//     in this file goes RED on that deletion; Habitat_DrawCountUnchanged pins that site's
//     RNG-neutrality only, never its effect.
//
// TOPOLOGY: standalone, in-process, no world, no PIE, no RHI. The whole unit is a pure CPU solver over
// plain arrays (FArcForestSimulator's PURITY contract) plus a game-thread curve flattener; it holds no
// replicated state, runs no actor, and is never executed on a client or a dedicated server as anything
// other than the same pure function. A PIE-network fixture would add a world and change nothing the
// assertions read, so standalone is the at-risk topology in full.
//
// PRODUCTION PATH, not a proxy — every fixture below:
//   1. bakes a surface HEIGHT + SUITABILITY grid by calling ProjectPoint per cell centre on a real
//      UPCGSpatialData subclass, exactly as FPCGForestSimElement's prebake does
//      (PCGForestSimSettings.cpp:243-275);
//   2. derives the morphology fields through the shipped ArcForestTerrainFields::Derive;
//   3. authors real FRuntimeFloatCurve keys on FArcForestSpeciesEntry::FieldResponses and flattens them
//      with the shipped ArcForestSpeciesHabitat::FlattenFieldResponses;
//   4. drives FArcForestPatchField::Build / FArcForestSimulator::Simulate.
// No response LUT is hand-filled and no evaluator is re-implemented; the only hand-built input is a
// deliberately CONSTANT slope grid in the analytic sub-assertion of Habitat_WeightZeroIsNeutral, where
// a constant is the point.
//
// Every case runs at Iterations = 0 so seeding is isolated from growth/competition/mortality.
//
// BOTH-SIDES DISCIPLINE: every threshold below was measured on this branch in BOTH arms (with and
// without the species' FieldResponses) and the measured pair is recorded in the per-test MEASURED
// comment. A threshold sits between the two measured values with margin, never adjacent to one.
//
// KNOWN GAPS — recorded, not silently skipped. Nothing below is asserted anywhere in this file:
//   * SunExposure and Elevation axes. Only Slope (grid-derived) and Moisture (analytic) are exercised.
//     Every placement fixture's Elevation field is monotone in X exactly as its Slope field is, so a
//     Slope->Elevation cross-wire in SampleAxis would keep those tests green; the ONLY assertion that
//     distinguishes the two axes is the analytic block of Habitat_WeightZeroIsNeutral, whose hand-built
//     ConstantHabitat populates SlopeField and nothing else (do not populate the other fields there —
//     that is what makes the cross-wire go RED). SunExposure is untested entirely.
//   * The absent-axis RESPONSE-SKIP rule (ArcForestField.cpp:234-238, normative at
//     ArcForestField.h:290-299). Every fixture supplies the axis it keys on, so Axis.bSupplied is never
//     false. The keyless-curve arm of Habitat_EmptyResponsesReproduceBaseline covers the FLATTENER's
//     drop rule, which is a different rule.
//   * Non-integral Weight exponents. Only 0, 1 and 2 are asserted, i.e. the two short-circuits plus one
//     FMath::Pow case; the general Pow path's correctness at e.g. 0.5 is not pinned.
//   * The PCG element path (PCGForestSimSettings.cpp) — flatten-before-Launch ordering and the
//     bOverrideElevationRange == false partition-cell elevation normalisation are both unasserted here.
//   * ResultsIdentical TRUSTS that every parallel SoA array of FArcForestResult has the same length as
//     Num() rather than asserting it, so a result whose arrays desynchronised would be compared
//     element-wise only up to A.Num().

#include "CQTest.h"

#include "ArcForestTestHelpers.h"
#include "ArcForestTestSurfaceData.h"

#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"
#include "UObject/StrongObjectPtr.h"

#include "Forest/ArcForestField.h"
#include "Forest/ArcForestNoise.h"
#include "Forest/ArcForestPatchField.h"
#include "Forest/ArcForestSimTypes.h"
#include "Forest/ArcForestSimulator.h"
#include "Forest/ArcForestSpeciesDefinition.h"
#include "Forest/ArcForestTerrainFields.h"

#include "PCGPoint.h"
#include "Elements/PCGProjectionParams.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcForestHabitatTest, Log, All);

namespace ArcForestHabitatTestsLocal
{
	using namespace ArcForestTestHelpers;

	/** The node's own suitability/morphology grid resolution and dimension cap (PCGForestSimSettings). */
	constexpr float SuitabilityCellSizeCm = 200.0f;
	constexpr int32 MaxGridDim = 256;

	/** One baked fixture: the grids the solver consumes, plus the habitat bundle derived from them. */
	struct FHabitatFixture
	{
		FBox2f Bounds2D = FBox2f(ForceInit);
		FIntPoint GridDims = FIntPoint::ZeroValue;
		TArray<float> Suitability;
		FArcForestHabitatInputs Habitat;
	};

	/**
	 * Mirror of the node's game-thread prebake: one ProjectPoint per cell centre fills the suitability
	 * and surface-height grids, then ArcForestTerrainFields::Derive turns the height grid into the three
	 * morphology fields. Same query shape as PCGForestSimSettings.cpp:243-317 (query Z at the volume's
	 * top face, FallbackZ at the volume centre, hit mask recorded separately).
	 *
	 * MOISTURE IS SILENCED HERE ON PURPOSE. FArcForestNoiseParams default-constructs to a LIVE Perlin
	 * field (Frequency 0.001), so a default-constructed FArcForestHabitatInputs is NOT neutral — only
	 * FArcForestHabitatInputs::Empty() is, and it achieves that by zeroing Frequency. Zeroing it here
	 * makes moisture opt-in per fixture instead of leaking a live axis into the slope tests.
	 */
	void BakeFixtureFromSurface(
		const UArcForestTestSurfaceData& Surface,
		const FBox& VolumeBounds,
		const FArcForestTerrainFieldParams& TerrainParams,
		FHabitatFixture& OutFixture)
	{
		OutFixture.Bounds2D = FBox2f(
			FVector2f(static_cast<float>(VolumeBounds.Min.X), static_cast<float>(VolumeBounds.Min.Y)),
			FVector2f(static_cast<float>(VolumeBounds.Max.X), static_cast<float>(VolumeBounds.Max.Y)));
		OutFixture.GridDims =
			ArcForestField::ComputeGridDims(OutFixture.Bounds2D, SuitabilityCellSizeCm, MaxGridDim);

		const int32 CellCount = OutFixture.GridDims.X * OutFixture.GridDims.Y;
		OutFixture.Suitability.SetNumUninitialized(CellCount);

		TArray<float> HeightGrid;
		HeightGrid.SetNumUninitialized(CellCount);
		TArray<bool> HitGrid;
		HitGrid.SetNumUninitialized(CellCount);

		const double QueryZ = VolumeBounds.Max.Z;
		const float FallbackZ = static_cast<float>(VolumeBounds.GetCenter().Z);
		const FPCGProjectionParams ProjParams;

		for (int32 Row = 0; Row < OutFixture.GridDims.Y; ++Row)
		{
			for (int32 Col = 0; Col < OutFixture.GridDims.X; ++Col)
			{
				const int32 CellIndex = (Row * OutFixture.GridDims.X) + Col;
				const FVector2f Centre =
					ArcForestField::CellCentre(OutFixture.Bounds2D, OutFixture.GridDims, Col, Row);
				const FTransform QueryTransform(FVector(Centre.X, Centre.Y, QueryZ));

				FPCGPoint OutPoint;
				if (Surface.ProjectPoint(QueryTransform, FBox(ForceInit), ProjParams, OutPoint, nullptr))
				{
					OutFixture.Suitability[CellIndex] = FMath::Clamp(OutPoint.Density, 0.0f, 1.0f);
					HeightGrid[CellIndex] = static_cast<float>(OutPoint.Transform.GetLocation().Z);
					HitGrid[CellIndex] = true;
				}
				else
				{
					OutFixture.Suitability[CellIndex] = 0.0f;
					HeightGrid[CellIndex] = FallbackZ;
					HitGrid[CellIndex] = false;
				}
			}
		}

		ArcForestTerrainFields::Derive(
			HeightGrid,
			HitGrid,
			OutFixture.GridDims,
			OutFixture.Bounds2D,
			TerrainParams,
			OutFixture.Habitat.SlopeField,
			OutFixture.Habitat.SunExposureField,
			OutFixture.Habitat.ElevationField);

		// Opt-in moisture (see the note above): silence the live default.
		OutFixture.Habitat.MoistureNoise.Frequency = 0.0f;
	}

	/**
	 * A quadratically ramped surface over [0, SideCm] on X, flat along Y:
	 *   h(x) = x^2 / (2 * SideCm)  ->  dh/dx = x / SideCm, so the GRADIENT MAGNITUDE rises
	 * monotonically from 0 at x=0 to 1 (45 degrees) at x=SideCm.
	 *
	 * WHY QUADRATIC AND NOT LINEAR. A linear ramp has CONSTANT slope, so a slope-keyed response on one
	 * would be spatially uniform and every placement assertion below would be vacuous — the species
	 * would be thinned equally everywhere rather than moved. The quadratic is the simplest surface whose
	 * SLOPE field is itself a monotone ramp, which is what makes "biased upslope" a derivable direction:
	 * with MaxSlopeDegrees = 45 the normalised slope at position x is atan(x/SideCm)/atan(1).
	 *
	 * Density is 1 everywhere inside the footprint, so suitability is uniform and cannot itself bias the
	 * parent process — any positional bias the assertions see comes from the habitat term alone.
	 */
	TStrongObjectPtr<UArcForestTestSurfaceData> MakeSlopeRampSurface(float SideCm)
	{
		UArcForestTestSurfaceData* Surface = NewObject<UArcForestTestSurfaceData>();
		const float PeakZ = 0.5f * SideCm;
		Surface->Initialize(FBox(
			FVector(0.0, 0.0, -100.0),
			FVector(static_cast<double>(SideCm), static_cast<double>(SideCm), static_cast<double>(PeakZ) + 100.0)));
		Surface->FlatSurfaceZ = 0.0f;
		Surface->HeightFunction = [SideCm](float X, float /*Y*/) -> float
		{
			return (X * X) / (2.0f * SideCm);
		};
		return TStrongObjectPtr<UArcForestTestSurfaceData>(Surface);
	}

	/** A flat surface of the given side (Z = 0 everywhere) — morphology carries no structure. */
	TStrongObjectPtr<UArcForestTestSurfaceData> MakeFlatSurface(float SideCm)
	{
		UArcForestTestSurfaceData* Surface = NewObject<UArcForestTestSurfaceData>();
		Surface->Initialize(FBox(
			FVector(0.0, 0.0, -100.0),
			FVector(static_cast<double>(SideCm), static_cast<double>(SideCm), 100.0)));
		Surface->FlatSurfaceZ = 0.0f;
		return TStrongObjectPtr<UArcForestTestSurfaceData>(Surface);
	}

	/** Terrain params matching the node defaults, with an explicit absolute elevation range. */
	FArcForestTerrainFieldParams MakeTerrainParams(float ElevationMaxCm)
	{
		FArcForestTerrainFieldParams TerrainParams;
		TerrainParams.MaxSlopeDegrees = 45.0f;
		TerrainParams.SunAzimuthDegrees = 0.0f;
		TerrainParams.ElevationMinCm = 0.0f;
		TerrainParams.ElevationMaxCm = ElevationMaxCm;
		return TerrainParams;
	}

	/**
	 * Author a response curve as PIECEWISE-LINEAR keys. Interp mode is forced to RCIM_Linear because
	 * FRichCurveKey defaults to RCIM_Cubic with auto tangents, which would ease the curve and make the
	 * flattened LUT differ from the straight line the expected values below are derived from.
	 */
	void SetLinearCurveKeys(FRuntimeFloatCurve& Curve, TArrayView<const FVector2f> Keys)
	{
		FRichCurve* Rich = Curve.GetRichCurve();
		Rich->Reset();
		for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
		{
			const FKeyHandle Handle = Rich->AddKey(Keys[KeyIndex].X, Keys[KeyIndex].Y);
			Rich->SetKeyInterpMode(Handle, RCIM_Linear);
		}
	}

	/** One authored habitat response over Field, weight Weight, curve given as linear keys. */
	FArcForestFieldResponse MakeResponse(
		EArcForestHabitatField Field,
		float Weight,
		TArrayView<const FVector2f> Keys)
	{
		FArcForestFieldResponse Response;
		Response.Field = Field;
		Response.Weight = Weight;
		SetLinearCurveKeys(Response.ResponseCurve, Keys);
		return Response;
	}

	/** Flatten a species table's authored curves into a COPY of the fixture bundle (production path). */
	FArcForestHabitatInputs FlattenInto(
		const FArcForestHabitatInputs& BaseHabitat,
		const UArcForestSpeciesDefinition& Species)
	{
		FArcForestHabitatInputs Habitat = BaseHabitat;
		ArcForestSpeciesHabitat::FlattenFieldResponses(Species, Habitat);
		return Habitat;
	}

	/** Mean X of a point set; 0 for an empty set (callers assert non-emptiness first). */
	float MeanX(TArrayView<const FVector2f> Points)
	{
		if (Points.Num() == 0)
		{
			return 0.0f;
		}
		double Sum = 0.0;
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			Sum += static_cast<double>(Points[Index].X);
		}
		return static_cast<float>(Sum / static_cast<double>(Points.Num()));
	}

	/** Mean X of one species' placed plants; 0 for an empty slice. */
	float MeanXOfSpecies(const FArcForestResult& Result, int32 SpeciesId)
	{
		double Sum = 0.0;
		int32 Count = 0;
		for (int32 Index = 0; Index < Result.Num(); ++Index)
		{
			if (Result.Species[Index] == SpeciesId)
			{
				Sum += static_cast<double>(Result.PosXY[Index].X);
				++Count;
			}
		}
		return Count > 0 ? static_cast<float>(Sum / static_cast<double>(Count)) : 0.0f;
	}

	/** Number of placed plants of one species. */
	int32 CountOfSpecies(const FArcForestResult& Result, int32 SpeciesId)
	{
		int32 Count = 0;
		for (int32 Index = 0; Index < Result.Num(); ++Index)
		{
			Count += (Result.Species[Index] == SpeciesId) ? 1 : 0;
		}
		return Count;
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

	/**
	 * ELEMENT-WISE identity across EVERY parallel array of a result — not a count/statistic match.
	 * This is what "reproduces the baseline exactly" has to mean for an IEEE-754 identity claim: a
	 * habitat term that multiplied by a computed 0.999999 instead of an exact 1.0 would still produce
	 * the same COUNT most of the time, and would sail through any statistical comparison.
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

	/** Pearson correlation of two equal-length samples; 0 when either has no variance. */
	float PearsonCorrelation(TArrayView<const float> X, TArrayView<const float> Y)
	{
		const int32 Count = FMath::Min(X.Num(), Y.Num());
		if (Count < 2)
		{
			return 0.0f;
		}

		double SumX = 0.0;
		double SumY = 0.0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			SumX += static_cast<double>(X[Index]);
			SumY += static_cast<double>(Y[Index]);
		}
		const double MeanXValue = SumX / static_cast<double>(Count);
		const double MeanYValue = SumY / static_cast<double>(Count);

		double Covariance = 0.0;
		double VarianceX = 0.0;
		double VarianceY = 0.0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double Dx = static_cast<double>(X[Index]) - MeanXValue;
			const double Dy = static_cast<double>(Y[Index]) - MeanYValue;
			Covariance += Dx * Dy;
			VarianceX += Dx * Dx;
			VarianceY += Dy * Dy;
		}
		if (VarianceX <= 0.0 || VarianceY <= 0.0)
		{
			return 0.0f;
		}
		return static_cast<float>(Covariance / FMath::Sqrt(VarianceX * VarianceY));
	}

	/**
	 * Bucket a result's plants into a BucketsPerAxis^2 grid over Bounds and report, per bucket, the
	 * plant count and the moisture-noise value at the bucket centre. Feeding those two series to
	 * PearsonCorrelation is the "realized density correlates with the sampled field" statistic.
	 */
	void BucketDensityAgainstNoise(
		const FArcForestResult& Result,
		const FBox2f& Bounds,
		int32 BucketsPerAxis,
		const FArcForestNoiseParams& NoiseParams,
		int32 SolveSeed,
		TArray<float>& OutCounts,
		TArray<float>& OutNoise)
	{
		const int32 BucketCount = BucketsPerAxis * BucketsPerAxis;
		OutCounts.Reset();
		OutCounts.SetNumZeroed(BucketCount);
		OutNoise.Reset();
		OutNoise.SetNumUninitialized(BucketCount);

		const FVector2f Size = Bounds.GetSize();
		const FIntPoint BucketDims(BucketsPerAxis, BucketsPerAxis);
		for (int32 Row = 0; Row < BucketsPerAxis; ++Row)
		{
			for (int32 Col = 0; Col < BucketsPerAxis; ++Col)
			{
				const FVector2f Centre = ArcForestField::CellCentre(Bounds, BucketDims, Col, Row);
				OutNoise[(Row * BucketsPerAxis) + Col] =
					ArcForestNoise::SampleNoise01(NoiseParams, Centre, SolveSeed);
			}
		}

		for (int32 Index = 0; Index < Result.Num(); ++Index)
		{
			const FVector2f Local = Result.PosXY[Index] - Bounds.Min;
			const int32 Col = FMath::Clamp(
				FMath::FloorToInt((Local.X / Size.X) * static_cast<float>(BucketsPerAxis)),
				0, BucketsPerAxis - 1);
			const int32 Row = FMath::Clamp(
				FMath::FloorToInt((Local.Y / Size.Y) * static_cast<float>(BucketsPerAxis)),
				0, BucketsPerAxis - 1);
			OutCounts[(Row * BucketsPerAxis) + Col] += 1.0f;
		}
	}

	/** Arithmetic mean of a scalar series; 0 for an empty series. */
	float MeanOf(TArrayView<const float> Values)
	{
		if (Values.Num() == 0)
		{
			return 0.0f;
		}
		double Sum = 0.0;
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			Sum += static_cast<double>(Values[Index]);
		}
		return static_cast<float>(Sum / static_cast<double>(Values.Num()));
	}

	/** Mean moisture-noise value sampled at a point set; 0 for an empty set. */
	float MeanNoiseAtPoints(
		TArrayView<const FVector2f> Points,
		const FArcForestNoiseParams& NoiseParams,
		int32 SolveSeed)
	{
		if (Points.Num() == 0)
		{
			return 0.0f;
		}
		double Sum = 0.0;
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			Sum += static_cast<double>(
				ArcForestNoise::SampleNoise01(NoiseParams, Points[Index], SolveSeed));
		}
		return static_cast<float>(Sum / static_cast<double>(Points.Num()));
	}
}

TEST_CLASS(ArcForestHabitat, "ArcPCGExtension.ForestSim.Habitat")
{
	// CLUMP PARENTS FOLLOW THE SLOPE GRADIENT — the primary consumption-site test.
	//
	// MUTANT KILLED: deleting the HabitatResponse factor from FArcForestPatchField::Build
	// (ArcForestPatchField.cpp — `AcceptProb = Clamp(Suitability * HabitatResponse)` reverted to
	// `Rng.FRand() < Suitability`). The assertion reads PARENT POSITIONS straight out of
	// FArcForestPatchField::Build, so under that mutant the habitat arm's parent mean X collapses back
	// onto the domain midpoint -> RED. Nothing else in this module asserts on Build's output under a
	// non-uniform habitat, so nothing else catches it.
	//
	// THE BASELINE ARM *IS* THE MUTANT'S OUTPUT, BIT-FOR-BIT — which is how both sides are measured here
	// without touching production code. Under the mutant EvaluateHabitatResponse is never called and
	// AcceptProb == Suitability; with an empty response set it is called and returns EXACTLY 1.0, so
	// AcceptProb == Clamp(Suitability * 1.0) == Suitability. Same computation, same per-species stream
	// (seeded from HashCombine(Seed, SpeciesIndex+1), independent of the response set), same suitability
	// grid. So the control numbers below are literally the numbers the mutant would produce.
	//
	// WHY THE MEAN IS DERIVABLE: suitability is 1.0 in every cell (surface Density 1), so the ONLY
	// spatially varying acceptance factor is the response. Parents are candidates drawn uniformly over
	// the bounds and kept with probability w(x) = slope01(x) = atan(x/Side)/atan(1), so the accepted set
	// has density proportional to w and E[x]/Side = INT u*atan(u) du / INT atan(u) du over [0,1]
	// = 0.2854/0.4388 = 0.6504 — i.e. a predicted 2602 cm on a 4000 cm domain versus a 2000 cm midpoint.
	// (The realised 2573.5 is slightly lower because the one-sided border differences and the bilinear
	// half-cell clamp lift slope01 off 0 at the low edge — measured 0.0795 there, not 0.)
	//
	// MEASURED on this branch, BOTH SIDES (seed 31337, 40x40 m, PatchDensity 0.25 -> 400 candidates,
	// slope01 ramping 0.0795 -> 0.9589 across the 20x20 derived grid):
	//   BASELINE / MUTANT (no FieldResponses): 400 parents, mean X = 1995.0 cm (midpoint 2000.0).
	//   HABITAT           (rising slope curve): 222 parents, mean X = 2573.5 cm.
	// Thresholds: habitat mean must exceed 2350 (margin +223.5 on the live side, -355.0 on the mutant
	// side) and the arm-to-arm difference must exceed 350 (measured 578.5). The uniform arm's standard
	// error on the mean is 1155/sqrt(400) ~= 58 cm, so the control bound of 250 cm is ~4.3 SE.
	//
	// THE EXACT PARENT COUNTS ARE PINNED HERE, and this is the only place in the repo that pins them
	// (ArcForestSeedingContractTests asserts only > 0 / == 0, and every other parent assertion in this
	// file is a loose inequality). Both numbers are fully deterministic — one fixed seed, one integer
	// LCG, no threading — so:
	//   * BASELINE == 400 pins Build's DETERMINISTIC CANDIDATE COUNT, round(PatchDensity * AreaMeters2)
	//     (ArcForestPatchField.cpp:79-80). Suitability is 1 in every cell and an empty response set
	//     returns exactly 1.0, so AcceptProb == 1.0 and FRand() ∈ [0,1) accepts every candidate: the
	//     count is 400 iff the candidate count is 400.
	//   * HABITAT == 222 pins Build's PER-CANDIDATE DRAW COUNT — three FRand-family draws per candidate
	//     (two for the position, one for acceptance) in exactly that order. Adding a fourth draw inside
	//     the habitat branch re-rolls every authored forest's clump layout while leaving every
	//     statistical assertion in this file green (the thinned distribution is unchanged, only the
	//     realisation), so a golden count is the only assertion that catches it. It is deliberately
	//     brittle against ANY change to the seeding stream or the slope derivation: that is the point.
	TEST_METHOD(Habitat_ClumpParentsFollowSlopeGradient)
	{
		using namespace ArcForestHabitatTestsLocal;

		constexpr float SideCm = 4000.0f;
		constexpr int32 ExpectedBaselineParents = 400;
		constexpr int32 ExpectedHabitatParents = 222;

		TStrongObjectPtr<UArcForestTestSurfaceData> Surface = MakeSlopeRampSurface(SideCm);
		FHabitatFixture Fixture;
		BakeFixtureFromSurface(*Surface, Surface->GetBounds(), MakeTerrainParams(0.5f * SideCm), Fixture);

		// FIXTURE PRECONDITION — the slope field must actually RAMP along X, else a slope-keyed response
		// is spatially uniform and the whole test is vacuous (the linear-ramp trap in the helper note).
		const float SlopeAtLowX = Fixture.Habitat.SlopeField.SampleBilinear(FVector2f(200.0f, 0.5f * SideCm));
		const float SlopeAtHighX = Fixture.Habitat.SlopeField.SampleBilinear(FVector2f(3800.0f, 0.5f * SideCm));
		UE_LOGF(LogArcForestHabitatTest, Display,"[SlopeGradient] slope01 lowX=%.4f highX=%.4f dims=%dx%d",
			SlopeAtLowX, SlopeAtHighX, Fixture.GridDims.X, Fixture.GridDims.Y);
		ASSERT_THAT(IsTrue(SlopeAtHighX - SlopeAtLowX > 0.5f,
			TEXT("The derived slope field must rise by more than 0.5 across the domain, else a "
				"slope-keyed response is spatially uniform and every placement assertion is vacuous")));

		// Species rows are IDENTICAL between the arms except for FieldResponses, which is set
		// post-construction on the entry (MakeSpecies deliberately carries no habitat parameters).
		TArray<FArcForestSpeciesEntry> BaselineTable;
		BaselineTable.Add(MakeSpecies(TEXT("SlopeFern"), EArcForestLayer::Field,
			100.0f, 60.0f, 0.2f, 0.1f, /*PatchRadiusSigma=*/300.0f, /*PatchDensity=*/0.25f));
		TStrongObjectPtr<UArcForestSpeciesDefinition> BaselineDef = MakeDefinition(BaselineTable);

		TArray<FArcForestSpeciesEntry> HabitatTable = BaselineTable;
		const FVector2f RisingKeys[2] = { FVector2f(0.0f, 0.0f), FVector2f(1.0f, 1.0f) };
		HabitatTable[0].FieldResponses.Add(
			MakeResponse(EArcForestHabitatField::Slope, 1.0f, RisingKeys));
		TStrongObjectPtr<UArcForestSpeciesDefinition> HabitatDef = MakeDefinition(MoveTemp(HabitatTable));

		FArcForestSimParams Params;
		Params.Seed = 31337;
		Params.SeedingDensity = 1.0f;
		Params.Iterations = 0;
		Params.Bounds2D = Fixture.Bounds2D;

		const FArcForestHabitatInputs BaselineHabitat = FlattenInto(Fixture.Habitat, *BaselineDef);
		const FArcForestHabitatInputs ResponsiveHabitat = FlattenInto(Fixture.Habitat, *HabitatDef);

		FArcForestPatchField BaselineField;
		BaselineField.Build(*BaselineDef, Params, Fixture.Suitability, Fixture.GridDims, BaselineHabitat);

		FArcForestPatchField ResponsiveField;
		ResponsiveField.Build(*HabitatDef, Params, Fixture.Suitability, Fixture.GridDims, ResponsiveHabitat);

		const TArrayView<const FVector2f> BaselineParents = BaselineField.GetParentPositions(0);
		const TArrayView<const FVector2f> ResponsiveParents = ResponsiveField.GetParentPositions(0);

		const float Midpoint = Fixture.Bounds2D.GetCenter().X;
		const float BaselineMeanX = MeanX(BaselineParents);
		const float ResponsiveMeanX = MeanX(ResponsiveParents);

		UE_LOGF(LogArcForestHabitatTest, Display,"[SlopeGradient] parents baseline=%d meanX=%.1f | habitat=%d meanX=%.1f | midpoint=%.1f",
			BaselineParents.Num(), BaselineMeanX, ResponsiveParents.Num(), ResponsiveMeanX, Midpoint);

		// EXACT counts, not inequalities — see the DRAW COUNT note above. These are the only assertions
		// in the repo that pin FArcForestPatchField::Build's candidate count and per-candidate draw
		// count; every other parent assertion (here and in ArcForestSeedingContractTests) is loose
		// enough that an extra FRand() in Build's habitat branch stays green.
		ASSERT_THAT(AreEqual(ExpectedBaselineParents, BaselineParents.Num(),
			TEXT("Build must draw exactly round(PatchDensity * AreaMeters2) = 400 candidates and, with "
				"uniform suitability 1.0 and an exact-1.0 empty response, accept every one of them")));
		ASSERT_THAT(AreEqual(ExpectedHabitatParents, ResponsiveParents.Num(),
			TEXT("The habitat arm's parent count is a GOLDEN value pinning Build's per-candidate draw "
				"count and order (position X, position Y, acceptance). RED if a draw is added, removed "
				"or reordered inside Build's habitat branch — which would silently re-roll every "
				"authored forest's clump layout while leaving every statistic in this file green")));

		// CONTROL — with no FieldResponses the parent process is unbiased, so its mean sits on the
		// midpoint. This is the value the mutant collapses the habitat arm onto.
		ASSERT_THAT(IsTrue(FMath::Abs(BaselineMeanX - Midpoint) < 250.0f,
			TEXT("Without FieldResponses, parents scatter uniformly and their mean X must sit within "
				"250 cm of the domain midpoint — this pins the value the habitat arm must move AWAY from")));

		// THE CONTRACT — parents must be biased materially upslope.
		ASSERT_THAT(IsTrue(ResponsiveMeanX > Midpoint + 350.0f,
			TEXT("A rising slope response must bias CLUMP PARENTS upslope: mean parent X must exceed the "
				"domain midpoint by more than 350 cm. RED if FArcForestPatchField::Build's HabitatResponse "
				"factor is removed, because the habitat arm then equals the unbiased baseline arm above")));
		ASSERT_THAT(IsTrue(ResponsiveMeanX - BaselineMeanX > 350.0f,
			TEXT("Stated as a DIFFERENCE against the same-seed baseline as well, so the assertion cannot "
				"be satisfied by a fixture drift that moved both arms")));
	}

	// OPPOSITE CURVES SEPARATE SPECIES — the "fern here, oak there" acceptance test.
	//
	// MUTANT KILLED: the same deleted HabitatResponse factor in FArcForestPatchField::Build. The two
	// species share one surface, one suitability grid and one seed and differ ONLY in their response
	// curve (rising vs falling over Slope), so under the mutant both parent sets become unbiased and the
	// separation collapses to noise -> RED. Note the third assertion (PLANT separation) would survive
	// that mutant on its own, because SeedPopulation's dart-acceptance term still thins within clumps —
	// which is exactly why the PARENT separation carries the mutant kill and the plant separation is
	// carried as the user-visible claim, not as the proof.
	//
	// The two species sit in DIFFERENT strata (mature heights 100 and 300 cm -> Field and Shrub) so
	// their placements are not coupled through the Poisson-disk floor; the separation measured is
	// habitat, not mutual exclusion.
	//
	// MEASURED on this branch, BOTH SIDES (seed 20260726, 40x40 m). As in the test above, the baseline
	// arm IS the mutant's output bit-for-bit (empty response set -> exact 1.0 -> AcceptProb == Suitability):
	//   BASELINE / MUTANT (no FieldResponses): 400/400 parents, meanX 2038.6 vs 1904.0, separation 134.6.
	//   HABITAT           (mirrored curves):   231/187 parents, meanX 2607.0 vs 1251.2, separation 1355.9;
	//                                          plants 543/453, meanX 2879.6 vs 955.0, separation 1924.5.
	// Thresholds: separation > 700 (margin +655.9 on the live side, -565.3 on the mutant side) and the
	// control |separation| < 250. The two species draw from INDEPENDENT per-species streams of 400
	// uniform candidates each, so the difference of their means has SE ~= sqrt(2)*58 = 82 cm; the
	// measured 134.6 is 1.6 SE and the 250 cm control bound is ~3 SE. Deterministic seed, so this is a
	// fixture-drift bound, not a flake bound.
	TEST_METHOD(Habitat_OppositeCurvesSeparateSpecies)
	{
		using namespace ArcForestHabitatTestsLocal;

		constexpr float SideCm = 4000.0f;
		constexpr int32 UpslopeSpecies = 0;
		constexpr int32 DownslopeSpecies = 1;

		TStrongObjectPtr<UArcForestTestSurfaceData> Surface = MakeSlopeRampSurface(SideCm);
		FHabitatFixture Fixture;
		BakeFixtureFromSurface(*Surface, Surface->GetBounds(), MakeTerrainParams(0.5f * SideCm), Fixture);

		TArray<FArcForestSpeciesEntry> BaselineTable;
		BaselineTable.Add(MakeSpecies(TEXT("UpslopeMoss"), EArcForestLayer::Field,
			/*MaxHeight=*/100.0f, 60.0f, 0.2f, /*TargetDensity=*/0.5f, 300.0f, 0.25f));
		BaselineTable.Add(MakeSpecies(TEXT("DownslopeFern"), EArcForestLayer::Shrub,
			/*MaxHeight=*/300.0f, 60.0f, 0.2f, /*TargetDensity=*/0.5f, 300.0f, 0.25f));
		TStrongObjectPtr<UArcForestSpeciesDefinition> BaselineDef = MakeDefinition(BaselineTable);

		TArray<FArcForestSpeciesEntry> HabitatTable = BaselineTable;
		const FVector2f RisingKeys[2] = { FVector2f(0.0f, 0.0f), FVector2f(1.0f, 1.0f) };
		const FVector2f FallingKeys[2] = { FVector2f(0.0f, 1.0f), FVector2f(1.0f, 0.0f) };
		HabitatTable[UpslopeSpecies].FieldResponses.Add(
			MakeResponse(EArcForestHabitatField::Slope, 1.0f, RisingKeys));
		HabitatTable[DownslopeSpecies].FieldResponses.Add(
			MakeResponse(EArcForestHabitatField::Slope, 1.0f, FallingKeys));
		TStrongObjectPtr<UArcForestSpeciesDefinition> HabitatDef = MakeDefinition(MoveTemp(HabitatTable));

		FArcForestSimParams Params;
		Params.Seed = 20260726;
		Params.SeedingDensity = 1.0f;
		Params.SpacingFraction = 0.2f;
		Params.WPatch = 20.0f; // p ~= 0.95 — nearly every dart is a Gaussian offspring of a parent
		Params.Iterations = 0;
		Params.Bounds2D = Fixture.Bounds2D;

		const FArcForestHabitatInputs BaselineHabitat = FlattenInto(Fixture.Habitat, *BaselineDef);
		const FArcForestHabitatInputs ResponsiveHabitat = FlattenInto(Fixture.Habitat, *HabitatDef);

		FArcForestPatchField BaselineField;
		BaselineField.Build(*BaselineDef, Params, Fixture.Suitability, Fixture.GridDims, BaselineHabitat);
		FArcForestPatchField ResponsiveField;
		ResponsiveField.Build(*HabitatDef, Params, Fixture.Suitability, Fixture.GridDims, ResponsiveHabitat);

		const float BaselineUpMeanX = MeanX(BaselineField.GetParentPositions(UpslopeSpecies));
		const float BaselineDownMeanX = MeanX(BaselineField.GetParentPositions(DownslopeSpecies));
		const float ResponsiveUpMeanX = MeanX(ResponsiveField.GetParentPositions(UpslopeSpecies));
		const float ResponsiveDownMeanX = MeanX(ResponsiveField.GetParentPositions(DownslopeSpecies));

		const float BaselineSeparation = BaselineUpMeanX - BaselineDownMeanX;
		const float ResponsiveSeparation = ResponsiveUpMeanX - ResponsiveDownMeanX;

		UE_LOGF(LogArcForestHabitatTest, Display,"[Separation] parents baseline up=%d/%.1f down=%d/%.1f sep=%.1f | habitat up=%d/%.1f "
				"down=%d/%.1f sep=%.1f",
			BaselineField.GetParentPositions(UpslopeSpecies).Num(), BaselineUpMeanX,
			BaselineField.GetParentPositions(DownslopeSpecies).Num(), BaselineDownMeanX, BaselineSeparation,
			ResponsiveField.GetParentPositions(UpslopeSpecies).Num(), ResponsiveUpMeanX,
			ResponsiveField.GetParentPositions(DownslopeSpecies).Num(), ResponsiveDownMeanX,
			ResponsiveSeparation);

		ASSERT_THAT(IsTrue(ResponsiveField.GetParentPositions(UpslopeSpecies).Num() > 50
			&& ResponsiveField.GetParentPositions(DownslopeSpecies).Num() > 50,
			TEXT("Both species must retain a large parent set, else the separation is noise")));

		// NON-VACUITY GUARD FOR THE CONTROL BELOW. MeanX returns 0 for an EMPTY set, so a regression that
		// emptied the baseline arm's parent sets would make |0 - 0| = 0 satisfy the control assertion
		// while proving nothing at all. The guard must cover the BASELINE arm specifically — the habitat
		// arm's guard above says nothing about it.
		ASSERT_THAT(IsTrue(BaselineField.GetParentPositions(UpslopeSpecies).Num() > 50
			&& BaselineField.GetParentPositions(DownslopeSpecies).Num() > 50,
			TEXT("The BASELINE arm must have non-empty parent sets for both species, else the zero "
				"separation asserted below is MeanX's empty-set 0 rather than a measured coincidence")));

		// CONTROL — identical rows minus the curves produce NO separation. This pins the value the
		// mutant collapses the habitat arm onto.
		ASSERT_THAT(IsTrue(FMath::Abs(BaselineSeparation) < 250.0f,
			TEXT("Without FieldResponses the two species' parent means must coincide within 250 cm — "
				"they differ only in their (absent) curves, so any separation here would be fixture noise")));

		// THE CONTRACT — mirrored curves must drive the two species apart along the gradient axis.
		ASSERT_THAT(IsTrue(ResponsiveSeparation > 700.0f,
			TEXT("Mirrored slope curves must separate the two species' CLUMP PARENTS by more than 700 cm "
				"along the gradient axis. RED if FArcForestPatchField::Build's HabitatResponse factor is "
				"removed: both parent sets then revert to the unbiased baseline above and the separation "
				"collapses to the control's ~0")));

		// The user-visible claim, on realized plants rather than parents. Carried as a claim, NOT as the
		// mutant proof — see the header note: dart acceptance alone would keep this green.
		const FArcForestSimulator Simulator;
		const FArcForestResult ResponsiveResult =
			Simulator.Simulate(Params, *HabitatDef, Fixture.Suitability, Fixture.GridDims, ResponsiveHabitat);
		const float PlantUpMeanX = MeanXOfSpecies(ResponsiveResult, UpslopeSpecies);
		const float PlantDownMeanX = MeanXOfSpecies(ResponsiveResult, DownslopeSpecies);

		UE_LOGF(LogArcForestHabitatTest, Display,"[Separation] plants up=%d/%.1f down=%d/%.1f sep=%.1f",
			CountOfSpecies(ResponsiveResult, UpslopeSpecies), PlantUpMeanX,
			CountOfSpecies(ResponsiveResult, DownslopeSpecies), PlantDownMeanX,
			PlantUpMeanX - PlantDownMeanX);

		ASSERT_THAT(IsTrue(CountOfSpecies(ResponsiveResult, UpslopeSpecies) > 50
			&& CountOfSpecies(ResponsiveResult, DownslopeSpecies) > 50,
			TEXT("Both species must actually seed plants, else the plant separation is vacuous")));
		ASSERT_THAT(IsTrue(PlantUpMeanX - PlantDownMeanX > 700.0f,
			TEXT("The realized STANDS must separate too, not just the parent lattice — this is the "
				"'fern here, oak there' outcome the feature exists for")));
	}

	// DART ACCEPTANCE BIASES A PARENTLESS SPECIES — the SECOND consumption site, isolated from the first.
	//
	// MUTANT KILLED: deleting the `* HabitatResponse` factor from SeedPopulation's dart acceptance
	// (ArcForestSimulator.cpp:329-335) while leaving FArcForestPatchField::Build fully intact. EVERY other
	// placement case in this file survives that mutant, because their claims are carried by the parent
	// lattice, which Build still biases — clumps land in the right places and only the density WITHIN
	// them stops respecting the curve. The shipped defect that hides behind that: a sharp Moisture curve
	// yields wet-hollow clumps of UNIFORM density instead of graded ones. This test is the only assertion
	// in the suite that goes RED on it.
	//
	// WHY THE ATTRIBUTION TO THE DART SITE IS AIRTIGHT, not merely plausible: the species is authored with
	// PatchDensity = 0, so Build's per-species loop hits `continue` at ArcForestPatchField.cpp:63-68
	// BEFORE reaching its candidate loop. Its habitat factor at :104-108 is never evaluated for this
	// species even once, and the species has no parents at all. With Parents.Num() == 0 the branch test at
	// ArcForestSimulator.cpp:294 is always false, so every dart's position is the raw uniform draw
	// (DrawUniformX, DrawUniformY) in BOTH arms — identical position distributions. The ONLY term that can
	// move the realized mean is therefore the dart-acceptance habitat factor. The precondition below
	// asserts the parent set is empty in BOTH arms so this cannot silently stop being true.
	//
	// PREDICTED MEAN: the same integral as Habitat_ClumpParentsFollowSlopeGradient, since this is again
	// uniform candidates kept with probability w(x) = slope01(x) = atan(x/Side)/atan(1):
	// E[x]/Side = INT u*atan(u) du / INT atan(u) du = 0.6504, i.e. ~2602 cm on a 4000 cm domain.
	//
	// AS IN THE TESTS ABOVE, THE BASELINE ARM IS THE MUTANT'S OUTPUT BIT-FOR-BIT: with no FieldResponses
	// EvaluateHabitatResponse returns EXACTLY 1.0, so AcceptProb == Clamp(Suitability * 1.0) ==
	// Suitability — the identical computation the mutant performs, on the identical serial stream (the
	// FIXED 7-DRAW BLOCK is unconditional, and the habitat term takes no stream). The control numbers
	// below are literally the numbers the mutant produces.
	//
	// MEASURED on this branch, BOTH SIDES (seed 515151, 40x40 m, PatchDensity 0 -> 0 parents,
	// TargetDensity 0.5 -> 800 uniform darts, SpacingFraction 0.2 with crown radius 60 -> MinDist 24 cm):
	//   BASELINE / MUTANT (no FieldResponses): 759 plants, mean X = 2030.9 cm (midpoint 2000.0).
	//   HABITAT           (rising slope curve): 433 plants, mean X = 2586.5 cm.
	// Thresholds: habitat mean must exceed midpoint + 350 (= 2350; margin +236.5 on the live side, -319.1
	// on the mutant side) and the arm-to-arm difference must exceed 350 (measured 555.6). The uniform
	// arm's SE on the mean is 1155/sqrt(759) ~= 42 cm, so the 250 cm control bound is ~6 SE.
	//
	// POISSON-DISK REJECTION IS SMALL AND WORKS AGAINST THE CLAIM, so it cannot manufacture the result:
	// 41 of 800 baseline darts were rejected for spacing (759 placed). Rejection rises with LOCAL density,
	// so in the habitat arm it thins the crowded upslope half harder than the sparse downslope half —
	// pulling the measured mean DOWN, not up. The 2586.5 is therefore a lower bound on the term's effect.
	TEST_METHOD(Habitat_DartAcceptanceBiasesParentlessSpecies)
	{
		using namespace ArcForestHabitatTestsLocal;

		constexpr float SideCm = 4000.0f;
		constexpr int32 ParentlessSpecies = 0;

		TStrongObjectPtr<UArcForestTestSurfaceData> Surface = MakeSlopeRampSurface(SideCm);
		FHabitatFixture Fixture;
		BakeFixtureFromSurface(*Surface, Surface->GetBounds(), MakeTerrainParams(0.5f * SideCm), Fixture);

		// FIXTURE PRECONDITION — the slope field must RAMP, else a slope-keyed response is uniform and the
		// whole test is vacuous (the linear-ramp trap documented on MakeSlopeRampSurface).
		const float SlopeAtLowX = Fixture.Habitat.SlopeField.SampleBilinear(FVector2f(200.0f, 0.5f * SideCm));
		const float SlopeAtHighX = Fixture.Habitat.SlopeField.SampleBilinear(FVector2f(3800.0f, 0.5f * SideCm));
		ASSERT_THAT(IsTrue(SlopeAtHighX - SlopeAtLowX > 0.5f,
			TEXT("The derived slope field must rise by more than 0.5 across the domain, else a "
				"slope-keyed response is spatially uniform and the placement assertion is vacuous")));

		// PatchDensity = 0 is the whole construction: it makes Build skip the species entirely, so its
		// habitat factor provably cannot contribute to anything measured below.
		TArray<FArcForestSpeciesEntry> BaselineTable;
		BaselineTable.Add(MakeSpecies(TEXT("ParentlessSedge"), EArcForestLayer::Field,
			/*MaxHeight=*/100.0f, /*MaxCrownRadius=*/60.0f, 0.2f, /*TargetDensity=*/0.5f,
			/*PatchRadiusSigma=*/200.0f, /*PatchDensity=*/0.0f));
		TStrongObjectPtr<UArcForestSpeciesDefinition> BaselineDef = MakeDefinition(BaselineTable);

		TArray<FArcForestSpeciesEntry> HabitatTable = BaselineTable;
		const FVector2f RisingKeys[2] = { FVector2f(0.0f, 0.0f), FVector2f(1.0f, 1.0f) };
		HabitatTable[ParentlessSpecies].FieldResponses.Add(
			MakeResponse(EArcForestHabitatField::Slope, 1.0f, RisingKeys));
		TStrongObjectPtr<UArcForestSpeciesDefinition> HabitatDef = MakeDefinition(MoveTemp(HabitatTable));

		FArcForestSimParams Params;
		Params.Seed = 515151;
		Params.SeedingDensity = 1.0f;
		Params.SpacingFraction = 0.2f;
		Params.WPatch = 20.0f; // irrelevant with no parents; set explicitly so it is not a hidden default
		Params.Iterations = 0;
		Params.Bounds2D = Fixture.Bounds2D;

		const FArcForestHabitatInputs BaselineHabitat = FlattenInto(Fixture.Habitat, *BaselineDef);
		const FArcForestHabitatInputs ResponsiveHabitat = FlattenInto(Fixture.Habitat, *HabitatDef);

		// FIXTURE PRECONDITION — the parent sets must be EMPTY in BOTH arms. This is what forbids
		// FArcForestPatchField::Build from being the cause of any bias measured below.
		FArcForestPatchField BaselineField;
		BaselineField.Build(*BaselineDef, Params, Fixture.Suitability, Fixture.GridDims, BaselineHabitat);
		FArcForestPatchField ResponsiveField;
		ResponsiveField.Build(*HabitatDef, Params, Fixture.Suitability, Fixture.GridDims, ResponsiveHabitat);
		ASSERT_THAT(AreEqual(0, BaselineField.GetParentPositions(ParentlessSpecies).Num(),
			TEXT("PatchDensity = 0 must leave the baseline arm's parent set EMPTY — Build skips the "
				"species before its candidate loop")));
		ASSERT_THAT(AreEqual(0, ResponsiveField.GetParentPositions(ParentlessSpecies).Num(),
			TEXT("PatchDensity = 0 must leave the HABITAT arm's parent set empty too. If this ever fails, "
				"the bias asserted below could come from Build instead of from dart acceptance and the "
				"attribution of this test collapses")));

		const FArcForestSimulator Simulator;
		const FArcForestResult BaselineResult =
			Simulator.Simulate(Params, *BaselineDef, Fixture.Suitability, Fixture.GridDims, BaselineHabitat);
		const FArcForestResult ResponsiveResult =
			Simulator.Simulate(Params, *HabitatDef, Fixture.Suitability, Fixture.GridDims, ResponsiveHabitat);

		const int32 BaselineCount = CountOfSpecies(BaselineResult, ParentlessSpecies);
		const int32 ResponsiveCount = CountOfSpecies(ResponsiveResult, ParentlessSpecies);
		const float BaselineMeanX = MeanXOfSpecies(BaselineResult, ParentlessSpecies);
		const float ResponsiveMeanX = MeanXOfSpecies(ResponsiveResult, ParentlessSpecies);
		const float Midpoint = Fixture.Bounds2D.GetCenter().X;

		UE_LOGF(LogArcForestHabitatTest, Display,"[DartAcceptance] plants baseline=%d meanX=%.1f | habitat=%d meanX=%.1f | midpoint=%.1f",
			BaselineCount, BaselineMeanX, ResponsiveCount, ResponsiveMeanX, Midpoint);

		// MeanXOfSpecies returns 0 for an empty slice, so both arms are guarded before any mean is read.
		ASSERT_THAT(IsTrue(BaselineCount > 100,
			TEXT("The baseline arm must seed a large stand, else its mean X is MeanXOfSpecies' empty-set 0 "
				"or pure noise rather than a measured control")));
		ASSERT_THAT(IsTrue(ResponsiveCount > 100,
			TEXT("The habitat arm must still seed a large stand, else the biased mean is noise")));

		// CONTROL — with no response the uniform darts are unbiased, so the mean sits on the midpoint.
		// This is the value the mutant collapses the habitat arm onto.
		ASSERT_THAT(IsTrue(FMath::Abs(BaselineMeanX - Midpoint) < 250.0f,
			TEXT("Without FieldResponses a parentless species' darts stay uniform, so their mean X must "
				"sit within 250 cm of the domain midpoint — this pins the value the habitat arm must move "
				"AWAY from, and it is exactly what the mutant produces")));

		// THE CONTRACT — the dart-acceptance factor alone must bias placement upslope.
		ASSERT_THAT(IsTrue(ResponsiveMeanX > Midpoint + 350.0f,
			TEXT("A rising slope response must bias a PARENTLESS species' realized plants upslope by more "
				"than 350 cm past the midpoint. Since FArcForestPatchField::Build skips this species "
				"entirely (PatchDensity = 0), the only term that can produce this is the habitat factor in "
				"SeedPopulation's dart acceptance. RED if `* HabitatResponse` is deleted there, even with "
				"Build left fully intact")));
		ASSERT_THAT(IsTrue(ResponsiveMeanX - BaselineMeanX > 350.0f,
			TEXT("Stated as a DIFFERENCE against the same-seed baseline as well, so the assertion cannot "
				"be satisfied by a fixture drift that moved both arms")));
		ASSERT_THAT(IsTrue(ResponsiveCount < BaselineCount,
			TEXT("A response curve that is <= 1 everywhere can only thin, so the habitat arm must place "
				"strictly fewer plants — pins the DIRECTION of the dart-acceptance factor as well as its "
				"spatial effect")));
	}

	// MOISTURE NOISE DRIVES PLACEMENT — the one axis that is analytic rather than grid-derived, and the
	// one whose "absent" marker is Frequency <= 0 rather than an empty grid.
	//
	// MUTANTS KILLED:
	//   (1) the Moisture arm of ArcForestFieldLocal::SampleAxis returning unsupplied (or the response
	//       being skipped) when a live field IS supplied — placement then decorrelates from the noise.
	//   (2) sampling the noise at a fixed/wrong position (e.g. the bounds centre) instead of Pos — the
	//       per-bucket correlation collapses because the factor becomes spatially constant.
	//   (3) the deleted HabitatResponse factor in FArcForestPatchField::Build — caught by the
	//       parent-side assertion (mean noise AT PARENTS), which reverts to the domain mean.
	// The surface is FLAT here on purpose: slope 0 / exposure 0.5 / elevation 0 carry no structure, so
	// moisture is provably the only spatially varying factor.
	//
	// MEASURED on this branch, BOTH SIDES (seed 606061, 80x80 m, Perlin f=0.001, 3 octaves, 20x20
	// buckets; domain mean noise over bucket centres = 0.5014):
	//   BASELINE / MUTANT (no FieldResponses): 1850 plants, bucket corr(count, noise) = 0.0968;
	//                                          320 parents, mean noise at parents 0.5052.
	//   HABITAT  (0.45->0.55 wet ramp):        1210 plants, bucket corr(count, noise) = 0.6148;
	//                                          174 parents, mean noise at parents 0.5679.
	// Thresholds: corr > 0.35 (margin +0.265 live / -0.253 mutant) and parent mean noise > domain + 0.035
	// i.e. > 0.5364 (margin +0.0315 live / -0.0312 mutant) — deliberately balanced across the two sides.
	// The baseline correlation is not exactly 0 because a Thomas process' bucket counts are spatially
	// autocorrelated and pick up a little spurious alignment with a smooth field; 0.0968 against the
	// 0.15 control bound is the honest size of that effect, measured rather than assumed.
	TEST_METHOD(Habitat_MoistureNoiseDrivesPlacement)
	{
		using namespace ArcForestHabitatTestsLocal;

		constexpr float SideCm = 8000.0f;
		constexpr int32 BucketsPerAxis = 20;

		TStrongObjectPtr<UArcForestTestSurfaceData> Surface = MakeFlatSurface(SideCm);
		FHabitatFixture Fixture;
		BakeFixtureFromSurface(*Surface, Surface->GetBounds(), MakeTerrainParams(1000.0f), Fixture);

		// Opt the moisture axis IN. A default-constructed FArcForestNoiseParams is already a LIVE field,
		// but BakeFixtureFromSurface zeroes Frequency, so the field is enabled explicitly here — and
		// Frequency > 0 is precisely the "supplied" marker the response-skip rule keys off.
		FArcForestNoiseParams Noise;
		Noise.Type = EArcForestNoiseType::Perlin;
		Noise.Frequency = 0.001f;
		Noise.Octaves = 3;
		Noise.Lacunarity = 2.0f;
		Noise.Gain = 0.5f;
		Noise.SeedOffset = 0;
		Fixture.Habitat.MoistureNoise = Noise;

		TArray<FArcForestSpeciesEntry> BaselineTable;
		BaselineTable.Add(MakeSpecies(TEXT("WetlandSedge"), EArcForestLayer::Field,
			100.0f, 60.0f, 0.2f, /*TargetDensity=*/0.3f, /*PatchRadiusSigma=*/250.0f,
			/*PatchDensity=*/0.05f));
		TStrongObjectPtr<UArcForestSpeciesDefinition> BaselineDef = MakeDefinition(BaselineTable);

		TArray<FArcForestSpeciesEntry> HabitatTable = BaselineTable;
		// SHARP relative to the realized spread: Perlin at 3 octaves clusters well inside [0,1] around
		// 0.5, so a 0.45 -> 0.55 ramp is a decisive wet/dry split rather than a gentle preference. A
		// gentler 0.35 -> 0.65 ramp was measured first and gave corr 0.5447 / parent mean noise 0.5512 —
		// live, but with only ~0.01 of margin on the parent statistic, so the curve was sharpened rather
		// than the threshold lowered to meet it.
		const FVector2f WetKeys[4] =
		{
			FVector2f(0.0f, 0.0f), FVector2f(0.45f, 0.0f), FVector2f(0.55f, 1.0f), FVector2f(1.0f, 1.0f)
		};
		HabitatTable[0].FieldResponses.Add(
			MakeResponse(EArcForestHabitatField::Moisture, 1.0f, WetKeys));
		TStrongObjectPtr<UArcForestSpeciesDefinition> HabitatDef = MakeDefinition(MoveTemp(HabitatTable));

		FArcForestSimParams Params;
		Params.Seed = 606061;
		Params.SeedingDensity = 1.0f;
		Params.SpacingFraction = 0.2f;
		Params.WPatch = 20.0f;
		Params.Iterations = 0;
		Params.Bounds2D = Fixture.Bounds2D;

		const FArcForestHabitatInputs BaselineHabitat = FlattenInto(Fixture.Habitat, *BaselineDef);
		const FArcForestHabitatInputs ResponsiveHabitat = FlattenInto(Fixture.Habitat, *HabitatDef);

		// FIXTURE PRECONDITION — the morphology fields must be structureless, so moisture is the ONLY
		// varying factor and the correlation cannot be smuggled in by slope or elevation.
		const float SlopeLow = Fixture.Habitat.SlopeField.SampleBilinear(FVector2f(400.0f, 400.0f));
		const float SlopeHigh = Fixture.Habitat.SlopeField.SampleBilinear(FVector2f(7600.0f, 7600.0f));
		ASSERT_THAT(IsTrue(FMath::Abs(SlopeHigh - SlopeLow) < KINDA_SMALL_NUMBER,
			TEXT("The flat surface must yield a structureless slope field, so moisture is provably the "
				"only spatially varying habitat factor in this fixture")));

		const FArcForestSimulator Simulator;
		const FArcForestResult BaselineResult =
			Simulator.Simulate(Params, *BaselineDef, Fixture.Suitability, Fixture.GridDims, BaselineHabitat);
		const FArcForestResult ResponsiveResult =
			Simulator.Simulate(Params, *HabitatDef, Fixture.Suitability, Fixture.GridDims, ResponsiveHabitat);

		TArray<float> BaselineCounts;
		TArray<float> BucketNoise;
		BucketDensityAgainstNoise(BaselineResult, Fixture.Bounds2D, BucketsPerAxis, Noise, Params.Seed,
			BaselineCounts, BucketNoise);
		TArray<float> ResponsiveCounts;
		TArray<float> ResponsiveBucketNoise;
		BucketDensityAgainstNoise(ResponsiveResult, Fixture.Bounds2D, BucketsPerAxis, Noise, Params.Seed,
			ResponsiveCounts, ResponsiveBucketNoise);

		const float BaselineCorrelation = PearsonCorrelation(BaselineCounts, BucketNoise);
		const float ResponsiveCorrelation = PearsonCorrelation(ResponsiveCounts, ResponsiveBucketNoise);

		// Parent-side statistic: the mean noise the accepted CLUMP PARENTS sit at, against the domain
		// mean over bucket centres. This is the assertion that fails if Build's factor is deleted.
		FArcForestPatchField BaselineField;
		BaselineField.Build(*BaselineDef, Params, Fixture.Suitability, Fixture.GridDims, BaselineHabitat);
		FArcForestPatchField ResponsiveField;
		ResponsiveField.Build(*HabitatDef, Params, Fixture.Suitability, Fixture.GridDims, ResponsiveHabitat);

		const float BaselineParentNoise =
			MeanNoiseAtPoints(BaselineField.GetParentPositions(0), Noise, Params.Seed);
		const float ResponsiveParentNoise =
			MeanNoiseAtPoints(ResponsiveField.GetParentPositions(0), Noise, Params.Seed);
		const float DomainMeanNoise = MeanOf(BucketNoise);

		UE_LOGF(LogArcForestHabitatTest, Display,"[Moisture] plants baseline=%d corr=%.4f | habitat=%d corr=%.4f || parents baseline=%d "
				"meanNoise=%.4f habitat=%d meanNoise=%.4f domainMeanNoise=%.4f",
			BaselineResult.Num(), BaselineCorrelation, ResponsiveResult.Num(), ResponsiveCorrelation,
			BaselineField.GetParentPositions(0).Num(), BaselineParentNoise,
			ResponsiveField.GetParentPositions(0).Num(), ResponsiveParentNoise, DomainMeanNoise);

		ASSERT_THAT(IsTrue(BaselineResult.Num() > 200 && ResponsiveResult.Num() > 200,
			TEXT("Both arms must seed enough plants for a 400-bucket correlation to mean anything")));
		ASSERT_THAT(IsTrue(ResponsiveField.GetParentPositions(0).Num() > 30,
			TEXT("The habitat arm must retain enough clump parents for the parent-side statistic")));

		// CONTROL — with no response the placement is independent of the noise field.
		ASSERT_THAT(IsTrue(FMath::Abs(BaselineCorrelation) < 0.15f,
			TEXT("Without a Moisture response, realized bucket density must be uncorrelated with the "
				"noise field — this pins the value the habitat arm has to beat")));

		// THE CONTRACT — realized density tracks the noise field.
		ASSERT_THAT(IsTrue(ResponsiveCorrelation > 0.35f,
			TEXT("A Moisture-keyed species must CONCENTRATE where the analytic noise is high: the "
				"per-bucket correlation between realized plant count and sampled noise must exceed 0.35. "
				"RED if the Moisture axis reports 'unsupplied' for a live field (Frequency > 0), or if the "
				"noise is sampled anywhere other than the candidate position")));
		ASSERT_THAT(IsTrue(ResponsiveParentNoise > DomainMeanNoise + 0.035f,
			TEXT("The accepted CLUMP PARENTS themselves must sit at above-average moisture. RED if "
				"FArcForestPatchField::Build's HabitatResponse factor is removed — parents then scatter "
				"uniformly and their mean noise reverts to the domain mean, as the baseline arm shows")));
		ASSERT_THAT(IsTrue(FMath::Abs(BaselineParentNoise - DomainMeanNoise) < 0.03f,
			TEXT("Control for the parent-side statistic: an unresponsive species' parents must sit at the "
				"domain mean moisture")));
	}

	// EMPTY RESPONSES REPRODUCE THE BASELINE — ELEMENT-WISE, not statistically.
	//
	// MUTANTS KILLED:
	//   (1) removing EvaluateHabitatResponse's exact-1.0 fast path in favour of anything that performs
	//       arithmetic on the empty set (a computed 0.99999f product breaks the IEEE-754 identity and
	//       perturbs FRand comparisons at the acceptance boundary, shifting placements);
	//   (2) the FLATTENER'S DROP RULE for an unauthored curve (ArcForestSpeciesDefinition.cpp:54-57): the
	//       second arm below authors a response row whose ResponseCurve has NO KEYS, and
	//       FlattenFieldResponses must skip it so no LUT is emitted at all. A mutant that dropped the
	//       HasAuthoredKeys guard would emit a LUT filled from FRichCurve::Eval's default 0 — a
	//       zero-filled LUT that multiplies the species' acceptance to 0 and empties its whole slice.
	//       NOT the absent-axis response-skip rule (ArcForestField.cpp:234-238, normative at
	//       ArcForestField.h:290-299): this fixture SUPPLIES Slope, so Axis.bSupplied is never false and
	//       that branch is not reached. Absent-axis skipping is an acknowledged gap — see the file-level
	//       KNOWN GAPS note.
	// Both arms are compared against a run that omits the Habitat argument ENTIRELY — i.e. the
	// pre-Step-7 call shape — so the claim proven is exactly "adding habitat changed nothing".
	//
	// MEASURED on this branch (seed 8080, 40x40 m, two species): baseline 1146 plants; empty-responses arm
	// 1146 plants, ELEMENT-WISE identical; keyless-curve arm 1146 plants, ELEMENT-WISE identical; the
	// flattener emitted 2 species slots and 0 LUTs for the keyless row. A zero-filled-LUT mutant would
	// multiply species 0's acceptance to 0 and drop its whole slice.
	TEST_METHOD(Habitat_EmptyResponsesReproduceBaseline)
	{
		using namespace ArcForestHabitatTestsLocal;

		constexpr float SideCm = 4000.0f;

		TStrongObjectPtr<UArcForestTestSurfaceData> Surface = MakeSlopeRampSurface(SideCm);
		FHabitatFixture Fixture;
		BakeFixtureFromSurface(*Surface, Surface->GetBounds(), MakeTerrainParams(0.5f * SideCm), Fixture);

		// A LIVE moisture field too: the bundle must be fully populated, so the identity claim covers
		// "every axis supplied, no species response" rather than "nothing supplied".
		FArcForestNoiseParams Noise;
		Noise.Frequency = 0.001f;
		Fixture.Habitat.MoistureNoise = Noise;

		TArray<FArcForestSpeciesEntry> Table;
		Table.Add(MakeSpecies(TEXT("PlainMoss"), EArcForestLayer::Field,
			100.0f, 60.0f, 0.2f, 0.5f, 300.0f, 0.05f));
		Table.Add(MakeSpecies(TEXT("PlainHazel"), EArcForestLayer::Shrub,
			300.0f, 90.0f, 0.15f, 0.3f, 300.0f, 0.03f));

		TStrongObjectPtr<UArcForestSpeciesDefinition> EmptyDef = MakeDefinition(Table);

		// Same scalar rows, but species 0 carries a response row whose curve was never authored.
		TArray<FArcForestSpeciesEntry> KeylessTable = Table;
		FArcForestFieldResponse KeylessResponse;
		KeylessResponse.Field = EArcForestHabitatField::Slope;
		KeylessResponse.Weight = 1.0f; // curve deliberately left with ZERO keys
		KeylessTable[0].FieldResponses.Add(MoveTemp(KeylessResponse));
		TStrongObjectPtr<UArcForestSpeciesDefinition> KeylessDef = MakeDefinition(MoveTemp(KeylessTable));

		FArcForestSimParams Params;
		Params.Seed = 8080;
		Params.SeedingDensity = 1.0f;
		Params.SpacingFraction = 0.3f;
		Params.WPatch = 4.0f;
		Params.Iterations = 0;
		Params.Bounds2D = Fixture.Bounds2D;

		const FArcForestSimulator Simulator;

		// The PRE-STEP-7 CALL SHAPE: no Habitat argument at all.
		const FArcForestResult Baseline =
			Simulator.Simulate(Params, *EmptyDef, Fixture.Suitability, Fixture.GridDims);

		const FArcForestHabitatInputs EmptyResponses = FlattenInto(Fixture.Habitat, *EmptyDef);
		const FArcForestResult WithEmptyResponses =
			Simulator.Simulate(Params, *EmptyDef, Fixture.Suitability, Fixture.GridDims, EmptyResponses);

		const FArcForestHabitatInputs KeylessResponses = FlattenInto(Fixture.Habitat, *KeylessDef);
		const FArcForestResult WithKeylessCurve =
			Simulator.Simulate(Params, *KeylessDef, Fixture.Suitability, Fixture.GridDims, KeylessResponses);

		UE_LOGF(LogArcForestHabitatTest, Display,"[EmptyResponses] baseline=%d emptyResponses=%d keylessCurve=%d | flattened slots=%d/%d",
			Baseline.Num(), WithEmptyResponses.Num(), WithKeylessCurve.Num(),
			EmptyResponses.SpeciesResponses.Num(), KeylessResponses.SpeciesResponses.Num());

		ASSERT_THAT(IsTrue(Baseline.Num() > 100,
			TEXT("Baseline must seed a substantial stand, else element-wise identity is trivial")));

		// The flattener must have sized a slot per species (so the IsValidIndex fast path is NOT what is
		// being tested here) and must have DROPPED the keyless response.
		ASSERT_THAT(IsTrue(EmptyResponses.SpeciesResponses.Num() == 2
			&& KeylessResponses.SpeciesResponses.Num() == 2,
			TEXT("FlattenFieldResponses must size SpeciesResponses to the species count, so this test "
				"exercises the empty-response-set path and not the out-of-range-index shortcut")));
		ASSERT_THAT(IsTrue(KeylessResponses.SpeciesResponses[0].Responses.Num() == 0,
			TEXT("A response whose curve carries NO KEYS must be dropped by the flattener, never emitted "
				"as a zero-filled LUT (which would multiply the species to zero everywhere)")));

		ASSERT_THAT(IsTrue(ResultsIdentical(Baseline, WithEmptyResponses),
			TEXT("A fully populated habitat bundle with NO authored FieldResponses must reproduce the "
				"pre-Step-7 result ELEMENT-WISE across every parallel array — not merely the same count. "
				"RED if EvaluateHabitatResponse's exact-1.0 fast path is replaced by anything that "
				"computes, since the product then stops being an IEEE-754 identity")));
		ASSERT_THAT(IsTrue(ResultsIdentical(Baseline, WithKeylessCurve),
			TEXT("A response row with an UNAUTHORED (keyless) curve must also be element-wise identical "
				"to the baseline: it is a half-finished edit, not a total exclusion")));
	}

	// DRAW COUNT UNCHANGED — the FIXED 7-DRAW CONTRACT under a habitat term, proven by a
	// DOWNSTREAM-STRATUM stream offset (the technique from
	// ArcForestSeedingContractTests::DrawCountInvariance_DownstreamStratumStreamAligned).
	//
	// SeedPopulation drives every dart off ONE serial FRandomStream, traversing strata Field -> Shrub ->
	// Understory -> Canopy. A species therefore starts reading at exactly 7*sum(CandidateCount) of
	// everything traversed before it — IF and ONLY IF the seven per-dart draws stay unconditional and
	// nothing else consumes the stream.
	//
	// FIXTURE: species A (Field stratum, traversed FIRST, patchy) carries a STRONG slope response.
	// Species B (Shrub stratum, traversed SECOND, PatchDensity = 0 so parentless) carries NONE. The two
	// arms are "no habitat at all" versus "the full bundle with A responsive", and we assert:
	//   (1) CONTROL — A's slice DIFFERS across the arms, proving the habitat term genuinely bit. Without
	//       this the identity below would be a tautology over a dead input.
	//   (2) THE CONTRACT — B's slice is ELEMENT-WISE IDENTICAL. Since A demonstrably accepted a
	//       different dart set, this can only hold if A consumed the same number of draws in both arms.
	// Different strata matter: min spacing is enforced only WITHIN a stratum, so B is coupled to A's RNG
	// CONSUMPTION and to nothing else.
	//
	// MUTANT FAMILY KILLED: acquiring or advancing a stream inside EvaluateHabitatResponse; gating any
	// of the seven per-dart draws on the habitat value (e.g. `if (Habitat...) { DrawAccept = ... }`);
	// early-returning out of the dart loop on a zero response before the remaining draws are taken;
	// drawing a per-candidate value inside FArcForestPatchField::Build's habitat branch (which would
	// break B through A's parent stream only if shared — see NOT COVERED).
	//
	// NOT COVERED HERE: FArcForestPatchField::Build uses a SEPARATE per-species stream, so an extra draw
	// added to Build's habitat branch for species A cannot shift species B's slice. That family is
	// covered instead by the EXACT golden parent count asserted in
	// Habitat_ClumpParentsFollowSlopeGradient — not by this test, and not by any inequality.
	//
	// ALSO NOT COVERED: this test says nothing about whether the dart-acceptance habitat factor has any
	// EFFECT — only that it is RNG-neutral. That effect is carried by
	// Habitat_DartAcceptanceBiasesParentlessSpecies.
	//
	// MEASURED on this branch (seed 9001, 40x40 m): species A 569 plants without habitat vs 388 with, so
	// the term really bit; species B 348 plants in BOTH arms, ELEMENT-WISE identical (position and
	// inherited patch id). The control assertion below is `IsFalse(SlicesIdentical(...))`, and
	// SlicesIdentical returns false on a COUNT mismatch before it compares any element — so what the
	// control actually establishes is the weaker "A's accepted dart set changed", which is all the
	// identity argument needs. It does NOT establish that A accepted a differently-POSITIONED set.
	TEST_METHOD(Habitat_DrawCountUnchanged)
	{
		using namespace ArcForestHabitatTestsLocal;

		constexpr float SideCm = 4000.0f;
		constexpr int32 SpeciesA = 0; // Field stratum, responsive — the stream producer
		constexpr int32 SpeciesB = 1; // Shrub stratum, parentless, unresponsive — the stream probe

		TStrongObjectPtr<UArcForestTestSurfaceData> Surface = MakeSlopeRampSurface(SideCm);
		FHabitatFixture Fixture;
		BakeFixtureFromSurface(*Surface, Surface->GetBounds(), MakeTerrainParams(0.5f * SideCm), Fixture);

		TArray<FArcForestSpeciesEntry> Table;
		Table.Add(MakeSpecies(TEXT("ResponsiveMoss"), EArcForestLayer::Field,
			/*MaxHeight=*/100.0f, 60.0f, 0.2f, /*TargetDensity=*/0.5f, 200.0f, /*PatchDensity=*/0.03f));
		Table.Add(MakeSpecies(TEXT("LonelyHazel"), EArcForestLayer::Shrub,
			/*MaxHeight=*/300.0f, 90.0f, 0.15f, /*TargetDensity=*/0.3f, 200.0f, /*PatchDensity=*/0.0f));

		TStrongObjectPtr<UArcForestSpeciesDefinition> PlainDef = MakeDefinition(Table);

		TArray<FArcForestSpeciesEntry> ResponsiveTable = Table;
		const FVector2f RisingKeys[2] = { FVector2f(0.0f, 0.0f), FVector2f(1.0f, 1.0f) };
		ResponsiveTable[SpeciesA].FieldResponses.Add(
			MakeResponse(EArcForestHabitatField::Slope, 1.0f, RisingKeys));
		TStrongObjectPtr<UArcForestSpeciesDefinition> ResponsiveDef =
			MakeDefinition(MoveTemp(ResponsiveTable));

		FArcForestSimParams Params;
		Params.Seed = 9001;
		Params.SeedingDensity = 1.0f;
		Params.SpacingFraction = 0.5f;
		Params.WPatch = 8.0f;
		Params.Iterations = 0;
		Params.Bounds2D = Fixture.Bounds2D;

		// FIXTURE PRECONDITION — B really is parentless, so it stays on the uniform branch in both arms
		// and its output can only move if the RNG stream shifted underneath it.
		const FArcForestHabitatInputs ResponsiveHabitat = FlattenInto(Fixture.Habitat, *ResponsiveDef);
		FArcForestPatchField ProbeField;
		ProbeField.Build(*ResponsiveDef, Params, Fixture.Suitability, Fixture.GridDims, ResponsiveHabitat);
		ASSERT_THAT(IsTrue(ProbeField.GetParentPositions(SpeciesA).Num() > 0,
			TEXT("Species A must have clump parents so its clumped branch is live")));
		ASSERT_THAT(IsTrue(ProbeField.GetParentPositions(SpeciesB).Num() == 0,
			TEXT("Species B must be parentless (PatchDensity=0) — it must share no placement coupling "
				"with A, only the RNG stream")));

		const FArcForestSimulator Simulator;

		// Arm 1 — the pre-habitat call shape (no Habitat argument).
		const FArcForestResult NoHabitat =
			Simulator.Simulate(Params, *PlainDef, Fixture.Suitability, Fixture.GridDims);
		// Arm 2 — the full bundle, species A responsive.
		const FArcForestResult WithHabitat =
			Simulator.Simulate(Params, *ResponsiveDef, Fixture.Suitability, Fixture.GridDims,
				ResponsiveHabitat);

		TArray<FVector2f> PlainPosA;
		TArray<int32> PlainPatchIdA;
		TArray<FVector2f> ResponsivePosA;
		TArray<int32> ResponsivePatchIdA;
		ExtractSpeciesSlice(NoHabitat, SpeciesA, PlainPosA, PlainPatchIdA);
		ExtractSpeciesSlice(WithHabitat, SpeciesA, ResponsivePosA, ResponsivePatchIdA);

		TArray<FVector2f> PlainPosB;
		TArray<int32> PlainPatchIdB;
		TArray<FVector2f> ResponsivePosB;
		TArray<int32> ResponsivePatchIdB;
		ExtractSpeciesSlice(NoHabitat, SpeciesB, PlainPosB, PlainPatchIdB);
		ExtractSpeciesSlice(WithHabitat, SpeciesB, ResponsivePosB, ResponsivePatchIdB);

		UE_LOGF(LogArcForestHabitatTest, Display,"[DrawCount] A: plain=%d responsive=%d | B: plain=%d responsive=%d",
			PlainPosA.Num(), ResponsivePosA.Num(), PlainPosB.Num(), ResponsivePosB.Num());

		ASSERT_THAT(IsTrue(PlainPosB.Num() > 20 && ResponsivePosB.Num() > 20,
			TEXT("The downstream probe species must seed substantially in both arms, else the identity "
				"assertion is vacuous")));

		// (1) CONTROL — the habitat term must genuinely have bitten on the upstream species.
		ASSERT_THAT(IsFalse(SlicesIdentical(PlainPosA, PlainPatchIdA, ResponsivePosA, ResponsivePatchIdA),
			TEXT("Species A must DIFFER between the no-habitat and habitat arms: this is what proves the "
				"habitat term is live. Without it the identity assertion below would be a tautology")));

		// (2) THE CONTRACT — the downstream stratum must be untouched.
		ASSERT_THAT(IsTrue(SlicesIdentical(PlainPosB, PlainPatchIdB, ResponsivePosB, ResponsivePatchIdB),
			TEXT("Species B (a DOWNSTREAM stratum) must be ELEMENT-WISE IDENTICAL across the two arms. "
				"Species A demonstrably accepted a different dart set, so this can only hold if the "
				"habitat term changed the VALUE of AcceptProb without changing the number or order of "
				"FRandomStream draws. RED if EvaluateHabitatResponse ever draws, or if any of the seven "
				"per-dart draws becomes conditional on the habitat value")));
	}

	// WEIGHT ZERO IS NEUTRAL, AND THE COMBINE IS Pow(Clamp(curve), Weight) — the formula test.
	//
	// MUTANTS KILLED:
	//   (1) Weight clamping wrong at flatten time — e.g. `Lut.Weight = FMath::Max(Response.Weight, 1.0f)`
	//       or a clamp to a non-zero floor: a Weight-0 response then bites and the identity arm goes RED.
	//   (2) the Pow combine replaced by a LINEAR blend — `Combined *= CurveValue * Weight` (Weight 0
	//       zeroes the species out entirely -> RED), or `Combined *= CurveValue` regardless of Weight
	//       (Weight 0 then bites -> RED, and the Weight-2 arm stops differing from Weight-1 -> RED).
	//   (3) the exponent dropped/ignored for non-integral-fast-path weights: the analytic block asserts
	//       Pow(0.6, 2) == 0.36 to 1e-4, so an ignored exponent reads 0.6 and goes RED.
	//
	// The analytic block feeds a deliberately CONSTANT slope grid (0.6 in every cell, so any sampler
	// reads exactly 0.6) through an IDENTITY curve authored as real FRuntimeFloatCurve keys and
	// flattened by the production flattener. Expected: Weight 0 -> 1.0, Weight 1 -> 0.6, Weight 2 -> 0.36.
	//
	// MEASURED on this branch, BOTH SIDES (seed 4711, 40x40 m):
	//   analytic: weight0 = 1.000000, weight1 = 0.600000, weight2 = 0.360000 — i.e. exactly 1, 0.6 and
	//             0.6^2 at a constant input of 0.6, against tolerances of 1e-5 / 1e-4 / 1e-4.
	//   end-to-end: baseline 724 plants; Weight 0 arm 724 plants ELEMENT-WISE identical;
	//               Weight 1 arm 474 plants (differs); Weight 2 arm 424 plants (differs from Weight 1 and
	//               is strictly sparser, as squaring a value in [0,1] requires).
	TEST_METHOD(Habitat_WeightZeroIsNeutral)
	{
		using namespace ArcForestHabitatTestsLocal;

		constexpr float SideCm = 4000.0f;
		constexpr float ConstantSlope = 0.6f;

		TStrongObjectPtr<UArcForestTestSurfaceData> Surface = MakeSlopeRampSurface(SideCm);
		FHabitatFixture Fixture;
		BakeFixtureFromSurface(*Surface, Surface->GetBounds(), MakeTerrainParams(0.5f * SideCm), Fixture);

		// ---- ANALYTIC BLOCK: pin the combine formula itself -------------------------------------
		// Three species differing ONLY in Weight, one identity curve over Slope each.
		const FVector2f IdentityKeys[2] = { FVector2f(0.0f, 0.0f), FVector2f(1.0f, 1.0f) };
		TArray<FArcForestSpeciesEntry> WeightTable;
		WeightTable.Add(MakeSpecies(TEXT("W0"), EArcForestLayer::Field, 100.0f, 60.0f, 0.2f, 0.1f, 300.0f, 0.05f));
		WeightTable.Add(MakeSpecies(TEXT("W1"), EArcForestLayer::Field, 100.0f, 60.0f, 0.2f, 0.1f, 300.0f, 0.05f));
		WeightTable.Add(MakeSpecies(TEXT("W2"), EArcForestLayer::Field, 100.0f, 60.0f, 0.2f, 0.1f, 300.0f, 0.05f));
		WeightTable[0].FieldResponses.Add(MakeResponse(EArcForestHabitatField::Slope, 0.0f, IdentityKeys));
		WeightTable[1].FieldResponses.Add(MakeResponse(EArcForestHabitatField::Slope, 1.0f, IdentityKeys));
		WeightTable[2].FieldResponses.Add(MakeResponse(EArcForestHabitatField::Slope, 2.0f, IdentityKeys));
		TStrongObjectPtr<UArcForestSpeciesDefinition> WeightDef = MakeDefinition(MoveTemp(WeightTable));

		FArcForestHabitatInputs ConstantHabitat;
		ConstantHabitat.MoistureNoise.Frequency = 0.0f; // silence the LIVE default (see the bake note)
		ConstantHabitat.SlopeField.Bounds = Fixture.Bounds2D;
		ConstantHabitat.SlopeField.Dims = FIntPoint(2, 2);
		ConstantHabitat.SlopeField.Values.Init(ConstantSlope, 4);
		ArcForestSpeciesHabitat::FlattenFieldResponses(*WeightDef, ConstantHabitat);

		const FVector2f Probe = Fixture.Bounds2D.GetCenter();
		const float Weight0Response =
			ArcForestField::EvaluateHabitatResponse(ConstantHabitat, 0, Probe, /*SolveSeed=*/1);
		const float Weight1Response =
			ArcForestField::EvaluateHabitatResponse(ConstantHabitat, 1, Probe, /*SolveSeed=*/1);
		const float Weight2Response =
			ArcForestField::EvaluateHabitatResponse(ConstantHabitat, 2, Probe, /*SolveSeed=*/1);

		UE_LOGF(LogArcForestHabitatTest, Display,"[WeightZero] analytic weight0=%.6f weight1=%.6f weight2=%.6f (constant slope=%.2f)",
			Weight0Response, Weight1Response, Weight2Response, ConstantSlope);

		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Weight0Response, 1.0f, 1.0e-5f),
			TEXT("Weight 0 must make a response an exact no-op: Pow(x,0) == 1. RED if the flattener "
				"clamps Weight to a non-zero floor, or if the combine is a linear blend by Weight")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Weight1Response, ConstantSlope, 1.0e-4f),
			TEXT("Weight 1 must pass the curve value through unchanged (Pow(x,1) == x)")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Weight2Response, ConstantSlope * ConstantSlope, 1.0e-4f),
			TEXT("Weight 2 must SQUARE the curve value — this is what makes the combine "
				"Pow(Clamp(curve), Weight) rather than a multiply by Weight or an ignored exponent")));

		// ---- END-TO-END BLOCK: the same three weights through the real solver -------------------
		TArray<FArcForestSpeciesEntry> BaselineTable;
		BaselineTable.Add(MakeSpecies(TEXT("WeightedFern"), EArcForestLayer::Field,
			100.0f, 60.0f, 0.2f, /*TargetDensity=*/0.5f, 300.0f, 0.05f));
		TStrongObjectPtr<UArcForestSpeciesDefinition> BaselineDef = MakeDefinition(BaselineTable);

		TArray<FArcForestSpeciesEntry> ZeroWeightTable = BaselineTable;
		ZeroWeightTable[0].FieldResponses.Add(
			MakeResponse(EArcForestHabitatField::Slope, 0.0f, IdentityKeys));
		TStrongObjectPtr<UArcForestSpeciesDefinition> ZeroWeightDef =
			MakeDefinition(MoveTemp(ZeroWeightTable));

		TArray<FArcForestSpeciesEntry> UnitWeightTable = BaselineTable;
		UnitWeightTable[0].FieldResponses.Add(
			MakeResponse(EArcForestHabitatField::Slope, 1.0f, IdentityKeys));
		TStrongObjectPtr<UArcForestSpeciesDefinition> UnitWeightDef =
			MakeDefinition(MoveTemp(UnitWeightTable));

		TArray<FArcForestSpeciesEntry> DoubleWeightTable = BaselineTable;
		DoubleWeightTable[0].FieldResponses.Add(
			MakeResponse(EArcForestHabitatField::Slope, 2.0f, IdentityKeys));
		TStrongObjectPtr<UArcForestSpeciesDefinition> DoubleWeightDef =
			MakeDefinition(MoveTemp(DoubleWeightTable));

		FArcForestSimParams Params;
		Params.Seed = 4711;
		Params.SeedingDensity = 1.0f;
		Params.SpacingFraction = 0.3f;
		Params.WPatch = 4.0f;
		Params.Iterations = 0;
		Params.Bounds2D = Fixture.Bounds2D;

		// Bundles bound to NAMED locals, never passed as temporaries: the solver holds the habitat by
		// const reference for the whole call (see the HABITAT INPUTS contract on FArcForestSimulator).
		const FArcForestHabitatInputs ZeroWeightHabitat = FlattenInto(Fixture.Habitat, *ZeroWeightDef);
		const FArcForestHabitatInputs UnitWeightHabitat = FlattenInto(Fixture.Habitat, *UnitWeightDef);
		const FArcForestHabitatInputs DoubleWeightHabitat = FlattenInto(Fixture.Habitat, *DoubleWeightDef);

		const FArcForestSimulator Simulator;
		const FArcForestResult Baseline =
			Simulator.Simulate(Params, *BaselineDef, Fixture.Suitability, Fixture.GridDims);
		const FArcForestResult ZeroWeight = Simulator.Simulate(Params, *ZeroWeightDef,
			Fixture.Suitability, Fixture.GridDims, ZeroWeightHabitat);
		const FArcForestResult UnitWeight = Simulator.Simulate(Params, *UnitWeightDef,
			Fixture.Suitability, Fixture.GridDims, UnitWeightHabitat);
		const FArcForestResult DoubleWeight = Simulator.Simulate(Params, *DoubleWeightDef,
			Fixture.Suitability, Fixture.GridDims, DoubleWeightHabitat);

		UE_LOGF(LogArcForestHabitatTest, Display,"[WeightZero] end-to-end baseline=%d weight0=%d weight1=%d weight2=%d",
			Baseline.Num(), ZeroWeight.Num(), UnitWeight.Num(), DoubleWeight.Num());

		ASSERT_THAT(IsTrue(Baseline.Num() > 100,
			TEXT("Baseline must seed a substantial stand, else element-wise identity is trivial")));

		ASSERT_THAT(IsTrue(ResultsIdentical(Baseline, ZeroWeight),
			TEXT("A Weight = 0 response must be ELEMENT-WISE identical to having no response at all. RED "
				"if the flattener's Weight clamp lifts 0, or if the combine multiplies by Weight instead "
				"of raising to it")));

		// VACUITY GUARDS — the curve must genuinely bite at Weight 1, and the exponent must genuinely
		// sharpen at Weight 2. Without these, the identity above could hold because the whole response
		// pipeline was dead.
		ASSERT_THAT(IsFalse(ResultsIdentical(Baseline, UnitWeight),
			TEXT("The SAME curve at Weight 1 must change the result — this proves the Weight-0 identity "
				"above is neutrality, not a dead response pipeline")));
		ASSERT_THAT(IsFalse(ResultsIdentical(UnitWeight, DoubleWeight),
			TEXT("Weight 2 must differ from Weight 1: the exponent is applied, not ignored")));
		ASSERT_THAT(IsTrue(DoubleWeight.Num() < UnitWeight.Num(),
			TEXT("Squaring a curve value in [0,1] can only lower acceptance, so the Weight-2 stand must "
				"be sparser than the Weight-1 stand — pins the DIRECTION of the exponent as well")));
	}
};
