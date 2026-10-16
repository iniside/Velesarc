// Copyright Lukasz Baran. All Rights Reserved.

#include "Surface/ArcTerrainSurfaceSample.h"

namespace ArcTerrainSurfaceSampleFileLocal
{
	// Resolves the four bilinear taps + weights for a (Quads+3)-wide apron tile at fractional cell
	// coordinates. Shared by the float and vector samplers so both agree bit-for-bit on tap
	// selection (BilinearApron doc contract).
	struct FBilinearTaps
	{
		int32 Index00 = 0;
		int32 Index10 = 0;
		int32 Index01 = 0;
		int32 Index11 = 0;
		float FracX = 0.0f;
		float FracY = 0.0f;
	};

	FBilinearTaps ResolveTaps(int32 Quads, float FracX, float FracY)
	{
		const int32 TileRes = Quads + 3;

		const float VertexX = FMath::Clamp(FracX, 0.0f, 1.0f) * static_cast<float>(Quads);
		const float VertexY = FMath::Clamp(FracY, 0.0f, 1.0f) * static_cast<float>(Quads);

		const int32 Column0 = FMath::Clamp(FMath::FloorToInt32(VertexX), 0, Quads);
		const int32 Row0 = FMath::Clamp(FMath::FloorToInt32(VertexY), 0, Quads);

		FBilinearTaps Taps;
		Taps.FracX = VertexX - static_cast<float>(Column0);
		Taps.FracY = VertexY - static_cast<float>(Row0);
		// Interior vertex (c, r) -> texel (c+1, r+1); Column0+2 <= Quads+2 stays inside the tile.
		Taps.Index00 = (Row0 + 1) * TileRes + (Column0 + 1);
		Taps.Index10 = Taps.Index00 + 1;
		Taps.Index01 = Taps.Index00 + TileRes;
		Taps.Index11 = Taps.Index01 + 1;
		return Taps;
	}
}

namespace ArcTerrainSurface
{
	float BilinearApron(TConstArrayView<float> Grid, int32 Quads, float FracX, float FracY)
	{
		check(Quads > 0 && Grid.Num() == (Quads + 3) * (Quads + 3));
		const ArcTerrainSurfaceSampleFileLocal::FBilinearTaps Taps = ArcTerrainSurfaceSampleFileLocal::ResolveTaps(Quads, FracX, FracY);

		const float Top = FMath::Lerp(Grid[Taps.Index00], Grid[Taps.Index10], Taps.FracX);
		const float Bottom = FMath::Lerp(Grid[Taps.Index01], Grid[Taps.Index11], Taps.FracX);
		return FMath::Lerp(Top, Bottom, Taps.FracY);
	}

	FVector3f BilinearApronVec(TConstArrayView<FVector3f> Grid, int32 Quads, float FracX, float FracY)
	{
		check(Quads > 0 && Grid.Num() == (Quads + 3) * (Quads + 3));
		const ArcTerrainSurfaceSampleFileLocal::FBilinearTaps Taps = ArcTerrainSurfaceSampleFileLocal::ResolveTaps(Quads, FracX, FracY);

		const FVector3f Top = FMath::Lerp(Grid[Taps.Index00], Grid[Taps.Index10], Taps.FracX);
		const FVector3f Bottom = FMath::Lerp(Grid[Taps.Index01], Grid[Taps.Index11], Taps.FracX);
		return FMath::Lerp(Top, Bottom, Taps.FracY);
	}

	FArcTerrainSurfaceCellCache BuildCellCache(
		FIntPoint Key,
		const TArray<float>& HeightsWithApron,
		int32 RenderQuads,
		double CellSize)
	{
		const int32 TileRes = RenderQuads + 3;
		check(RenderQuads > 0 && CellSize > 0.0);
		check(HeightsWithApron.Num() == TileRes * TileRes);

		FArcTerrainSurfaceCellCache Cell;
		Cell.Key = Key;
		Cell.RenderQuads = RenderQuads;
		Cell.HeightsWithApron = HeightsWithApron;

		// Finite-difference normals per texel — the central-difference gradient
		// (central diff of neighbour heights over texel spacing, Z = 1, normalize). The apron makes
		// every interior/edge vertex a true central difference identical to the neighbour cell's;
		// the outermost apron texels fall back to a one-sided difference (only ever touched with
		// zero bilinear weight).
		const float QuadSizeCm = static_cast<float>(CellSize / static_cast<double>(RenderQuads));
		Cell.NormalsWithApron.SetNumUninitialized(TileRes * TileRes);
		for (int32 TexelY = 0; TexelY < TileRes; ++TexelY)
		{
			const int32 NeighborY0 = FMath::Max(TexelY - 1, 0);
			const int32 NeighborY1 = FMath::Min(TexelY + 1, TileRes - 1);
			const float SpacingY = static_cast<float>(NeighborY1 - NeighborY0) * QuadSizeCm;
			for (int32 TexelX = 0; TexelX < TileRes; ++TexelX)
			{
				const int32 NeighborX0 = FMath::Max(TexelX - 1, 0);
				const int32 NeighborX1 = FMath::Min(TexelX + 1, TileRes - 1);
				const float SpacingX = static_cast<float>(NeighborX1 - NeighborX0) * QuadSizeCm;

				const float DZDX = (HeightsWithApron[TexelY * TileRes + NeighborX1]
					- HeightsWithApron[TexelY * TileRes + NeighborX0]) / SpacingX;
				const float DZDY = (HeightsWithApron[NeighborY1 * TileRes + TexelX]
					- HeightsWithApron[NeighborY0 * TileRes + TexelX]) / SpacingY;
				Cell.NormalsWithApron[TexelY * TileRes + TexelX] = FVector3f(-DZDX, -DZDY, 1.0f).GetSafeNormal();
			}
		}

		return Cell;
	}

	FArcTerrainSurfaceCellCache BuildFlatCellCache(
		float HeightCm,
		int32 RenderQuads,
		double CellSize)
	{
		check(RenderQuads > 0 && CellSize > 0.0);
		const int32 TileRes = RenderQuads + 3;

		TArray<float> FlatHeights;
		FlatHeights.Init(HeightCm, TileRes * TileRes);
		return BuildCellCache(FIntPoint::ZeroValue, FlatHeights, RenderQuads, CellSize);
	}
}
