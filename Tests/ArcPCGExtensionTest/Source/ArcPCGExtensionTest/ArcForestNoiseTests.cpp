// Copyright Lukasz Baran. All Rights Reserved.

// ArcForestNoiseTests.cpp
//
// Coverage of ArcForestNoise::SampleNoise01 (Forest/ArcForestNoise.{h,cpp}, Step 5) and of
// FArcForestHabitatInputs::Empty()'s neutral-moisture guard (Forest/ArcForestField.h, Step 5's fix).
//
// SampleNoise01 is a pure function of (Params, Pos, SolveSeed) — no RNG, no simulation, no PCG, no
// fixture asset. Every assertion here is either exact (determinism, the Frequency<=0 guard, the
// Gain==0 collapse, the parameter clamps, and the Ridged+Billow affine identity — all genuinely exact
// arithmetic) or a STATED, loose distribution check over a fixed position set. Per the header's own
// measured properties, this file deliberately does NOT assert "spans [0,1]" as the sole containment
// check (see the dedicated non-saturation assertion below) and does NOT use a correlation threshold
// between Ridged and Billow (they are anticorrelated, not independent, at corr ~= -0.99).
//
// UNTESTED BY DESIGN: the per-octave non-integer offset stride (OctaveOffsetStrideX/Y in
// ArcForestNoise.cpp) is an anti-grid-artifact measure with no robust closed-form oracle — there is no
// statistical test that would fail for the RIGHT reason if it regressed, so none is written here.
// SampleNoise01 also currently has NO production consumer (Step 7 wires it in) — this suite's green is
// proof about a standalone helper, not about a shipped habitat feature.

#include "CQTest.h"

#include "Forest/ArcForestField.h"
#include "Forest/ArcForestNoise.h"

namespace ArcForestNoiseTestLocal
{
	/** Fixed set of world XY sample positions (cm), spread over a few km so lattice tiling isn't hit trivially. */
	TArray<FVector2f> MakeSamplePositions()
	{
		TArray<FVector2f> Positions;
		for (int32 Row = 0; Row < 12; ++Row)
		{
			for (int32 Col = 0; Col < 12; ++Col)
			{
				Positions.Add(FVector2f(static_cast<float>(Col) * 137.0f, static_cast<float>(Row) * 219.0f));
			}
		}
		return Positions;
	}

	/**
	 * Every EArcForestNoiseType value, read from the enum's own reflection data rather than a hand-typed
	 * array. Per ArcForestNoise.cpp's KernelFor comment, a new enumerator without a KernelFor arm falls
	 * through silently in Shipping/Test (MSVC's missing-enumerator warning is off at this project's
	 * warning level), so any test that hardcodes "the four types" would not notice a fifth. Driving every
	 * type loop from this helper means a fifth enumerator is swept automatically.
	 */
	TArray<EArcForestNoiseType> AllNoiseTypes()
	{
		TArray<EArcForestNoiseType> Types;
		const UEnum* EnumPtr = StaticEnum<EArcForestNoiseType>();
		check(EnumPtr != nullptr);
		// UHT GUARANTEES every UENUM's Names list ends with a "_MAX" sentinel — it auto-generates one
		// when the C++ declaration doesn't supply it (EArcForestNoiseType has none), so NumEnums() always
		// counts one MORE than the real enumerators and the LAST entry is never a value KernelFor's
		// switch handles. Confirmed empirically: including it fired ArcForestNoise.cpp:89's
		// checkNoEntry() at runtime (value 4, one past Turbulence=3). ContainsExistingMax() reports
		// whether that sentinel is present in Names AT ALL (true for every UHT-generated enum, per the
		// checkfSlow in UEnum::InitializeNames — Engine/Source/Runtime/CoreUObject/Private/UObject/
		// Enum.cpp:1108), so subtracting 1 exactly when it's true matches the engine's own idiom for
		// this (InstanceDataObjectUtils.cpp:1044) and skips only the sentinel, never a genuine value.
		const int32 NumRealEntries = EnumPtr->ContainsExistingMax() ? EnumPtr->NumEnums() - 1 : EnumPtr->NumEnums();
		for (int32 Index = 0; Index < NumRealEntries; ++Index)
		{
			Types.Add(static_cast<EArcForestNoiseType>(EnumPtr->GetValueByIndex(Index)));
		}
		return Types;
	}

