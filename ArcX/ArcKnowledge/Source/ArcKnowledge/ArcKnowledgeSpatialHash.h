// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcKnowledgeTypes.h"

/**
 * Lightweight spatial hash for knowledge entries.
 * Follows the same grid-based pattern as FMassSpatialHashGrid but stores
 * FArcKnowledgeHandle instead of FMassEntityHandle.
 *
 * Owned by UArcKnowledgeSubsystem — maintained inline during knowledge CRUD.
 */
struct ARCKNOWLEDGE_API FArcKnowledgeSpatialHash
{
	struct FEntry
	{
		FArcKnowledgeHandle Handle;
		FVector Location;
	};

	/** Grid cell size in world units. Larger = fewer cells, less precision. */
	float CellSize = 10000.0f;

	/** Add an entry to the spatial hash. */
	void Add(FArcKnowledgeHandle Handle, const FVector& Location);

	/** Remove an entry from the spatial hash. */
	void Remove(FArcKnowledgeHandle Handle, const FVector& Location);

	/** Update an entry's position (remove from old cell, add to new). */
	void Update(FArcKnowledgeHandle Handle, const FVector& OldLocation, const FVector& NewLocation);

	/** Clear all entries. */
	void Clear();

	/** Gather all handles within a sphere. Appends to OutHandles. */
	void QuerySphere(const FVector& Center, float Radius, TArray<FArcKnowledgeHandle>& OutHandles) const;

	/** Gather handles with distance info within a sphere. Appends to OutEntries. */
	void QuerySphereWithDistance(const FVector& Center, float Radius, TArray<FEntry>& OutEntries) const;

private:
	FIntVector WorldToGrid(const FVector& Pos) const
	{
		return FIntVector(
			FMath::FloorToInt(Pos.X / CellSize),
			FMath::FloorToInt(Pos.Y / CellSize),
			FMath::FloorToInt(Pos.Z / CellSize)
		);
	}

	TMap<FIntVector, TArray<FEntry>> Cells;
};

// ---------- Inline implementations ----------

inline void FArcKnowledgeSpatialHash::Add(FArcKnowledgeHandle Handle, const FVector& Location)
{
	const FIntVector GridCoords = WorldToGrid(Location);
	Cells.FindOrAdd(GridCoords).Add({Handle, Location});
}

inline void FArcKnowledgeSpatialHash::Remove(FArcKnowledgeHandle Handle, const FVector& Location)
{
	const FIntVector GridCoords = WorldToGrid(Location);
	if (TArray<FEntry>* Bucket = Cells.Find(GridCoords))
	{
		Bucket->RemoveAll([Handle](const FEntry& E) { return E.Handle == Handle; });
		if (Bucket->IsEmpty())
		{
			Cells.Remove(GridCoords);
		}
	}
}

inline void FArcKnowledgeSpatialHash::Update(FArcKnowledgeHandle Handle, const FVector& OldLocation, const FVector& NewLocation)
{
	const FIntVector OldGrid = WorldToGrid(OldLocation);
	const FIntVector NewGrid = WorldToGrid(NewLocation);

	if (OldGrid == NewGrid)
	{
		// Same cell — just update position in place
		if (TArray<FEntry>* Bucket = Cells.Find(OldGrid))
		{
			for (FEntry& E : *Bucket)
			{
				if (E.Handle == Handle)
				{
					E.Location = NewLocation;
					break;
				}
			}
		}
	}
	else
	{
		Remove(Handle, OldLocation);
		Add(Handle, NewLocation);
	}
}

inline void FArcKnowledgeSpatialHash::Clear()
{
	Cells.Empty();
}

inline void FArcKnowledgeSpatialHash::QuerySphere(const FVector& Center, float Radius, TArray<FArcKnowledgeHandle>& OutHandles) const
{
	const float RadiusSq = Radius * Radius;
	const FVector Extent(Radius);
	const FIntVector MinGrid = WorldToGrid(Center - Extent);
	const FIntVector MaxGrid = WorldToGrid(Center + Extent);

	for (int32 X = MinGrid.X; X <= MaxGrid.X; ++X)
	{
		for (int32 Y = MinGrid.Y; Y <= MaxGrid.Y; ++Y)
		{
			for (int32 Z = MinGrid.Z; Z <= MaxGrid.Z; ++Z)
			{
				if (const TArray<FEntry>* Bucket = Cells.Find(FIntVector(X, Y, Z)))
				{
					for (const FEntry& E : *Bucket)
					{
						if (FVector::DistSquared(Center, E.Location) <= RadiusSq)
						{
							OutHandles.Add(E.Handle);
						}
					}
				}
			}
		}
	}
}

inline void FArcKnowledgeSpatialHash::QuerySphereWithDistance(const FVector& Center, float Radius, TArray<FEntry>& OutEntries) const
{
	const float RadiusSq = Radius * Radius;
	const FVector Extent(Radius);
	const FIntVector MinGrid = WorldToGrid(Center - Extent);
	const FIntVector MaxGrid = WorldToGrid(Center + Extent);

	for (int32 X = MinGrid.X; X <= MaxGrid.X; ++X)
	{
		for (int32 Y = MinGrid.Y; Y <= MaxGrid.Y; ++Y)
		{
			for (int32 Z = MinGrid.Z; Z <= MaxGrid.Z; ++Z)
			{
				if (const TArray<FEntry>* Bucket = Cells.Find(FIntVector(X, Y, Z)))
				{
					for (const FEntry& E : *Bucket)
					{
						const float DistSq = FVector::DistSquared(Center, E.Location);
						if (DistSq <= RadiusSq)
						{
							OutEntries.Add(E);
						}
					}
				}
			}
		}
	}
}
