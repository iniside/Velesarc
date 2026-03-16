# ArcCore Plugin

Core gameplay systems: items, inventory, attachments, quick bar, replicated commands.

## Items System Architecture

Composition-based item system. Items are NOT subclassed — all behavior is defined by combining fragments on a definition asset.

### Three-Layer Type Hierarchy

| Type | Purpose | Mutability | Storage |
|------|---------|-----------|---------|
| `UArcItemDefinition` | Data asset template | Immutable asset | Asset registry |
| `FArcItemSpec` | Serializable item snapshot | Immutable struct | Transfer, save/load, Mass entities |
| `FArcItemData` | Runtime item with full state | Mutable, `TSharedPtr` | `UArcItemsStoreComponent` |

### UArcItemDefinition (Data Asset)

Defines an item type through composition. Never subclassed.

- `FragmentSet` — `TSet<FArcInstancedStruct>` of `FArcItemFragment` subclasses
- `ScalableFloatFragmentSet` — Level-scaling numeric data (`FArcScalableFloatItemFragment`)
- `StackMethod` — `TInstancedStruct<FArcItemStackMethod>` (never stack, stack by type, unique only)
- `AddFragment<T>()` — Programmatic fragment addition (test/editor use)
- `FindFragment<T>()` — Retrieve fragment by type

### FArcItemSpec (Immutable Snapshot)

Serializable specification for creating or transferring items.

- `ItemId` (FGuid), `Amount` (uint16), `Level` (uint8)
- `ItemDefinitionId` — `FPrimaryAssetId` referencing `UArcItemDefinition`
- `InstanceData` — Polymorphic persistent instance fragments
- Factory: `NewItem()` from definition or asset ID
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
| `FArcItemFragment_GrantedAbilities` | GAS abilities granted on equip |
| `FArcItemFragment_GrantedPassiveAbilities` | Auto-applied passive abilities |
| `FArcItemFragment_GrantedGameplayEffects` | GAS effects applied on equip |
| `FArcItemFragment_AttributeModifier` | Attribute modifiers via SetByCaller GameplayEffect |
| `FArcItemFragment_ItemStats` | Item stat data |
| `FArcItemFragment_UIDescription` | UI display data |
| `FArcItemFragment_SocketSlots` | Attachment socket slot definitions |
| `FArcItemFragment_DropActor` | Spawning dropped item actors |
| `FArcItemFragment_Droppable` | Droppability behavior |
| `FArcItemFragment_RequiredItems` | Items required to use this item |
| `FArcItemFragment_RequirePresetSlot` | Restricts to specific slots |
| `FArcItemFragment_NiagaraSystem` | VFX reference |
| `FArcItemFragment_AbilityMontages` | Animation montages |

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
