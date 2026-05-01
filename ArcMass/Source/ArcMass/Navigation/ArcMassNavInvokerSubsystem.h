// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "ArcMass/Navigation/ArcMassNavInvokerTypes.h"
#include "ArcMassNavInvokerSubsystem.generated.h"

struct FNavMeshDirtyTileElement;

/**
 * World subsystem that owns a sparse array of FArcNavInvokerData slots and
 * manages navmesh tile activation/deactivation on behalf of Mass entities.
 *
 * Processors allocate a slot per entity (AllocateSlot), update its location each
 * frame (UpdateSlotLocation), and free it on entity removal (FreeSlot).  The
 * subsystem tracks per-tile reference counts so that overlapping invokers share
 * tiles without double-building.
 */
UCLASS()
class ARCMASS_API UArcMassNavInvokerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- Slot management ----

	/** Register a new invoker and return its index. */
	int32 AllocateSlot(const FArcNavInvokerData& Data);

	/** Unregister an invoker by index (removes its tiles). */
	void FreeSlot(int32 Index);

	/** Update the world-space location of an existing slot in-place (no tile rebuild). */
	void UpdateSlotLocation(int32 Index, const FVector& NewLocation);

	// ---- Diff buffer (parallel pipeline) ----

	/** Replace pending diffs with the provided array. Called by cell-detect processor (worker thread). */
	void SwapInDiffs(TArray<FArcNavTileDiffEntry>&& InDiffs);

	/** Drain and return all pending diffs. Called by tile-apply processor (game thread). */
	TArray<FArcNavTileDiffEntry> DrainDiffs();

	// ---- Refcount manipulation (used by batched apply) ----

	/** Increment refcount for a single tile. Does not trigger navmesh operations. */
	void IncrementTileRefCount(const FIntPoint& Tile);

	/** Decrement refcount for a single tile. Removes entry at zero. Does not trigger navmesh operations. */
	void DecrementTileRefCount(const FIntPoint& Tile);

	/** Apply batched tile rebuild/remove in a single ForEachValidNavMesh pass. */
	void ApplyBatchedTileChanges(
		const TArray<FNavMeshDirtyTileElement>& TilesToRebuild,
		const TArray<FIntPoint>& TilesToRemove);

	// ---- Config accessors for parallel processor ----

	FVector GetNavmeshOrigin() const { return NavmeshOrigin; }
	float GetTileDim() const { return TileDim; }

	// ---- Accessors ----

	const TSparseArray<FArcNavInvokerData>& GetSlots() const { return InvokerSlots; }
	float GetCellSize() const { return CellSize; }
	const TMap<FIntPoint, int32>& GetTileRefCounts() const { return TileRefCounts; }

	// ---- Tile management ----

	/** Add tiles for an invoker (GenerationRadius). Increments refcounts; triggers RebuildTile on 0->1. */
	void AddTilesForInvoker(const FArcNavInvokerData& Invoker);

	/** Remove tiles for an invoker (RemovalRadius). Decrements refcounts; triggers RemoveTiles on 1->0. */
	void RemoveTilesForInvoker(const FArcNavInvokerData& Invoker);

private:
	/** Lazy-cache the Recast navmesh origin and tile dimension from the first valid ARecastNavMesh. */
	void CacheRecastConfig();

	/** Populate CachedNavMeshActors via TActorIterator. */
	void GatherNavMeshActors();

	/** Callback for world actor spawned — appends new ARecastNavMesh actors to cache. */
	void HandleActorSpawned(AActor* Actor);

	/** Iterate all cached navmesh actors, pruning stale weak pointers inline. */
	template<typename TCallback>
	void ForEachValidNavMesh(TCallback&& Callback)
	{
		for (int32 i = CachedNavMeshActors.Num() - 1; i >= 0; --i)
		{
			ARecastNavMesh* NavMesh = CachedNavMeshActors[i].Get();
			if (NavMesh == nullptr || NavMesh->GetGenerator() == nullptr)
			{
				CachedNavMeshActors.RemoveAtSwap(i);
				continue;
			}
			Callback(*NavMesh);
		}
	}

	// ---- Data ----

	/** Sparse array of active invoker slots. Index is stable until FreeSlot is called. */
	TSparseArray<FArcNavInvokerData> InvokerSlots;

	/** Per-tile reference count. When a count reaches 0 the tile is deactivated. */
	TMap<FIntPoint, int32> TileRefCounts;

	/** Pending tile diffs from the parallel cell-detect processor, consumed by the apply processor. */
	TArray<FArcNavTileDiffEntry> PendingDiffs;

	/** Coarse cell size for dirty-tracking in the Mass processor (not a navmesh parameter). */
	float CellSize = 1000.f;

	// ---- Cached Recast config ----

	/** NavMeshOriginOffset from the first valid ARecastNavMesh (UE world space). */
	FVector NavmeshOrigin = FVector::ZeroVector;

	/** Tile size in world units (ARecastNavMesh::GetTileSizeUU()). */
	float TileDim = 0.f;

	/** True only when NavmeshOrigin and TileDim have been populated and TileDim > 0. */
	bool bRecastConfigCached = false;

	/** Cached navmesh actors for tile manipulation calls. */
	TArray<TWeakObjectPtr<ARecastNavMesh>> CachedNavMeshActors;

	/** Handle for the OnActorSpawned delegate, unbound in Deinitialize. */
	FDelegateHandle OnActorSpawnedHandle;
};
