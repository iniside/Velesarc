// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Foliage/ArcEcotopeFoliageFieldPayload.h"

class UArcEcotopeModel;
struct FArcEcotopeSiteMetadata;
struct FArcForestWorldSnapshot;

#if WITH_EDITOR

namespace UE::ArcEcotope
{
	struct FSpeciesCrown
	{
		double CrownRadiusHeightRatio = 0.0;
		double CrownOccupancyWeight = 1.0;
	};

	struct FFieldRaster
	{
		TArray<float> Occupancy;
		FIntPoint Dimensions = FIntPoint::ZeroValue;
		FVector2D OriginLocal = FVector2D::ZeroVector;
		double TexelSizeCm = ArcEcotopeFoliageFieldLimits::TargetTexelSizeCm;
	};

	inline FVector2D TexelCentreLocal(const FFieldRaster& Raster, const int32 X, const int32 Y)
	{
		return FVector2D(
			Raster.OriginLocal.X + (static_cast<double>(X) + 0.5) * Raster.TexelSizeCm,
			Raster.OriginLocal.Y + (static_cast<double>(Y) + 0.5) * Raster.TexelSizeCm);
	}

	ARCECOTOPE_API TMap<FGuid, FSpeciesCrown> BuildSpeciesCrowns(const UArcEcotopeModel& Ecotope);

	/** Occupancy per texel of Raster's own frame; an unset SpeciesFilter accumulates every species. */
	ARCECOTOPE_API void RasterizeRecordOccupancy(
		const FArcForestWorldSnapshot& Snapshot,
		const FArcEcotopeSiteMetadata& Site,
		const TMap<FGuid, FSpeciesCrown>& SpeciesById,
		TOptional<FGuid> SpeciesFilter,
		FFieldRaster& Raster);
}

#endif // WITH_EDITOR
