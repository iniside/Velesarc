// Copyright Lukasz Baran. All Rights Reserved.

#include "Ocean/ArcOceanWaves.h"

#include "Math/RandomStream.h"

namespace ArcOceanWavesBuildLocal
{
	constexpr int32 DirectionAndPhaseSeed = 0x0C3A11E5;
	constexpr int32 ShallowBandSeed = 0x2B0E5A17;
	constexpr int32 MaxShallowWaves = 8;

	using FStratumOrder = TArray<int32, TInlineAllocator<ArcOcean::MaxWaves>>;

	struct FBandSpec
	{
		int32 WaveCount = 0;
		float LongestWavelengthCm = 0.0f;
		float WavelengthRatio = 0.0f;
		float AmplitudeRatio = 0.0f;
		float WindAngle = 0.0f;
		float SpreadRadians = 0.0f;
		float BandSteepness = 0.0f;
		uint32 BandFlags = 0;
	};

	void BuildStratumOrder(const FRandomStream& InStream, int32 InCount, FStratumOrder& OutOrder)
	{
		OutOrder.Reset(InCount);
		for (int32 Index = 0; Index < InCount; ++Index)
		{
			OutOrder.Add(Index);
		}

		for (int32 Index = InCount - 1; Index > 0; --Index)
		{
			OutOrder.Swap(Index, InStream.RandHelper(Index + 1));
		}
	}

	float ContinuationWavelengthCm(float InLongestWavelengthCm, float InWavelengthRatio, int32 InWaveCount)
	{
		float Wavelength = InLongestWavelengthCm;
		for (int32 WaveIndex = 0; WaveIndex < InWaveCount; ++WaveIndex)
		{
			Wavelength *= InWavelengthRatio;
		}

		return Wavelength;
	}

	void EmitBand(const FBandSpec& InSpec, const FRandomStream& InStream, TArray<FArcGerstnerWave>& OutWaves)
	{
		FStratumOrder StratumOrder;
		BuildStratumOrder(InStream, InSpec.WaveCount, StratumOrder);

		// Steepness is the per-wave Qi*ki*Ai, so splitting the authored Q across N waves keeps their sum at Q <= 1 and makes self-intersection impossible for any N.
		const float PerWaveSteepness = InSpec.WaveCount > 0
			? InSpec.BandSteepness / static_cast<float>(InSpec.WaveCount)
			: 0.0f;

		for (int32 WaveIndex = 0; WaveIndex < InSpec.WaveCount; ++WaveIndex)
		{
			const float Wavelength = ContinuationWavelengthCm(InSpec.LongestWavelengthCm, InSpec.WavelengthRatio, WaveIndex);
			const float Stratum = InSpec.WaveCount > 1
				? ((static_cast<float>(StratumOrder[WaveIndex]) + InStream.FRand()) / static_cast<float>(InSpec.WaveCount)) * 2.0f - 1.0f
				: 0.0f;
			const float Angle = InSpec.WindAngle + Stratum * InSpec.SpreadRadians;

			FArcGerstnerWave& Wave = OutWaves.AddDefaulted_GetRef();
			Wave.Direction = FVector2f(FMath::Cos(Angle), FMath::Sin(Angle));
			Wave.Wavelength = Wavelength;
			Wave.Amplitude = Wavelength * InSpec.AmplitudeRatio;
			Wave.Steepness = PerWaveSteepness;
			Wave.Phase = InStream.FRand() * UE_TWO_PI;
			Wave.BandFlags = InSpec.BandFlags;
		}
	}
}

