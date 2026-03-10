# ArcMCP

Editor-only plugin that provides game-specific MCP commands and tools for the **ECABridge** engine plugin. ECABridge (Epic Code Assistant) is an experimental plugin by Epic Games that exposes an MCP server from the Unreal Editor, allowing AI assistants to interact with the editor through registered commands.

ArcMCP extends ECABridge with ArcGame-specific commands for StateTree creation, item system management, and asset operations.

## ECABridge Integration

All commands implement the `IECACommand` interface from ECABridge and auto-register via the `REGISTER_ECA_COMMAND(ClassName)` macro. Parameters and results use `FJsonObject`. The ECABridge plugin handles MCP protocol communication — ArcMCP only provides command implementations.

**Key types from ECABridge:**
- `IECACommand` — base interface (GetName, GetDescription, GetCategory, GetParameters, Execute)
- `FECACommandResult` — return type with `Success(JsonData)` / `Error(Message)` statics
- `FECACommandParam` — parameter definition (name, type, description, required flag)
- `REGISTER_ECA_COMMAND(Class)` — auto-registration macro, use in .cpp files

## Commands

### StateTree Commands (Category: "StateTree")

| Command | Purpose |
|---------|---------|
| `create_statetree` | Create a compiled StateTree asset from a JSON description |
| `validate_statetree` | Validate a StateTree JSON without creating the asset |
| `list_statetree_schemas` | List all StateTree schema classes with their context data |
| `list_statetree_nodes` | List available node types (tasks, conditions, evaluators, considerations, property functions) for a schema |

### Item Commands (Category: "Items")

| Command | Purpose |
|---------|---------|
| `describe_fragments` | Get rich descriptions of item fragment types (properties, pairings, prerequisites) |
| `create_item_from_template` | Create a new UArcItemDefinition from a UArcItemDefinitionTemplate |
| `template_add_fragment` | Add a fragment type to a template by struct name |
| `template_remove_fragment` | Remove a fragment type from a template by struct name |
| `update_items_from_template` | Re-sync all item definitions that reference a given template |

### Asset Commands (Category: "Asset")

| Command | Purpose |
|---------|---------|
| `create_data_asset` | Create a new UDataAsset of a specified subclass |

## Structure

```
Source/ArcMCP/
├── ArcMCPModule.h/.cpp              # Module startup/shutdown
├── ArcMCP.Build.cs                  # Build config
├── Commands/
│   ├── ArcMCPAssetCommands.h/.cpp          # create_data_asset
│   ├── ArcMCPCommand_CreateStateTree.h/.cpp
│   ├── ArcMCPCommand_ListStateTreeNodes.h/.cpp
│   ├── ArcMCPCommand_ListStateTreeSchemas.h/.cpp
│   ├── ArcMCPCommand_ValidateStateTree.h/.cpp
│   ├── ArcMCPDescribeCommands.h/.cpp       # describe_fragments
│   └── ArcMCPTemplateCommands.h/.cpp       # template CRUD + update propagation
└── StateTree/
    ├── ArcStateTreeBuilder.h/.cpp          # JSON → compiled UStateTree asset
    ├── ArcStateTreeJsonParser.h/.cpp       # JSON parsing + structural validation
    ├── ArcStateTreeNodeRegistry.h/.cpp     # Reflection-based node discovery, snake_case aliases
    └── ArcStateTreeTypes.h                 # Intermediate description structs (FArcSTDescription, etc.)
```

## Key Patterns

- **F-prefix handling**: UHT strips the `F` prefix from USTRUCT names in reflection. All struct name lookups accept names with or without the prefix (e.g. both `FArcItemFragment_Stacks` and `ArcItemFragment_Stacks` work).
- **Snake_case aliases**: StateTree node types are aliased from C++ struct names for AI-friendly discovery (e.g. `FStateTreeCompareIntCondition` → `condition_compare_int`). Property functions use `function_` prefix (e.g. `FStateTreeAddFloatPropertyFunction` → `function_add_float`).
- **Metadata extraction**: Registry captures `DisplayName` and tooltip `Description` from UE reflection for nodes and properties. Property function instance data properties are classified with `Role` ("input"/"output") based on their UPROPERTY Category metadata.
- **Lazy registry init**: StateTree node registry defers discovery until `FCoreDelegates::OnAllModuleLoadingPhasesComplete`.
- **Validation before creation**: `validate_statetree` lets AI verify a description before committing to asset creation.

## Dependencies

- **ECABridge** (public) — command framework
- **ArcCore** (private) — item definitions, fragments, templates
- **StateTreeModule, StateTreeEditorModule** (private) — StateTree API
- **Json, JsonUtilities** (public) — parameter serialization
- **UnrealEd, AssetTools, AssetRegistry** (private) — editor asset operations
