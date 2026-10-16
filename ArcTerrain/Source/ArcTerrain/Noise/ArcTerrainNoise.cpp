// Copyright Lukasz Baran. All Rights Reserved.

#include "Noise/ArcTerrainNoise.h"

#include "Noise/ArcTerrainErosionFilter.h"

namespace ArcTerrainNoiseImpl_5f3a
{
	using ArcTerrainNoise::FloorToCell;
	using ArcTerrainNoise::HashCell;

	/** 10 000 km: past any authorable world. The UPROPERTY UIMax values are slider bounds, not
	 *  clamps, so a typed value arrives here unbounded. */
	static const double MaxAuthoredCm = 1.0e9;

	inline double SanitizeCm(double Value, double MinValue, double NonFiniteValue)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, MinValue, MaxAuthoredCm) : NonFiniteValue;
	}

	inline double Fade(double T)
	{
		return T * T * T * (T * (T * 6.0 - 15.0) + 10.0);
	}

	inline double Smoothstep01(double T)
	{
		return T * T * (3.0 - 2.0 * T);
	}

	inline double GradientDot(uint32 Hash, double DX, double DY)
	{
		static const double Diagonal = 0.70710678118654752;
		static const double GradientX[8] = { 1.0, -1.0, 0.0, 0.0, Diagonal, -Diagonal, Diagonal, -Diagonal };
		static const double GradientY[8] = { 0.0, 0.0, 1.0, -1.0, Diagonal, Diagonal, -Diagonal, -Diagonal };

		const uint32 Index = Hash & 7u;
		return GradientX[Index] * DX + GradientY[Index] * DY;
	}

	double GradientNoise2D(double X, double Y, uint32 Seed)
	{
		static const double UnitScale = 1.4142135623730951;

		const double FloorX = FMath::FloorToDouble(X);
		const double FloorY = FMath::FloorToDouble(Y);
		const int64 CellX = FloorToCell(FloorX);
		const int64 CellY = FloorToCell(FloorY);
		const double DX = X - FloorX;
		const double DY = Y - FloorY;

		const double Corner00 = GradientDot(HashCell(CellX, CellY, Seed), DX, DY);
		const double Corner10 = GradientDot(HashCell(CellX + 1, CellY, Seed), DX - 1.0, DY);
		const double Corner01 = GradientDot(HashCell(CellX, CellY + 1, Seed), DX, DY - 1.0);
		const double Corner11 = GradientDot(HashCell(CellX + 1, CellY + 1, Seed), DX - 1.0, DY - 1.0);

		const double U = Fade(DX);
		const double V = Fade(DY);
		const double Bottom = FMath::Lerp(Corner00, Corner10, U);
		const double Top = FMath::Lerp(Corner01, Corner11, U);
		return FMath::Lerp(Bottom, Top, V) * UnitScale;
	}

	double ValueNoise2D(double X, double Y, uint32 Seed)
	{
		static const double InvMaxUint32 = 1.0 / 4294967295.0;

		const double FloorX = FMath::FloorToDouble(X);
		const double FloorY = FMath::FloorToDouble(Y);
		const int64 CellX = FloorToCell(FloorX);
		const int64 CellY = FloorToCell(FloorY);

		const double Corner00 = static_cast<double>(HashCell(CellX, CellY, Seed)) * InvMaxUint32;
		const double Corner10 = static_cast<double>(HashCell(CellX + 1, CellY, Seed)) * InvMaxUint32;
		const double Corner01 = static_cast<double>(HashCell(CellX, CellY + 1, Seed)) * InvMaxUint32;
		const double Corner11 = static_cast<double>(HashCell(CellX + 1, CellY + 1, Seed)) * InvMaxUint32;

		const double U = Fade(X - FloorX);
		const double V = Fade(Y - FloorY);
		const double Bottom = FMath::Lerp(Corner00, Corner10, U);
		const double Top = FMath::Lerp(Corner01, Corner11, U);
		return FMath::Lerp(Bottom, Top, V) * 2.0 - 1.0;
	}

	/** 3x3 neighbourhood: the standard Worley approximation. F1 is exact for the jitter range used
	 *  here; F2 can miss a nearer second point across a cell corner, which leaves rare hairline
	 *  discontinuities in the F2-F1 field. */
	void VoronoiF1F2(double X, double Y, uint32 Seed, double& OutF1, double& OutF2)
	{
		static const double InvMaxUint16 = 1.0 / 65535.0;

		const int64 CellX = FloorToCell(X);
		const int64 CellY = FloorToCell(Y);

		double Nearest = TNumericLimits<double>::Max();
		double SecondNearest = TNumericLimits<double>::Max();

		for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
		{
			for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
			{
				const int64 NeighbourX = CellX + OffsetX;
				const int64 NeighbourY = CellY + OffsetY;
				const uint32 Hash = HashCell(NeighbourX, NeighbourY, Seed);

				const double PointX = static_cast<double>(NeighbourX)
					+ static_cast<double>(Hash & 0xFFFFu) * InvMaxUint16;
				const double PointY = static_cast<double>(NeighbourY)
					+ static_cast<double>((Hash >> 16) & 0xFFFFu) * InvMaxUint16;

				const double DeltaX = PointX - X;
				const double DeltaY = PointY - Y;
				const double Distance = FMath::Sqrt(DeltaX * DeltaX + DeltaY * DeltaY);

				if (Distance < Nearest)
				{
					SecondNearest = Nearest;
					Nearest = Distance;
				}
				else if (Distance < SecondNearest)
				{
					SecondNearest = Distance;
				}
			}
		}

		OutF1 = Nearest;
		OutF2 = SecondNearest;
	}
}