	/** Population mean of |A[i] - B[i]| over paired samples. */
	float MeanAbsDifference(const TArray<float>& A, const TArray<float>& B)
	{
		check(A.Num() == B.Num() && A.Num() > 0);
		double Sum = 0.0;
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			Sum += static_cast<double>(FMath::Abs(A[Index] - B[Index]));
		}
		return static_cast<float>(Sum / static_cast<double>(A.Num()));
	}

	/** Population mean of |Value| over a single array. */
	float MeanAbs(const TArray<float>& Values)
	{
		check(Values.Num() > 0);
		double Sum = 0.0;
		for (const float Value : Values)
		{
			Sum += static_cast<double>(FMath::Abs(Value));
		}
		return static_cast<float>(Sum / static_cast<double>(Values.Num()));
	}

	/** Population max of |Value| over a single array. */
	float MaxAbs(const TArray<float>& Values)
	{
		check(Values.Num() > 0);
		float Max = 0.0f;
		for (const float Value : Values)
		{
			Max = FMath::Max(Max, FMath::Abs(Value));
		}
		return Max;
	}

	/** Population variance (over N, not N-1 — this is a descriptive spread check, not a sample estimate). */
	float PopulationVariance(const TArray<float>& Values)
	{
		check(Values.Num() > 0);
		double Mean = 0.0;
		for (const float Value : Values)
		{
			Mean += static_cast<double>(Value);
		}
		Mean /= static_cast<double>(Values.Num());

		double SumSquaredDelta = 0.0;
		for (const float Value : Values)
		{
			const double Delta = static_cast<double>(Value) - Mean;
			SumSquaredDelta += Delta * Delta;
		}
		return static_cast<float>(SumSquaredDelta / static_cast<double>(Values.Num()));
	}

	TArray<float> SampleField(const FArcForestNoiseParams& Params, int32 SolveSeed, const TArray<FVector2f>& Positions)
	{
		TArray<float> Values;
		Values.Reserve(Positions.Num());
		for (const FVector2f& Pos : Positions)
		{
			Values.Add(ArcForestNoise::SampleNoise01(Params, Pos, SolveSeed));
		}
		return Values;
	}

	/** Count sign changes of (value - 0.5) along a 1D transect at fixed Y, X in [0, TransectLengthCm). */
	int32 CountZeroCrossings(const FArcForestNoiseParams& Params, int32 SolveSeed, float TransectLengthCm, int32 NumSamples)
	{
		int32 Crossings = 0;
		bool bHavePrevious = false;
		bool bPreviousAboveHalf = false;
		for (int32 Index = 0; Index < NumSamples; ++Index)
		{
			const float X = (TransectLengthCm * static_cast<float>(Index)) / static_cast<float>(NumSamples - 1);
			const float Value = ArcForestNoise::SampleNoise01(Params, FVector2f(X, 0.0f), SolveSeed);
			const bool bAboveHalf = Value >= 0.5f;
			if (bHavePrevious && (bAboveHalf != bPreviousAboveHalf))
			{
				++Crossings;
			}
			bPreviousAboveHalf = bAboveHalf;
			bHavePrevious = true;
		}
		return Crossings;
	}
}

