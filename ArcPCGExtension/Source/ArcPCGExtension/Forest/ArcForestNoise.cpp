// Copyright Lukasz Baran. All Rights Reserved.

#include "Forest/ArcForestNoise.h"

#include "Templates/TypeHash.h"

namespace ArcForestNoiseLocal
{
	/**
	 * The engine's lattice index is masked with & 255 (FMath::PerlinNoise2D in UnrealMath.cpp), so the
	 * noise tiles with a period of 256 lattice units. Seed offsets are drawn from [0, 256) because
	 * anything outside merely aliases back into that window.
	 */
	static constexpr float LatticePeriod = 256.0f;

	/**
	 * Added once per octave on top of the seed offset. Deliberately NON-INTEGER: PerlinNoise2D is
	 * exactly zero at every lattice point, so with a shared offset and an integer Lacunarity the
	 * octaves' zero sets would partly coincide and stack into a visible grid. A fractional per-octave
	 * shift makes that coincidence generic-position instead of systematic. Values are arbitrary,
	 * mutually incommensurate, and MUST NOT change without accepting that every field reshuffles.
	 */
	static constexpr float OctaveOffsetStrideX = 37.719f;
	static constexpr float OctaveOffsetStrideY = 61.283f;

	/** How one raw Perlin sample is shaped BEFORE it enters the amplitude-weighted octave sum. */
	enum class EOctaveShape : uint8
	{
		/** n, in [-1,1]. */
		Signed,
		/** |n|, in [0,1]. */
		AbsValue,
		/** (1 - |n|)^2, in [0,1]. */
		RidgeSquared
	};

	/** How the amplitude-normalised octave sum is mapped into [0,1]. */
	enum class ESumRemap : uint8
	{
		/** Sum is in [-1,1]: 0.5 * (x + 1). */
		SignedToUnit,
		/** Sum is already in [0,1] (only valid for a shape whose own range is [0,1]). */
		PassThrough,
		/** Sum is in [-1,1]: |x|. */
		AbsOfSum
	};

	/**
	 * A noise type is exactly a (per-octave shape, post-sum remap) pair. The invariant that keeps the
	 * output in [0,1] for every pair: the normalised sum is a convex combination of the shaped octave
	 * values, so it stays inside the shape's own range, and each remap maps that range onto [0,1].
	 * PassThrough therefore REQUIRES a shape whose range is [0,1].
	 */
	struct FNoiseKernel
	{
		EOctaveShape Shape = EOctaveShape::Signed;
		ESumRemap Remap = ESumRemap::SignedToUnit;
	};

	/** THE extension point: a new EArcForestNoiseType is one enum case plus one arm here. */
	FNoiseKernel KernelFor(EArcForestNoiseType Type)
	{
		switch (Type)
		{
		case EArcForestNoiseType::Perlin:
			return FNoiseKernel{ EOctaveShape::Signed, ESumRemap::SignedToUnit };

		case EArcForestNoiseType::Ridged:
			return FNoiseKernel{ EOctaveShape::RidgeSquared, ESumRemap::PassThrough };

		case EArcForestNoiseType::Billow:
			return FNoiseKernel{ EOctaveShape::AbsValue, ESumRemap::PassThrough };

		case EArcForestNoiseType::Turbulence:
			// Abs of the SUM, not the sum of abs — that is the whole difference from Billow.
			return FNoiseKernel{ EOctaveShape::Signed, ESumRemap::AbsOfSum };
		}

		// Unreachable for a valid enum value. Do NOT rely on the compiler for this: -Wswitch would
		// catch a new enumerator under clang, but MSVC's equivalent (C4062) is off at UE's warning
		// level on the Win64 toolchain this project builds with (UBT sets
		// SwitchUnhandledEnumeratorWarningLevel to Off), so a new type would silently fall through
		// here and behave as Perlin.
		// checkNoEntry() makes that loud in any DO_CHECK configuration — Debug, Development, and the
		// editor, i.e. everywhere a new type would actually be added and tested. It is COMPILED OUT
		// in Shipping/Test, where this path still falls back to the Perlin kernel rather than
		// crashing. That is the deliberate trade: fail loudly where a developer will see it, degrade
		// quietly in a cooked build rather than taking the game down over a habitat field.
		checkNoEntry();
		return FNoiseKernel{ EOctaveShape::Signed, ESumRemap::SignedToUnit };
	}

	float ShapeOctave(EOctaveShape Shape, float RawNoise)
	{
		switch (Shape)
		{
		case EOctaveShape::Signed:
			return RawNoise;

		case EOctaveShape::AbsValue:
			return FMath::Abs(RawNoise);

		case EOctaveShape::RidgeSquared:
			return FMath::Square(1.0f - FMath::Abs(RawNoise));
		}

		return RawNoise;
	}

