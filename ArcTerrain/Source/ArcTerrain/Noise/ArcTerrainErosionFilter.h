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

#pragma once

#include "CoreMinimal.h"

#include "ArcTerrainErosionFilter.generated.h"

/** Carves gullies and rounds ridges into an already-shaped height field by walking a few octaves of
 *  phacelle noise steered by the local slope. Every clamp here is mirrored in
 *  ArcTerrainNoise::Sanitize, which is the single site the rasteriser and the ContentId both go
 *  through. */
USTRUCT()
struct FArcTerrainErosionParams
{
	GENERATED_BODY()

	/** Runs the filter. Off costs nothing: the four extra height samples the slope needs are skipped. */
	UPROPERTY(EditAnywhere, Category = "Erosion")
	bool bEnabled = false;

	/** Depth of the coarsest channel, independent of how far apart the channels are. Finer octaves
	 *  scale down from here by Gain. */
	UPROPERTY(EditAnywhere, Category = "Erosion", meta = (ClampMin = "0.0", UIMax = "50000.0", ForceUnits = "cm", EditCondition = "bEnabled"))
	double ChannelDepthCm = 650.0;

	/** Balance between carved channels and the smooth peak/valley drift. 0 leaves only the drift. */
	UPROPERTY(EditAnywhere, Category = "Erosion", meta = (ClampMin = "0.0", ClampMax = "2.0", EditCondition = "bEnabled"))
	double GullyWeight = 1.0;

	/** How aggressively each octave narrows the previous octave's channels. Higher branches finer. */
	UPROPERTY(EditAnywhere, Category = "Erosion", meta = (ClampMin = "0.0", ClampMax = "8.0", EditCondition = "bEnabled"))
	double Detail = 2.0;

	/** Slope below which valley floors stay smooth instead of gullying. Higher leaves wider flat beds. */
	UPROPERTY(EditAnywhere, Category = "Erosion", meta = (ClampMin = "0.0", ClampMax = "2.0", EditCondition = "bEnabled"))
	double RoundingAtValley = 0.5;

	/** The same threshold on ridge tops. Low values let channels run right over the crests. */
	UPROPERTY(EditAnywhere, Category = "Erosion", meta = (ClampMin = "0.0", ClampMax = "2.0", EditCondition = "bEnabled"))
	double RoundingAtPeak = 0.1;

	/** Scales both rounding thresholds where they are tested against the INPUT terrain's slope. */
	UPROPERTY(EditAnywhere, Category = "Erosion", meta = (ClampMin = "0.0", ClampMax = "4.0", EditCondition = "bEnabled"))
	double RoundingInputScale = 1.0;

	/** Shrinks the rounding thresholds per octave, so fine channels round off less than coarse ones. */
	UPROPERTY(EditAnywhere, Category = "Erosion", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnabled"))
	double RoundingOctaveDecay = 0.75;

	/** Slope at which erosion starts on the input terrain. Raise it to keep gentle ground untouched. */
	UPROPERTY(EditAnywhere, Category = "Erosion", meta = (ClampMin = "0.0", ClampMax = "8.0", EditCondition = "bEnabled"))
	double OnsetInput = 0.5;

	/** The same onset applied to each octave's own slope, which is already unit-scaled. */
	UPROPERTY(EditAnywhere, Category = "Erosion", meta = (ClampMin = "0.0", ClampMax = "8.0", EditCondition = "bEnabled"))
	double OnsetOctave = 1.0;

	/** Slope magnitude channels are steered as if the terrain had. Keeps flat ground flowing somewhere. */
	UPROPERTY(EditAnywhere, Category = "Erosion", meta = (ClampMin = "0.0", ClampMax = "4.0", EditCondition = "bEnabled"))
	double AssumedSlopeMagnitude = 1.0;

	/** 0 steers purely by the real slope, 1 purely by AssumedSlopeMagnitude. */
	UPROPERTY(EditAnywhere, Category = "Erosion", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnabled"))
	double AssumedSlopeBlend = 0.5;

	/** Channel spacing as a fraction of Scale. Below 1 packs the network denser. */
	UPROPERTY(EditAnywhere, Category = "Erosion", meta = (ClampMin = "0.1", ClampMax = "4.0", EditCondition = "bEnabled"))
	double CellScale = 1.0;

	/** Refinement passes. Each one costs a full phacelle evaluation per texel. */
	UPROPERTY(EditAnywhere, Category = "Erosion", meta = (ClampMin = "1", ClampMax = "8", EditCondition = "bEnabled"))
	int32 Octaves = 8;

	/** Depth scale per octave. Below 0.5 the fine channels fade out fast. */
	UPROPERTY(EditAnywhere, Category = "Erosion", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnabled"))
	double Gain = 0.5;

	/** Spacing shrink per octave. 2 halves the channel spacing each pass. */
	UPROPERTY(EditAnywhere, Category = "Erosion", meta = (ClampMin = "1.1", ClampMax = "4.0", EditCondition = "bEnabled"))
	double Lacunarity = 2.0;

	/** How hard weak channels are pushed up to full contrast. 1 would divide by zero, so it stops short. */
	UPROPERTY(EditAnywhere, Category = "Erosion", meta = (ClampMin = "0.0", ClampMax = "0.999", EditCondition = "bEnabled"))
	double Normalization = 0.4;

	/** Spacing of the coarsest channels, in absolute cm. Reads best at roughly a quarter of the
	 *  footprint, so the network spans the stamp instead of repeating per hillock. */
	UPROPERTY(EditAnywhere, Category = "Erosion", meta = (ClampMin = "1.0", UIMin = "1000.0", UIMax = "200000.0", ForceUnits = "cm", EditCondition = "bEnabled"))
	double ScaleCm = 50000.0;
};

namespace ArcTerrainErosion
{
	FVector4d PhacelleNoise(
		FVector2d P, FVector2d NormDir, double Freq, double Offset, double Normalization, uint32 Seed);

	FVector3d ErosionFilter(
		FVector2d P, FVector3d HeightAndSlope, double FadeTarget, double CharacteristicSlope,
		const FArcTerrainErosionParams& Knobs, uint32 Seed);
}
