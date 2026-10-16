// Copyright Lukasz Baran. All Rights Reserved.

#include "Forest/ArcForestSpatialHash.h"

namespace ArcForestSpatialHashLocal
{
	/** Smallest cell size we will honour; guards divide-by-zero / degenerate grids. */
	static constexpr float MinCellSize = KINDA_SMALL_NUMBER;
}

void FArcForestSpatialHash::Build(TArrayView<const FVector2f> Positions, float InCellSize)
{
	Reset();

	CellSize = FMath::Max(InCellSize, ArcForestSpatialHashLocal::MinCellSize);
	PositionsView = Positions;

	for (int32 Index = 0; Index < Positions.Num(); ++Index)
	{
		const FIntPoint Cell = CellOf(Positions[Index]);
		TArray<int32>& Bucket = Cells.FindOrAdd(Cell);
		Bucket.Add(Index);
	}
}

void FArcForestSpatialHash::QueryRadius(FVector2f Center, float Radius, TArray<int32>& OutIndices) const
{
	OutIndices.Reset();

	if (Radius <= 0.0f || PositionsView.Num() == 0)
	{
		return;
	}

	const float RadiusSq = Radius * Radius;
	const FIntPoint CenterCell = CellOf(Center);

	// Number of cells the radius spans in each axis; ceil so a query straddling a
	// cell boundary still visits the far cell whose points may fall inside Radius.
	const int32 CellRange = FMath::CeilToInt(Radius / CellSize);

	for (int32 OffsetY = -CellRange; OffsetY <= CellRange; ++OffsetY)
	{
		for (int32 OffsetX = -CellRange; OffsetX <= CellRange; ++OffsetX)
		{
			const FIntPoint Cell(CenterCell.X + OffsetX, CenterCell.Y + OffsetY);
			const TArray<int32>* Bucket = Cells.Find(Cell);
			if (Bucket == nullptr)
			{
				continue;
			}

			for (int32 CandidateIndex : *Bucket)
			{
				const FVector2f Delta = PositionsView[CandidateIndex] - Center;
				if (Delta.SizeSquared() <= RadiusSq)
				{
					OutIndices.Add(CandidateIndex);
				}
			}
		}
	}
}

void FArcForestSpatialHash::Reset()
{
	// Reset (not Empty) retains the map's hash/element storage for cheap
	// per-band/per-iteration rebuilds — only the entries are cleared.
	Cells.Reset();
	PositionsView = TArrayView<const FVector2f>();
}

FIntPoint FArcForestSpatialHash::CellOf(FVector2f Position) const
{
	// FloorToInt (NOT an int cast) so negative coordinates floor toward -inf and
	// map to the correct cell — an int cast truncates toward zero and would merge
	// the cells straddling each axis's zero line.
	return FIntPoint(
		FMath::FloorToInt(Position.X / CellSize),
		FMath::FloorToInt(Position.Y / CellSize));
}
