// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainGPU/ArcTerrainAtlasLayout.h"

#include "ArcTerrainChannelSet.h"

/** File-scoped and uniquely named (CLAUDE.md C++ Code Style): free helpers must not collide in a
 *  unity build. */
namespace ArcPCGExtensionAtlasLayoutPrivate
{
	/** Texels of slack added on EVERY side of the touched span: 1 for the bilinear footprint of a
	 *  sample landing exactly on the span edge, 1 more for the normal's central-difference tap
	 *  (which steps one atlas texel outward before that bilinear fetch). */
	constexpr int32 PadTexels = 2;

	/** ArcTerrainTypes.cpp::ArcCellIndex — floor division, LWC-safe in double. */
	int32 CellIndex(double Coord, double CellSize)
	{
		return static_cast<int32>(FMath::FloorToDouble(Coord / CellSize));
	}

	/** ArcTerrainTypes.cpp::ArcOnCellBoundary — exact-boundary test (CellSize is contractually a
	 *  whole number of cm, so the Fmod comparison is meaningful). */
	bool OnCellBoundary(double Coord, double CellSize)
	{
		return FMath::Fmod(Coord, CellSize) == 0.0;
	}

	/** Padded, tile-clamped inclusive texel span [OutLo, OutHi] a [Frac0, Frac1] cell-local span
	 *  touches. Returns false when the span had to be clamped against the tile edge. */
	bool TexelSpan(float Frac0, float Frac1, int32 TileRes, int32& OutLo, int32& OutHi)
	{
		const float Texel0 = ArcPCGTerrainGPU::WorldToTileTexel(FMath::Min(Frac0, Frac1), TileRes);
		const float Texel1 = ArcPCGTerrainGPU::WorldToTileTexel(FMath::Max(Frac0, Frac1), TileRes);

		const int32 RawLo = FMath::FloorToInt32(Texel0) - PadTexels;
		const int32 RawHi = FMath::CeilToInt32(Texel1) + PadTexels;

		OutLo = FMath::Clamp(RawLo, 0, TileRes - 1);
		OutHi = FMath::Clamp(RawHi, 0, TileRes - 1);

		return RawLo == OutLo && RawHi == OutHi;
	}
}

