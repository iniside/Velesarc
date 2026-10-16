// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FArcTownSiteSnapshot;
struct FArcTownWorldSnapshot;

struct ARCTOWN_API FArcTownGeneratedPath
{
	/** Town-local XY of the polyline, already smoothed. */
	TArray<FVector2f> Points;

	/** Co-indexed with Points; feeds UArcTownPathMetadata::HalfWidth. */
	TArray<float> HalfWidth;

	bool bClosed = false;

	uint8 Hierarchy = 0;

	/** Graph vertex id at either end; INDEX_NONE on both for a closed loop. */
	int32 StartNodeId = INDEX_NONE;
	int32 EndNodeId = INDEX_NONE;
};

struct ARCTOWN_API FArcTownGeneratedJunction
{
	int32 NodeId = INDEX_NONE;

	FVector2f LocalXY = FVector2f::ZeroVector;

	uint8 Valence = 0;

	uint8 Hierarchy = 0;
};

struct ARCTOWN_API FArcTownPlacedBuilding
{
	/** Index into the frozen catalog, carried straight from FArcTownBuildingRecord::CatalogIndex. */
	int32 CatalogIndex = INDEX_NONE;

	/** Town-local: translation is FArcTownBuildingRecord::LocalXY at Z = 0, rotation its Yaw. */
	FTransform LocalXform;
};

namespace ArcTownLayout
{
	ARCTOWN_API bool BuildBuildingsFromSnapshot(
		const FArcTownWorldSnapshot& Snapshot,
		const FArcTownSiteSnapshot& Site,
		TArray<FArcTownPlacedBuilding>& OutPlaced,
		FText& OutError);

	ARCTOWN_API bool BuildPathsFromSnapshot(
		const FArcTownWorldSnapshot& Snapshot,
		const FArcTownSiteSnapshot& Site,
		TArray<FArcTownGeneratedPath>& OutPaths,
		TArray<FArcTownGeneratedJunction>& OutJunctions,
		FText& OutError);
}