void ArcOceanWaves::ResolveBandLimits(const FArcOceanWaveSettings& InSettings, FArcOceanBandLimits& OutLimits)
{
	OutLimits.SwellNumWaves = FMath::Clamp(InSettings.NumWaves, 1, ArcOcean::MaxWaves);
	OutLimits.SwellLongestWavelengthCm = FMath::Max(InSettings.LongestWavelengthCm, 1.0f);
	OutLimits.SwellWavelengthRatio = FMath::Clamp(InSettings.WavelengthRatio, 0.125f, 0.875f);
	OutLimits.SwellSteepness = FMath::Clamp(InSettings.Steepness, 0.0f, 1.0f);

	OutLimits.ShallowNumWaves = FMath::Min(
		FMath::Clamp(InSettings.ShallowBand.NumWaves, 0, ArcOceanWavesBuildLocal::MaxShallowWaves),
		ArcOcean::MaxWaves - OutLimits.SwellNumWaves);

	OutLimits.ShallowCeilingCm = FMath::Max(
		ArcOceanWavesBuildLocal::ContinuationWavelengthCm(
			OutLimits.SwellLongestWavelengthCm, OutLimits.SwellWavelengthRatio, OutLimits.SwellNumWaves),
		1.0f);
	OutLimits.ShallowLongestWavelengthCm = FMath::Clamp(
		InSettings.ShallowBand.LongestWavelengthCm, 1.0f, OutLimits.ShallowCeilingCm);
	OutLimits.ShallowSteepness = FMath::Min(
		FMath::Clamp(InSettings.ShallowBand.Steepness, 0.0f, 1.0f),
		FMath::Max(ArcOcean::ARC_OCEAN_MAX_STEEPNESS_SUM - OutLimits.SwellSteepness, 0.0f));
}

void ArcOceanWaves::BuildWaveSet(const FArcOceanWaveSettings& InSettings, TArray<FArcGerstnerWave>& OutWaves)
{
	FArcOceanBandLimits BandLimits;
	ArcOceanWaves::ResolveBandLimits(InSettings, BandLimits);

	OutWaves.Reset(BandLimits.SwellNumWaves + BandLimits.ShallowNumWaves);

	FVector2D WindDirection = InSettings.WindDirection;
	if (!WindDirection.Normalize())
	{
		WindDirection = FVector2D(1.0, 0.0);
	}

	const float WindAngle = static_cast<float>(FMath::Atan2(WindDirection.Y, WindDirection.X));

	ArcOceanWavesBuildLocal::FBandSpec SwellSpec;
	SwellSpec.WaveCount = BandLimits.SwellNumWaves;
	SwellSpec.LongestWavelengthCm = BandLimits.SwellLongestWavelengthCm;
	SwellSpec.WavelengthRatio = BandLimits.SwellWavelengthRatio;
	SwellSpec.AmplitudeRatio = FMath::Clamp(InSettings.WavelengthToAmplitude, 0.0009765625f, 0.125f);
	SwellSpec.WindAngle = WindAngle;
	SwellSpec.SpreadRadians = FMath::DegreesToRadians(FMath::Clamp(InSettings.SpreadDeg, 0.0f, 90.0f));
	SwellSpec.BandSteepness = BandLimits.SwellSteepness;
	SwellSpec.BandFlags = 0;

	const FRandomStream SwellStream(ArcOceanWavesBuildLocal::DirectionAndPhaseSeed);

	// Waves[0] is indexed from outside this module (BreakerCrestCm and the foam bore direction), so the swell owns the head and every other band appends to the tail.
	ArcOceanWavesBuildLocal::EmitBand(SwellSpec, SwellStream, OutWaves);

	if (BandLimits.ShallowNumWaves <= 0)
	{
		return;
	}

	ArcOceanWavesBuildLocal::FBandSpec ShallowSpec;
	ShallowSpec.WaveCount = BandLimits.ShallowNumWaves;
	ShallowSpec.LongestWavelengthCm = BandLimits.ShallowLongestWavelengthCm;
	ShallowSpec.WavelengthRatio = FMath::Clamp(InSettings.ShallowBand.WavelengthRatio, 0.125f, 0.875f);
	ShallowSpec.AmplitudeRatio = FMath::Clamp(InSettings.ShallowBand.WavelengthToAmplitude, 0.0009765625f, 0.125f);
	ShallowSpec.WindAngle = WindAngle;
	ShallowSpec.SpreadRadians = FMath::DegreesToRadians(FMath::Clamp(InSettings.ShallowBand.SpreadDeg, 0.0f, 90.0f));
	ShallowSpec.BandSteepness = BandLimits.ShallowSteepness;
	ShallowSpec.BandFlags = ArcOcean::ShallowBandFlag;

	const FRandomStream ShallowStream(ArcOceanWavesBuildLocal::ShallowBandSeed);
	ArcOceanWavesBuildLocal::EmitBand(ShallowSpec, ShallowStream, OutWaves);
}

