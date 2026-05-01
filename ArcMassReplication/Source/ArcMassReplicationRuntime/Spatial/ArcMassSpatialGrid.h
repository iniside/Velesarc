// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fragments/ArcMassNetId.h"

struct ARCMASSREPLICATIONRUNTIME_API FArcMassSpatialGrid
{
	float CellSize = 10000.f;

	TMap<FIntVector2, TSet<FArcMassNetId>> CellToEntities;
	TMap<FArcMassNetId, FIntVector2> EntityToCell;

	FIntVector2 WorldToCell(FVector Position) const;
	void AddEntity(FArcMassNetId NetId, FVector Position);
	void UpdateEntity(FArcMassNetId NetId, FVector NewPosition);
	void RemoveEntity(FArcMassNetId NetId);
	void GetRelevantCells(FVector Position, float Radius, TArray<FIntVector2>& OutCells) const;
};
