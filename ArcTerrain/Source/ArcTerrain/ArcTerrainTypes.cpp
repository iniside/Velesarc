// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainTypes.h"

namespace
{
	// Floor-divide a local coordinate by CellSize into an integer cell index (LWC double math).
	int32 ArcCellIndex(double Coord, double CellSize)
	{
		return FMath::FloorToInt32(Coord / CellSize);
	}

	// True when Coord lands exactly on a cell boundary (integer multiple of CellSize).
	bool ArcOnCellBoundary(double Coord, double CellSize)
	{
		return FMath::Fmod(Coord, CellSize) == 0.0;
	}

	// THE EDGE-INCLUSIVE RULE, one authority for every lattice (cells and collision patches). For each
	// axis:
	//   MinKey = FloorToInt(Min / UnitSize)
	//   MaxKey = FloorToInt(Max / UnitSize)
	// FloorToInt(x) yields the unit whose CLOSED interval [k*UnitSize, (k+1)*UnitSize] has k*UnitSize as
	// its lower edge. So:
	//  * When Max lands exactly on a boundary B = k*UnitSize, FloorToInt already returns k — the upper of
	//    the two units sharing B — and the lower one (k-1) is already inside [MinKey, MaxKey]. No
	//    adjustment on the Max side.
	//  * When Min lands exactly on B, FloorToInt returns k, but the LOWER unit (k-1), which also touches
	//    B under closed intervals, falls below the floor result and would be missed — hence the
	//    decrement.
	// Net effect: an edge exactly on a boundary intersects BOTH neighbours, which is what guarantees the
	// shared border texel composites identically on either side.
	//
	// The terrain is an ALWAYS-INFINITE plane: the raw geometric rectangle, NO bounds clamp.
	bool ArcUnitRangeIntersectingLocalBounds(const FBox& LocalBounds, double UnitSize,
		FIntPoint& OutMinKey, FIntPoint& OutMaxKey)
	{
		if (!LocalBounds.IsValid || UnitSize <= 0.0)
		{
			return false;
		}

		double MinX = FMath::Floor(LocalBounds.Min.X / UnitSize);
		double MinY = FMath::Floor(LocalBounds.Min.Y / UnitSize);
		const double MaxX = FMath::Floor(LocalBounds.Max.X / UnitSize);
		const double MaxY = FMath::Floor(LocalBounds.Max.Y / UnitSize);

		constexpr double CellIndexLimit = 1.0e9;
		if (!FMath::IsFinite(MinX) || !FMath::IsFinite(MinY)
			|| !FMath::IsFinite(MaxX) || !FMath::IsFinite(MaxY)
			|| FMath::Abs(MinX) > CellIndexLimit || FMath::Abs(MinY) > CellIndexLimit
			|| FMath::Abs(MaxX) > CellIndexLimit || FMath::Abs(MaxY) > CellIndexLimit)
		{
			return false;
		}

		if (ArcOnCellBoundary(LocalBounds.Min.X, UnitSize))
		{
			MinX -= 1.0;
		}
		if (ArcOnCellBoundary(LocalBounds.Min.Y, UnitSize))
		{
			MinY -= 1.0;
		}

		const int64 SpanX = static_cast<int64>(MaxX) - static_cast<int64>(MinX) + 1;
		const int64 SpanY = static_cast<int64>(MaxY) - static_cast<int64>(MinY) + 1;
		constexpr int64 MaxUnitRangeKeys = 1 << 20;
		if (SpanX <= 0 || SpanY <= 0 || SpanX * SpanY > MaxUnitRangeKeys)
		{
			return false;
		}

		OutMinKey = FIntPoint(static_cast<int32>(MinX), static_cast<int32>(MinY));
		OutMaxKey = FIntPoint(static_cast<int32>(MaxX), static_cast<int32>(MaxY));
		return true;
	}
}

namespace ArcTerrain
{
	FArcTerrainCellKey CellKeyFromLocal(const FVector& LocalPos, const FArcTerrainConfig& Config)
	{
		return FArcTerrainCellKey(
			ArcCellIndex(LocalPos.X, Config.CellSize()),
			ArcCellIndex(LocalPos.Y, Config.CellSize()));
	}

	FBox CellLocalBounds(FArcTerrainCellKey Key, const FArcTerrainConfig& Config)
	{
		const double MinX = static_cast<double>(Key.X) * Config.CellSize();
		const double MinY = static_cast<double>(Key.Y) * Config.CellSize();
		const FVector Min(MinX, MinY, -ArcTerrain::GCellZHalfExtent);
		const FVector Max(MinX + Config.CellSize(), MinY + Config.CellSize(), ArcTerrain::GCellZHalfExtent);
		return FBox(Min, Max);
	}

	void CellsIntersectingLocalBounds(const FBox& LocalBounds, const FArcTerrainConfig& Config,
		TArray<FArcTerrainCellKey>& OutCells)
	{
		OutCells.Reset();
		FIntPoint MinCell = FIntPoint::ZeroValue;
		FIntPoint MaxCell = FIntPoint::ZeroValue;
		if (!CellRangeIntersectingLocalBounds(LocalBounds, Config, MinCell, MaxCell))
		{
			return;
		}

		OutCells.Reserve((MaxCell.X - MinCell.X + 1) * (MaxCell.Y - MinCell.Y + 1));
		for (int32 CellY = MinCell.Y; CellY <= MaxCell.Y; ++CellY)
		{
			for (int32 CellX = MinCell.X; CellX <= MaxCell.X; ++CellX)
			{
				OutCells.Emplace(CellX, CellY);
			}
		}
	}

	bool CellRangeIntersectingLocalBounds(const FBox& LocalBounds, const FArcTerrainConfig& Config,
		FIntPoint& OutMinCell, FIntPoint& OutMaxCell)
	{
		return ArcUnitRangeIntersectingLocalBounds(LocalBounds, Config.CellSize(), OutMinCell, OutMaxCell);
	}

	FBox CollisionPatchLocalBounds(FArcTerrainCollisionKey Key, double PatchSize)
	{
		const double MinX = static_cast<double>(Key.X) * PatchSize;
		const double MinY = static_cast<double>(Key.Y) * PatchSize;
		const FVector Min(MinX, MinY, -ArcTerrain::GCellZHalfExtent);
		const FVector Max(MinX + PatchSize, MinY + PatchSize, ArcTerrain::GCellZHalfExtent);
		return FBox(Min, Max);
	}

	void CollisionPatchesIntersectingLocalBounds(const FBox& LocalBounds, double PatchSize,
		TArray<FArcTerrainCollisionKey>& OutKeys)
	{
		OutKeys.Reset();
		FIntPoint MinKey = FIntPoint::ZeroValue;
		FIntPoint MaxKey = FIntPoint::ZeroValue;
		if (!ArcUnitRangeIntersectingLocalBounds(LocalBounds, PatchSize, MinKey, MaxKey))
		{
			return;
		}

		OutKeys.Reserve((MaxKey.X - MinKey.X + 1) * (MaxKey.Y - MinKey.Y + 1));
		for (int32 KeyY = MinKey.Y; KeyY <= MaxKey.Y; ++KeyY)
		{
			for (int32 KeyX = MinKey.X; KeyX <= MaxKey.X; ++KeyX)
			{
				OutKeys.Emplace(KeyX, KeyY);
			}
		}
	}
}
