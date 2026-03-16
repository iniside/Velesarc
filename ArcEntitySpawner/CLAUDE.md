# ArcEntitySpawner

Mass entity spawner with TQS integration and StateTree-driven spawn modes.

**Status:** Experimental
**Dependencies:** MassGameplay, StateTree, ArcMass, ArcAI

## Overview

ArcEntitySpawner provides two approaches to spawning Mass entities:

1. **Default Mode** — Place `AArcEntitySpawner` in a level, configure a TQS query for spawn locations, and it spawns entities automatically on BeginPlay (or on demand via `DoSpawning()`).
2. **StateTree Mode** — A Mass agent runs a StateTree on the spawner entity, using `FArcSpawnEntitiesTask` / `FArcDespawnEntitiesTask` tasks for full programmatic control over when and where entities spawn.

Both modes support post-spawn initializers, entity tracking, and async event broadcasting.

**Networking:** Spawning only runs on Server / Standalone. Clients are skipped in `BeginPlay`.

## Quick Start — Default Mode

1. Place an `AArcEntitySpawner` actor in your level.
2. Set **SpawnMode** to `Default`.
3. Add one or more entries to **EntityTypes** (each entry is an `FMassSpawnedEntityType` with a `UMassEntityConfigAsset` reference and a proportion weight).
4. Set **Count** to the number of entities to spawn.
5. Assign a **SpawnQuery** (`UArcTQSQueryDefinition`) — this TQS query runs to determine spawn locations.
6. Leave **bAutoSpawnOnBeginPlay** enabled (default) to spawn on level start.
7. Optionally add **PostSpawnInitializers** to customize entities after spawn.

On BeginPlay the spawner will:
- Stream in entity config assets
- Run the TQS query asynchronously
- Spawn entities at the returned locations
- Run all initializers while the entity creation context is alive (before observers fire)
- Broadcast `OnSpawningFinished`

### Spawning On Demand

Disable `bAutoSpawnOnBeginPlay` and call these Blueprint-callable methods:

```cpp
// Trigger spawn (runs TQS query, spawns at results)
Spawner->DoSpawning();

// Spawn at explicit locations (bypasses TQS)
Spawner->SpawnEntitiesAtLocations(Count, Locations);

// Spawn from pre-computed TQS results
Spawner->SpawnEntitiesFromResults(TQSResults);

// Despawn all
Spawner->DoDespawning();
```

### Scaling Spawn Count

```cpp
Spawner->ScaleSpawningCount(0.5f); // Halve the count
int32 Effective = Spawner->GetSpawnCount(); // Count * SpawningCountScale
```

## Quick Start — StateTree Mode

1. Place an `AArcEntitySpawner` actor in your level.
2. Set **SpawnMode** to `StateTree`.
3. The spawner has a `UMassAgentComponent` — assign a StateTree asset to it.
4. In the StateTree, use `FArcSpawnEntitiesTask` and `FArcDespawnEntitiesTask` tasks.

On BeginPlay the spawner enables the Mass agent, which starts running the StateTree. The StateTree tasks handle spawning/despawning directly through the Mass subsystems.

**Note:** The StateTree tasks are self-contained — they work independently from `AArcEntitySpawner` and can be used in any Mass entity's StateTree.

## Post-Spawn Initializers

Initializers run while the `FEntityCreationContext` is alive, meaning observers haven't fired yet. This lets you modify fragments before any observer-based system sees the entities.

### Built-in Initializers

**UArcSetFragmentValuesInitializer** — Sets fragment values on all spawned entities:

```
PostSpawnInitializers:
  - UArcSetFragmentValuesInitializer
      FragmentValues:
        - FMyCustomFragment { Health: 100, Team: 2 }
        - FAnotherFragment { Speed: 500.0 }
```

Each `FInstancedStruct` entry must be a `FMassFragment` subtype. The initializer copies the struct data into the matching fragment on every spawned entity.

**UArcAddTagsInitializer** — Adds Mass tags to spawned entities:

```
PostSpawnInitializers:
  - UArcAddTagsInitializer
      Tags:
        - FMyCustomTag {}
        - FAnotherTag {}
```

Each `FInstancedStruct` entry must be a `FMassTag` subtype.

### Custom Initializers

Subclass `UArcEntityInitializer`:

**C++:**
```cpp
UCLASS()
class UMyInitializer : public UArcEntityInitializer
{
    GENERATED_BODY()
public:
    virtual void InitializeEntities(
        FMassEntityManager& EntityManager,
        TArrayView<const FMassEntityHandle> Entities) override
    {
        for (const FMassEntityHandle& Entity : Entities)
        {
            FMyFragment& Fragment = EntityManager.GetFragmentDataChecked<FMyFragment>(Entity);
            Fragment.Value = ComputedValue;
        }
    }
};
```

**Blueprint:** Override `BP_InitializeEntities(const TArray<FMassEntityHandle>& Entities)`.

## StateTree Tasks

### FArcSpawnEntitiesTask

Spawns Mass entities. Works in any Mass entity's StateTree (not just `AArcEntitySpawner`).

**Instance Data (bindable):**

| Field | Type | Direction | Description |
|-------|------|-----------|-------------|
| Count | int32 | Input | Number of entities to spawn (default: 10) |
| EntityType | FMassSpawnedEntityType | Input | Entity config + proportion |
| SpawnLocations | TArray\<FArcTQSTargetItem\> | Input (Optional) | Locations from a TQS query |
| SpawnedEntities | TArray\<FMassEntityHandle\> | Output | Handles of spawned entities |
| bSuccess | bool | Output | Whether spawn succeeded |

