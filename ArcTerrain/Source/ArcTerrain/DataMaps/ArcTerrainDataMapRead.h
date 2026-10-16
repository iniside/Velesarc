// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataMaps/ArcTerrainDataMapTypes.h"

#if WITH_EDITOR

class AArcTerrain;
class FArcTerrainSourceCache;
struct FArcTerrainState;
struct FArcTerrainTileGrid;

namespace ArcTerrainDataMaps
{
	bool FindDataMapSourceRegion(const AArcTerrain& Terrain, const FArcTerrainState& State,
		EArcTerrainDataMap Map, FBox& OutTerrainLocalBounds, FArcTerrainTileGrid& OutDataGrid);
	bool ReadDataMapTexelsFromSources(const AArcTerrain& Terrain, FArcTerrainState& State,
		FArcTerrainSourceCache& SourceCache, EArcTerrainDataMap Map,
		FIntPoint MinTexel, FIntPoint MaxTexel, TArray<float>& OutValues,
		FIntPoint& OutDims, bool& bOutComplete);
}

#endif
