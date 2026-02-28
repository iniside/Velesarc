# ArcSettlement Plugin

## Overview

Meta-knowledge layer for settlements, regions, and world state queries. An **information broker** — indexes facts about the world, answers queries, and holds action postings. Never decides anything for NPCs.

## Core Concepts

- **Knowledge Entry** (`FArcKnowledgeEntry`): A fact — gameplay tags + location + optional `FInstancedStruct` payload. Generic: resource availability, events, capabilities are all just different tag combinations.
- **Knowledge Query** (`FArcKnowledgeQuery`): Tags + `FInstancedStruct` filter/scorer pipelines. Returns scored results. NPCs decide what to do with them.
- **Actions**: Knowledge entries with action semantics (quests/requests). Posted by one NPC, queried and claimed by another.
- **Settlements** (`FArcSettlementData`): Knowledge scopes — grouping mechanism for entries. Not Mass entities.
- **Regions** (`FArcRegionData`): Optional spatial grouping of settlements. Distance queries always work; regions provide designer-controlled grouping.

## Architecture

### Subsystem
`UArcSettlementSubsystem` (UTickableWorldSubsystem) owns:
- Knowledge index (tag-indexed `TMap`)
- Settlement registry
- Region registry
- Query execution pipeline

### Pipeline Pattern
Follows TQS pattern: `FInstancedStruct` arrays with `ExcludeBaseStruct`, base types with virtual methods:
- `FArcKnowledgeFilter` — binary pass/fail
- `FArcKnowledgeScorer` — 0-1 normalized, weight exponent for compensatory scoring

### Data Assets
- `UArcSettlementDefinition` — Settlement type (tags, initial knowledge, radius)
- `UArcRegionDefinition` — Region type (tags)
- `UArcKnowledgeQueryDefinition` — Reusable query pipeline (tags, filters, scorers, selection mode)

### Handles
All lightweight `uint32` wrappers with `Make()` factory: `FArcKnowledgeHandle`, `FArcSettlementHandle`, `FArcRegionHandle`, `FArcFactionHandle`.

### Level Actors
- `AArcSettlementVolume` — Sphere-based actor defining settlement bounds. Registers with subsystem on BeginPlay via `CreateSettlement()`.
- `AArcRegionVolume` — Sphere-based actor defining region bounds. Registers via `CreateRegion()`. Settlements within the sphere auto-associate.

### Mass Integration
- `FArcSettlementMemberFragment` — per-entity: settlement handle + role tag + registered knowledge handles
- `FArcSettlementMemberConfigFragment` — const shared: capability tags per archetype
- `FArcSettlementMemberTag` — tag for observer processors
- `UArcSettlementMemberTrait` — adds fragment + tag + const shared config
- `UArcSettlementMemberAddObserver` — on Add: auto-assigns settlement, registers capabilities as knowledge
- `UArcSettlementMemberRemoveObserver` — on Remove: cleans up registered knowledge

### StateTree Integration
- `FArcMassQueryKnowledgeTask` — Runs knowledge query, outputs results (supports DataAsset or inline definition)
- `FArcMassRegisterKnowledgeTask` — Registers knowledge entry at entity's location/settlement
- `FArcMassPostActionTask` — Posts action (quest/request) for others to claim
- `FArcMassClaimActionTask` — Claims an action, with release/complete on exit options
- `FArcSettlementHasKnowledgeCondition` — Boolean: settlement has matching knowledge?
- `FArcKnowledgeAvailabilityConsideration` — Utility score based on nearby matching knowledge

### TQS Integration
- `FArcTQSGenerator_KnowledgeEntries` — Generates TQS target items from knowledge query results. Bridges knowledge system into TQS for spatial scoring.
- `FArcTQSContextProvider_Settlements` — Provides settlement locations as context points for TQS generators.

### Debug
- `FGameplayDebuggerCategory_ArcSettlement` — Visualizes settlements, regions, knowledge entries. Shift+S to cycle settlements.

## Key Files
- `ArcSettlementTypes.h` — Handles, enums
- `ArcKnowledgeEntry.h` — Entry struct
- `ArcKnowledgeFilter.h/.cpp` — Filter base + built-ins
- `ArcKnowledgeScorer.h/.cpp` — Scorer base + built-ins
- `ArcKnowledgeQuery.h` — Query, context, result structs
- `ArcSettlementSubsystem.h/.cpp` — Central subsystem
- `ArcSettlementVolume.h/.cpp` — Settlement level actor (sphere bounds)
- `ArcRegionVolume.h/.cpp` — Region level actor (sphere bounds)
- `Mass/ArcSettlementFragments.h` — Mass fragments, tags, shared fragments
- `Mass/ArcSettlementTrait.h/.cpp` — Settlement member trait
- `Mass/ArcSettlementObservers.h/.cpp` — Add/remove observer processors
- `StateTree/*.h/.cpp` — All StateTree tasks, conditions, considerations
- `TQS/ArcTQSGenerator_KnowledgeEntries.h/.cpp` — Knowledge-to-TQS bridge generator
- `TQS/ArcTQSContextProvider_Settlements.h/.cpp` — Settlement locations context provider
- `Debug/GameplayDebuggerCategory_ArcSettlement.h/.cpp` — Gameplay debugger visualization
