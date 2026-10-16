// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Foliage/ArcEcotopeRecordRaster.h"
#include "Misc/Guid.h"
#include "Misc/Optional.h"

struct FArcEcotopeInstanceFootprint
{
	FVector2D LocalXY = FVector2D::ZeroVector;
	double RadiusCm = 0.0;
	FGuid SpeciesId;
	uint8 Kind = 0;
};

struct FArcEcotopeInstanceRasterRequest
{
	TOptional<FGuid> SpeciesFilter;
	TOptional<uint8> KindFilter;
};

namespace UE::ArcEcotope
{
	/** Its own deposit, not RasterizeRecordOccupancy's: that one spreads crown area as a fraction the GPU ground-foliage payload reads, and this one overrides to 1.0. */
	void RasterizeInstanceOccupancy(
		TConstArrayView<FArcEcotopeInstanceFootprint> Footprints,
		const FArcEcotopeInstanceRasterRequest& Request,
		FFieldRaster& Raster);
}
