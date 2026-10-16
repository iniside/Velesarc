// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBuilder.h"
#include "River/ArcWaterRiverModifierTypes.h"
#include "Templates/Function.h"

struct FArcWaterRiverNetworkJunctionRef
{
	int32 JunctionId = INDEX_NONE;
	int32 PointIndex = INDEX_NONE;
};

struct FArcWaterRiverNetworkReachInput
{
	TArray<FVector2D> Points;
	TArray<FArcWaterRiverNetworkJunctionRef> Junctions;
	bool bClosed = false;
	bool bStartIsOriginalEndpoint = false;
	bool bEndIsOriginalEndpoint = false;
};

struct FArcWaterRiverTerrainInput
{
	TFunction<bool(const FVector2D&, float&)> SampleHeight;
	float WorldZOffsetCm = 0.0f;
	float WorldZScale = 1.0f;
};

struct FArcWaterRiverNetworkInput
{
	TArray<FArcWaterRiverNetworkReachInput> Reaches;
	TArray<FVector2D> JunctionPositions;
	FArcWaterRiverTerrainInput Terrain;
};

namespace ArcWaterRiverChannel
{
	ARCWATER_API bool BuildChannels(const FArcWaterRiverNetworkInput& Input,
		const FArcWaterRiverModifierSettings& Settings,
		FArcWaterRiverGeneratedData& OutData, FText& OutError,
		TFunctionRef<bool()> ShouldContinue);

	ARCWATER_API void BuildWaterMesh(const FArcWaterRiverGeneratedData& Data, int32 ReachIndex,
		TArray<FDynamicMeshVertex>& OutVertices, TArray<uint32>& OutIndices);
}
