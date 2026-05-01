// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Navigation/ArcMassNavInvokerSubsystem.h"

#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "ArcMass/Navigation/ArcMassNavInvokerTypes.h"
#include "ArcMass/Navigation/ArcRecastNavMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassNavInvokerSubsystem)

// ---------------------------------------------------------------------------

void UArcMassNavInvokerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	if (World != nullptr)
	{
		OnActorSpawnedHandle = World->AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateUObject(this, &UArcMassNavInvokerSubsystem::HandleActorSpawned));
	}
}

void UArcMassNavInvokerSubsystem::Deinitialize()
{
	UWorld* World = GetWorld();
	if (World != nullptr && OnActorSpawnedHandle.IsValid())
	{
		World->RemoveOnActorSpawnedHandler(OnActorSpawnedHandle);
		OnActorSpawnedHandle.Reset();
	}

	InvokerSlots.Empty();
	TileRefCounts.Empty();
	PendingDiffs.Empty();
	CachedNavMeshActors.Empty();
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Slot management
// ---------------------------------------------------------------------------

int32 UArcMassNavInvokerSubsystem::AllocateSlot(const FArcNavInvokerData& Data)
{
	if (!bRecastConfigCached)
	{
		CacheRecastConfig();
	}

	const int32 Index = InvokerSlots.Add(Data);
	AddTilesForInvoker(Data);
	return Index;
}

void UArcMassNavInvokerSubsystem::FreeSlot(int32 Index)
{
	if (!InvokerSlots.IsValidIndex(Index))
	{
		return;
	}
	RemoveTilesForInvoker(InvokerSlots[Index]);
	InvokerSlots.RemoveAt(Index);
}

void UArcMassNavInvokerSubsystem::UpdateSlotLocation(int32 Index, const FVector& NewLocation)
{
	if (!InvokerSlots.IsValidIndex(Index))
	{
		return;
	}
	InvokerSlots[Index].Location = NewLocation;
}

// ---------------------------------------------------------------------------
// Tile management
// ---------------------------------------------------------------------------

void UArcMassNavInvokerSubsystem::AddTilesForInvoker(const FArcNavInvokerData& Invoker)
{
	const UWorld* World = GetWorld();
	if (World == nullptr || World->bIsTearingDown)
	{
		return;
	}

	if (!bRecastConfigCached)
	{
		return;
	}

	TArray<FNavMeshDirtyTileElement> TilesToRebuild;
	TilesToRebuild.Reserve(16);

	ArcNavInvoker::ForEachTileInRadius(
		Invoker.Location,
		Invoker.RadiusMin,
		NavmeshOrigin,
		TileDim,
		[this, &Invoker, &TilesToRebuild](const FIntPoint& Tile)
		{
			int32& RefCount = TileRefCounts.FindOrAdd(Tile, 0);
			++RefCount;
			if (RefCount == 1)
			{
				// First reference — activate the tile on every navmesh
				ForEachValidNavMesh([&Tile](ARecastNavMesh& NavMesh)
				{
					NavMesh.GetActiveTileSet().Add(Tile);
				});

				FNavMeshDirtyTileElement Element;
				Element.Coordinates = Tile;
				Element.InvokerDistanceSquared = 0.f;
				Element.InvokerPriority = Invoker.Priority;
				TilesToRebuild.Add(Element);
			}
		});

	if (TilesToRebuild.Num() > 0)
	{
		ForEachValidNavMesh([&TilesToRebuild](ARecastNavMesh& NavMesh)
		{
			AArcRecastNavMesh::FMassApplyScope MassApplyScope(Cast<AArcRecastNavMesh>(&NavMesh));

			NavMesh.RebuildTile(TilesToRebuild);
		});
	}
}

void UArcMassNavInvokerSubsystem::RemoveTilesForInvoker(const FArcNavInvokerData& Invoker)
{
	const UWorld* World = GetWorld();
	if (World == nullptr || World->bIsTearingDown)
	{
		return;
	}

	if (!bRecastConfigCached)
	{
		return;
	}

	TArray<FIntPoint> TilesToRemove;
	TilesToRemove.Reserve(16);

	ArcNavInvoker::ForEachTileInRadius(
		Invoker.Location,
		Invoker.RadiusMax,
		NavmeshOrigin,
		TileDim,
		[this, &TilesToRemove](const FIntPoint& Tile)
		{
			int32* RefCount = TileRefCounts.Find(Tile);
			if (RefCount == nullptr)
			{
				return;
			}

			--(*RefCount);
			if (*RefCount <= 0)
			{
				TileRefCounts.Remove(Tile);

				ForEachValidNavMesh([&Tile](ARecastNavMesh& NavMesh)
				{
					NavMesh.GetActiveTileSet().Remove(Tile);
				});

				TilesToRemove.Add(Tile);
			}
		});

	if (TilesToRemove.Num() > 0)
	{
		ForEachValidNavMesh([&TilesToRemove](ARecastNavMesh& NavMesh)
		{
			AArcRecastNavMesh::FMassApplyScope MassApplyScope(Cast<AArcRecastNavMesh>(&NavMesh));

			NavMesh.RemoveTiles(TilesToRemove);
		});
	}
}

// ---------------------------------------------------------------------------
// Diff buffer
// ---------------------------------------------------------------------------

void UArcMassNavInvokerSubsystem::SwapInDiffs(TArray<FArcNavTileDiffEntry>&& InDiffs)
{
	PendingDiffs = MoveTemp(InDiffs);
}

TArray<FArcNavTileDiffEntry> UArcMassNavInvokerSubsystem::DrainDiffs()
{
	TArray<FArcNavTileDiffEntry> Result = MoveTemp(PendingDiffs);
	PendingDiffs.Reset();
	return Result;
}

// ---------------------------------------------------------------------------
// Refcount manipulation
// ---------------------------------------------------------------------------

void UArcMassNavInvokerSubsystem::IncrementTileRefCount(const FIntPoint& Tile)
{
	int32& RefCount = TileRefCounts.FindOrAdd(Tile, 0);
	++RefCount;
}

void UArcMassNavInvokerSubsystem::DecrementTileRefCount(const FIntPoint& Tile)
{
	int32* RefCount = TileRefCounts.Find(Tile);
	if (RefCount == nullptr)
	{
		return;
	}

	--(*RefCount);
	if (*RefCount <= 0)
	{
		TileRefCounts.Remove(Tile);
	}
}

void UArcMassNavInvokerSubsystem::ApplyBatchedTileChanges(
	const TArray<FNavMeshDirtyTileElement>& TilesToRebuild,
	const TArray<FIntPoint>& TilesToRemove)
{
	const UWorld* World = GetWorld();
	if (World == nullptr || World->bIsTearingDown)
	{
		return;
	}

	ForEachValidNavMesh([&TilesToRebuild, &TilesToRemove](ARecastNavMesh& NavMesh)
	{
		AArcRecastNavMesh::FMassApplyScope MassApplyScope(Cast<AArcRecastNavMesh>(&NavMesh));

		TSet<FIntPoint>& ActiveTiles = NavMesh.GetActiveTileSet();

		for (const FNavMeshDirtyTileElement& Element : TilesToRebuild)
		{
			ActiveTiles.Add(Element.Coordinates);
		}

		for (const FIntPoint& Tile : TilesToRemove)
		{
			ActiveTiles.Remove(Tile);
		}

		if (TilesToRebuild.Num() > 0)
		{
			NavMesh.RebuildTile(TilesToRebuild);
		}
		if (TilesToRemove.Num() > 0)
		{
			NavMesh.RemoveTiles(TilesToRemove);
		}
	});
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void UArcMassNavInvokerSubsystem::GatherNavMeshActors()
{
	CachedNavMeshActors.Reset();
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<ARecastNavMesh> It(World); It; ++It)
	{
		if (ARecastNavMesh* NavMesh = *It)
		{
			CachedNavMeshActors.Add(NavMesh);
		}
	}
}

void UArcMassNavInvokerSubsystem::CacheRecastConfig()
{
	if (bRecastConfigCached)
	{
		return;
	}

	GatherNavMeshActors();

	for (TWeakObjectPtr<ARecastNavMesh>& WeakNavMesh : CachedNavMeshActors)
	{
		ARecastNavMesh* NavMesh = WeakNavMesh.Get();
		if (NavMesh == nullptr)
		{
			continue;
		}

		const float LocalTileDim = NavMesh->GetTileSizeUU();
		if (LocalTileDim <= 0.f)
		{
			continue;
		}

		NavmeshOrigin = NavMesh->NavMeshOriginOffset;
		TileDim = LocalTileDim;
		bRecastConfigCached = true;
		return;
	}
}

void UArcMassNavInvokerSubsystem::HandleActorSpawned(AActor* Actor)
{
	ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(Actor);
	if (NavMesh == nullptr)
	{
		return;
	}

	CachedNavMeshActors.Add(NavMesh);

	// If we haven't cached config yet, try now
	if (!bRecastConfigCached)
	{
		const float LocalTileDim = NavMesh->GetTileSizeUU();
		if (LocalTileDim > 0.f)
		{
			NavmeshOrigin = NavMesh->NavMeshOriginOffset;
			TileDim = LocalTileDim;
			bRecastConfigCached = true;
		}
	}
}
