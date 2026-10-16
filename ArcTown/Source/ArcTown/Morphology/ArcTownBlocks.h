// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FArcTownTrafficSettings;

namespace ArcTownRoadArrangement
{
	struct FArrangement;
}

struct ARCTOWN_API FArcTownBlock
{
	/** Town-local, CCW, without the repeated closing vertex. */
	TArray<FVector2f> Boundary;

	/** Per side: snapshot edge index, or INDEX_NONE. */
	TArray<int32> BoundaryEdgeIndex;

	TArray<uint8> BoundaryHierarchy;

	/** Per side: carriageway half width of that edge. */
	TArray<float> BoundaryHalfWidth;

	double AreaCm2 = 0.0;
};

struct ARCTOWN_API FArcTownBlockRejections
{
	int32 OuterFaces = 0;

	int32 TooFewSides = 0;

	int32 NonSimpleBoundary = 0;

	int32 AreaBelowThreshold = 0;

	int32 FrontageCollapsed = 0;

	int32 StripBelowThreshold = 0;

	int32 StripDepthUnusable = 0;

	int32 Total() const
	{
		return OuterFaces + TooFewSides + NonSimpleBoundary + AreaBelowThreshold + FrontageCollapsed
			+ StripBelowThreshold + StripDepthUnusable;
	}
};

namespace ArcTownBlocks
{
	ARCTOWN_API void ExtractBlocks(
		const ArcTownRoadArrangement::FArrangement& Arrangement,
		const double MinAreaCm2,
		TArray<FArcTownBlock>& OutBlocks,
		FArcTownBlockRejections& OutRejections);

	ARCTOWN_API bool BuildFrontagePolygon(
		const FArcTownBlock& Block,
		const FArcTownTrafficSettings& TrafficSettings,
		const double MinAreaCm2,
		FArcTownBlock& OutFrontage);

	ARCTOWN_API void BuildFrontage(
		const ArcTownRoadArrangement::FArrangement& Arrangement,
		const FArcTownTrafficSettings& TrafficSettings,
		const double MaxPlotDepthCm,
		const double MinAreaCm2,
		TArray<FArcTownBlock>& OutFrontage,
		FArcTownBlockRejections& OutRejections);
}
