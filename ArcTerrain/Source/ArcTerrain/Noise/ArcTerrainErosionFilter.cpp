/*
=====================================================================================

Advanced Terrain Erosion Filter and Phacelle Noise function
copyright (c) 2025 Rune Skovbo Johansen.
Converted to burstable C# by Luke Mitchell, 2026.
Ported to C++ for ArcTerrain, 2026.

== MPL 2.0 License ==

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
=====================================================================================
*/

#include "Noise/ArcTerrainErosionFilter.h"

#include "Noise/ArcTerrainNoise.h"

namespace ArcTerrainErosionImpl_7b2e
{
	static const double Tau = 6.2831853071795864769;
	static const double Epsilon = 1.0e-10;
	static const double MinCharacteristicSlope = 1.0e-6;

	inline double Clamp01(double Value)
	{
		return FMath::Clamp(Value, 0.0, 1.0);
	}

	inline double PowInv(double T, double Power)
	{
		return 1.0 - FMath::Pow(1.0 - Clamp01(T), Power);
	}

	inline double EaseOut(double T)
	{
		const double V = 1.0 - Clamp01(T);
		return 1.0 - V * V;
	}

	inline double SmoothStart(double T, double Smoothing)
	{
		if (T >= Smoothing)
		{
			return T - 0.5 * Smoothing;
		}
		return 0.5 * T * T / Smoothing;
	}

	inline double SignOrZero(double Value)
	{
		return Value > 0.0 ? 1.0 : (Value < 0.0 ? -1.0 : 0.0);
	}

	inline FVector2d CellOffset(FVector2d GridPoint, uint32 Seed)
	{
		static const double InvMaxUint16 = 1.0 / 65535.0;

		const uint32 Hash = ArcTerrainNoise::HashCell(
			ArcTerrainNoise::FloorToCell(GridPoint.X), ArcTerrainNoise::FloorToCell(GridPoint.Y), Seed);

		return FVector2d(
			static_cast<double>(Hash & 0xFFFFu) * InvMaxUint16 * 2.0 - 1.0,
			static_cast<double>((Hash >> 16) & 0xFFFFu) * InvMaxUint16 * 2.0 - 1.0);
	}
}

namespace ArcTerrainErosion
{
	FVector4d PhacelleNoise(
		FVector2d P, FVector2d NormDir, double Freq, double Offset, double Normalization, uint32 Seed)
	{
		using namespace ArcTerrainErosionImpl_7b2e;

		const FVector2d SideDir = FVector2d(-NormDir.Y, NormDir.X) * (Freq * Tau);
		const double PhaseOffset = Offset * Tau;

		const FVector2d PInt(FMath::FloorToDouble(P.X), FMath::FloorToDouble(P.Y));
		const FVector2d PFrac = P - PInt;

		FVector2d PhaseDir(0.0, 0.0);
		double WeightSum = 0.0;

		for (int32 I = -1; I <= 2; ++I)
		{
			for (int32 J = -1; J <= 2; ++J)
			{
				const FVector2d GridOffset(static_cast<double>(I), static_cast<double>(J));
				const FVector2d RandomOffset = CellOffset(PInt + GridOffset, Seed) * 0.5;
				const FVector2d VectorFromCellPoint = PFrac - GridOffset - RandomOffset;

				const double SqrDist = FVector2d::DotProduct(VectorFromCellPoint, VectorFromCellPoint);

				// Makes the weight actually zero at distance 1.5, avoiding grid-line artefacts.
				const double Weight = FMath::Max(0.0, FMath::Exp(-SqrDist * 2.0) - 0.01111);
				WeightSum += Weight;

				const double WaveInput = FVector2d::DotProduct(VectorFromCellPoint, SideDir) + PhaseOffset;
				PhaseDir += FVector2d(FMath::Cos(WaveInput), FMath::Sin(WaveInput)) * Weight;
			}
		}

		const FVector2d Interpolated = PhaseDir / WeightSum;

		double Magnitude = FMath::Sqrt(FVector2d::DotProduct(Interpolated, Interpolated));
		Magnitude = FMath::Max(1.0 - Normalization, Magnitude);

		const FVector2d Normalized = Interpolated / Magnitude;
		return FVector4d(Normalized.X, Normalized.Y, SideDir.X, SideDir.Y);
	}

