// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace ArcTownStampKeys
{
	/**
	 * Content identity of one building pad's raster. Deterministic by contract: the terrain's collision
	 * and Nanite cache keys fold this guid and never hash the raster itself, so minting a fresh one per
	 * rebuild would invalidate every cell the town touches every time.
	 */
	ARCTOWN_API FGuid MakePadContentId(const FGuid& TownId, int32 BuildingIndex,
		const FVector2D& FootprintXY, double FloorHeight);
}