namespace ArcPCGTerrainGPU
{

FArcPCGAtlasLayout ComputeAtlasLayout(const FBox& LocalBounds, double CellSize, int32 HeightTileRes,
	int32 WeightTileRes, int32 MaxCells)
{
	using namespace ArcPCGExtensionAtlasLayoutPrivate;

	FArcPCGAtlasLayout Layout;

	// HeightTileRes < 4 is not a degraded case but an impossible one: TileRes == Quads + 3 with
	// Quads >= 1, so anything below 4 means the caller has no composite raster at all.
	if (!LocalBounds.IsValid || CellSize <= 0.0 || HeightTileRes < 4 || MaxCells <= 0)
	{
		return Layout;
	}

	// EDGE-INCLUSIVE, identical to ArcTerrain::CellsIntersectingLocalBounds (ArcTerrainTypes.cpp).
	int32 MinCellX = CellIndex(LocalBounds.Min.X, CellSize);
	int32 MinCellY = CellIndex(LocalBounds.Min.Y, CellSize);
	const int32 MaxCellX = CellIndex(LocalBounds.Max.X, CellSize);
	const int32 MaxCellY = CellIndex(LocalBounds.Max.Y, CellSize);

	if (OnCellBoundary(LocalBounds.Min.X, CellSize))
	{
		--MinCellX;
	}
	if (OnCellBoundary(LocalBounds.Min.Y, CellSize))
	{
		--MinCellY;
	}

	int32 CountX = FMath::Max(MaxCellX - MinCellX + 1, 1);
	int32 CountY = FMath::Max(MaxCellY - MinCellY + 1, 1);

	Layout.RequestedCellCount = FIntPoint(CountX, CountY);

	// Shrink the LARGER axis first so the retained rectangle stays as square as possible around the
	// min corner. The min corner is kept because the subsystem's gather scans row-major from the same
	// corner, so the retained rectangle is the part the gather is most likely to have filled.
	while (static_cast<int64>(CountX) * static_cast<int64>(CountY) > static_cast<int64>(MaxCells))
	{
		Layout.bCellRectClamped = true;
		if (CountX >= CountY && CountX > 1)
		{
			--CountX;
		}
		else if (CountY > 1)
		{
			--CountY;
		}
		else
		{
			break;
		}
	}

	Layout.OriginCell = FIntPoint(MinCellX, MinCellY);
	Layout.CellCount = FIntPoint(CountX, CountY);

	const bool bHasWeights = WeightTileRes >= 4;

	// Pass 1: per-cell padded spans, and the union size every slice will share.
	TArray<FIntPoint> HeightLo;
	TArray<FIntPoint> WeightLo;
	HeightLo.Reserve(CountX * CountY);
	WeightLo.Reserve(CountX * CountY);

	FIntPoint HeightSize(1, 1);
	FIntPoint WeightSize(1, 1);

	for (int32 CellY = MinCellY; CellY < MinCellY + CountY; ++CellY)
	{
		for (int32 CellX = MinCellX; CellX < MinCellX + CountX; ++CellX)
		{
			const double CellMinX = static_cast<double>(CellX) * CellSize;
			const double CellMinY = static_cast<double>(CellY) * CellSize;

			// Overlap of the bounds with THIS cell, expressed as a cell-local [0,1] fraction. A cell
			// that only touches the bounds on its shared boundary collapses to a point here — that is
			// correct, the padding below still gives it a usable footprint.
			const double OverlapMinX = FMath::Max(LocalBounds.Min.X, CellMinX);
			const double OverlapMaxX = FMath::Min(LocalBounds.Max.X, CellMinX + CellSize);
			const double OverlapMinY = FMath::Max(LocalBounds.Min.Y, CellMinY);
			const double OverlapMaxY = FMath::Min(LocalBounds.Max.Y, CellMinY + CellSize);

			const float FracMinX = static_cast<float>(FMath::Clamp((OverlapMinX - CellMinX) / CellSize, 0.0, 1.0));
			const float FracMaxX = static_cast<float>(FMath::Clamp((OverlapMaxX - CellMinX) / CellSize, 0.0, 1.0));
			const float FracMinY = static_cast<float>(FMath::Clamp((OverlapMinY - CellMinY) / CellSize, 0.0, 1.0));
			const float FracMaxY = static_cast<float>(FMath::Clamp((OverlapMaxY - CellMinY) / CellSize, 0.0, 1.0));

			int32 LoX = 0;
			int32 HiX = 0;
			int32 LoY = 0;
			int32 HiY = 0;
			const bool bFitX = TexelSpan(FracMinX, FracMaxX, HeightTileRes, LoX, HiX);
			const bool bFitY = TexelSpan(FracMinY, FracMaxY, HeightTileRes, LoY, HiY);
			Layout.bSubRectClamped |= !bFitX || !bFitY;

			HeightLo.Add(FIntPoint(LoX, LoY));
			HeightSize.X = FMath::Max(HeightSize.X, HiX - LoX + 1);
			HeightSize.Y = FMath::Max(HeightSize.Y, HiY - LoY + 1);

			if (bHasWeights)
			{
				int32 WLoX = 0;
				int32 WHiX = 0;
				int32 WLoY = 0;
				int32 WHiY = 0;
				const bool bWFitX = TexelSpan(FracMinX, FracMaxX, WeightTileRes, WLoX, WHiX);
				const bool bWFitY = TexelSpan(FracMinY, FracMaxY, WeightTileRes, WLoY, WHiY);
				Layout.bSubRectClamped |= !bWFitX || !bWFitY;

				WeightLo.Add(FIntPoint(WLoX, WLoY));
				WeightSize.X = FMath::Max(WeightSize.X, WHiX - WLoX + 1);
				WeightSize.Y = FMath::Max(WeightSize.Y, WHiY - WLoY + 1);
			}
			else
			{
				WeightLo.Add(FIntPoint::ZeroValue);
			}
		}
	}

	HeightSize.X = FMath::Min(HeightSize.X, HeightTileRes);
	HeightSize.Y = FMath::Min(HeightSize.Y, HeightTileRes);
	if (bHasWeights)
	{
		WeightSize.X = FMath::Min(WeightSize.X, WeightTileRes);
		WeightSize.Y = FMath::Min(WeightSize.Y, WeightTileRes);
	}
	else
	{
		WeightSize = FIntPoint(1, 1);
	}

	Layout.HeightSubRect = HeightSize;
	Layout.WeightSubRect = WeightSize;

	// Pass 2: clamp each origin so origin + size stays inside its tile. Because size >= that slot's
	// own required span, the clamped window still covers [Lo, Hi] on both axes.
	Layout.Slots.Reserve(CountX * CountY);
	for (int32 LocalY = 0; LocalY < CountY; ++LocalY)
	{
		for (int32 LocalX = 0; LocalX < CountX; ++LocalX)
		{
			const int32 Index = LocalY * CountX + LocalX;

			FArcPCGAtlasSlot Slot;
			Slot.Key = FIntPoint(MinCellX + LocalX, MinCellY + LocalY);
			Slot.Slice = Index;
			Slot.HeightSrcOrigin = FIntPoint(
				FMath::Clamp(HeightLo[Index].X, 0, HeightTileRes - HeightSize.X),
				FMath::Clamp(HeightLo[Index].Y, 0, HeightTileRes - HeightSize.Y));
			Slot.WeightSrcOrigin = bHasWeights
				? FIntPoint(
					FMath::Clamp(WeightLo[Index].X, 0, WeightTileRes - WeightSize.X),
					FMath::Clamp(WeightLo[Index].Y, 0, WeightTileRes - WeightSize.Y))
				: FIntPoint::ZeroValue;

			Layout.Slots.Add(Slot);
		}
	}

	return Layout;
}

float WorldToTileTexel(float Frac, int32 TileRes)
{
	return Frac * static_cast<float>(TileRes - 3) + 1.5f;
}

int32 ResolveLayerFilterChannel(FName LayerChannel, const UArcTerrainChannelSet* ChannelSet,
	bool& bOutAuthoringError)
{
	bOutAuthoringError = false;

	if (LayerChannel.IsNone())
	{
		return INDEX_NONE;
	}

	if (ChannelSet == nullptr)
	{
		bOutAuthoringError = true;
		return INDEX_NONE;
	}

	const int32 Index = ChannelSet->FindChannelIndex(LayerChannel);
	if (Index == INDEX_NONE)
	{
		bOutAuthoringError = true;
		return INDEX_NONE;
	}

	return Index;
}

} // namespace ArcPCGTerrainGPU