	FVector3d ErosionFilter(
		FVector2d P, FVector3d HeightAndSlope, double FadeTarget, double CharacteristicSlope,
		const FArcTerrainErosionParams& Knobs, uint32 Seed)
	{
		using namespace ArcTerrainErosionImpl_7b2e;

		// The loop's strength IS a depth in cm, so authoring it as one keeps ScaleCm free to move the
		// channels apart without deepening them; Sanitize is what bounds the ratio the filter needs.
		double Strength = Knobs.ChannelDepthCm;
		FadeTarget = FMath::Clamp(FadeTarget, -1.0, 1.0);

		const FVector3d InputHeightAndSlope = HeightAndSlope;
		double Freq = 1.0 / (Knobs.ScaleCm * Knobs.CellScale);

		const FVector2d InputSlope(HeightAndSlope.Y, HeightAndSlope.Z);
		const double SlopeLength = FMath::Max(InputSlope.Size(), Epsilon);
		// Onset is a multiple of what this terrain calls steep, so it survives a change of amplitude or
		// feature size instead of shutting the mask.
		const double NormalizedSlope =
			SlopeLength / FMath::Max(CharacteristicSlope, MinCharacteristicSlope);
		double RoundingMult = 1.0;

		const double RoundingForInput = FMath::Lerp(
			Knobs.RoundingAtValley, Knobs.RoundingAtPeak, Clamp01(FadeTarget + 0.5)) * Knobs.RoundingInputScale;
		double CombiMask = EaseOut(SmoothStart(
			NormalizedSlope * Knobs.OnsetInput, RoundingForInput * Knobs.OnsetInput));

		FVector2d GullySlope = FMath::Lerp(
			InputSlope, InputSlope / SlopeLength * Knobs.AssumedSlopeMagnitude, Knobs.AssumedSlopeBlend);

		for (int32 Octave = 0; Octave < Knobs.Octaves; ++Octave)
		{
			const FVector4d Phacelle = PhacelleNoise(
				P * Freq, GullySlope.GetSafeNormal(0.0), Knobs.CellScale, 0.25, Knobs.Normalization, Seed);

			const FVector2d PhacelleDir = FVector2d(Phacelle.Z, Phacelle.W) * -Freq;
			const double Sloping = FMath::Abs(Phacelle.Y);

			GullySlope += PhacelleDir * (SignOrZero(Phacelle.Y) * Strength * Knobs.GullyWeight);

			const FVector3d Gullies(Phacelle.X, Phacelle.Y * PhacelleDir.X, Phacelle.Y * PhacelleDir.Y);
			const FVector3d FadedGullies = FMath::Lerp(
				FVector3d(FadeTarget, 0.0, 0.0), Gullies * Knobs.GullyWeight, CombiMask);

			HeightAndSlope += FadedGullies * Strength;
			FadeTarget = FadedGullies.X;

			const double RoundingForOctave = FMath::Lerp(
				Knobs.RoundingAtValley, Knobs.RoundingAtPeak, Clamp01(Phacelle.X + 0.5)) * RoundingMult;
			const double NewMask = EaseOut(SmoothStart(
				Sloping * Knobs.OnsetOctave, RoundingForOctave * Knobs.OnsetOctave));
			CombiMask = PowInv(CombiMask, Knobs.Detail) * NewMask;

			Strength *= Knobs.Gain;
			Freq *= Knobs.Lacunarity;
			RoundingMult *= Knobs.RoundingOctaveDecay;
		}

		return HeightAndSlope - InputHeightAndSlope;
	}
}