void ArcOcean::PackWaveParams(const FArcOceanWaveSettings& InSettings, FArcOceanWaveFields& OutFields)
{
	TArray<FArcGerstnerWave> Waves;
	ArcOceanWaves::BuildWaveSet(InSettings, Waves);

	for (int32 SlotIndex = 0; SlotIndex < ArcOcean::MaxWaves * 2; ++SlotIndex)
	{
		OutFields.Waves[SlotIndex] = FVector4f::Zero();
	}

	const int32 WaveCount = FMath::Min(Waves.Num(), ArcOcean::MaxWaves);
	for (int32 WaveIndex = 0; WaveIndex < WaveCount; ++WaveIndex)
	{
		const FArcGerstnerWave& Wave = Waves[WaveIndex];
		OutFields.Waves[WaveIndex * 2 + 0] = FVector4f(Wave.Direction.X, Wave.Direction.Y, Wave.Wavelength, Wave.Amplitude);
		OutFields.Waves[WaveIndex * 2 + 1] = FVector4f(Wave.Steepness, Wave.Phase, static_cast<float>(Wave.BandFlags), 0.0f);
	}

	OutFields.WaveCount = WaveCount;
}

namespace ArcOceanWavesSolveLocal
{
	constexpr float GravityCmS2 = 980.0f;
	constexpr float WorldUnitCm = 1.0f;
	constexpr float BreakerHeightRatio = 0.78f;
	constexpr float FrameEpsilon = 1e-4f;
	constexpr float ResidualToleranceCm = 1.0f;
	constexpr int32 MaxNewtonIterations = 6;

	struct FDepthLimitedWave
	{
		FVector2f Direction = FVector2f::ZeroVector;
		float WaveNumber = 0.0f;
		float AngularFrequency = 0.0f;
		float Amplitude = 0.0f;
		float Steepness = 0.0f;
		float Phase = 0.0f;
		uint32 BandFlags = 0;
	};

	using FDepthLimitedWaveSet = TArray<FDepthLimitedWave, TInlineAllocator<ArcOcean::MaxWaves>>;

	struct FSurfaceSample
	{
		FVector2f DisplacementXY = FVector2f::ZeroVector;
		float DisplacementZ = 0.0f;
		FVector2f SlopeCos = FVector2f::ZeroVector;
		float SumXX = 0.0f;
		float SumYY = 0.0f;
		float SumXY = 0.0f;
		float VerticalRate = 0.0f;
	};

	float DepthScale(float InDeepAmplitudeCm, float InDepthCm)
	{
		const float BreakingAmplitudeCm = 0.5f * BreakerHeightRatio * FMath::Max(InDepthCm, 0.0f);
		return FMath::Min(FMath::Max(BreakingAmplitudeCm / FMath::Max(InDeepAmplitudeCm, WorldUnitCm), 0.0f), 1.0f);
	}

	float ShallowScale(float InDeepAmplitudeCm, float InWavelengthCm, float InDepthCm)
	{
		if (InWavelengthCm > 0.0f)
		{
			const float ShallowFraction = 1.0f - FMath::Min(FMath::Max(2.0f * InDepthCm / InWavelengthCm, 0.0f), 1.0f);
			const float BreakingAmplitudeCm = 0.5f * BreakerHeightRatio * FMath::Max(InDepthCm, 0.0f);
			const float AmplitudeCm = FMath::Lerp(InDeepAmplitudeCm, BreakingAmplitudeCm, ShallowFraction);
			return AmplitudeCm / FMath::Max(InDeepAmplitudeCm, WorldUnitCm);
		}

		return 0.0f;
	}

	float ShorewardKH(float InWaveNumber, float InDepthCm)
	{
		return FMath::Min(1.0f, InWaveNumber * FMath::Max(InDepthCm, 0.0f));
	}

