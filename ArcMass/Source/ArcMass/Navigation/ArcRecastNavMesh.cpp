// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Navigation/ArcRecastNavMesh.h"

#include "ArcMass/Navigation/ArcMassNavInvokerSubsystem.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcRecastNavMesh)

void AArcRecastNavMesh::RemoveTiles(const TArray<FIntPoint>& Tiles)
{
	check(IsInGameThread());

	TRACE_CPUPROFILER_EVENT_SCOPE(AArcRecastNavMesh_RemoveTiles);
	
	if (bInsideMassApply)
	{
		Super::RemoveTiles(Tiles);
		return;
	}

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		Super::RemoveTiles(Tiles);
		return;
	}

	const UArcMassNavInvokerSubsystem* Subsystem = World->GetSubsystem<UArcMassNavInvokerSubsystem>();
	if (Subsystem == nullptr)
	{
		Super::RemoveTiles(Tiles);
		return;
	}

	const TMap<FIntPoint, int32>& RefCounts = Subsystem->GetTileRefCounts();
	if (RefCounts.Num() == 0)
	{
		Super::RemoveTiles(Tiles);
		return;
	}

	// Quick scan: does any tile overlap our protected set?
	bool bHasProtectedTile = false;
	for (const FIntPoint& Tile : Tiles)
	{
		if (RefCounts.Contains(Tile))
		{
			bHasProtectedTile = true;
			break;
		}
	}

	if (!bHasProtectedTile)
	{
		Super::RemoveTiles(Tiles);
		return;
	}

	TArray<FIntPoint> FilteredTiles;
	FilteredTiles.Reserve(Tiles.Num());
	for (const FIntPoint& Tile : Tiles)
	{
		if (!RefCounts.Contains(Tile))
		{
			FilteredTiles.Add(Tile);
		}
	}

	if (FilteredTiles.Num() > 0)
	{
		Super::RemoveTiles(FilteredTiles);
	}
}

void AArcRecastNavMesh::UpdateActiveTiles(const TArray<FNavigationInvokerRaw>& InvokerLocations)
{
	Super::UpdateActiveTiles(InvokerLocations);

	TRACE_CPUPROFILER_EVENT_SCOPE(AArcRecastNavMesh_RemoveTiles);
	
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const UArcMassNavInvokerSubsystem* Subsystem = World->GetSubsystem<UArcMassNavInvokerSubsystem>();
	if (Subsystem == nullptr)
	{
		return;
	}

	const TMap<FIntPoint, int32>& RefCounts = Subsystem->GetTileRefCounts();
	if (RefCounts.Num() == 0)
	{
		return;
	}

	TSet<FIntPoint>& ActiveTiles = GetActiveTileSet();
	for (const TPair<FIntPoint, int32>& TileEntry : RefCounts)
	{
		ActiveTiles.Add(TileEntry.Key);
	}
}
