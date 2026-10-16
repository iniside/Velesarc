// Copyright Lukasz Baran. All Rights Reserved.

#include "DataMaps/ArcTerrainDataMapSample.h"

namespace ArcTerrainDataMapSample_5c19
{
	// Tolerance on the tile-relative texel coordinate: a caller landing a hair outside the tile frame
	// (double rounding through the world transform) is admitted and clamped; anything further out
	// belongs to another tile and must read false.
	constexpr double TexelTolerance = 1.0e-4;
}

namespace ArcTerrainDataMaps
{
	bool SampleTileMask(const FArcTerrainTilePayload& Payload, const FArcTerrainTileGrid& Grid,
		EArcTerrainDataMap Map, const FVector2D& LocalXY, float& OutValue)
	{
		using namespace ArcTerrainDataMapSample_5c19;

		OutValue = 0.0f;

		if (!Grid.IsValid() || Payload.Rail != EArcTerrainRail::Data
			|| Payload.Resolution() != Grid.Resolution()
			|| !FMath::IsFinite(LocalXY.X) || !FMath::IsFinite(LocalXY.Y))
		{
			return false;
		}

		const int32 PlaneIndex = Payload.FindDataPlane(static_cast<uint8>(Map));
		if (!Payload.Planes.IsValidIndex(PlaneIndex)
			|| !Payload.Header.PlaneQuantMin.IsValidIndex(PlaneIndex)
			|| !Payload.Header.PlaneQuantMax.IsValidIndex(PlaneIndex))
		{
			return false;
		}

		const int32 Resolution = Payload.Resolution();
		const TArray<uint8>& Plane = Payload.Planes[PlaneIndex];
		if (Plane.Num() != Resolution * Resolution)
		{
			return false;
		}

		const double TexelToLocal =
			static_cast<double>(Grid.TileExtentM) * 100.0 / static_cast<double>(Grid.TileQuads);

		// Global texel index -> index inside the payload: the tile's own apron origin is
		// TileKey * TileQuads - 1, so the interior spans [1, TileQuads + 1].
		const double PosX = LocalXY.X / TexelToLocal
			- static_cast<double>(Payload.TileKey.X) * static_cast<double>(Grid.TileQuads) + 1.0;
		const double PosY = LocalXY.Y / TexelToLocal
			- static_cast<double>(Payload.TileKey.Y) * static_cast<double>(Grid.TileQuads) + 1.0;
		const double PosMax = static_cast<double>(Grid.TileQuads) + 1.0;
		if (!FMath::IsFinite(PosX) || !FMath::IsFinite(PosY)
			|| PosX < 1.0 - TexelTolerance || PosX > PosMax + TexelTolerance
			|| PosY < 1.0 - TexelTolerance || PosY > PosMax + TexelTolerance)
		{
			return false;
		}

		const double ClampedX = FMath::Clamp(PosX, 1.0, PosMax);
		const double ClampedY = FMath::Clamp(PosY, 1.0, PosMax);
		const int32 Column0 = FMath::Clamp(FMath::FloorToInt32(ClampedX), 1, Resolution - 2);
		const int32 Row0 = FMath::Clamp(FMath::FloorToInt32(ClampedY), 1, Resolution - 2);
		const float FracX = static_cast<float>(ClampedX - static_cast<double>(Column0));
		const float FracY = static_cast<float>(ClampedY - static_cast<double>(Row0));

		const int32 Index00 = Row0 * Resolution + Column0;
		const int32 Index10 = Index00 + 1;
		const int32 Index01 = Index00 + Resolution;
		const int32 Index11 = Index01 + 1;

		// Bilinear over the BYTES, then one affine decode: the window is per plane, so both orders agree
		// and this pays the multiply once.
		constexpr float QuantStep = 1.0f / 255.0f;
		const float Top = FMath::Lerp(
			static_cast<float>(Plane[Index00]), static_cast<float>(Plane[Index10]), FracX);
		const float Bottom = FMath::Lerp(
			static_cast<float>(Plane[Index01]), static_cast<float>(Plane[Index11]), FracX);
		const float Normalized = FMath::Lerp(Top, Bottom, FracY) * QuantStep;

		const float QuantMin = Payload.Header.PlaneQuantMin[PlaneIndex];
		const float QuantMax = Payload.Header.PlaneQuantMax[PlaneIndex];
		OutValue = QuantMin + Normalized * (QuantMax - QuantMin);
		return true;
	}
}