	float ShorewardWeight(FVector2f InDirection, float InWaveNumber, float InDepthCm, FVector2f InShoreward)
	{
		if (InShoreward.IsZero())
		{
			return 1.0f;
		}

		const float KH = ShorewardKH(InWaveNumber, InDepthCm);
		const float Alignment = FVector2f::DotProduct(InDirection, InShoreward);
		const float ConeWeight = FMath::Clamp(
			(Alignment * FMath::Abs(Alignment) - (1.0f - KH)) / FMath::Max(KH, 1e-4f), 0.0f, 1.0f);
		return FMath::Lerp(1.0f, ConeWeight, 1.0f - KH);
	}

	float DepthLimitWaves(TConstArrayView<FArcGerstnerWave> InWaves, float InDepthCm, float InEffectiveCellSize,
		FVector2f InShoreward, FDepthLimitedWaveSet& OutWaves)
	{
		OutWaves.Reset();

		const float SafeCellSize = FMath::Max(InEffectiveCellSize, 1e-6f);

		float SwellWaveCount = 0.0f;
		float SwellWeightSum = 0.0f;
		float SwellSteepnessSum = 0.0f;
		float ShallowWaveCount = 0.0f;
		float ShallowWeightSum = 0.0f;
		float ShallowSteepnessSum = 0.0f;

		for (const FArcGerstnerWave& Wave : InWaves)
		{
			if (Wave.Wavelength <= 0.0f)
			{
				continue;
			}

			const bool bShallowBand = (Wave.BandFlags & ArcOcean::ShallowBandFlag) != 0u;
			const float ShoalingScale = bShallowBand
				? ShallowScale(Wave.Amplitude, Wave.Wavelength, InDepthCm)
				: 1.0f;

			FDepthLimitedWave& Out = OutWaves.AddDefaulted_GetRef();
			Out.Direction = Wave.Direction;
			Out.WaveNumber = 2.0f * UE_PI / Wave.Wavelength;
			Out.AngularFrequency = FMath::Sqrt(GravityCmS2 * Out.WaveNumber);
			Out.BandFlags = Wave.BandFlags;

			const float Weight = ShorewardWeight(Out.Direction, Out.WaveNumber, InDepthCm, InShoreward);
			const float Attenuation = FMath::SmoothStep(2.0f * SafeCellSize, 4.0f * SafeCellSize, 2.0f * UE_PI / Out.WaveNumber);
			Out.Amplitude = Wave.Amplitude * ShoalingScale * Weight * Attenuation;
			Out.Steepness = Wave.Steepness * ShoalingScale * Weight * Attenuation;
			Out.Phase = Wave.Phase;

			if (bShallowBand)
			{
				ShallowWaveCount += 1.0f;
				ShallowWeightSum += Weight;
				ShallowSteepnessSum += Out.Steepness;
			}
			else
			{
				SwellWaveCount += 1.0f;
				SwellWeightSum += Weight;
				SwellSteepnessSum += Out.Steepness;
			}
		}

		const float SwellGain = SwellWaveCount > 0.0f ? SwellWaveCount / FMath::Max(SwellWeightSum, 1e-4f) : 1.0f;
		const float ShallowGain = ShallowWaveCount > 0.0f ? ShallowWaveCount / FMath::Max(ShallowWeightSum, 1e-4f) : 1.0f;

		const float SteepnessSum = SwellSteepnessSum * SwellGain + ShallowSteepnessSum * ShallowGain;

		// Renormalization scales amplitude together with steepness: the horizontal term is (Steepness/k)*cos with Steepness == Q*k*A, so moving one without the other rewrites the authored Q.
		const float Renormalize = ArcOcean::ARC_OCEAN_MAX_STEEPNESS_SUM
			/ FMath::Max(SteepnessSum, ArcOcean::ARC_OCEAN_MAX_STEEPNESS_SUM);

		float AmplitudeSumCm = 0.0f;
		for (FDepthLimitedWave& Out : OutWaves)
		{
			const float BandGain = (Out.BandFlags & ArcOcean::ShallowBandFlag) != 0u ? ShallowGain : SwellGain;
			const float PreCapAmplitude = Out.Amplitude * BandGain * Renormalize;
			const float BreakingCap = DepthScale(PreCapAmplitude, InDepthCm);

			Out.Amplitude = PreCapAmplitude * BreakingCap;
			Out.Steepness *= BandGain * Renormalize * BreakingCap;

			AmplitudeSumCm += Out.Amplitude;
		}

		return AmplitudeSumCm;
	}

