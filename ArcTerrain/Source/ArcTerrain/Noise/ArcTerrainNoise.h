// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Noise/ArcTerrainNoiseParams.h"

/**
 * CPU procedural noise for terrain stamps. There is no GPU counterpart and none is intended: the
 * composite consumes rasters, so a noise stamp bakes this into an FArcTerrainBakedField once and the
 * shader never sees a noise function.
 */
namespace ArcTerrainNoise
{
	/** The authored knobs reduced to the values the evaluator actually uses. Anything keying the
	 *  raster's identity MUST hash THIS, not the authored struct: two authored values that clamp to
	 *  one effective value produce one raster, and two ids for it would re-bake the footprint for
	 *  nothing. */
	ARCTERRAIN_API FArcTerrainNoiseParams Sanitize(const FArcTerrainNoiseParams& Params);

	/** Integer hash, NOT a permutation table: FMath::PerlinNoise's 256-entry table (UnrealMath.cpp)
	 *  makes the field repeat every 256 units on each axis, which a multi-octave terrain shows as
	 *  visible tiling, and it carries no seed. This mixes the cell coordinates and the seed directly,
	 *  so the field has no period and reseeding actually produces a different landscape.
	 *  Cells are int64 because the top octave of a large footprint at a small feature size passes
	 *  INT32_MAX (1e6 cm / 1 cm * 4^11), and a wrap there would reintroduce the tiling this exists to
	 *  avoid; both halves are folded so the truncation happens nowhere but here. */
	inline uint32 HashCell(int64 CellX, int64 CellY, uint32 Seed)
	{
		const uint64 UnsignedX = static_cast<uint64>(CellX);
		const uint64 UnsignedY = static_cast<uint64>(CellY);

		uint32 Hash = Seed;
		Hash ^= static_cast<uint32>(UnsignedX) * 0x9E3779B1u;
		Hash ^= static_cast<uint32>(UnsignedX >> 32) * 0x7FEB352Du;
		Hash ^= static_cast<uint32>(UnsignedY) * 0x85EBCA77u;
		Hash ^= static_cast<uint32>(UnsignedY >> 32) * 0x846CA68Bu;
		Hash ^= Hash >> 15;
		Hash *= 0x2C1B3C6Du;
		Hash ^= Hash >> 12;
		Hash *= 0x297A2D39u;
		Hash ^= Hash >> 15;
		return Hash;
	}

	/** The clamp is the domain guard on the cast, not a taste choice: an out-of-range double -> int64
	 *  conversion is UB, and on x64 it yields INT64_MIN, which collapses the whole field to one cell. */
	inline int64 FloorToCell(double Value)
	{
		return static_cast<int64>(FMath::Clamp(FMath::FloorToDouble(Value), -9.0e18, 9.0e18));
	}

	/**
	 * The per-field constants derived once from FArcTerrainNoiseParams, so the per-texel call does no
	 * division and no enum-to-constant work. Construct one per field build, then call EvaluateCm for
	 * every texel.
	 */
	struct ARCTERRAIN_API FEvaluator
	{
		/** InDerivativeStepCm is a FLOOR on the slope-sampling distance, not the distance: erosion
		 *  raises it to a fraction of its channel spacing. Non-positive falls back to a fraction of
		 *  FeatureSizeCm. */
		explicit FEvaluator(const FArcTerrainNoiseParams& Params, double InDerivativeStepCm = 0.0);

		/** Signed height in cm at a stamp-local XY in cm. Pure and re-entrant — BuildNoiseField calls
		 *  it from ParallelFor, and it touches no global state. */
		double EvaluateCm(double PatchX, double PatchY) const;

	private:
		/** Distinguishes the delegated-to constructor, whose argument has already been through
		 *  Sanitize — the public one cannot clamp in its own initializer list. */
		struct FSanitizedTag {};
		FEvaluator(const FArcTerrainNoiseParams& SanitizedParams, double InDerivativeStepCm, FSanitizedTag);

		/** One octave of the configured basis, normalized to signed [-1, 1]. */
		double Basis(double X, double Y, uint32 OctaveSeed) const;

		/** The octave loop, normalized to signed [-1, 1] by the amplitude sum. */
		double Accumulate(double X, double Y) const;

		/** Warp plus Accumulate at a stamp-local XY in cm, still signed [-1, 1]. */
		double RawAt(double PatchX, double PatchY) const;

		EArcTerrainNoiseBasis BasisKind;
		EArcTerrainNoiseFractal FractalMode;
		uint32 Seed;
		uint32 WarpSeedX;
		uint32 WarpSeedY;
		int32 Octaves;
		double BaseFrequency;
		double Lacunarity;
		double Gain;
		double AmplitudeCm;
		double InvAmplitudeSum;
		double WarpStrengthCm;
		double WarpFrequency;
		double TerraceStepCm;
		double TerraceSharpness;
		double DerivativeStepCm;
		double CharacteristicSlope;
		bool bErosionEnabled;
		FArcTerrainErosionParams Erosion;
	};
}