namespace ArcTerrainNoise
{
	FArcTerrainNoiseParams Sanitize(const FArcTerrainNoiseParams& Params)
	{
		using namespace ArcTerrainNoiseImpl_5f3a;

		static const FArcTerrainNoiseParams Defaults;

		FArcTerrainNoiseParams Result = Params;
		Result.Octaves = FMath::Clamp(Params.Octaves, 1, 12);
		Result.FeatureSizeCm = SanitizeCm(Params.FeatureSizeCm, 1.0, Defaults.FeatureSizeCm);
		Result.Lacunarity = FMath::Clamp(Params.Lacunarity, 1.1f, 4.0f);
		Result.Gain = FMath::Clamp(Params.Gain, 0.0f, 1.0f);
		Result.WarpStrengthCm = SanitizeCm(Params.WarpStrengthCm, 0.0, 0.0);
		Result.WarpFeatureSizeCm = Result.WarpStrengthCm > 0.0
			? SanitizeCm(Params.WarpFeatureSizeCm, 1.0, Defaults.WarpFeatureSizeCm)
			: Defaults.WarpFeatureSizeCm;
		Result.TerraceStepCm = SanitizeCm(Params.TerraceStepCm, 0.0, 0.0);
		Result.TerraceSharpness = Result.TerraceStepCm > 0.0
			? FMath::Clamp(Params.TerraceSharpness, 0.0f, 1.0f)
			: Defaults.TerraceSharpness;
		Result.AmplitudeCm = SanitizeCm(Params.AmplitudeCm, -MaxAuthoredCm, 0.0);

		Result.Erosion.GullyWeight = FMath::Clamp(Params.Erosion.GullyWeight, 0.0, 2.0);
		Result.Erosion.Detail = FMath::Clamp(Params.Erosion.Detail, 0.0, 8.0);
		Result.Erosion.RoundingAtValley = FMath::Clamp(Params.Erosion.RoundingAtValley, 0.0, 2.0);
		Result.Erosion.RoundingAtPeak = FMath::Clamp(Params.Erosion.RoundingAtPeak, 0.0, 2.0);
		Result.Erosion.RoundingInputScale = FMath::Clamp(Params.Erosion.RoundingInputScale, 0.0, 4.0);
		Result.Erosion.RoundingOctaveDecay = FMath::Clamp(Params.Erosion.RoundingOctaveDecay, 0.0, 1.0);
		Result.Erosion.OnsetInput = FMath::Clamp(Params.Erosion.OnsetInput, 0.0, 8.0);
		Result.Erosion.OnsetOctave = FMath::Clamp(Params.Erosion.OnsetOctave, 0.0, 8.0);
		Result.Erosion.AssumedSlopeMagnitude = FMath::Clamp(Params.Erosion.AssumedSlopeMagnitude, 0.0, 4.0);
		Result.Erosion.AssumedSlopeBlend = FMath::Clamp(Params.Erosion.AssumedSlopeBlend, 0.0, 1.0);
		Result.Erosion.CellScale = FMath::Clamp(Params.Erosion.CellScale, 0.1, 4.0);
		Result.Erosion.Octaves = FMath::Clamp(Params.Erosion.Octaves, 1, 8);
		Result.Erosion.Gain = FMath::Clamp(Params.Erosion.Gain, 0.0, 1.0);
		Result.Erosion.Lacunarity = FMath::Clamp(Params.Erosion.Lacunarity, 1.1, 4.0);
		Result.Erosion.Normalization = FMath::Clamp(Params.Erosion.Normalization, 0.0, 0.999);
		Result.Erosion.ScaleCm = SanitizeCm(Params.Erosion.ScaleCm, 1.0, Defaults.Erosion.ScaleCm);
		// The filter's domain is on the ratio, not the depth: a channel deeper than four times its own
		// spacing is a cliff, not a gully.
		Result.Erosion.ChannelDepthCm = FMath::Min(
			SanitizeCm(Params.Erosion.ChannelDepthCm, 0.0, Defaults.Erosion.ChannelDepthCm),
			4.0 * Result.Erosion.ScaleCm);
		return Result;
	}