	void Evaluate(TConstArrayView<FDepthLimitedWave> InWaves, FVector2f InBaseXY, float InTimeSeconds,
		FSurfaceSample& OutSample)
	{
		OutSample = FSurfaceSample();

		for (const FDepthLimitedWave& Wave : InWaves)
		{
			const float Theta = Wave.WaveNumber * FVector2f::DotProduct(Wave.Direction, InBaseXY)
				- Wave.AngularFrequency * InTimeSeconds + Wave.Phase;

			float SinTheta = 0.0f;
			float CosTheta = 0.0f;
			FMath::SinCos(&SinTheta, &CosTheta, Theta);

			const float WaveKA = Wave.WaveNumber * Wave.Amplitude;

			OutSample.DisplacementXY += Wave.Direction * ((Wave.Steepness / Wave.WaveNumber) * CosTheta);
			OutSample.DisplacementZ += Wave.Amplitude * SinTheta;

			OutSample.SlopeCos += Wave.Direction * (WaveKA * CosTheta);
			OutSample.SumXX += Wave.Steepness * Wave.Direction.X * Wave.Direction.X * SinTheta;
			OutSample.SumYY += Wave.Steepness * Wave.Direction.Y * Wave.Direction.Y * SinTheta;
			OutSample.SumXY += Wave.Steepness * Wave.Direction.X * Wave.Direction.Y * SinTheta;

			OutSample.VerticalRate -= Wave.Amplitude * Wave.AngularFrequency * CosTheta;
		}
	}
}

void ArcOceanWaves::SolveSurfaceAt(
	TConstArrayView<FArcGerstnerWave> InWaves,
	FVector2f ObservedXY,
	float TimeSeconds,
	float EffectiveCellSize,
	float DepthCm,
	FVector2f InShoreward,
	float& OutHeightZ,
	FVector3f& OutNormal,
	float& OutVerticalSpeed,
	float& OutAmplitudeSumCm)
{
	ArcOceanWavesSolveLocal::FDepthLimitedWaveSet LimitedWaves;
	OutAmplitudeSumCm = ArcOceanWavesSolveLocal::DepthLimitWaves(
		InWaves, DepthCm, EffectiveCellSize, InShoreward, LimitedWaves);

	FVector2f BaseXY = ObservedXY;
	ArcOceanWavesSolveLocal::FSurfaceSample Sample;
	ArcOceanWavesSolveLocal::Evaluate(LimitedWaves, BaseXY, TimeSeconds, Sample);

	// Newton, not fixed point: the authored Steepness maximum puts the sum of per-wave steepness at 1, where the fixed-point map stops being a contraction and no iteration count bounds the error.
	for (int32 Iteration = 0; Iteration < ArcOceanWavesSolveLocal::MaxNewtonIterations; ++Iteration)
	{
		const FVector2f Residual = BaseXY + Sample.DisplacementXY - ObservedXY;
		if (Residual.Size() <= ArcOceanWavesSolveLocal::ResidualToleranceCm)
		{
			break;
		}

		const float J00 = 1.0f - Sample.SumXX;
		const float J11 = 1.0f - Sample.SumYY;
		const float J01 = -Sample.SumXY;
		const float Determinant = J00 * J11 - J01 * J01;

		FVector2f Step = Residual;
		if (FMath::Abs(Determinant) > ArcOceanWavesSolveLocal::FrameEpsilon)
		{
			Step = FVector2f(
				(J11 * Residual.X - J01 * Residual.Y) / Determinant,
				(J00 * Residual.Y - J01 * Residual.X) / Determinant);
		}

		BaseXY -= Step;
		ArcOceanWavesSolveLocal::Evaluate(LimitedWaves, BaseXY, TimeSeconds, Sample);
	}

	OutHeightZ = Sample.DisplacementZ;
	OutNormal = FVector3f(
		-Sample.SlopeCos.X,
		-Sample.SlopeCos.Y,
		FMath::Max(1.0f - (Sample.SumXX + Sample.SumYY), ArcOceanWavesSolveLocal::FrameEpsilon)).GetSafeNormal();
	OutVerticalSpeed = FMath::Abs(Sample.VerticalRate);
}
