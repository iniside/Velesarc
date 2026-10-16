// Copyright Lukasz Baran. All Rights Reserved.

#include "Terrain/ArcEcotopeInstanceRaster.h"

namespace ArcEcotopeInstanceRasterPrivate
{
	using UE::ArcEcotope::FFieldRaster;
	using UE::ArcEcotope::TexelCentreLocal;

	void LightDisc(FFieldRaster& Raster, const FVector2D& CentreLocal, const double RadiusCm)
	{
		const int32 MinX = FMath::Max(
			0, FMath::FloorToInt32((CentreLocal.X - RadiusCm - Raster.OriginLocal.X) / Raster.TexelSizeCm));
		const int32 MaxX = FMath::Min(
			Raster.Dimensions.X - 1,
			FMath::CeilToInt32((CentreLocal.X + RadiusCm - Raster.OriginLocal.X) / Raster.TexelSizeCm));
		const int32 MinY = FMath::Max(
			0, FMath::FloorToInt32((CentreLocal.Y - RadiusCm - Raster.OriginLocal.Y) / Raster.TexelSizeCm));
		const int32 MaxY = FMath::Min(
			Raster.Dimensions.Y - 1,
			FMath::CeilToInt32((CentreLocal.Y + RadiusCm - Raster.OriginLocal.Y) / Raster.TexelSizeCm));

		const double RadiusSquared = RadiusCm * RadiusCm;
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				if (FVector2D::DistSquared(TexelCentreLocal(Raster, X, Y), CentreLocal) > RadiusSquared)
				{
					continue;
				}

				Raster.Occupancy[X + Y * Raster.Dimensions.X] = 1.0f;
			}
		}
	}
}

namespace UE::ArcEcotope
{
	void RasterizeInstanceOccupancy(
		const TConstArrayView<FArcEcotopeInstanceFootprint> Footprints,
		const FArcEcotopeInstanceRasterRequest& Request,
		FFieldRaster& Raster)
	{
		using namespace ArcEcotopeInstanceRasterPrivate;

		if (Raster.Dimensions.X <= 0 || Raster.Dimensions.Y <= 0
			|| !(Raster.TexelSizeCm > 0.0)
			|| Raster.Occupancy.Num() != Raster.Dimensions.X * Raster.Dimensions.Y)
		{
			return;
		}

		for (const FArcEcotopeInstanceFootprint& Footprint : Footprints)
		{
			if (Request.SpeciesFilter.IsSet() && Footprint.SpeciesId != Request.SpeciesFilter.GetValue())
			{
				continue;
			}

			if (Request.KindFilter.IsSet() && Footprint.Kind != Request.KindFilter.GetValue())
			{
				continue;
			}

			if (!(Footprint.RadiusCm > 0.0) || !FMath::IsFinite(Footprint.RadiusCm)
				|| !FMath::IsFinite(Footprint.LocalXY.X) || !FMath::IsFinite(Footprint.LocalXY.Y))
			{
				continue;
			}

			LightDisc(Raster, Footprint.LocalXY, Footprint.RadiusCm);
		}
	}
}
