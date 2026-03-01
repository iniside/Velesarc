# ArcKnowledge Plugin

## Overview

World knowledge system — an **information broker** that indexes facts about the world, answers queries, posts advertisements, and broadcasts events. Never decides anything for NPCs.

Fully generic — the system has no hardcoded awareness of any specific domain. Settlements, factions, squads, etc. are all expressible as knowledge entries with appropriate tags + `FInstancedStruct` payloads.

## Core Concepts

- **Knowledge Entry** (`FArcKnowledgeEntry`): A fact — gameplay tags + location + optional `FInstancedStruct` payload + optional `FInstancedStruct` instruction. Generic: resource availability, events, capabilities are all just different tag combinations.
- **Knowledge Payload** (`FArcKnowledgePayload`): Empty base struct for typed payloads. Derive to create domain-specific payloads (e.g., settlement data, vacancy data). Entry's `Payload` field uses `ExcludeBaseStruct` meta to enforce subtypes.
- **Knowledge Query** (`FArcKnowledgeQuery`): Tags + `FInstancedStruct` filter/scorer pipelines. Returns scored results. NPCs decide what to do with them.
- **Advertisements**: Knowledge entries with advertisement semantics. Posted by one entity, queried and claimed by another. Carry optional behavioral instructions (`FArcAdvertisementInstruction` subtypes) telling the claimer HOW to fulfill the advertisement.
- **Advertisement Instructions** (`FArcAdvertisementInstruction`): Empty base struct for behavioral instructions attached to advertisements. `FArcAdvertisementInstruction_StateTree` carries a `FStateTreeReference` that drives claimer behavior via `FArcAdvertisementExecutionContext`.
- **Knowledge Events** (`FArcKnowledgeChangedEvent`): Push notifications when knowledge changes. Global broadcast (all listeners) and spatial broadcast (per-entity endpoints within radius).
- **Knowledge Handle** (`FArcKnowledgeHandle`): Lightweight uint32 wrapper identifying entries in the subsystem.

## Architecture

### Subsystem
`UArcKnowledgeSubsystem` (UTickableWorldSubsystem) owns:
- Knowledge index (tag-indexed `TMap`)
- Spatial hash (`FArcKnowledgeSpatialHash`) for location-based queries
- Source entity index (ownership-based cleanup)
- Query execution pipeline (with spatial pre-filtering)
- Advertisement management (post, claim, complete, cancel)
- Event broadcaster
- Lifetime-based expiration (entries with `Lifetime > 0` expire after that many seconds; default 0 = infinite)
- Spatial query API: `QueryKnowledgeInRadius()`, `FindNearestKnowledge()`

### Spatial Hash
`FArcKnowledgeSpatialHash` — lightweight grid-based spatial index for knowledge entries, following the same pattern as `FMassSpatialHashGrid` but storing `FArcKnowledgeHandle` instead of `FMassEntityHandle`.
- `TMap<FIntVector, TArray<FEntry>>` grid cells (FEntry = Handle + Location)
- Configurable cell size (default 1000 world units)
- Maintained inline during CRUD (Register/Update/Remove)
- `QuerySphere()` / `QuerySphereWithDistance()` for radius queries
- Filters expose `GetSpatialRadius()` so the query engine can extract spatial hints for pre-filtering

### Pipeline Pattern
Follows TQS pattern: `FInstancedStruct` arrays with `ExcludeBaseStruct`, base types with virtual methods:
- `FArcKnowledgeFilter` — binary pass/fail. `GetSpatialRadius()` virtual for spatial pre-filtering hints.
- `FArcKnowledgeScorer` — 0-1 normalized, weight exponent for compensatory scoring

### Advertisement Instruction System
- `FArcAdvertisementInstruction` — empty base struct for behavioral instructions (same `FInstancedStruct` + `ExcludeBaseStruct` pattern)
- `FArcAdvertisementInstruction_StateTree` — carries `FStateTreeReference` with `UArcAdvertisementStateTreeSchema`
- `UArcAdvertisementStateTreeSchema` — defines context data for advertisement StateTrees: ContextActor (optional), ExecutingEntityHandle, SourceEntityHandle, AdvertisementHandle
- `FArcAdvertisementExecutionContext` — lightweight StateTree execution context (Activate/Tick/Deactivate). Like `FArcGameplayInteractionContext` but without SmartObject dependencies. Owner is any `UObject*` (typically `UArcKnowledgeSubsystem`), not necessarily an actor. ContextActor is optional — pure Mass entities without actors can execute advertisement behaviors.
- Shared name constants: `UE::ArcKnowledge::Names` namespace (declared in `ArcAdvertisementStateTreeSchema.h`, defined in `.cpp`)

### Event System
- `FArcKnowledgeEventBroadcaster` — broadcasts via AsyncMessageSystem
- Global: `QueueMessageForBroadcast(GlobalMessageId, Event)` — all listeners
- Spatial: `QuerySphere()` → per-entity `FArcMassAsyncMessageEndpointFragment` → `QueueMessageForBroadcast(SpatialMessageId, Event, EntityEndpoint)`
- Per-entry `SpatialBroadcastRadius` override (0 = use config default)