**Task Properties:**

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| bMarkSpawnedEntities | bool | false | Adds `FArcSpawnedByFragment` to each spawned entity |
| MessageChannel | FGameplayTag | (empty) | If set, broadcasts `FArcEntitiesSpawnedMessage` on this channel |

**Behavior:**
- Spawns entities, sets `FTransformFragment` from locations
- Tracks spawned entities in `FArcSpawnerFragment` on the owning entity
- Registers with `UArcEntitySpawnerSubsystem`
- Signals entity with `NewStateTreeTaskRequired` to advance the StateTree
- Returns `Succeeded`

### FArcDespawnEntitiesTask

Despawns Mass entities. Works in any Mass entity's StateTree.

**Instance Data (bindable):**

| Field | Type | Direction | Description |
|-------|------|-----------|-------------|
| EntitiesToDespawn | TArray\<FMassEntityHandle\> | Input (Optional) | Specific entities to despawn |
| bDespawnAll | bool | Input | If true, despawn all from `FArcSpawnerFragment` (default: true) |
| bSuccess | bool | Output | Whether despawn succeeded |

**Behavior:**
- If `bDespawnAll`: reads all entities from the owning entity's `FArcSpawnerFragment`
- Otherwise: uses the bound `EntitiesToDespawn` list
- Destroys via `UMassSpawnerSubsystem`, updates `FArcSpawnerFragment`, unregisters from subsystem
- Signals `NewStateTreeTaskRequired`, returns `Succeeded`

## Entity Tracking

### Fragments

**FArcSpawnerFragment** (on the spawner entity):
```cpp
TArray<FMassEntityHandle> SpawnedEntities;
```
Tracks all entities spawned by this spawner.

**FArcSpawnedByFragment** (on spawned entities, optional):
```cpp
FMassEntityHandle SpawnerEntity;
```
Back-reference to the spawner. Only added when `bMarkSpawnedEntities = true` (StateTree task) or when using the subsystem directly.

### UArcEntitySpawnerSubsystem

World subsystem maintaining bidirectional spawner-to-entity mappings.

```cpp
// Query
TArray<FMassEntityHandle> Entities = Subsystem->GetSpawnedEntities(SpawnerHandle);
FMassEntityHandle Spawner = Subsystem->GetSpawnerForEntity(EntityHandle);

// Register/unregister (called internally by tasks and spawner actor)
Subsystem->RegisterSpawnedEntities(Spawner, Entities);
Subsystem->UnregisterSpawnedEntities(Spawner, Entities);
Subsystem->UnregisterAllForSpawner(Spawner);
```

### Event Broadcasting

```cpp
Subsystem->BroadcastSpawnEvent(Message, ChannelTag);
```

Broadcasts `FArcEntitiesSpawnedMessage` via the async message system:
```cpp
struct FArcEntitiesSpawnedMessage
{
    TArray<FMassEntityHandle> SpawnedEntities;
    TArray<FVector> SpawnLocations;
    FMassEntityHandle SpawnerEntity;
    TSoftObjectPtr<UMassEntityConfigAsset> EntityConfig;
};
```

Listen for these events using `UAsyncMessageWorldSubsystem` on the configured `FGameplayTag` channel.

## Delegates

`AArcEntitySpawner` exposes two multicast delegates:

```cpp
UPROPERTY(BlueprintAssignable)
FArcEntitySpawnerOnSpawningFinished OnSpawningFinished;

UPROPERTY(BlueprintAssignable)
FArcEntitySpawnerOnDespawningFinished OnDespawningFinished;
```

Bind to these in Blueprint or C++ to react when spawning/despawning completes.

## Debug (Editor Only)

`AArcEntitySpawner` exposes two editor-only methods (callable from details panel or console):

- `DEBUG_Spawn()` — Triggers spawning
- `DEBUG_Clear()` — Triggers despawning

## Architecture Summary

```
AArcEntitySpawner (Actor)
  |
  |-- Default Mode ---------> TQS Query -> Spawn at results
  |                              |
  |-- StateTree Mode --------> UMassAgentComponent -> StateTree
  |                              |
  |                         FArcSpawnEntitiesTask / FArcDespawnEntitiesTask
  |
  |-- PostSpawnInitializers --> UArcSetFragmentValuesInitializer
  |                         --> UArcAddTagsInitializer
  |                         --> (custom subclasses)
  |
  +-- Tracking --------------> FArcSpawnerFragment (on spawner)
                            --> FArcSpawnedByFragment (on spawned)
                            --> UArcEntitySpawnerSubsystem (bidirectional registry)
                            --> FArcEntitiesSpawnedMessage (async broadcast)
```

## File Map

| File | Purpose |
|------|---------|
| `ArcEntitySpawner.h/cpp` | Main spawner actor |
| `ArcEntitySpawnerTypes.h/cpp` | Enums, fragments, messages, base initializer class |
| `ArcEntityInitializers.h/cpp` | Built-in initializers (set fragments, add tags) |
| `ArcEntitySpawnerSubsystem.h/cpp` | Entity tracking registry and event broadcasting |
| `StateTree/ArcSpawnEntitiesTask.h/cpp` | StateTree spawn task |
| `StateTree/ArcDespawnEntitiesTask.h/cpp` | StateTree despawn task |
