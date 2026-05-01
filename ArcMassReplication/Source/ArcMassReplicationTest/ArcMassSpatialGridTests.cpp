// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Spatial/ArcMassSpatialGrid.h"
#include "Fragments/ArcMassNetId.h"

TEST_CLASS(ArcMassSpatialGrid, "ArcMassReplication.SpatialGrid")
{
	FArcMassSpatialGrid Grid;

	BEFORE_EACH()
	{
		Grid = FArcMassSpatialGrid();
		Grid.CellSize = 1000.f;
	}

	TEST_METHOD(WorldToCell_ConvertsPositionToGridCoords)
	{
		FIntVector2 Cell = Grid.WorldToCell(FVector(500.f, 500.f, 0.f));
		ASSERT_THAT(AreEqual(Cell.X, 0));
		ASSERT_THAT(AreEqual(Cell.Y, 0));

		FIntVector2 Cell2 = Grid.WorldToCell(FVector(1500.f, -500.f, 0.f));
		ASSERT_THAT(AreEqual(Cell2.X, 1));
		ASSERT_THAT(AreEqual(Cell2.Y, -1));
	}

	TEST_METHOD(AddEntity_AppearsInCorrectCell)
	{
		FArcMassNetId Id(1);
		Grid.AddEntity(Id, FVector(500.f, 500.f, 0.f));

		FIntVector2 Cell = Grid.WorldToCell(FVector(500.f, 500.f, 0.f));
		const TSet<FArcMassNetId>* Entities = Grid.CellToEntities.Find(Cell);
		ASSERT_THAT(IsNotNull(Entities));
		ASSERT_THAT(IsTrue(Entities->Contains(Id)));
	}

	TEST_METHOD(UpdateEntity_MovesToNewCell)
	{
		FArcMassNetId Id(1);
		Grid.AddEntity(Id, FVector(500.f, 500.f, 0.f));
		Grid.UpdateEntity(Id, FVector(1500.f, 1500.f, 0.f));

		FIntVector2 OldCell = Grid.WorldToCell(FVector(500.f, 500.f, 0.f));
		FIntVector2 NewCell = Grid.WorldToCell(FVector(1500.f, 1500.f, 0.f));

		const TSet<FArcMassNetId>* OldEntities = Grid.CellToEntities.Find(OldCell);
		ASSERT_THAT(IsTrue(OldEntities == nullptr || !OldEntities->Contains(Id)));

		const TSet<FArcMassNetId>* NewEntities = Grid.CellToEntities.Find(NewCell);
		ASSERT_THAT(IsNotNull(NewEntities));
		ASSERT_THAT(IsTrue(NewEntities->Contains(Id)));
	}

	TEST_METHOD(RemoveEntity_CleansUpCell)
	{
		FArcMassNetId Id(1);
		Grid.AddEntity(Id, FVector(500.f, 500.f, 0.f));
		Grid.RemoveEntity(Id);

		ASSERT_THAT(AreEqual(Grid.EntityToCell.Num(), 0));
	}

	TEST_METHOD(GetRelevantCells_ReturnsNearbyCells)
	{
		Grid.AddEntity(FArcMassNetId(1), FVector(500.f, 500.f, 0.f));
		Grid.AddEntity(FArcMassNetId(2), FVector(1500.f, 500.f, 0.f));
		Grid.AddEntity(FArcMassNetId(3), FVector(5500.f, 5500.f, 0.f));

		TArray<FIntVector2> Cells;
		Grid.GetRelevantCells(FVector(500.f, 500.f, 0.f), 1500.f, Cells);

		FIntVector2 Center = Grid.WorldToCell(FVector(500.f, 500.f, 0.f));
		FIntVector2 Neighbor = Grid.WorldToCell(FVector(1500.f, 500.f, 0.f));
		FIntVector2 Far = Grid.WorldToCell(FVector(5500.f, 5500.f, 0.f));

		ASSERT_THAT(IsTrue(Cells.Contains(Center)));
		ASSERT_THAT(IsTrue(Cells.Contains(Neighbor)));
		ASSERT_THAT(IsFalse(Cells.Contains(Far)));
	}

	TEST_METHOD(UpdateEntity_SameCell_NoChange)
	{
		FArcMassNetId Id(1);
		Grid.AddEntity(Id, FVector(100.f, 100.f, 0.f));
		Grid.UpdateEntity(Id, FVector(200.f, 200.f, 0.f));

		FIntVector2 Cell = Grid.WorldToCell(FVector(100.f, 100.f, 0.f));
		const TSet<FArcMassNetId>* Entities = Grid.CellToEntities.Find(Cell);
		ASSERT_THAT(IsNotNull(Entities));
		ASSERT_THAT(IsTrue(Entities->Contains(Id)));
		ASSERT_THAT(AreEqual(Grid.CellToEntities.Num(), 1));
	}
};
