// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "GameplayTagContainer.h"
#include "MassEntityHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcEntitySpawnerSubsystem.generated.h"

struct FArcEntitiesSpawnedMessage;

/**
 * Registry for spawner-to-spawned entity mappings and spawn event broadcasting.
 * Used by StateTree tasks to track which entities were spawned by which spawner.
 */
UCLASS()
class ARCENTITYSPAWNER_API UArcEntitySpawnerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- Registry ---

	/** Register entities as spawned by a spawner. */
	void RegisterSpawnedEntities(FMassEntityHandle Spawner, TConstArrayView<FMassEntityHandle> Entities);

	/** Unregister specific entities from a spawner. */
	void UnregisterSpawnedEntities(FMassEntityHandle Spawner, TConstArrayView<FMassEntityHandle> Entities);

	/** Unregister all entities for a spawner (e.g. on despawn all). */
	void UnregisterAllForSpawner(FMassEntityHandle Spawner);

	// --- Queries ---

	/** Get all entities spawned by a specific spawner. Returns empty if spawner not found. */
	TArray<FMassEntityHandle> GetSpawnedEntities(FMassEntityHandle Spawner) const;

	/** Get the spawner that created a specific entity. Returns invalid handle if not found. */
	FMassEntityHandle GetSpawnerForEntity(FMassEntityHandle SpawnedEntity) const;

	// --- Events ---

	/** Broadcast a spawn event via AsyncMessageSystem on the given channel tag. */
	void BroadcastSpawnEvent(const FArcEntitiesSpawnedMessage& Message, FGameplayTag Channel);

private:
	/** Spawner entity -> array of spawned entity handles */
	TMap<FMassEntityHandle, TArray<FMassEntityHandle>> SpawnerToEntities;

	/** Spawned entity -> spawner entity (reverse lookup) */
	TMap<FMassEntityHandle, FMassEntityHandle> EntityToSpawner;
};
