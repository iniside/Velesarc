// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

namespace ArcWaterRiverNetworkExtraction
{
	struct FArcWaterRiverMaskTile
	{
		FIntPoint TileCoord = FIntPoint::ZeroValue;
		TBitArray<> Pixels;
		int32 ActivePixelCount = 0;
	};

	struct FArcWaterRiverMaskGrid
	{
		static constexpr int32 TileSide = 256;

		FIntRect SourceRect;
		TMap<FIntPoint, FArcWaterRiverMaskTile> Tiles;
		int64 ActivePixelCount = 0;

		bool AddActivePixel(FIntPoint Pixel);
		bool IsActive(FIntPoint Pixel) const;
		bool RemoveActivePixel(FIntPoint Pixel);
		int64 GetAllocatedBytes() const;
	};

	struct FArcWaterRiverJunctionRef
	{
		int32 JunctionId = INDEX_NONE;
		int32 PointIndex = INDEX_NONE;
	};

	struct FArcWaterRiverPath
	{
		TArray<FVector2D> Points;
		TArray<FArcWaterRiverJunctionRef> Junctions;
		double LengthCm = 0.0;
		bool bClosed = false;
		bool bStartIsOriginalEndpoint = false;
		bool bEndIsOriginalEndpoint = false;
	};

	bool ExtractNetwork(
		FArcWaterRiverMaskGrid&& Mask,
		const FVector2D& CmPerTexel,
		double SplineToleranceCm,
		TArray<FArcWaterRiverPath>& OutPaths,
		TArray<FIntPoint>& OutJunctionTexels,
		FText& OutError,
		TFunctionRef<bool(float)> ShouldContinue);
}
