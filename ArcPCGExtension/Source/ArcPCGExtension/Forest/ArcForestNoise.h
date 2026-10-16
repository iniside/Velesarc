// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcForestNoise.generated.h"

/**
 * Analytic fBm noise for the forest solver's habitat fields — today the MOISTURE axis.
 *
 * WHY NOISE AND NOT A CHANNEL. Nothing about habitat is authored per-map: drop a PCG volume anywhere
 * a surface can be sampled and the habitat must generate itself. Moisture in particular is NOT a
 * painted channel and is NOT derived from the height grid. It replaces a curvature/Laplacian field
 * that was dropped because a discrete Laplacian's magnitude scales as 1/dx^2 while the morphology
 * grid is 200 cm/cell clamped by MaxGridDim = 256 (see the RESOLVABLE-FEATURE LIMIT note in
 * Forest/ArcForestTerrainFields.h), so it resolved only landform-scale bowls. Noise has no
 * resolution problem AT ALL because it is sampled analytically at the exact position asked for —
 * there is no grid, no bake, and no game-thread cost.
 *
 * PURITY: UObject-free and worker-safe. FMath::PerlinNoise2D (Core, UnrealMathUtility.h:2472) is a
 * pure static function over a const permutation table — no state, no allocation, deterministic, and
 * safe to call from the UE::Tasks worker the solve runs on (see the PURITY contracts at
 * ArcForestSimulator.h and ArcForestPatchField.h). No PCG types are pulled in here, so the solver
 * core keeps including this header freely.
 *
 * The reflection macros are the ONLY concession: FArcForestNoiseParams is a USTRUCT so the node can
 * expose the whole parameter block as one nested property in the details panel instead of six
 * flattened scalars (Forest/ArcForestSimTypes.h already carries a .generated.h for the same reason,
 * so this costs nothing new). A USTRUCT is not a UObject: it is still POD-by-value, still copied on
 * the game thread into FArcForestHabitatInputs, and still read-only on the worker.
 */

/**
 * Which fBm variant SampleNoise01 evaluates.
 *
 * EXTENSION POINT: adding a type is one enum case here plus one arm in the single switch in
 * ArcForestNoiseLocal::KernelFor (ArcForestNoise.cpp). Nothing else changes — the octave loop, the
 * amplitude normalisation and the final clamp are shared by every type.
 *
 * These names are muddled across the industry (Perlin's original "turbulence" is what libnoise calls
 * "billow"), so the formula each one means HERE is spelled out per entry and is normative. n_i is a
 * raw FMath::PerlinNoise2D sample in [-1,1] for octave i, A_i = Gain^i, and S = sum(A_i).
 */
UENUM(BlueprintType)
enum class EArcForestNoiseType : uint8
{
	/** Signed fBm: sum(A_i * n_i)/S in [-1,1], remapped to [0,1]. Smooth blobs, symmetric about 0.5. */
	Perlin UMETA(DisplayName = "Perlin (signed fBm)"),

	/**
	 * Ridged multifractal: sum(A_i * (1 - |n_i|)^2)/S, already in [0,1]. Sharp high ridges over broad
	 * low basins; mean sits HIGH (~0.64 at 3 octaves — that is the mean's position, not the sign of
	 * the skewness coefficient, which is mildly negative). The square is load-bearing, not
	 * decoration: the bare complement (1 - |n_i|) is an AFFINE function of |n_i|, so with amplitude
	 * normalisation it would come out EXACTLY 1 - Billow (correlation -1) and this would not be a
	 * distinct type at all.
	 *
	 * Honest limit on "distinct": squaring breaks the exact identity, but Ridged and Billow still
	 * pick out nearly the same regions with opposite sign — measured corr(Ridged, Billow) = -0.99 at
	 * 3 octaves, with mean|difference| ~0.44. They are distinct in VALUE and DISTRIBUTION, not
	 * spatially independent. Do not key two species off Ridged and Billow expecting uncorrelated
	 * habitats; use SeedOffset for that. Perlin is near-independent of all three (|corr| <= 0.05).
	 */
	Ridged UMETA(DisplayName = "Ridged (sharp ridges)"),

	/** Billow: sum(A_i * |n_i|)/S, already in [0,1]. Bulbous clumps; mean sits LOW (~0.21 at 3 octaves — the mean's position, not the sign of the skewness coefficient, which is positive). Creases at the zero set of every octave. */
	Billow UMETA(DisplayName = "Billow (bulbous clumps)"),

	/**
	 * Turbulence: |sum(A_i * n_i)/S| in [0,1] — the octave sum is taken FIRST and the absolute value
	 * applied to the SUM, which is what makes it non-affine w.r.t. Billow (abs-of-sum vs sum-of-abs).
	 * Long connected creases along the zero set of the signed fBm rather than per-octave creases.
	 * NOTE: at Octaves == 1 there is only one term, so abs-of-sum and sum-of-abs coincide and
	 * Turbulence is identical to Billow. That is arithmetic, not a defect — use >= 2 octaves to tell
	 * them apart.
	 */
	Turbulence UMETA(DisplayName = "Turbulence (creased)")
};

