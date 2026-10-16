// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

namespace ArcRiverNetworkExtraction
{
	constexpr int32 MaxMaskDim = 8192;

	struct FArcRiverJunctionRef
	{
		int32 JunctionId = INDEX_NONE;
		int32 PointIndex = INDEX_NONE;
	};

	struct FArcRiverChain
	{
		TArray<FVector2f> TexelXY;
		TArray<float> HalfWidthTexels;
		// Every junction the chain passes through, interior ones included; PointIndex indexes TexelXY.
		TArray<FArcRiverJunctionRef> Junctions;
	};

	struct FArcRiverExtractionStats
	{
		int32 WaterTexelCount = 0;
		int32 WaterTexelsAfterWidthFilter = 0;
		int32 SkeletonTexelCount = 0;
		int32 ChainsBeforePrune = 0;
		int32 ChainsAfterPrune = 0;
		int32 StemCount = 0;
		int32 PrunePasses = 0;
		bool bPruneHitPassBound = false;
	};

	void ExtractChains(
		TConstArrayView<float> Mask,
		FIntPoint Dims,
		float Threshold,
		float MinHalfWidthTexels,
		float SimplifyToleranceTexels,
		int32 MinChainTexels,
		TArray<FArcRiverChain>& OutChains,
		TArray<FIntPoint>& OutJunctionTexels,
		FArcRiverExtractionStats& OutStats,
		FText& OutError,
		TFunctionRef<bool(float /*Progress01*/)> ShouldContinue);
}
