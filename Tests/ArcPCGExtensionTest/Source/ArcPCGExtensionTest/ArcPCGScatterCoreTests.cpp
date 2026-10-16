// Copyright Lukasz Baran. All Rights Reserved.

// ArcPCGScatterCoreTests.cpp
// Pure-math CQTest coverage for Scatter/ArcPCGScatterCore.{h,cpp} — no PCG element, no editor context,
// no RHI. Every case drives the free functions directly with a hand-seeded FRandomStream.
//
// MUTANTS THESE TESTS KILL (Step 3 of docs/plans/2026-07-27-0549-arcpcgextension-graph-nodes-plan.md):
//   - SampleAnnulus regressing from Sqrt(Lerp(Min^2,Max^2,U)) to a naive Lerp(Min,Max,U): the naive
//     form still returns values inside [Min,Max] (so a plain range check would stay green) but is NOT
//     area-uniform — it over-weights the inner ring, so its median lands well below
//     Sqrt((Min^2+Max^2)/2). The AreaUniform case pins the median directly against that closed form.
//   - SampleAnnulus consuming a different number of draws than exactly two (e.g. an early-out or an
//     extra guard draw): the ConsumesExactlyTwoDraws case re-derives the same two draws on a clone
//     stream and then checks several MORE draws stay aligned, which only holds if the count is exactly
//     two, unconditionally.
//   - EvaluateFalloff losing its hard d > R cutoff, or losing the d == 0 => 1 guarantee, in any of the
//     three modes.
//   - EvaluateFalloff's Gaussian mode gaining a redundant cutoff-at-R special case that would collapse
//     the deliberate ~0.011 step at d == R down to 0.
//   - MakeStreamForPosition keying on something other than position (e.g. an index, or dropping the
//     salt/seed mix), which would make two distinct positions collide or the same position diverge.
//     Pinned three ways: same (Seed, Salt, Position) collides; different Salt at a fixed (Seed,
//     Position) diverges; different Seed at a fixed (Salt, Position) diverges; different Position at a
//     fixed (Seed, Salt) diverges. A `MakeStreamForPosition` that dropped Salt and/or Seed from the mix
//     (e.g. `FRandomStream(ComputeSeedFromPosition(P))`) would still pass the position-only cases but
//     fail the Salt/Seed cases.
//   - SampleAnnulus's two-draw contract holding only for one radius pair: the clone-stream alignment
//     check is repeated at Min == Max and at Min == Max == 0, so a mutant that special-cases a
//     degenerate range (e.g. skipping the V draw when the range is a point) cannot hide behind a
//     single "normal" radius pair.

#include "CQTest.h"

#include "Scatter/ArcPCGScatterCore.h"

#include "Math/RandomStream.h"
#include "Math/UnrealMathUtility.h"

namespace ArcPCGScatterCoreTestsLocal
{
	/** Magnitude of an XY offset, i.e. the sampled annulus radius. */
	double OffsetMagnitude(const FVector2D& Offset)
	{
		return FMath::Sqrt(Offset.X * Offset.X + Offset.Y * Offset.Y);
	}

	/**
	 * Re-derives the same two draws SampleAnnulus should consume on a clone stream, then checks several
	 * MORE draws stay aligned between the two streams — that only holds if SampleAnnulus consumed
	 * exactly two draws, unconditionally, for the given (MinRadiusCm, MaxRadiusCm) pair. Shared so the
	 * degenerate ranges (Min == Max, Min == Max == 0) can be pinned with the same rigour as the normal
	 * pair, rather than leaving a mutant that special-cases a zero-width range undetected.
	 */
	void AssertConsumesExactlyTwoDraws(FAutomationTestBase& TestRunner, double MinRadiusCm, double MaxRadiusCm, int32 StreamSeed)
	{
		FRandomStream RngUnderTest(StreamSeed);
		FRandomStream RngReference(StreamSeed);

		const FVector2D Offset = ArcPCGScatterCore::SampleAnnulus(RngUnderTest, MinRadiusCm, MaxRadiusCm);

		const double U = static_cast<double>(RngReference.FRand());
		const double V = static_cast<double>(RngReference.FRand());
		const double ExpectedRadius =
			FMath::Sqrt(FMath::Lerp(MinRadiusCm * MinRadiusCm, MaxRadiusCm * MaxRadiusCm, U));
		const double ExpectedTheta = 2.0 * UE_DOUBLE_PI * V;
		const FVector2D ExpectedOffset(ExpectedRadius * FMath::Cos(ExpectedTheta), ExpectedRadius * FMath::Sin(ExpectedTheta));

		TestRunner.TestTrue(
			TEXT("SampleAnnulus must reproduce Sqrt(Lerp(Min^2,Max^2,U)) at theta=2*PI*V from exactly two draws"),
			Offset.Equals(ExpectedOffset, 1e-6));

		// If SampleAnnulus consumed a different number of draws, these subsequent draws desynchronise.
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const float NextUnderTest = RngUnderTest.FRand();
			const float NextReference = RngReference.FRand();
			TestRunner.TestTrue(
				TEXT("Streams must stay draw-aligned after SampleAnnulus; a draw-count regression desyncs them"),
				FMath::IsNearlyEqual(NextUnderTest, NextReference, 1e-6f));
		}
	}
}