/** Authored parameters for one analytic noise field. POD; copied by value onto the worker. */
USTRUCT(BlueprintType)
struct FArcForestNoiseParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Noise", meta = (Tooltip = "Which fBm variant to evaluate: Perlin (smooth blobs), Ridged (sharp high ridges), Billow (bulbous clumps) or Turbulence (long creases)."))
	EArcForestNoiseType Type = EArcForestNoiseType::Perlin;

	/**
	 * Cycles per CENTIMETRE, matching the solver's units (every length in the forest pipeline is cm).
	 * One noise lattice cell spans 1/Frequency cm and a feature is roughly one to two cells, so
	 * 0.001 gives ~10 m patches, 0.0001 gives ~100 m regions and 0.01 gives ~1 m mottling.
	 * <= 0 is not an error: it yields a constant 0.5 field (see SampleNoise01).
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Noise", meta = (ClampMin = "0.0", Tooltip = "Cycles per cm: 0.001 is roughly a 10 m patch, 0.0001 a 100 m region, 0.01 a 1 m mottle. Zero or less gives a flat 0.5 field."))
	float Frequency = 0.001f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Noise", meta = (ClampMin = "1", ClampMax = "8", Tooltip = "How many octaves are summed; more octaves add finer detail but flatten the realised spread. Clamped to 1-8."))
	int32 Octaves = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Noise", meta = (ClampMin = "1.0", ClampMax = "8.0", Tooltip = "Frequency multiplier per octave; 2.0 doubles the detail each octave. Clamped to 1-8."))
	float Lacunarity = 2.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Noise", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Amplitude multiplier per octave; 0.5 halves each octave's contribution. Clamped to 0-1."))
	float Gain = 0.5f;

	/**
	 * Decorrelates this field from every other field sharing the same solve seed. Two different
	 * SeedOffset values must give statistically independent fields — see the SEEDING note on
	 * SampleNoise01 — so extra habitat axes can be added later without them all tracking moisture.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Noise", meta = (Tooltip = "Per-field salt: two different values give independent fields from the same solve seed. Change it to reshuffle this field without touching the sim seed."))
	int32 SeedOffset = 0;
};

namespace ArcForestNoise
{
	/**
	 * Evaluate the field at a world XY position (cm). Result is in [0,1], always finite, and depends
	 * on nothing but the arguments — same arguments, same value, on any thread, in any order.
	 *
	 * AMPLITUDE NORMALISATION (the part most likely to be subtly wrong). The octave sum is divided by
	 * the amplitude sum S = sum(Gain^i) ACCUMULATED IN THE SAME LOOP as the sum itself, so the
	 * divisor can never disagree with the amplitudes actually used. sum(A_i * v_i)/S is a CONVEX
	 * COMBINATION of the per-octave values, therefore it lies inside the range of those values
	 * exactly, for ANY Octaves and ANY Gain. That is what makes a species response curve authored
	 * against 3 octaves still mean the same thing at 8; without it, the raw sum's range grows with
	 * the octave count and every authored curve silently rescales.
	 *
	 * What normalisation does NOT do — and a test must not assume otherwise: it fixes the theoretical
	 * BOUNDS, not the realised spread. Summing more octaves averages more roughly-independent samples,
	 * so the distribution narrows toward 0.5 as Octaves rises (a standard fBm property). Perlin
	 * gradient noise also rarely approaches |1| in practice, so a Perlin field's realised values
	 * cluster well inside [0,1] even at one octave. Both are inherent, not normalisation bugs; the
	 * invariant that matters is that the mapping is Octaves/Gain-independent in range.
	 *
	 * SEEDING. FMath::PerlinNoise2D takes NO seed, so the seed enters as a POSITION OFFSET — the
	 * standard technique for a fixed-permutation gradient noise. The offset is derived from
	 * HashCombine(uint32(SolveSeed), uint32(SeedOffset)) and lands in [0,256) per axis because the
	 * engine's lattice index is masked with & 255 (UnrealMath.cpp), i.e. the noise TILES with a
	 * period of 256 lattice units, so offsets outside that range merely alias. Two offsets drawn that
	 * way are typically tens of lattice units apart, and Perlin samples decorrelate beyond ~1 lattice
	 * unit, so distinct SeedOffset values give fields with near-zero correlation.
	 * (Practical consequence of the same tiling: the field repeats every 256/Frequency cm — 2.56 km
	 * at the default 0.001 — which is far beyond any single PCG volume.)
	 *
	 * GUARDS: Octaves is clamped to [1,8], Lacunarity to [1,8], Gain to [0,1] (so S >= 1 and the
	 * division is always safe), and a Frequency <= 0 returns a flat 0.5 rather than collapsing every
	 * octave onto the same lattice point.
	 */
	ARCPCGEXTENSION_API float SampleNoise01(const FArcForestNoiseParams& Params, FVector2f Pos, int32 SolveSeed);
}
