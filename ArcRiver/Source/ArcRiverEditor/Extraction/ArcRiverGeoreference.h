// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Extraction/ArcRiverNetworkExtraction.h"

class AArcTerrain;
class UArcTerrainTexturePatchModifier;

namespace ArcRiverGeoreference
{
	struct FArcRiverPolyline
	{
		/** Z is the solved water surface. */
		TArray<FVector> WorldPoints;
		TArray<float> HalfWidthCm;
		TArray<float> DepthCm;
		TArray<float> BedZ;
		/** Unsolved terrain height under the point; the bank cut climbs from BedZ back to it. */
		TArray<float> GroundZ;
	};

	struct FGeoreferenceInput
	{
		/** Footprint the mask covers: CoverageXY plus the component transform. */
		const UArcTerrainTexturePatchModifier* Modifier = nullptr;
		const AArcTerrain* Terrain = nullptr;
		FIntPoint MaskDims = FIntPoint::ZeroValue;
		float WidthToDepthRatio = 16.0f;
	};

	/** World cm one mask texel spans: CoverageXY folded with the modifier's world scale, tighter axis. */
	double ComputeCmPerTexel(const UArcTerrainTexturePatchModifier& Modifier, FIntPoint MaskDims);

	bool BuildPolylines(
		TConstArrayView<ArcRiverNetworkExtraction::FArcRiverChain> Chains,
		TConstArrayView<FIntPoint> JunctionTexels,
		const FGeoreferenceInput& Input,
		TArray<FArcRiverPolyline>& OutPolylines,
		FText& OutError);
}
