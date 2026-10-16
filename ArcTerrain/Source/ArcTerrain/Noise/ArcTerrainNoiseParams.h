// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Noise/ArcTerrainErosionFilter.h"

#include "ArcTerrainNoiseParams.generated.h"

/** Base noise family. Every entry is normalized to a signed [-1, 1] before the fractal mode runs, so
 *  the mode and the amplitude mean the same thing whichever family is picked. */
UENUM()
enum class EArcTerrainNoiseBasis : uint8
{
	Perlin UMETA(DisplayName = "Perlin (gradient)", ToolTip = "Gradient noise. Smooth rolling relief; the default terrain base."),
	Value UMETA(DisplayName = "Value", ToolTip = "Value noise. Cheaper and blockier than Perlin; useful as a fine grain layer."),
	VoronoiF1 UMETA(DisplayName = "Voronoi F1 (cells)", ToolTip = "Distance to the nearest feature point. Craters and basins."),
	VoronoiF2F1 UMETA(DisplayName = "Voronoi F2-F1 (ridges)", ToolTip = "Cell-boundary distance. Sharp ridge networks.")
};

/** How the octaves accumulate. All four are re-centred to a signed [-1, 1] output. */
UENUM()
enum class EArcTerrainNoiseFractal : uint8
{
	Standard UMETA(DisplayName = "fBm", ToolTip = "Plain sum of octaves. Rolling hills."),
	Billow UMETA(DisplayName = "Billow", ToolTip = "abs() per octave. Bulbous, cloud-like mounds."),
	Ridged UMETA(DisplayName = "Ridged", ToolTip = "1-abs() per octave. Sharp crests."),
	RidgedMulti UMETA(DisplayName = "Ridged Multifractal", ToolTip = "Ridged with each octave weighted by the previous one. Mountain ranges: detail collects on the crests and leaves the valleys smooth.")
};

/** Shape of one procedural height field. Every knob is a real-world quantity — a wavelength or a
 *  height in cm — so the same settings mean the same landform whatever the footprint or the raster
 *  resolution. */
USTRUCT()
struct FArcTerrainNoiseParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Noise")
	EArcTerrainNoiseBasis Basis = EArcTerrainNoiseBasis::Perlin;

	UPROPERTY(EditAnywhere, Category = "Noise")
	EArcTerrainNoiseFractal Fractal = EArcTerrainNoiseFractal::Standard;

	/** Reseeds the whole field. Two stamps differing only here share no features. */
	UPROPERTY(EditAnywhere, Category = "Noise")
	int32 Seed = 0;

	/** Wavelength of the FIRST octave: the size of the largest landform the field produces. */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (ClampMin = "1.0", UIMin = "1000.0", UIMax = "1000000.0", ForceUnits = "cm"))
	double FeatureSizeCm = 50000.0;

	/** Scales the accumulated field. */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (UIMin = "0.0", UIMax = "200000.0", ForceUnits = "cm"))
	double AmplitudeCm = 5000.0;

	/** Detail levels. Each one costs a full noise evaluation per texel. */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (ClampMin = "1", ClampMax = "12"))
	int32 Octaves = 6;

	/** Wavelength shrink per octave. 2 halves it, which is the usual choice; irrational-ish values
	 *  break up the axis-aligned beating that an exact 2 can produce. */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (ClampMin = "1.1", ClampMax = "4.0"))
	float Lacunarity = 2.0f;

	/** Amplitude scale per octave. Below 0.5 the field smooths out, above it roughens. */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Gain = 0.5f;

	// --- Domain warp: bends the sample grid before the octaves run, which is what turns
	//     characterless fBm into something with flow direction. ---

	/** How far the sample position is displaced. 0 disables the warp and its two noise evaluations. */
	UPROPERTY(EditAnywhere, Category = "Noise|Warp", meta = (ClampMin = "0.0", UIMax = "100000.0", ForceUnits = "cm"))
	double WarpStrengthCm = 0.0;

	/** Wavelength of the displacement field. Larger than FeatureSizeCm bends whole landforms; smaller
	 *  frays their edges. */
	UPROPERTY(EditAnywhere, Category = "Noise|Warp", meta = (ClampMin = "1.0", UIMax = "1000000.0", ForceUnits = "cm", EditCondition = "WarpStrengthCm > 0.0"))
	double WarpFeatureSizeCm = 100000.0;

	// --- Terracing: quantises the finished height into shelves. ---

	/** Height of one shelf. 0 disables terracing. */
	UPROPERTY(EditAnywhere, Category = "Noise|Terrace", meta = (ClampMin = "0.0", UIMax = "20000.0", ForceUnits = "cm"))
	double TerraceStepCm = 0.0;

	/** 0 leaves the surface untouched, 1 is a flat shelf with a steep riser. */
	UPROPERTY(EditAnywhere, Category = "Noise|Terrace", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "TerraceStepCm > 0.0"))
	float TerraceSharpness = 0.5f;

	// --- Erosion: cuts slope-following channels into the accumulated relief, before terracing. ---

	UPROPERTY(EditAnywhere, Category = "Noise|Erosion", meta = (ShowOnlyInnerProperties))
	FArcTerrainErosionParams Erosion;
};
