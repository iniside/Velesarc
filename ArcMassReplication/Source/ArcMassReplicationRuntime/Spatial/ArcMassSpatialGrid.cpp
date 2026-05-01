// Copyright Lukasz Baran. All Rights Reserved.

#include "Spatial/ArcMassSpatialGrid.h"

FIntVector2 FArcMassSpatialGrid::WorldToCell(FVector Position) const
{
	return FIntVector2(
		FMath::FloorToInt32(Position.X / CellSize),
		FMath::FloorToInt32(Position.Y / CellSize));
}

void FArcMassSpatialGrid::AddEntity(FArcMassNetId NetId, FVector Position)
{
	FIntVector2 Cell = WorldToCell(Position);
	CellToEntities.FindOrAdd(Cell).Add(NetId);
	EntityToCell.Add(NetId, Cell);
}

void FArcMassSpatialGrid::UpdateEntity(FArcMassNetId NetId, FVector NewPosition)
{
	FIntVector2 NewCell = WorldToCell(NewPosition);

	FIntVector2* OldCellPtr = EntityToCell.Find(NetId);
	if (!OldCellPtr)
	{
		AddEntity(NetId, NewPosition);
		return;
	}

	if (*OldCellPtr == NewCell)
	{
		return;
	}

	TSet<FArcMassNetId>* OldSet = CellToEntities.Find(*OldCellPtr);
	if (OldSet)
	{
		OldSet->Remove(NetId);
		if (OldSet->Num() == 0)
		{
			CellToEntities.Remove(*OldCellPtr);
		}
	}

	CellToEntities.FindOrAdd(NewCell).Add(NetId);
	*OldCellPtr = NewCell;
}

void FArcMassSpatialGrid::RemoveEntity(FArcMassNetId NetId)
{
	FIntVector2 Cell;
	if (!EntityToCell.RemoveAndCopyValue(NetId, Cell))
	{
		return;
	}

	TSet<FArcMassNetId>* CellSet = CellToEntities.Find(Cell);
	if (CellSet)
	{
		CellSet->Remove(NetId);
		if (CellSet->Num() == 0)
		{
			CellToEntities.Remove(Cell);
		}
	}
}

void FArcMassSpatialGrid::GetRelevantCells(FVector Position, float Radius, TArray<FIntVector2>& OutCells) const
{
	OutCells.Reset();

	int32 CellRadius = FMath::CeilToInt32(Radius / CellSize);
	FIntVector2 Center = WorldToCell(Position);

	for (int32 X = Center.X - CellRadius; X <= Center.X + CellRadius; ++X)
	{
		for (int32 Y = Center.Y - CellRadius; Y <= Center.Y + CellRadius; ++Y)
		{
			FIntVector2 Cell(X, Y);
			if (CellToEntities.Contains(Cell))
			{
				OutCells.Add(Cell);
			}
		}
	}
}