	float RemapSum(ESumRemap Remap, float NormalisedSum)
	{
		switch (Remap)
		{
		case ESumRemap::SignedToUnit:
			return 0.5f * (NormalisedSum + 1.0f);

		case ESumRemap::PassThrough:
			return NormalisedSum;

		case ESumRemap::AbsOfSum:
			return FMath::Abs(NormalisedSum);
		}

		return NormalisedSum;
	}

	/**
	 * Seed as a position offset — the standard technique for a fixed-permutation gradient noise, since
	 * FMath::PerlinNoise2D exposes no seed. One hash of (SolveSeed, SeedOffset) feeds two further
	 * hashes so X and Y are independent, each mapped into [0, LatticePeriod).
	 *
	 * 24 bits per axis gives an offset granularity of ~1.5e-5 lattice units, so distinct inputs land
	 * on genuinely different sub-lattices. Two offsets are typically tens of lattice units apart and
	 * Perlin decorrelates beyond ~1 lattice unit, hence the near-zero correlation between fields with
	 * different SeedOffset values.
	 */
	FVector2f DeriveSeedOffset(int32 SolveSeed, int32 SeedOffset)
	{
		const uint32 Combined = HashCombine(static_cast<uint32>(SolveSeed), static_cast<uint32>(SeedOffset));
		const uint32 HashX = HashCombine(Combined, 0x9E3779B9u);
		const uint32 HashY = HashCombine(Combined, 0x85EBCA6Bu);

		static constexpr uint32 Mask24 = 0x00FFFFFFu;
		static constexpr float InvMask24 = 1.0f / static_cast<float>(Mask24 + 1u);

		const float OffsetX = static_cast<float>(HashX & Mask24) * InvMask24 * LatticePeriod;
		const float OffsetY = static_cast<float>(HashY & Mask24) * InvMask24 * LatticePeriod;
		return FVector2f(OffsetX, OffsetY);
	}
}

namespace ArcForestNoise
{
	float SampleNoise01(const FArcForestNoiseParams& Params, FVector2f Pos, int32 SolveSeed)
	{
		using namespace ArcForestNoiseLocal;

		// A non-positive frequency would collapse every octave onto the same lattice point (a constant),
		// so answer with the neutral mid value explicitly instead of relying on that accident.
		if (!(Params.Frequency > 0.0f))
		{
			return 0.5f;
		}

		const int32 Octaves = FMath::Clamp(Params.Octaves, 1, 8);
		const float Lacunarity = FMath::Clamp(Params.Lacunarity, 1.0f, 8.0f);
		const float Gain = FMath::Clamp(Params.Gain, 0.0f, 1.0f);

		const FNoiseKernel Kernel = KernelFor(Params.Type);
		const FVector2f SeedOffset = DeriveSeedOffset(SolveSeed, Params.SeedOffset);

		double WeightedSum = 0.0;
		double AmplitudeSum = 0.0;
		float Amplitude = 1.0f;
		float Frequency = Params.Frequency;

		for (int32 Octave = 0; Octave < Octaves; ++Octave)
		{
			const float OctaveOffsetX = SeedOffset.X + (static_cast<float>(Octave) * OctaveOffsetStrideX);
			const float OctaveOffsetY = SeedOffset.Y + (static_cast<float>(Octave) * OctaveOffsetStrideY);

			const FVector2D SamplePos(
				(static_cast<double>(Pos.X) * static_cast<double>(Frequency)) + static_cast<double>(OctaveOffsetX),
				(static_cast<double>(Pos.Y) * static_cast<double>(Frequency)) + static_cast<double>(OctaveOffsetY));

			const float RawNoise = FMath::PerlinNoise2D(SamplePos);

			// AmplitudeSum accumulates the amplitudes ACTUALLY used, in the same loop, so the divisor can
			// never disagree with the numerator (a closed-form sum(Gain^i) computed separately is exactly
			// where this goes subtly wrong when Octaves or Gain is clamped).
			WeightedSum += static_cast<double>(Amplitude) * static_cast<double>(ShapeOctave(Kernel.Shape, RawNoise));
			AmplitudeSum += static_cast<double>(Amplitude);

			Amplitude *= Gain;
			Frequency *= Lacunarity;
		}

		// AmplitudeSum >= 1 always: the first octave's amplitude is 1 and Gain is clamped non-negative.
		const float Normalised = static_cast<float>(WeightedSum / AmplitudeSum);

		return FMath::Clamp(RemapSum(Kernel.Remap, Normalised), 0.0f, 1.0f);
	}
}