// NOTE: named ArcPCGScatterCoreTests, not ArcPCGScatterCore — the latter collides with the
// production `namespace ArcPCGScatterCore` this file includes (TEST_CLASS expands to a class
// declaration in the same scope). The automation FILTER string (second argument) is unaffected.
TEST_CLASS(ArcPCGScatterCoreTests, "ArcPCGExtension.Scatter.Core")
{
	// --- SampleAnnulus -------------------------------------------------------------------------

	TEST_METHOD(SampleAnnulus_AllRadiiWithinMinMax)
	{
		constexpr double MinRadiusCm = 100.0;
		constexpr double MaxRadiusCm = 500.0;
		FRandomStream Rng(1001);

		for (int32 Index = 0; Index < 2000; ++Index)
		{
			const FVector2D Offset = ArcPCGScatterCore::SampleAnnulus(Rng, MinRadiusCm, MaxRadiusCm);
			const double Radius = ArcPCGScatterCoreTestsLocal::OffsetMagnitude(Offset);
			ASSERT_THAT(IsTrue(Radius >= MinRadiusCm - KINDA_SMALL_NUMBER,
				TEXT("Sampled radius must never fall below MinRadiusCm")));
			ASSERT_THAT(IsTrue(Radius <= MaxRadiusCm + KINDA_SMALL_NUMBER,
				TEXT("Sampled radius must never exceed MaxRadiusCm")));
		}
	}

	// THE mutant this kills: Lerp(Min, Max, U) also passes the range check above but bunches samples
	// toward the inner ring, so its median sits well below Sqrt((Min^2+Max^2)/2).
	TEST_METHOD(SampleAnnulus_IsAreaUniform_MedianMatchesSquareRootLawNotLinearLerp)
	{
		constexpr double MinRadiusCm = 100.0;
		constexpr double MaxRadiusCm = 500.0;
		constexpr int32 SampleCount = 20000;
		FRandomStream Rng(2002);

		TArray<double> Radii;
		Radii.Reserve(SampleCount);
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			const FVector2D Offset = ArcPCGScatterCore::SampleAnnulus(Rng, MinRadiusCm, MaxRadiusCm);
			Radii.Add(ArcPCGScatterCoreTestsLocal::OffsetMagnitude(Offset));
		}
		Radii.Sort();

		const double MedianRadius = Radii[SampleCount / 2];
		const double ExpectedMedian =
			FMath::Sqrt((MinRadiusCm * MinRadiusCm + MaxRadiusCm * MaxRadiusCm) / 2.0);
		// A naive Lerp(Min, Max, U) median would be (Min+Max)/2 = 300, versus the area-uniform
		// ~360.55 expected here — a 2% tolerance keeps this well clear of that mutant's value.
		const double Tolerance = ExpectedMedian * 0.02;

		ASSERT_THAT(IsTrue(FMath::Abs(MedianRadius - ExpectedMedian) <= Tolerance,
			TEXT("Median radius must match Sqrt((Min^2+Max^2)/2); a naive linear Lerp(Min,Max,U) would ")
			TEXT("under-shoot this by bunching samples toward the inner ring")));
	}

	// THE mutant this kills: an early-out or an extra guard draw changing the draw count away from
	// exactly two. Re-derive the same two draws on a clone stream, then check several MORE draws stay
	// aligned between the two streams — that only holds if SampleAnnulus consumed exactly two.
	//
	// Repeated at THREE radius pairs, not just one: a mutant that special-cases a degenerate range
	// (e.g. `if (MaxR > MinR) { draw U; } else { U = 0; }`) would still consume exactly two draws at
	// (50, 250) but could skip a real draw at Min == Max or Min == Max == 0 — both are exercised here.
	TEST_METHOD(SampleAnnulus_ConsumesExactlyTwoDrawsUnconditionally)
	{
		ArcPCGScatterCoreTestsLocal::AssertConsumesExactlyTwoDraws(*TestRunner, 50.0, 250.0, 3003);
		ArcPCGScatterCoreTestsLocal::AssertConsumesExactlyTwoDraws(*TestRunner, 100.0, 100.0, 3004);
		ArcPCGScatterCoreTestsLocal::AssertConsumesExactlyTwoDraws(*TestRunner, 0.0, 0.0, 3005);
	}

	// --- EvaluateFalloff -------------------------------------------------------------------------

	TEST_METHOD(EvaluateFalloff_Constant_ExactCutoffAndCentre)
	{
		constexpr double Radius = 200.0;
		ASSERT_THAT(IsTrue(ArcPCGScatterCore::EvaluateFalloff(EArcPCGFalloff::Constant, Radius + 1.0, Radius) == 0.0f,
			TEXT("Constant falloff must return EXACTLY 0 past the radius")));
		ASSERT_THAT(IsTrue(ArcPCGScatterCore::EvaluateFalloff(EArcPCGFalloff::Constant, 0.0, Radius) == 1.0f,
			TEXT("Constant falloff must return EXACTLY 1 at d == 0")));
		ASSERT_THAT(IsTrue(ArcPCGScatterCore::EvaluateFalloff(EArcPCGFalloff::Constant, Radius, Radius) == 1.0f,
			TEXT("Constant falloff is 1 up to and including d == R")));
	}

	TEST_METHOD(EvaluateFalloff_Linear_ExactCutoffCentreAndZeroAtRadius)
	{
		constexpr double Radius = 200.0;
		ASSERT_THAT(IsTrue(ArcPCGScatterCore::EvaluateFalloff(EArcPCGFalloff::Linear, Radius + 1.0, Radius) == 0.0f,
			TEXT("Linear falloff must return EXACTLY 0 past the radius")));
		ASSERT_THAT(IsTrue(ArcPCGScatterCore::EvaluateFalloff(EArcPCGFalloff::Linear, 0.0, Radius) == 1.0f,
			TEXT("Linear falloff must return EXACTLY 1 at d == 0")));
		ASSERT_THAT(IsTrue(ArcPCGScatterCore::EvaluateFalloff(EArcPCGFalloff::Linear, Radius, Radius) == 0.0f,
			TEXT("Linear falloff must be EXACTLY 0 at d == R (1 - R/R)")));
		ASSERT_THAT(IsTrue(ArcPCGScatterCore::EvaluateFalloff(EArcPCGFalloff::Linear, Radius * 0.5, Radius) == 0.5f,
			TEXT("Linear falloff must be 0.5 at half the radius")));
	}

	// Pins the deliberate ~0.011 step at d == R: a hard "return 0 at d==R" branch would silently
	// collapse this to 0.0f, which is the previously-discussed mutant this case exists to catch.
	TEST_METHOD(EvaluateFalloff_Gaussian_ExactCutoffCentreAndNonZeroAtRadius)
	{
		constexpr double Radius = 300.0;
		ASSERT_THAT(IsTrue(ArcPCGScatterCore::EvaluateFalloff(EArcPCGFalloff::Gaussian, Radius + 1.0, Radius) == 0.0f,
			TEXT("Gaussian falloff must return EXACTLY 0 past the radius")));
		ASSERT_THAT(IsTrue(ArcPCGScatterCore::EvaluateFalloff(EArcPCGFalloff::Gaussian, 0.0, Radius) == 1.0f,
			TEXT("Gaussian falloff must return EXACTLY 1 at d == 0")));

		const float AtRadius = ArcPCGScatterCore::EvaluateFalloff(EArcPCGFalloff::Gaussian, Radius, Radius);
		constexpr float Expected = 0.011109f; // Exp(-4.5), sigma = R/3, deliberate non-zero edge value.
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(AtRadius, Expected, 1e-5f),
			TEXT("Gaussian falloff at d == R must be Exp(-4.5) ~= 0.011109, NOT 0 — this is the deliberate ")
			TEXT("edge value the hard cutoff introduces, not a rounding artefact")));
		ASSERT_THAT(IsTrue(AtRadius > 0.0f,
			TEXT("Gaussian falloff at d == R must be strictly positive")));
	}

	// --- MakeStreamForPosition -------------------------------------------------------------------

	TEST_METHOD(MakeStreamForPosition_SamePositionYieldsIdenticalStream)
	{
		const FVector Position(1234.0, -567.0, 89.0);
		const FRandomStream StreamA = ArcPCGScatterCore::MakeStreamForPosition(42, 0xABCDu, Position);
		const FRandomStream StreamB = ArcPCGScatterCore::MakeStreamForPosition(42, 0xABCDu, Position);

		ASSERT_THAT(AreEqual(StreamA.GetInitialSeed(), StreamB.GetInitialSeed(),
			TEXT("Identical (Seed, Salt, Position) inputs must produce identical streams")));
	}

	// Positions are >1 cm apart specifically to avoid ComputeSeedFromPosition's integer-centimetre
	// truncation, which is documented (not a bug) to collide positions inside the same 1 cm cell.
	TEST_METHOD(MakeStreamForPosition_DifferentPositionsYieldDifferentStreams)
	{
		const FVector PositionA(0.0, 0.0, 0.0);
		const FVector PositionB(500.0, 0.0, 0.0);
		const FVector PositionC(0.0, 500.0, 0.0);

		const FRandomStream StreamA = ArcPCGScatterCore::MakeStreamForPosition(42, 0xABCDu, PositionA);
		const FRandomStream StreamB = ArcPCGScatterCore::MakeStreamForPosition(42, 0xABCDu, PositionB);
		const FRandomStream StreamC = ArcPCGScatterCore::MakeStreamForPosition(42, 0xABCDu, PositionC);

		ASSERT_THAT(IsTrue(StreamA.GetInitialSeed() != StreamB.GetInitialSeed(),
			TEXT("Distinct positions must not collide onto the same stream")));
		ASSERT_THAT(IsTrue(StreamA.GetInitialSeed() != StreamC.GetInitialSeed(),
			TEXT("Distinct positions must not collide onto the same stream")));
		ASSERT_THAT(IsTrue(StreamB.GetInitialSeed() != StreamC.GetInitialSeed(),
			TEXT("Distinct positions must not collide onto the same stream")));
	}

	// THE mutant this kills: a MakeStreamForPosition that drops Salt and/or NodeSeed from the mix
	// (e.g. `FRandomStream(ComputeSeedFromPosition(P))`) still passes both tests above (same position
	// collides, different positions diverge) because neither varies Salt or Seed at a fixed position.
	// Without this, two scatter nodes (different Salt) or two actors (different NodeSeed) sharing a
	// position would reroll in lockstep.
	TEST_METHOD(MakeStreamForPosition_DifferentSaltOrSeedYieldsDifferentStream)
	{
		const FVector Position(1234.0, -567.0, 89.0);

		const FRandomStream StreamSaltA = ArcPCGScatterCore::MakeStreamForPosition(42, 0xABCDu, Position);
		const FRandomStream StreamSaltB = ArcPCGScatterCore::MakeStreamForPosition(42, 0x1234u, Position);
		ASSERT_THAT(IsTrue(StreamSaltA.GetInitialSeed() != StreamSaltB.GetInitialSeed(),
			TEXT("Differing Salt at a fixed (Seed, Position) must yield a differing stream — two nodes ")
			TEXT("sharing a Seed and a source position must not reroll in lockstep")));

		const FRandomStream StreamSeedA = ArcPCGScatterCore::MakeStreamForPosition(42, 0xABCDu, Position);
		const FRandomStream StreamSeedB = ArcPCGScatterCore::MakeStreamForPosition(99, 0xABCDu, Position);
		ASSERT_THAT(IsTrue(StreamSeedA.GetInitialSeed() != StreamSeedB.GetInitialSeed(),
			TEXT("Differing NodeSeed at a fixed (Salt, Position) must yield a differing stream — two ")
			TEXT("actors running the same graph must not reroll in lockstep")));
	}
};
