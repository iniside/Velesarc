# ArcCore Plugin

Core gameplay systems: items, inventory, attachments, quick bar, replicated commands.

## Items System Architecture

Composition-based item system. Items are NOT subclassed — all behavior is defined by combining fragments on a definition asset.

### Three-Layer Type Hierarchy

| Type | Purpose | Mutability | Storage |
|------|---------|-----------|---------|
| `FArcItemDefinitionData` | JSON record (`.arcitem`, ArcMassItems) | Immutable record | JSON index (`FindJsonRecord`) |
| `FArcItemSpec` | Serializable item snapshot | Immutable struct | Transfer, save/load, Mass entities |
| `FArcItemData` | Runtime item with full state | Mutable, `TSharedPtr` | `UArcItemsStoreComponent` |

### FArcItemDefinitionData (JSON record)

Defines an item type through composition. See [ArcMassItems CLAUDE.md](../ArcMassItems/CLAUDE.md).

- `FragmentSet` — `TSet<FArcInstancedStruct>` of `FArcItemFragment` subclasses
- `ScalableFloatFragmentSet` — Level-scaling numeric data (`FArcScalableFloatItemFragment`)
- `StackMethod` — `TInstancedStruct<FArcItemStackMethod>` (never stack, stack by type, unique only)
- `FindFragment<T>()` — Retrieve fragment by type; `Find(Id)` resolves a record by id

### FArcItemSpec (Immutable Snapshot)

Serializable specification for creating or transferring items.

- `ItemId` (FGuid), `Amount` (uint16), `Level` (uint8)
- `ItemDefinitionId` — id of an `ArcItem` record
- `InstanceData` — Polymorphic persistent instance fragments
- Factory: `NewItem()` from a record id
- Fluent setters: `SetItemId()`, `SetAmount()`, `SetItemLevel()`
- Iris `FFastArraySerializerItem` for network replication

### FArcItemData (Mutable Runtime State)

Full runtime item. Stored as `TSharedPtr<FArcItemData>` for pointer stability during array reallocation.

Key state:
- `Stacks` (uint16), `Level`, `OwnerId` (parent item), `Slot` (FGameplayTag)
- `AttachedItems` — Array of child `FArcItemId`s (sockets/attachments)
- `DynamicTags` — Runtime-modifiable tags
- `ItemAggregatedTags` — Merged tags from self + all attachments
- `ScalableFloatFragments` — Merged scalable values from item + all sockets
- `InstancedData` — `TMap<FName, FArcItemInstance>` for mutable per-instance data

Key methods:
- `FindFragment<T>()`, `FindSpecFragment<T>()` — Search definition and spec fragments
- `GetValue()`, `GetValueWithLevel()` — Evaluate scalable floats
- `NewFromSpec()` — Factory from `FArcItemSpec`
- `Duplicate()` — Deep copy (optionally preserving ItemId)
- Lifecycle: `Initialize()`, `OnItemAdded()`, `OnItemChanged()`, `OnPreRemove()`

### FArcItemId

`FGuid`-based unique item instance identifier. SaveGame, network serializable. Static `InvalidId` constant.

### Item Fragments

All item behavior defined through fragments on the definition:

| Fragment | Purpose |
|----------|---------|
| `FArcItemFragment_Tags` | ItemTags, AssetTags, GrantedTags, RequiredTags, DenyTags |
| `FArcItemFragment_Stacks` | Stack behavior with mutable `FArcItemInstance_Stacks` |
| `FArcItemFragment_ItemStats` | Item stat data |
| `FArcItemFragment_UIDescription` | UI display data |
| `FArcItemFragment_SocketSlots` | Attachment socket slot definitions |
| `FArcItemFragment_DropActor` | Spawning dropped item actors |
| `FArcItemFragment_Droppable` | Droppability behavior |
| `FArcItemFragment_RequiredItems` | Items required to use this item |
| `FArcItemFragment_RequirePresetSlot` | Restricts to specific slots |

> Cross-domain item fragments (ability, effect, targeting, animation, VFX bridges) live in **ArcAbilityKit** — see [its CLAUDE.md](../ArcAbilityKit/CLAUDE.md).