TEST_CLASS(ArcForestNoiseTests, "ArcPCGExtension.ForestSim.Noise")
{
	TEST_METHOD(Noise_IsDeterministicForSameSeed)
	{
		using namespace ArcForestNoiseTestLocal;

		FArcForestNoiseParams Params;
		Params.Type = EArcForestNoiseType::Perlin;
		Params.Frequency = 0.001f;
		Params.Octaves = 3;
		Params.Lacunarity = 2.0f;
		Params.Gain = 0.5f;
		Params.SeedOffset = 0;

		const TArray<FVector2f> Positions = MakeSamplePositions();

		// Same params, same SolveSeed, same positions -> bit-identical (pure function, no tolerance).
		const TArray<float> RunA = SampleField(Params, 42, Positions);
		const TArray<float> RunB = SampleField(Params, 42, Positions);
		ASSERT_THAT(AreEqual(RunA.Num(), RunB.Num()));
		for (int32 Index = 0; Index < RunA.Num(); ++Index)
		{
			ASSERT_THAT(IsTrue(RunA[Index] == RunB[Index],
				*FString::Printf(TEXT("Index %d: same seed must be bit-identical, got %f vs %f"), Index, RunA[Index], RunB[Index])));
		}

		// A different SolveSeed must give a different field somewhere over this many positions.
		const TArray<float> RunDifferentSeed = SampleField(Params, 999, Positions);
		bool bAnyDifferent = false;
		for (int32 Index = 0; Index < RunA.Num(); ++Index)
		{
			if (RunA[Index] != RunDifferentSeed[Index])
			{
				bAnyDifferent = true;
				break;
			}
		}
		ASSERT_THAT(IsTrue(bAnyDifferent, TEXT("A different SolveSeed must not reproduce the same field everywhere")));
	}

	TEST_METHOD(Noise_OutputStaysInUnitRange)
	{
		using namespace ArcForestNoiseTestLocal;

		// Containment only, per the header's measured properties: normalisation guarantees the BOUNDS
		// [0,1], NOT that the realised distribution spans them. Realised Perlin range narrows toward
		// 0.5 as Octaves rises (fBm averaging), so asserting a spread here would fail against CORRECT
		// code — see the header's AMPLITUDE NORMALISATION doc block. This sweep is unfalsifiable on its
		// own (SampleNoise01 ends in FMath::Clamp(...,0,1), so "in [0,1]" restates the clamp's own
		// contract) — kept cheap and as documentation, with the dedicated non-saturation assertion below
		// actually proving the amplitude-normalisation division runs.
		const TArray<FVector2f> Positions = MakeSamplePositions();
		const TArray<EArcForestNoiseType> Types = AllNoiseTypes();
		const int32 OctaveValues[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
		const float GainValues[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
		const float LacunarityValues[] = { 1.0f, 2.0f, 8.0f };

		for (const EArcForestNoiseType Type : Types)
		{
			for (const int32 Octaves : OctaveValues)
			{
				for (const float Gain : GainValues)
				{
					for (const float Lacunarity : LacunarityValues)
					{
						FArcForestNoiseParams Params;
						Params.Type = Type;
						Params.Frequency = 0.001f;
						Params.Octaves = Octaves;
						Params.Gain = Gain;
						Params.Lacunarity = Lacunarity;

						for (const FVector2f& Pos : Positions)
						{
							const float Value = ArcForestNoise::SampleNoise01(Params, Pos, 7);
							ASSERT_THAT(IsTrue(Value >= 0.0f && Value <= 1.0f,
								*FString::Printf(TEXT("Type %d Octaves %d Gain %f Lacunarity %f: value %f outside [0,1]"),
									static_cast<int32>(Type), Octaves, Gain, Lacunarity, Value)));
						}
					}
				}
			}
		}

		// Non-saturation: proves the "/ AmplitudeSum" division actually runs, which the containment
		// sweep above cannot (it is satisfied by a constant-1.0 field just as much as a spread one).
		// Derivation: deleting the division leaves Ridged's raw octave sum of (1-|n_i|)^2 terms
		// unnormalised. At Gain=1 every octave has amplitude 1, so at Octaves=8 the raw sum can reach up
		// to 8 (each term is in [0,1]) and the final FMath::Clamp(...,0,1) saturates EVERY position to
		// exactly 1.0f, giving fraction-at-one = 1.0 and variance = 0 under the mutant. Correct code
		// keeps the convex-combination bound sum(A_i*v_i)/S in [0,1] regardless of Octaves/Gain, and per
		// the header Ridged's realised mean sits ~0.64 with visible spread at 3 octaves, so a fraction
		// near 0 and a variance clearly above the floor is expected at 8 octaves too.
		FArcForestNoiseParams NonSaturationParams;
		NonSaturationParams.Type = EArcForestNoiseType::Ridged;
		NonSaturationParams.Frequency = 0.001f;
		NonSaturationParams.Octaves = 8;
		NonSaturationParams.Gain = 1.0f;
		NonSaturationParams.Lacunarity = 2.0f;
		const TArray<float> RidgedSaturationCheck = SampleField(NonSaturationParams, 7, Positions);

		int32 NumExactlyOne = 0;
		for (const float Value : RidgedSaturationCheck)
		{
			if (Value == 1.0f)
			{
				++NumExactlyOne;
			}
		}
		const float FractionExactlyOne = static_cast<float>(NumExactlyOne) / static_cast<float>(RidgedSaturationCheck.Num());
		const float SaturationVariance = PopulationVariance(RidgedSaturationCheck);

		ASSERT_THAT(IsTrue(FractionExactlyOne < 0.05f,
			*FString::Printf(TEXT("Ridged Gain=1 Octaves=8: %d/%d positions clamp to exactly 1.0 (%f%%) — amplitude normalisation looks skipped"),
				NumExactlyOne, RidgedSaturationCheck.Num(), FractionExactlyOne * 100.0f)));
		ASSERT_THAT(IsTrue(SaturationVariance > 1e-4f,
			*FString::Printf(TEXT("Ridged Gain=1 Octaves=8: field variance %f is at/near zero — looks saturated to a constant"), SaturationVariance)));
	}

	TEST_METHOD(Noise_FrequencyControlsFeatureSize)
	{
		using namespace ArcForestNoiseTestLocal;

		// Zero-crossing count along a fixed 40m transect must rise STRICTLY MONOTONICALLY with
		// Frequency. Reference measurements (sanity only, not asserted exactly):
		// 1e-4 -> 6, 1e-3 -> 19, 3e-3 -> 59, 1e-2 -> 192, 2e-2 -> 625, 3e-2 -> 1582.
		constexpr float TransectLengthCm = 4000.0f; // 40 m
		constexpr int32 NumSamples = 4001; // 1cm step
		const float Frequencies[] = { 1e-4f, 1e-3f, 3e-3f, 1e-2f, 2e-2f, 3e-2f };

		FArcForestNoiseParams Params;
		Params.Type = EArcForestNoiseType::Perlin;
		Params.Octaves = 3;
		Params.Lacunarity = 2.0f;
		Params.Gain = 0.5f;

		// The first frequency's own crossing count is asserted positive rather than seeding
		// PreviousCrossings at -1: with a -1 sentinel, "Crossings > PreviousCrossings" is true for ANY
		// non-negative count on the very first iteration, including 0 — so a mutant that zeroed out
		// Frequency's effect on the transect (e.g. dropping "Frequency *= Lacunarity" so every octave
		// samples the same lattice, or breaking the X-scaling entirely) could pass index 0 vacuously.
		// Reference measurement at 1e-4 is 6 crossings, so requiring > 0 is a real, cheap floor.
		Params.Frequency = Frequencies[0];
		int32 PreviousCrossings = CountZeroCrossings(Params, 7, TransectLengthCm, NumSamples);
		ASSERT_THAT(IsTrue(PreviousCrossings > 0,
			*FString::Printf(TEXT("Frequency %f: zero-crossing count must be positive, got %d"), Frequencies[0], PreviousCrossings)));

		for (int32 Index = 1; Index < UE_ARRAY_COUNT(Frequencies); ++Index)
		{
			Params.Frequency = Frequencies[Index];
			const int32 Crossings = CountZeroCrossings(Params, 7, TransectLengthCm, NumSamples);
			ASSERT_THAT(IsTrue(Crossings > PreviousCrossings,
				*FString::Printf(TEXT("Frequency %f: zero-crossing count %d must exceed the previous frequency's %d"),
					Frequencies[Index], Crossings, PreviousCrossings)));
			PreviousCrossings = Crossings;
		}
	}

	TEST_METHOD(Noise_TypesAreDistinctAtTwoOrMoreOctaves)
	{
		using namespace ArcForestNoiseTestLocal;

		// Octaves = 3, deliberately >= 2: at Octaves == 1 Turbulence is IDENTICAL to Billow
		// (abs-of-sum == sum-of-abs for a single term), so a distinctness test at one octave would be
		// vacuous for that pair. Distinctness is asserted on mean|difference| over the position set,
		// NOT on a correlation threshold: corr(Ridged, Billow) ~= -0.99 at 3 octaves (they remain
		// spatially anticorrelated, not independent), so a correlation-based check would fail against
		// CORRECT code for that pair. Perlin is near-independent of the other three (|corr| <= 0.05
		// per the header), so it is not singled out here.
		const TArray<FVector2f> Positions = MakeSamplePositions();
		FArcForestNoiseParams Params;
		Params.Octaves = 3;
		Params.Lacunarity = 2.0f;
		Params.Gain = 0.5f;
		Params.Frequency = 0.001f;

		Params.Type = EArcForestNoiseType::Perlin;
		const TArray<float> PerlinValues = SampleField(Params, 7, Positions);
		Params.Type = EArcForestNoiseType::Ridged;
		const TArray<float> RidgedValues = SampleField(Params, 7, Positions);
		Params.Type = EArcForestNoiseType::Billow;
		const TArray<float> BillowValues = SampleField(Params, 7, Positions);
		Params.Type = EArcForestNoiseType::Turbulence;
		const TArray<float> TurbulenceValues = SampleField(Params, 7, Positions);

		constexpr float MinMeanAbsDifference = 0.05f;
		const float PerlinVsRidged = MeanAbsDifference(PerlinValues, RidgedValues);
		const float PerlinVsBillow = MeanAbsDifference(PerlinValues, BillowValues);
		const float PerlinVsTurbulence = MeanAbsDifference(PerlinValues, TurbulenceValues);
		// NOTE: Ridged vs Billow is NOT the load-bearing check for this pair — see the affine-identity
		// assertion below. This mean|difference| check is kept because it still genuinely distinguishes
		// them (they are anticorrelated, not identical), but per the derivation below it scores the
		// square-removal mutant AS PASSING OR BETTER than correct code, so it must not be the only guard
		// on this pair.
		const float RidgedVsBillow = MeanAbsDifference(RidgedValues, BillowValues);
		const float RidgedVsTurbulence = MeanAbsDifference(RidgedValues, TurbulenceValues);
		const float BillowVsTurbulence = MeanAbsDifference(BillowValues, TurbulenceValues);

		ASSERT_THAT(IsTrue(PerlinVsRidged > MinMeanAbsDifference, *FString::Printf(TEXT("Perlin vs Ridged mean|diff| too small: %f"), PerlinVsRidged)));
		ASSERT_THAT(IsTrue(PerlinVsBillow > MinMeanAbsDifference, *FString::Printf(TEXT("Perlin vs Billow mean|diff| too small: %f"), PerlinVsBillow)));
		ASSERT_THAT(IsTrue(PerlinVsTurbulence > MinMeanAbsDifference, *FString::Printf(TEXT("Perlin vs Turbulence mean|diff| too small: %f"), PerlinVsTurbulence)));
		ASSERT_THAT(IsTrue(RidgedVsBillow > MinMeanAbsDifference, *FString::Printf(TEXT("Ridged vs Billow mean|diff| too small: %f (expected ~0.44 per header)"), RidgedVsBillow)));
		ASSERT_THAT(IsTrue(RidgedVsTurbulence > MinMeanAbsDifference, *FString::Printf(TEXT("Ridged vs Turbulence mean|diff| too small: %f"), RidgedVsTurbulence)));
		ASSERT_THAT(IsTrue(BillowVsTurbulence > MinMeanAbsDifference, *FString::Printf(TEXT("Billow vs Turbulence mean|diff| too small: %f"), BillowVsTurbulence)));

		// LOAD-BEARING affine-identity check for the Ridged/Billow pair (ArcForestNoise.cpp:104,
		// EOctaveShape::RidgeSquared = FMath::Square(1 - |n|)). With S = sum(A_i):
		//   Ridged + Billow - 1 = sum(A_i * [(1-|n_i|)^2 + |n_i| - 1]) / S
		//                       = sum(A_i * [-|n_i| + |n_i|^2 + |n_i|]) / S   (expand the square)
		//                       = sum(A_i * |n_i| * (|n_i| - 1)) / S
		//                       = -sum(A_i * |n_i| * (1 - |n_i|)) / S
		// Every term |n_i|*(1-|n_i|) is >= 0 (product of two values in [0,1]) and > 0 almost everywhere,
		// so this is STRICTLY NEGATIVE for correct code, with expected magnitude ~ E[|n|(1-|n|)] ~ 0.17.
		// Under the mutant (drop the square: RidgeSquared -> 1 - |n|), the bracketed term becomes
		// [(1-|n_i|) + |n_i| - 1] = 0 EXACTLY for every octave, at every position, so the sum is
		// identically 0 — a deterministic kill, unlike the statistical mean|difference| check above
		// (which the mutant PASSES more comfortably than correct code: ~0.58 vs ~0.44).
		TArray<float> RidgedPlusBillowMinusOne;
		RidgedPlusBillowMinusOne.Reserve(RidgedValues.Num());
		for (int32 Index = 0; Index < RidgedValues.Num(); ++Index)
		{
			RidgedPlusBillowMinusOne.Add(RidgedValues[Index] + BillowValues[Index] - 1.0f);
		}
		const float MaxAbsIdentityResidual = MaxAbs(RidgedPlusBillowMinusOne);
		const float MeanAbsIdentityResidual = MeanAbs(RidgedPlusBillowMinusOne);

		ASSERT_THAT(IsTrue(MaxAbsIdentityResidual > 0.01f,
			*FString::Printf(TEXT("max|Ridged+Billow-1| = %f: expected strictly away from 0 for correct RidgeSquared; the square-removal mutant gives exactly 0 everywhere"), MaxAbsIdentityResidual)));
		ASSERT_THAT(IsTrue(MeanAbsIdentityResidual > 0.05f,
			*FString::Printf(TEXT("mean|Ridged+Billow-1| = %f: expected ~0.17 for correct RidgeSquared; the square-removal mutant gives exactly 0"), MeanAbsIdentityResidual)));
	}

	TEST_METHOD(Noise_SeedOffsetDecorrelates)
	{
		using namespace ArcForestNoiseTestLocal;

		// Two SeedOffset values must give near-zero-correlation fields, so future extra habitat axes
		// sharing the same SolveSeed are genuinely independent of moisture.
		const TArray<FVector2f> Positions = MakeSamplePositions();
		FArcForestNoiseParams ParamsA;
		ParamsA.Type = EArcForestNoiseType::Perlin;
		ParamsA.Octaves = 3;
		ParamsA.Lacunarity = 2.0f;
		ParamsA.Gain = 0.5f;
		ParamsA.Frequency = 0.001f;
		ParamsA.SeedOffset = 0;
		FArcForestNoiseParams ParamsB = ParamsA;
		ParamsB.SeedOffset = 1;

		const TArray<float> ValuesA = SampleField(ParamsA, 7, Positions);
		const TArray<float> ValuesB = SampleField(ParamsB, 7, Positions);

		double MeanA = 0.0, MeanB = 0.0;
		for (int32 Index = 0; Index < ValuesA.Num(); ++Index)
		{
			MeanA += static_cast<double>(ValuesA[Index]);
			MeanB += static_cast<double>(ValuesB[Index]);
		}
		MeanA /= ValuesA.Num();
		MeanB /= ValuesB.Num();

		double Covariance = 0.0, VarianceA = 0.0, VarianceB = 0.0;
		for (int32 Index = 0; Index < ValuesA.Num(); ++Index)
		{
			const double DeltaA = static_cast<double>(ValuesA[Index]) - MeanA;
			const double DeltaB = static_cast<double>(ValuesB[Index]) - MeanB;
			Covariance += DeltaA * DeltaB;
			VarianceA += DeltaA * DeltaA;
			VarianceB += DeltaB * DeltaB;
		}

		// A degenerate variance (a mutant that collapses either field to a constant) must be a HARD
		// FAILURE, not silently scored as "perfectly decorrelated". The previous version defaulted
		// Correlation to 0.0 whenever the denominator was near-zero, which made a constant field pass
		// the |Correlation| < 0.2 check below trivially — exactly backwards, since a constant field
		// proves nothing about independence.
		ASSERT_THAT(IsTrue(VarianceA > KINDA_SMALL_NUMBER,
			*FString::Printf(TEXT("SeedOffset=0 field has degenerate variance %f (collapsed/constant field, not proof of decorrelation)"), VarianceA)));
		ASSERT_THAT(IsTrue(VarianceB > KINDA_SMALL_NUMBER,
			*FString::Printf(TEXT("SeedOffset=1 field has degenerate variance %f (collapsed/constant field, not proof of decorrelation)"), VarianceB)));

		const double Correlation = Covariance / FMath::Sqrt(VarianceA * VarianceB);
		ASSERT_THAT(IsTrue(FMath::Abs(Correlation) < 0.2, *FString::Printf(TEXT("SeedOffset 0 vs 1: expected near-zero correlation, got %f"), Correlation)));
	}

	TEST_METHOD(Noise_NonPositiveFrequencyIsFlatHalfForEveryType)
	{
		using namespace ArcForestNoiseTestLocal;

		// LOAD-BEARING: FArcForestHabitatInputs::Empty() relies on precisely this guard (Frequency=0)
		// to mean "no moisture input". If this regressed, every solve that omits the habitat argument
		// would silently receive live noise instead of a neutral 0.5.
		const TArray<EArcForestNoiseType> Types = AllNoiseTypes();
		const float NonPositiveFrequencies[] = { 0.0f, -1.0f };
		const TArray<FVector2f> Positions = MakeSamplePositions();

		for (const EArcForestNoiseType Type : Types)
		{
			for (const float Frequency : NonPositiveFrequencies)
			{
				FArcForestNoiseParams Params;
				Params.Type = Type;
				Params.Frequency = Frequency;
				Params.Octaves = 3;
				Params.Lacunarity = 2.0f;
				Params.Gain = 0.5f;

				for (const FVector2f& Pos : Positions)
				{
					const float Value = ArcForestNoise::SampleNoise01(Params, Pos, 7);
					ASSERT_THAT(IsTrue(Value == 0.5f,
						*FString::Printf(TEXT("Type %d Frequency %f Pos (%f,%f): expected exact 0.5, got %f"),
							static_cast<int32>(Type), Frequency, Pos.X, Pos.Y, Value)));
				}
			}
		}
	}

	TEST_METHOD(Habitat_EmptyInputsAreNeutralForMoisture)
	{
		// Guards the Step-5 fix directly: FArcForestHabitatInputs::Empty() is NOT default-constructed
		// — it explicitly zeroes MoistureNoise.Frequency. A future edit that "tidies" Empty() back to
		// default construction (a LIVE Perlin field per FArcForestNoiseParams's defaults) turns this
		// red via the Frequency assertion below, and the sampled-value assertion catches it even if
		// the Frequency field itself were renamed/reshuffled without updating this guard.
		const FArcForestHabitatInputs& Empty = FArcForestHabitatInputs::Empty();
		ASSERT_THAT(IsTrue(Empty.MoistureNoise.Frequency == 0.0f,
			*FString::Printf(TEXT("Empty().MoistureNoise.Frequency must be exactly 0.0, got %f"), Empty.MoistureNoise.Frequency)));

		const float SampledAtOrigin = ArcForestNoise::SampleNoise01(Empty.MoistureNoise, FVector2f(0.0f, 0.0f), 7);
		const float SampledElsewhere = ArcForestNoise::SampleNoise01(Empty.MoistureNoise, FVector2f(1234.5f, -678.9f), 12345);
		ASSERT_THAT(IsTrue(SampledAtOrigin == 0.5f,
			*FString::Printf(TEXT("Sampling Empty().MoistureNoise must return exact 0.5, got %f"), SampledAtOrigin)));
		ASSERT_THAT(IsTrue(SampledElsewhere == 0.5f,
			*FString::Printf(TEXT("Sampling Empty().MoistureNoise at a different position/seed must still return exact 0.5, got %f"), SampledElsewhere)));
	}

	TEST_METHOD(Noise_GainZeroCollapsesToSingleOctave)
	{
		// Gain = 0 with Octaves = 8 must return BIT-IDENTICALLY the same value as Octaves = 1.
		// RELABELLED: this kills "AmplitudeSum += 1.0" (a divide-by-octave-count mutant), NOT a
		// closed-form-divisor mutant. At Gain=0, ANY closed form for S = sum(Gain^i) — accumulated in
		// the loop, or a separately-computed sum(Gain^i) — gives S=1 for both Octaves=1 and Octaves=8
		// (0^0 is conventionally 1 for i>0 terms which are exactly 0, and the geometric-series closed
		// form (1-Gain)/(1-Gain) is a 0/0 indeterminate at Gain=1 but reduces cleanly to 1 term at
		// Gain=0), so a closed-form divisor is BIT-IDENTICAL to the accumulated one at this specific
		// param combination and this test cannot distinguish them. What it DOES kill: an
		// "AmplitudeSum += 1.0" mutant (dividing by octave count instead of amplitude sum) would give
		// Normalised = WeightedSum/8 at Octaves=8 vs WeightedSum/1 at Octaves=1 — an 8x scale error this
		// bit-identity check catches immediately. See Noise_OutOfRangeParamsClampToTheirInRangeEquivalent
		// for a test that actually distinguishes closed-form vs accumulated divisors via the clamps.
		const TArray<FVector2f> Positions = ArcForestNoiseTestLocal::MakeSamplePositions();
		const TArray<EArcForestNoiseType> Types = ArcForestNoiseTestLocal::AllNoiseTypes();

		for (const EArcForestNoiseType Type : Types)
		{
			FArcForestNoiseParams ParamsOneOctave;
			ParamsOneOctave.Type = Type;
			ParamsOneOctave.Frequency = 0.001f;
			ParamsOneOctave.Octaves = 1;
			ParamsOneOctave.Lacunarity = 2.0f;
			ParamsOneOctave.Gain = 0.0f;

			FArcForestNoiseParams ParamsEightOctaves = ParamsOneOctave;
			ParamsEightOctaves.Octaves = 8;

			for (const FVector2f& Pos : Positions)
			{
				const float ValueOneOctave = ArcForestNoise::SampleNoise01(ParamsOneOctave, Pos, 7);
				const float ValueEightOctaves = ArcForestNoise::SampleNoise01(ParamsEightOctaves, Pos, 7);
				ASSERT_THAT(IsTrue(ValueOneOctave == ValueEightOctaves,
					*FString::Printf(TEXT("Type %d Pos (%f,%f): Gain=0 Octaves=1 (%f) must bit-match Octaves=8 (%f)"),
						static_cast<int32>(Type), Pos.X, Pos.Y, ValueOneOctave, ValueEightOctaves)));
			}
		}
	}

	TEST_METHOD(Noise_OutOfRangeParamsClampToTheirInRangeEquivalent)
	{
		using namespace ArcForestNoiseTestLocal;

		// Every field's ClampMin/ClampMax UPROPERTY meta is EDITOR-UI-ONLY (a details-panel slider
		// limit), and every field is BlueprintReadWrite, so an out-of-range value is reachable at
		// runtime (Blueprint set, C++ construction, deserialised data). ArcForestNoise.cpp:165-167
		// re-clamps Octaves to [1,8], Lacunarity to [1,8] and Gain to [0,1] via FMath::Clamp before use,
		// so an out-of-range value must be BIT-IDENTICAL to sampling with its clamped equivalent set
		// directly. Values derived directly from those three FMath::Clamp calls:
		//   Octaves:    Clamp(-3,1,8)=1, Clamp(0,1,8)=1, Clamp(12,1,8)=8
		//   Gain:       Clamp(-1,0,1)=0, Clamp(5,0,1)=1
		//   Lacunarity: Clamp(0,1,8)=1,  Clamp(-2,1,8)=1, Clamp(99,1,8)=8
		const TArray<FVector2f> Positions = MakeSamplePositions();
		const TArray<EArcForestNoiseType> Types = AllNoiseTypes();

		const int32 OutOfRangeOctaves[] = { -3, 0, 12 };
		const int32 ClampedOctaves[] = { 1, 1, 8 };
		const float OutOfRangeGain[] = { -1.0f, 5.0f };
		const float ClampedGain[] = { 0.0f, 1.0f };
		const float OutOfRangeLacunarity[] = { 0.0f, -2.0f, 99.0f };
		const float ClampedLacunarity[] = { 1.0f, 1.0f, 8.0f };

		for (const EArcForestNoiseType Type : Types)
		{
			FArcForestNoiseParams Base;
			Base.Type = Type;
			Base.Frequency = 0.001f;
			Base.Octaves = 3;
			Base.Lacunarity = 2.0f;
			Base.Gain = 0.5f;

			for (int32 Index = 0; Index < UE_ARRAY_COUNT(OutOfRangeOctaves); ++Index)
			{
				FArcForestNoiseParams OutOfRangeParams = Base;
				OutOfRangeParams.Octaves = OutOfRangeOctaves[Index];
				FArcForestNoiseParams ClampedParams = Base;
				ClampedParams.Octaves = ClampedOctaves[Index];

				for (const FVector2f& Pos : Positions)
				{
					const float OutOfRangeValue = ArcForestNoise::SampleNoise01(OutOfRangeParams, Pos, 7);
					const float ClampedValue = ArcForestNoise::SampleNoise01(ClampedParams, Pos, 7);
					ASSERT_THAT(IsTrue(OutOfRangeValue == ClampedValue,
						*FString::Printf(TEXT("Type %d Octaves=%d must bit-match Octaves=%d, got %f vs %f"),
							static_cast<int32>(Type), OutOfRangeOctaves[Index], ClampedOctaves[Index], OutOfRangeValue, ClampedValue)));
				}
			}

			for (int32 Index = 0; Index < UE_ARRAY_COUNT(OutOfRangeGain); ++Index)
			{
				FArcForestNoiseParams OutOfRangeParams = Base;
				OutOfRangeParams.Gain = OutOfRangeGain[Index];
				FArcForestNoiseParams ClampedParams = Base;
				ClampedParams.Gain = ClampedGain[Index];

				for (const FVector2f& Pos : Positions)
				{
					const float OutOfRangeValue = ArcForestNoise::SampleNoise01(OutOfRangeParams, Pos, 7);
					const float ClampedValue = ArcForestNoise::SampleNoise01(ClampedParams, Pos, 7);
					ASSERT_THAT(IsTrue(OutOfRangeValue == ClampedValue,
						*FString::Printf(TEXT("Type %d Gain=%f must bit-match Gain=%f, got %f vs %f"),
							static_cast<int32>(Type), OutOfRangeGain[Index], ClampedGain[Index], OutOfRangeValue, ClampedValue)));
				}
			}

			for (int32 Index = 0; Index < UE_ARRAY_COUNT(OutOfRangeLacunarity); ++Index)
			{
				FArcForestNoiseParams OutOfRangeParams = Base;
				OutOfRangeParams.Lacunarity = OutOfRangeLacunarity[Index];
				FArcForestNoiseParams ClampedParams = Base;
				ClampedParams.Lacunarity = ClampedLacunarity[Index];

				for (const FVector2f& Pos : Positions)
				{
					const float OutOfRangeValue = ArcForestNoise::SampleNoise01(OutOfRangeParams, Pos, 7);
					const float ClampedValue = ArcForestNoise::SampleNoise01(ClampedParams, Pos, 7);
					ASSERT_THAT(IsTrue(OutOfRangeValue == ClampedValue,
						*FString::Printf(TEXT("Type %d Lacunarity=%f must bit-match Lacunarity=%f, got %f vs %f"),
							static_cast<int32>(Type), OutOfRangeLacunarity[Index], ClampedLacunarity[Index], OutOfRangeValue, ClampedValue)));
				}
			}
		}
	}

	TEST_METHOD(Noise_LacunarityChangesTheField)
	{
		using namespace ArcForestNoiseTestLocal;

		// Deleting "Frequency *= Lacunarity" (ArcForestNoise.cpp:195) makes every octave sample the SAME
		// frequency regardless of Lacunarity, so the field at Lacunarity=1.0 and Lacunarity=4.0 becomes
		// BIT-IDENTICAL under that mutant (Lacunarity is otherwise unused once the multiply is gone).
		// Correct code multiplies Frequency by Lacunarity every octave, so at Octaves=3 the two
		// Lacunarity values sample genuinely different per-octave frequencies (Lacunarity=1.0 keeps all
		// 3 octaves at the base frequency; Lacunarity=4.0 runs them at 1x/4x/16x), producing a real
		// mean|difference| over the position set. This is a deterministic kill (mutant gives exactly 0),
		// not a statistical threshold pinned to an observed number.
		const TArray<FVector2f> Positions = MakeSamplePositions();
		FArcForestNoiseParams ParamsLacunarityOne;
		ParamsLacunarityOne.Type = EArcForestNoiseType::Perlin;
		ParamsLacunarityOne.Frequency = 0.001f;
		ParamsLacunarityOne.Octaves = 3;
		ParamsLacunarityOne.Gain = 0.5f;
		ParamsLacunarityOne.Lacunarity = 1.0f;

		FArcForestNoiseParams ParamsLacunarityFour = ParamsLacunarityOne;
		ParamsLacunarityFour.Lacunarity = 4.0f;

		const TArray<float> ValuesLacunarityOne = SampleField(ParamsLacunarityOne, 7, Positions);
		const TArray<float> ValuesLacunarityFour = SampleField(ParamsLacunarityFour, 7, Positions);

		const float MeanAbsLacunarityDifference = MeanAbsDifference(ValuesLacunarityOne, ValuesLacunarityFour);
		ASSERT_THAT(IsTrue(MeanAbsLacunarityDifference > 0.05f,
			*FString::Printf(TEXT("mean|field@Lacunarity=1 - field@Lacunarity=4| = %f: Lacunarity must change the field (a dropped 'Frequency *= Lacunarity' gives exactly 0)"), MeanAbsLacunarityDifference)));
	}
};
