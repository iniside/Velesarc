// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTerrainTileAccess.h"
#include "ArcTerrainTypes.h"
#include "DataMaps/ArcTerrainDataMapTypes.h"

/**
 * The read side of the authored data masks: pure CPU math over one loaded data tile, with no residency,
 * no loading and no editor dependency. The residency half lives on UArcTerrainSubsystem::SampleDataMap /
 * SampleDataMapRegion.
 */
namespace ArcTerrainDataMaps
{
	/**
	 * Bilinear reading of one mask at a terrain-local XY inside the tile that carries it.
	 *
	 * The decode is the plane's own window, carried in the header:
	 * value = QuantMin + (Texel / 255) * (QuantMax - QuantMin). The window contains zero by construction,
	 * so a texel no source wrote reads as zero (to within one quantization step) rather than as absent —
	 * there is no coverage rect. It is also terrain-wide, so the quantization step does not change as a
	 * sample crosses a tile boundary.
	 *
	 * False when the position lies outside this tile, the tile carries no plane for this mask, or the
	 * lattice does not match the payload.
	 */
	ARCTERRAIN_API bool SampleTileMask(const FArcTerrainTilePayload& Payload, const FArcTerrainTileGrid& Grid,
		EArcTerrainDataMap Map, const FVector2D& LocalXY, float& OutValue);
}
