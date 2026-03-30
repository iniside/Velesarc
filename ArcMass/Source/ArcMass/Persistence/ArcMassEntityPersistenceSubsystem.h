// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityHandle.h"
#include "MassEntityTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcMassEntityPersistenceSubsystem.generated.h"

struct FMassEntityManager;
struct FArcMassPersistenceConfigFragment;

namespace UE::ArcMass::Persistence
{
	/** Convert world position to cell coordinates (2D grid, Z ignored). */
	inline FIntVector WorldToCell(const FVector& Position, float CellSize)
	{
		return FIntVector(
			FMath::FloorToInt32(Position.X / CellSize),
			FMath::FloorToInt32(Position.Y / CellSize),
			0);
	}

	/** Convert cell coordinates to a storage key string. */
	inline FString CellToKey(const FIntVector& Cell)
	{
		return FString::Printf(TEXT("%d_%d"), Cell.X, Cell.Y);
	}
}

/**
 * Manages cell-based persistence for Mass entities.
 *
 * Entities with FArcMassPersistenceTag are tracked spatially.
 * Cells are loaded/saved as single blobs via the ArcPersistence backend.
 * Source entities (players) drive which cells are active.
 */
UCLASS()
class ARCMASS_API UArcMassEntityPersistenceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Configuration ──────────────────────────────────────────────────

	/** Set grid parameters. Call before loading any cells. */
	void Configure(float InCellSize, float InLoadRadius, const FGuid& InWorldId);

	float GetCellSize() const { return CellSize; }
	float GetLoadRadius() const { return LoadRadius; }

	// ── Cell Operations ────────────────────────────────────────────────

	/** Load a cell's entity data from backend and spawn entities. */
	void LoadCell(const FIntVector& Cell);

	/** Save a cell's current entity data to backend. */
	void SaveCell(const FIntVector& Cell);

	/** Save and unload a cell. Entities in other loaded cells stay alive. */
	void UnloadCell(const FIntVector& Cell);

	/** Save all loaded cells and clear state. */
	void SaveAndUnloadAll();

	bool IsCellLoaded(const FIntVector& Cell) const;
	bool IsCellLoadingOrLoaded(const FIntVector& Cell) const;

	// ── Source Management ──────────────────────────────────────────────

	/** Update which cells should be active based on source positions. */
	void UpdateActiveCells(const TArray<FVector>& SourcePositions);

	// ── Entity Cell Tracking ───────────────────────────────────────────

	/** Called by processor when an entity changes cells. */
	void OnEntityCellChanged(const FGuid& Guid, const FIntVector& OldCell,
		const FIntVector& NewCell);

	// ── Serialization ──────────────────────────────────────────────────

	/** Serialize all entities in a cell to a byte blob. */
	TArray<uint8> SerializeCell(const FIntVector& Cell);

	/** Synchronously parse a cell blob and spawn entities into the given cell. */
	void LoadCellFromData(const FIntVector& Cell, const TArray<uint8>& Data);

	// ── Internal State (public for testing) ────────────────────────────

	FMassEntityManager& GetEntityManager() const;
	FString MakeCellStorageKey(const FIntVector& Cell) const;

	/** GUID -> entity handle for all spawned persistent entities. */
	TMap<FGuid, FMassEntityHandle> ActiveEntities;

	/** Cell -> set of GUIDs currently in that cell. */
	TMap<FIntVector, TSet<FGuid>> CellEntityMap;

	/** Currently loaded cells. */
	TSet<FIntVector> LoadedCells;

	/** Cells with pending async loads (prevents double-loads). */
	TSet<FIntVector> PendingCellLoads;

	/** Cells awaiting load completion before save can execute. */
	TSet<FIntVector> PendingSaveCells;

private:
	FGuid WorldId;
	float CellSize = 10000.f;
	float LoadRadius = 50000.f;

	/** Cached entity manager pointer. */
	FMassEntityManager* CachedEntityManager = nullptr;

	/** Flush any in-flight backend I/O. Call during shutdown. */
	void FlushBackend();
};