Base classes:
- `FArcItemFragment` — Base for all fragments (empty default)
- `FArcItemFragment_ItemInstanceBase` — Base for fragments that create mutable instances; provides lifecycle callbacks (`OnItemInitialize`, `OnItemAdded`, `OnItemChanged`, `OnPreRemoveItem`, slot/attachment callbacks)

### UArcItemsStoreComponent (Item Container)

Replicated actor component that holds items via `FArcItemsArray` (Iris fast-array serializer).

Key operations:
- `AddItem(FArcItemSpec)` → `FArcItemId` — Create and store item
- `RemoveItem(FArcItemId, Amount)` — Remove or reduce stacks
- `MoveItemFrom(OtherStore, ItemId)` — Transfer between stores
- Slot management: `AddItemToSlot()`, `RemoveItemFromSlot()`, `GetItemFromSlot()`
- Attachment: `InternalAttachToItemNew()`, `DetachItemFrom()`, `GetItemsAttachedTo()`
- Locking: `LockItem()`, `LockSlot()`, `LockAttachmentSlot()` (client-side prevention)
- Lookup: `GetItemByIdx()`, `GetItemPtr()`, `Contains()`, `GetItems()`

### FArcItemsArray (Replicated Storage)

Iris-enabled fast array wrapping `TArray<FArcItemDataInternalWrapper>`.

- `ItemsMap` — `TMap<FArcItemId, FArcItemData*>` for O(1) lookup
- `MarkItemDirtyIdx()`, `MarkItemDirtyHandle()` — Force replication

### Item Transfer & Conversion

**`FArcItemCopyContainerHelper`** — Bridge between `FArcItemData` and `FArcItemSpec`:
- `New(FArcItemData*)` — Capture current state including attachments
- `ToSpec()` — Convert `FArcItemData` → `FArcItemSpec` preserving persistent instance data
- `FromSpec(FArcItemSpec)` — Create helper from spec
- `AddItems(UArcItemsStoreComponent*)` — Add captured items to target store (recursive attachments)

### FArcItemInstance (Mutable Instance Data)

Base for polymorphic mutable runtime data on items.
- `ShouldPersist()` — Whether to save/load
- `Equals()` — Pure virtual comparison (replication change detection)
- `Duplicate()` — Deep copy
- Key subclass: `FArcItemInstance_Stacks` (mutable stack count, persists)

### Stack Methods

| Method | Behavior |
|--------|----------|
| `FArcItemStackMethod_CanNotStack` | Each item is unique instance |
| `FArcItemStackMethod_CanNotStackUnique` | Only one instance of definition allowed |
| `FArcItemStackMethod_StackByType` | Stack by definition type with `MaxStacks` (FScalableFloat) |

## Mass Entity Fragments

`Source/ArcCore/Mass/ArcMassItemFragments.h` — Generic reusable fragment:

- `FArcMassItemSpecArrayFragment` — `TArray<FArcItemSpec>` for Mass entities

## Key Helpers

- `ArcItemsHelper::GetFragment<T>(FArcItemData*)` — Fragment lookup on item data
- `ArcItemsHelper::FindMutableInstance<T>()` — Mutable instance access
- `ArcItemsHelper::TraverseHierarchy()` — Walk item → owner → sockets
- `Arcx::Utils::GetComponent(Actor, Class)` — Find component by class on actor

## Subsystems

- `UArcItemsSubsystem` — Game instance subsystem for global item events (OnItemAdded, OnItemRemoved, OnAddedToSlot, OnItemAttached, etc.)
- `UArcItemsStoreSubsystem` — Optional centralized store management

## Replicated Commands — Self-Contained

`FArcReplicatedCommand` subclasses (ArcCore / ArcCraft / ArcMassItems) must be **self-contained**. The base + dispatcher live here in ArcCore. API: `virtual bool Execute(UObject* WorldContext)` — the host (`IArcReplicatedCommandHost`, implemented by `AArcMassPlayerController`: `ServerSendReplicatedCommand_Implementation` + client counterpart) passes the PC as `WorldContext`.