### Data Assets
- `UArcKnowledgeQueryDefinition` — Reusable query pipeline (tags, filters, scorers, selection mode)
- `UArcKnowledgeEntryDefinition` — Reusable bundle of predefined knowledge entries (display name, tags, bounding radius, payload, initial knowledge entries). Referenced by volumes and StateTree tasks as a shared template. Subsystem API: `RegisterFromDefinition()`.

### Level Actors
- `AArcKnowledgeVolume` — Sphere-based actor marking a knowledge area (default radius 5000, cyan). Can reference a `UArcKnowledgeEntryDefinition` for shared presets. Per-instance properties override the definition when non-empty. Registers knowledge on BeginPlay, cleans up on EndPlay.
- `AArcKnowledgeRegionVolume` — Same concept at larger scale (default radius 50000, yellow). Also supports `UArcKnowledgeEntryDefinition`.

### Mass Integration
- `FArcKnowledgeMemberFragment` — per-entity: role tag + registered knowledge handles
- `FArcKnowledgeMemberConfigFragment` — const shared: optional `UArcKnowledgeEntryDefinition` + capability tags per archetype. Definition entries are registered first (primary), then capability tags individually.
- `FArcKnowledgeMemberTag` — tag for observer processors
- `UArcKnowledgeMemberTrait` — adds fragment + tag + const shared config
- `UArcKnowledgeMemberAddObserver` — on Add: registers definition entries via `RegisterFromDefinition()` (if set), then capabilities, then role as knowledge at entity location
- `UArcKnowledgeMemberRemoveObserver` — on Remove: cleans up registered knowledge

### StateTree Integration
- `FArcMassQueryKnowledgeTask` — Runs knowledge query, outputs results (supports DataAsset or inline definition)
- `FArcMassRegisterKnowledgeTask` — Registers knowledge entry at entity's location. Supports inline parameters or referencing a `UArcKnowledgeEntryDefinition` data asset
- `FArcMassPostAdvertisementTask` — Posts advertisement for others to claim (with optional payload + instruction)
- `FArcMassClaimAdvertisementTask` — Claims an advertisement, with release/complete on exit options
- `FArcMassExecuteAdvertisementTask` — Executes a claimed advertisement's StateTree instruction via `FArcAdvertisementExecutionContext`. Ticks the inner StateTree per frame; completes/cancels the advertisement on success/interrupt.
- `FArcKnowledgeAvailabilityConsideration` — Utility score based on nearby matching knowledge
- `FArcMassListenToKnowledgeEventTask` — Listens for knowledge events on entity's async message endpoint

### TQS Integration
- `FArcTQSGenerator_KnowledgeEntries` — Generates TQS target items from knowledge query results. Bridges knowledge system into TQS for spatial scoring.

### Debug
- `FGameplayDebuggerCategory_ArcKnowledge` — Visualizes knowledge entries and advertisements.

## Key Files
- `ArcKnowledgeTypes.h` — `FArcKnowledgeHandle`, `EArcKnowledgeSelectionMode`
- `ArcKnowledgePayload.h` — `FArcKnowledgePayload` base struct for typed payloads
- `ArcAdvertisementInstruction.h` — `FArcAdvertisementInstruction` base struct for behavioral instructions
- `ArcAdvertisementInstruction_StateTree.h` — StateTree-based instruction subtype
- `ArcAdvertisementStateTreeSchema.h/.cpp` — Schema for advertisement StateTrees
- `ArcAdvertisementExecutionContext.h/.cpp` — Lightweight StateTree execution context
- `ArcKnowledgeEntry.h` — Entry struct (with Payload + Instruction FInstancedStruct fields)
- `ArcKnowledgeSpatialHash.h` — Spatial hash struct (header-only, inline implementations)
- `ArcKnowledgeEntryDefinition.h` — Reusable data asset for predefined knowledge entry bundles
- `ArcKnowledgeFilter.h/.cpp` — Filter base + built-ins (MaxDistance, MinRelevance, MaxAge, PayloadType, NotClaimed)
- `ArcKnowledgeScorer.h/.cpp` — Scorer base + built-ins
- `ArcKnowledgeQuery.h` — Query, context, result structs
- `ArcKnowledgeEvent.h` — Event payload struct
- `ArcKnowledgeEventBroadcaster.h/.cpp` — Global + spatial event broadcaster
- `ArcKnowledgeSubsystem.h/.cpp` — Central subsystem
- `ArcKnowledgeVolume.h/.cpp` — Knowledge area level actor (sphere bounds)
- `ArcKnowledgeRegionVolume.h/.cpp` — Knowledge region level actor (sphere bounds)
- `Mass/ArcKnowledgeFragments.h` — Mass fragments, tags, shared fragments
- `Mass/ArcKnowledgeTrait.h/.cpp` — Knowledge member trait
- `Mass/ArcKnowledgeObservers.h/.cpp` — Add/remove observer processors
- `StateTree/*.h/.cpp` — All StateTree tasks, conditions, considerations
- `TQS/ArcTQSGenerator_KnowledgeEntries.h/.cpp` — Knowledge-to-TQS bridge generator
- `Debug/GameplayDebuggerCategory_ArcKnowledge.h/.cpp` — Gameplay debugger visualization