	FEvaluator::FEvaluator(const FArcTerrainNoiseParams& Params, double InDerivativeStepCm)
		: FEvaluator(Sanitize(Params), InDerivativeStepCm, FSanitizedTag{})
	{
	}

	FEvaluator::FEvaluator(
		const FArcTerrainNoiseParams& SanitizedParams, double InDerivativeStepCm, FSanitizedTag)
		: BasisKind(SanitizedParams.Basis)
		, FractalMode(SanitizedParams.Fractal)
		, Seed(static_cast<uint32>(SanitizedParams.Seed))
		// Off the Seed + Octave * 0x9E3779B9 ladder Accumulate walks: XOR-ing the same constant makes
		// the X warp field bit-identical to octave 1 at the default seed of 0.
		, WarpSeedX(HashCell(0, -1, static_cast<uint32>(SanitizedParams.Seed)))
		, WarpSeedY(HashCell(-1, 0, static_cast<uint32>(SanitizedParams.Seed)))
		, Octaves(SanitizedParams.Octaves)
		, BaseFrequency(1.0 / SanitizedParams.FeatureSizeCm)
		, Lacunarity(static_cast<double>(SanitizedParams.Lacunarity))
		, Gain(static_cast<double>(SanitizedParams.Gain))
		, AmplitudeCm(SanitizedParams.AmplitudeCm)
		, InvAmplitudeSum(1.0)
		, WarpStrengthCm(SanitizedParams.WarpStrengthCm)
		, WarpFrequency(1.0 / SanitizedParams.WarpFeatureSizeCm)
		, TerraceStepCm(SanitizedParams.TerraceStepCm)
		, TerraceSharpness(static_cast<double>(SanitizedParams.TerraceSharpness))
		, DerivativeStepCm(FMath::IsFinite(InDerivativeStepCm) && InDerivativeStepCm > 0.0
			? InDerivativeStepCm
			: SanitizedParams.FeatureSizeCm / 1024.0)
		, CharacteristicSlope(
			FMath::Abs(SanitizedParams.AmplitudeCm) / FMath::Max(SanitizedParams.FeatureSizeCm, 1.0))
		, bErosionEnabled(SanitizedParams.Erosion.bEnabled)
		, Erosion(SanitizedParams.Erosion)
	{
		// A quarter of the coarsest channel spacing, not a fraction of the texel: at Gain * Lacunarity
		// near 1 every octave weighs the same in the gradient, so a texel-wide difference points a new
		// way every few metres and the channels it steers never span one stripe.
		DerivativeStepCm = FMath::Max(0.25 * Erosion.ScaleCm * Erosion.CellScale, DerivativeStepCm);

		// Normalizing by the amplitude sum is what keeps AmplitudeCm meaning a height in cm: without
		// it, adding an octave would silently raise the whole surface.
		double AmplitudeSum = 0.0;
		double Amplitude = 1.0;
		for (int32 Octave = 0; Octave < Octaves; ++Octave)
		{
			AmplitudeSum += Amplitude;
			Amplitude *= Gain;
		}
		InvAmplitudeSum = AmplitudeSum > 0.0 ? 1.0 / AmplitudeSum : 1.0;
	}

	double FEvaluator::Basis(double X, double Y, uint32 OctaveSeed) const
	{
		using namespace ArcTerrainNoiseImpl_5f3a;

		switch (BasisKind)
		{
		case EArcTerrainNoiseBasis::Perlin:
			return GradientNoise2D(X, Y, OctaveSeed);

		case EArcTerrainNoiseBasis::Value:
			return ValueNoise2D(X, Y, OctaveSeed);

		case EArcTerrainNoiseBasis::VoronoiF1:
		{
			double Nearest = 0.0;
			double SecondNearest = 0.0;
			VoronoiF1F2(X, Y, OctaveSeed, Nearest, SecondNearest);
			return FMath::Clamp(Nearest, 0.0, 1.0) * 2.0 - 1.0;
		}

		case EArcTerrainNoiseBasis::VoronoiF2F1:
		{
			double Nearest = 0.0;
			double SecondNearest = 0.0;
			VoronoiF1F2(X, Y, OctaveSeed, Nearest, SecondNearest);
			return FMath::Clamp((SecondNearest - Nearest) * 1.5, 0.0, 1.0) * 2.0 - 1.0;
		}
		}

		checkNoEntry();
		return 0.0;
	}