**Prediction (opt-in).** A command returning true from `ExecutesOnClient()` runs `Execute` locally on a controller without authority before the RPC; standalone/server (`HasAuthority()`) skip it. Contract: a predicted `Execute` returns false before changing any local state, and then the command is not sent. Undo data lives in non-`UPROPERTY` fields, reset at the start of `Execute`; `UndoPrediction` restores it (no-op without data). The host queues predicted commands by `PredictionSequence` (assigned after the local `Execute`); on a rejected confirm it undoes later entries highest first, undoes and drops the rejected one, then re-executes the later ones; predicted sends during undo/replay are dropped with a warning. Helpers: `Arcx::ReplicatedCommands::PreparePredictedSend*` / `HandlePredictionConfirm` (`ArcReplicatedCommandDispatch.h`). Data the server must follow (e.g. a cycle's resolved target) goes into a `UPROPERTY` written by the local `Execute`.

- **Never add `World`/`PC`/`GameState` UPROPERTY fields** to stamp context before sending.
- Resolve `UWorld*` server-side via `WorldContext ? WorldContext->GetWorld() : nullptr`, then look up subsystems. Mass commands carry an `FMassEntityHandle` (via `FArcMassEntityHandleNetSerializer`). If the originating PC is genuinely needed, `Cast<APlayerController>(WorldContext)`.
- Tests pass `nullptr` for `WorldContext` when world resolution isn't needed.
- **Capture client-side trigger state** (pawn, target, aim) as command fields; don't re-resolve via `WorldContext->PC->GetPawn()` server-side — the environment may have moved on by the time the command runs.
- Use the `Arcx::SendServerCommand`/`SendClientCommand` helpers; never write per-call-site PC-resolve + handle boilerplate.

## Property JSON, JSON Assets and Id Sources

- `ArcPropertyJson` (`Source/ArcCore/Json/ArcPropertyJson.{h,cpp}`): generic reflection JSON over the engine Json DOM (`FJsonObjectConverter`): short `_structType` names, unknown-key/type diagnostics in `FArcPropertyJsonReport`, `ExportStructDelta`. Used by the ArcTactics and ArcTargetQuery editor/MCP JSON; not used by JSON assets.
- `ArcStructTypeNames` (`Json/ArcStructTypeNames.h`): `_structType` short name resolution (`FindType`, `GetTypeName`), shared by `ArcPropertyJson` and the record serializer.
- `GetMetaData`/`HasMetaData` checks (`BaseStruct`/`ExcludeBaseStruct`) in both serializers are compiled only under `WITH_METADATA`; cooked builds rely on editor validation.
- JSON asset runtime core (`Source/ArcCore/JsonAssets/`): type registry (`ArcJsonAssets::RegisterType`), content roots (`ArcJsonAssetPaths::GetContentRoots`, `OnContentRootChanged`; built and kept current by `FArcCoreModule`), file layer (`ArcJsonAssetFile`), yyjson record serializer (`ArcJsonRecord`, depends on the ArcJson plugin). The index and the shared records (`FindJsonRecord`) live in `UArcCoreAssetManager`. Format, index and rules: [ArcJsonAssets CLAUDE.md](../ArcJsonAssets/CLAUDE.md).
- `ArcCoreEditor/ArcPrimaryAssetIdSources.h`: extension point for the `FArcNamedPrimaryAssetId` details picker. `ArcPrimaryAssetIdSources::Register(Type, {GetEntries, BrowseTo})`; when every type in `AllowedTypes` has a source, the picker lists the sources' entries instead of the engine asset picker. Used by [ArcJsonAssets](../ArcJsonAssets/CLAUDE.md).
- `FArcNamedPrimaryAssetId::PostSerialize` marks a valid id as an Asset Registry searchable name on save (`MarkSearchableName(FArcNamedPrimaryAssetId::StaticStruct(), "Type:Name")`, like gameplay tags); queried by ArcMCP `FindRecordReferencers`. Packages saved before this carry no mark until resaved. A value equal to its archetype is not stored, so an inherited value marks only the package that stores it.
