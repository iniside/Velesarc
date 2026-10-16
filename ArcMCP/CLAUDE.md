# ArcMCP

Editor-only plugin that exposes game-side tools on the engine **ModelContextProtocol** MCP server. `FArcMCPModule` registers `UArcMCPToolset` (a `UToolsetDefinition`) with `UToolsetRegistry::RegisterToolsetClass` during `FCoreDelegates::OnAllModuleLoadingPhasesComplete`. The engine ModelContextProtocol plugin then surfaces the toolset's `UFUNCTION` methods alongside the engine `editor_toolset.toolsets.*` toolsets via the standard `list_toolsets` / `describe_toolset` / `call_tool` protocol.

The old ECABridge/`IECACommand` integration is fully removed.

## Tools

### StateTree (Category: "StateTree")

| Tool | Purpose |
|------|---------|
| `ListStateTreeSchemas` | List all StateTree schema classes with their context data and allowed features |
| `ListStateTreeNodes` | List node types (tasks, conditions, evaluators, considerations, property functions); filter by Schema, Category, or text Filter |
| `CreateStateTree` | Create a compiled StateTree asset from a JSON Description string |
| `ValidateStateTree` | Validate a StateTree JSON Description without creating an asset — returns errors and warnings |

### Records

| Tool | Purpose |
|------|---------|
| `FindRecordReferencers` | Who references a JSON record id (`Type:Name`, parsed and normalized). `assets`: packages from `IAssetRegistry::GetReferencers` on `FAssetIdentifier(FArcNamedPrimaryAssetId::StaticStruct(), id)`, `EDependencyCategory::SearchableName` (marks written by `FArcNamedPrimaryAssetId::PostSerialize` on save; packages saved earlier are not found until resaved). `records`: every indexed record walked by reflection (structs, arrays, sets, maps, `FInstancedStruct`/`TInstancedStruct`, registered instanced struct wrappers such as `FArcInstancedStruct`) with the property path, e.g. `Ingredients[1](ArcRecipeIngredient_ItemDef).ItemDefinitionId`. `indexed`: whether the id has a record. Errors while the Asset Registry is loading. Bare `FPrimaryAssetId` fields are not searched. |

### Tactics

| Tool | Purpose |
|------|---------|
| `ExportTacticsRecording` | Load an ArcTactics `.utrace` recording (empty path = newest in `Saved/ArcTactics/Traces`), write the whole recording as `<trace>.json` next to it, and return a per-entity summary (Write access: it writes a file). Options: `entity`, `fromTime`/`toTime`, `queryItems` all/valid/none. Loading and the format belong to ArcGameplayDebugger (`FArcTacticsTraceLoader::LoadBlocking`, `ArcTacticsRecordingJson::Write`). |

## Structure

```
Source/ArcMCP/
├── ArcMCPModule.h/.cpp              # Module startup/shutdown — registers UArcMCPToolset
├── ArcMCP.Build.cs                  # Build config
├── ArcMCPToolset.h                  # UArcMCPToolset : UToolsetDefinition (all UFUNCTION declarations)
├── ArcMCPToolsetUtils.h             # MakeError / JsonObjToString helpers
├── ArcMCPToolset_Containers.cpp     # GetStructInContainer / SetStructFieldInContainer / AddStructToContainer / RemoveStructFromContainer
├── ArcMCPToolset_Records.cpp        # FindRecordReferencers
├── ArcMCPToolset_Semantic.cpp       # SemanticSearchStart / SemanticSearchResult
├── ArcMCPToolset_StateTree.cpp      # CreateStateTree / ValidateStateTree / ListStateTreeSchemas / ListStateTreeNodes
├── ArcMCPToolset_Tactics.cpp        # ExportTacticsRecording
├── ArcMCPToolset_Trace.cpp          # QueryTrace
└── StateTree/
    ├── ArcStateTreeBuilder.h/.cpp          # JSON → compiled UStateTree asset
    ├── ArcStateTreeJsonParser.h/.cpp       # JSON parsing + structural validation
    ├── ArcStateTreeNodeRegistry.h/.cpp     # Reflection-based node discovery, snake_case aliases
    └── ArcStateTreeTypes.h                 # Intermediate description structs (FArcSTDescription, etc.)
```

## Key Patterns

- **F-prefix handling**: UHT strips the `F` prefix from USTRUCT names in reflection. All struct name lookups accept names with or without the prefix (e.g. both `FArcItemFragment_Stacks` and `ArcItemFragment_Stacks` work).
- **Snake_case aliases**: StateTree node types are aliased from C++ struct names for AI-friendly discovery (e.g. `FStateTreeCompareIntCondition` → `condition_compare_int`). Property functions use `function_` prefix.
- **Metadata extraction**: Registry captures `DisplayName` and tooltip `Description` from UE reflection. Property function instance data properties are classified with `Role` ("input"/"output") based on their UPROPERTY Category metadata.
- **Lazy registry init**: `FArcStateTreeNodeRegistry` defers discovery until `FCoreDelegates::OnAllModuleLoadingPhasesComplete` — same delegate used by module registration.
- **Validate before create**: `ValidateStateTree` lets an AI verify a description before committing to asset creation.

## Dependencies

- **ToolsetRegistry** (public) — `UToolsetDefinition`, `UToolsetRegistry`
- **ArcCore, ArcCoreEditor** (private)
- **StateTreeModule, StateTreeEditorModule** (private) — StateTree API
- **Json, JsonUtilities** (public) — parameter serialization
- **UnrealEd, AssetTools, AssetRegistry** (private) — editor asset operations
- **ArcGameplayDebugger** (private, plugin dependency) — tactics recording load and JSON export
