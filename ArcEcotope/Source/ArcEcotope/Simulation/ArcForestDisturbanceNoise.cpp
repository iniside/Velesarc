// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcForestDisturbanceNoise.h"

#include "Templates/TypeHash.h"

namespace ArcForestDisturbanceNoiseLocal
{
	constexpr float LatticePeriod = 256.0f;

	// Non-integer so the octaves' zero sets meet in generic position instead of stacking into a visible grid.
	constexpr float OctaveOffsetStrideX = 37.719f;
	constexpr float OctaveOffsetStrideY = 61.283f;

	constexpr float Gain = 0.5f;

	FVector2f DeriveSeedOffset(uint32 NoiseSeed)
	{
		const uint32 HashX = HashCombine(NoiseSeed, 0x9E3779B9u);
		const uint32 HashY = HashCombine(NoiseSeed, 0x85EBCA6Bu);

		constexpr uint32 Mask24 = 0x00FFFFFFu;
		constexpr float InvMask24 = 1.0f / static_cast<float>(Mask24 + 1u);

		const float OffsetX = static_cast<float>(HashX & Mask24) * InvMask24 * LatticePeriod;
		const float OffsetY = static_cast<float>(HashY & Mask24) * InvMask24 * LatticePeriod;
		return FVector2f(OffsetX, OffsetY);
	}
}

namespace UE::ArcForest
{
	float SampleDisturbanceNoise01(FVector2f LocalPos, uint32 NoiseSeed)
	{
		using namespace ArcForestDisturbanceNoiseLocal;

		const FVector2f SeedOffset = DeriveSeedOffset(NoiseSeed);

		double WeightedSum = 0.0;
		double AmplitudeSum = 0.0;
		float Amplitude = 1.0f;
		float Frequency = DisturbanceNoiseFrequencyPerCm;

		for (int32 Octave = 0; Octave < DisturbanceNoiseOctaves; ++Octave)
		{
			const float OctaveOffsetX = SeedOffset.X + (static_cast<float>(Octave) * OctaveOffsetStrideX);
			const float OctaveOffsetY = SeedOffset.Y + (static_cast<float>(Octave) * OctaveOffsetStrideY);

			const FVector2D SamplePos(
				(static_cast<double>(LocalPos.X) * static_cast<double>(Frequency)) + static_cast<double>(OctaveOffsetX),
				(static_cast<double>(LocalPos.Y) * static_cast<double>(Frequency)) + static_cast<double>(OctaveOffsetY));

			const float RawNoise = FMath::PerlinNoise2D(SamplePos);

			WeightedSum += static_cast<double>(Amplitude) * static_cast<double>(FMath::Abs(RawNoise));
			AmplitudeSum += static_cast<double>(Amplitude);

			Amplitude *= Gain;
			Frequency *= DisturbanceNoiseLacunarity;
		}

		const float Normalised = static_cast<float>(WeightedSum / AmplitudeSum);

		return FMath::Clamp(Normalised, 0.0f, 1.0f);
	}
}