	double FEvaluator::Accumulate(double X, double Y) const
	{
		double Frequency = BaseFrequency;
		double Amplitude = 1.0;
		double Sum = 0.0;
		double Weight = 1.0;
		bool bStrictlyPositive = false;

		for (int32 Octave = 0; Octave < Octaves; ++Octave)
		{
			const uint32 OctaveSeed = Seed + static_cast<uint32>(Octave) * 0x9E3779B9u;
			const double Value = Basis(X * Frequency, Y * Frequency, OctaveSeed);

			switch (FractalMode)
			{
			case EArcTerrainNoiseFractal::Standard:
				Sum += Amplitude * Value;
				break;

			case EArcTerrainNoiseFractal::Billow:
				Sum += Amplitude * (2.0 * FMath::Abs(Value) - 1.0);
				break;

			case EArcTerrainNoiseFractal::Ridged:
				Sum += Amplitude * (1.0 - 2.0 * FMath::Abs(Value));
				break;

			case EArcTerrainNoiseFractal::RidgedMulti:
			{
				double Signal = 1.0 - FMath::Abs(Value);
				Signal *= Signal;
				Signal *= Weight;
				Weight = FMath::Clamp(Signal * 2.0, 0.0, 1.0);
				Sum += Amplitude * Signal;
				bStrictlyPositive = true;
				break;
			}
			}

			Frequency *= Lacunarity;
			Amplitude *= Gain;
		}

		const double Normalized = Sum * InvAmplitudeSum;
		return bStrictlyPositive ? Normalized * 2.0 - 1.0 : Normalized;
	}

	double FEvaluator::RawAt(double PatchX, double PatchY) const
	{
		using namespace ArcTerrainNoiseImpl_5f3a;

		double SampleX = PatchX;
		double SampleY = PatchY;

		if (WarpStrengthCm > 0.0)
		{
			const double OffsetX = GradientNoise2D(
				PatchX * WarpFrequency + 17.3, PatchY * WarpFrequency + 5.1, WarpSeedX);
			const double OffsetY = GradientNoise2D(
				PatchX * WarpFrequency - 8.7, PatchY * WarpFrequency + 23.9, WarpSeedY);
			SampleX += WarpStrengthCm * OffsetX;
			SampleY += WarpStrengthCm * OffsetY;
		}

		return Accumulate(SampleX, SampleY);
	}

	double FEvaluator::EvaluateCm(double PatchX, double PatchY) const
	{
		using namespace ArcTerrainNoiseImpl_5f3a;

		const double Raw = RawAt(PatchX, PatchY);
		double HeightCm = Raw * AmplitudeCm;

		if (bErosionEnabled)
		{
			const double InvTwoStep = 0.5 / DerivativeStepCm;
			const double Left = RawAt(PatchX - DerivativeStepCm, PatchY) * AmplitudeCm;
			const double Right = RawAt(PatchX + DerivativeStepCm, PatchY) * AmplitudeCm;
			const double Down = RawAt(PatchX, PatchY - DerivativeStepCm) * AmplitudeCm;
			const double Up = RawAt(PatchX, PatchY + DerivativeStepCm) * AmplitudeCm;

			const FVector3d HeightAndSlope(
				HeightCm, (Right - Left) * InvTwoStep, (Up - Down) * InvTwoStep);

			// 0.6 is upstream's: the fraction of the height range that saturates to peak or valley.
			const double FadeTarget = FMath::Clamp(Raw / 0.6, -1.0, 1.0);

			// Raw is NOT remapped to [0, 1] as upstream does for a Unity heightmap: this field is signed
			// cm about the component plane and composites Additive, so the bias would lift the stamp.
			HeightCm += ArcTerrainErosion::ErosionFilter(
				FVector2d(PatchX, PatchY), HeightAndSlope, FadeTarget, CharacteristicSlope,
				Erosion, Seed).X;
		}

		if (TerraceStepCm > 0.0)
		{
			const double Steps = HeightCm / TerraceStepCm;
			const double Shelf = FMath::FloorToDouble(Steps);
			const double Fraction = Steps - Shelf;
			const double Eased = Smoothstep01(Smoothstep01(Fraction));
			HeightCm = (Shelf + FMath::Lerp(Fraction, Eased, TerraceSharpness)) * TerraceStepCm;
		}

		return FMath::IsFinite(HeightCm) ? HeightCm : 0.0;
	}
}
