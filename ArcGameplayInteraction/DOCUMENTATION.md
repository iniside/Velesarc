# ArcGameplayInteraction — User Guide

> Personal reference for how this plugin works and how to set it up in the editor.
> Plugin friendly name: **ArcGameplayInteraction** — "Extensions for Gameplay Interactions engine plugin."

## What it does

Lets the player interact with the world — open a container, harvest a resource, loot a corpse, sit, use a crafting station — by looking at a target and pressing a key. It works against:

- **Mass entities that have no actor** (the common case in this project — trees, deposits, dropped loot, crafting stations spawned as Mass entities), and
- **Actors** that expose an interaction component.

Under the hood each interaction runs a **SmartObject behavior**, which is a small **StateTree** of tasks (grant items, roll loot, transfer container, play a montage, open a UI panel, destroy the entity, …). The plugin is **not** GAS-based — pressing the interact key sends a lightweight replicated command to the server.

## The moving parts (what each piece is for)

| Piece | Type | Where it lives | Role |
|-------|------|----------------|------|
| **Interaction source** | `UArcCoreInteractionSourceComponent` | on the player's **PlayerState** | Continuously targets what you're looking at, shows the prompt, binds the keys, sends the trigger. Client-only. |
| **Entity executor** | `UArcEntityInteractionComponent` | on the player's **PlayerState** | Server-side. Runs the interaction against a Mass entity target. |
| **Actor interaction** | `UArcSmartObjectInteractionComponent` | on an **interactable actor** | Lets an actor (that is itself a SmartObject) be interacted with. |
| **Behavior definition** | `UArcInteractGameplayInteractionSmartObjectBehaviorDefinition` | inside a **SmartObject Definition** asset | Holds the StateTree that runs when interacted with. |
| **Display data** | `FArcInteractionDisplayData` / `FArcInteractionOption` | on the **SmartObject slot** | The label(s), tag(s), input action(s), and optional per-option behavior shown to the player. |
| **Mass trait** | `UArcMassSmartObjectOwnerTrait` (in ArcMass) | on the entity's **Mass config / trait list** | Gives the entity a SmartObject + references the SO definition + the interaction IMC. |
| **Targeting preset** | `UArcTargetingPreset` with `Filter: Interactable` | assigned on the source component | Decides which entity/actor you're currently aiming at. |

## How a single interaction flows (so I remember)

1. The source component on my PlayerState runs a **targeting loop** every frame using its `InteractionTargetingPreset`.
2. The best candidate is resolved to either an actor `IInteractionTarget` or a **Mass entity**.
3. For an entity, it reads the entity's SmartObject definition, builds the list of **options** from the slot's display data, broadcasts an "Interaction Acquired" message (UI listens to this to show the prompt), **pushes the interaction Input Mapping Context**, and **binds each option's Input Action**.
4. I press the bound key → it sends `FArcInteractCommand` to the server with the chosen **OptionTag** + **SlotIndex**.
5. Server claims the slot, picks the behavior (the option's override, or the slot default), and runs its StateTree to completion, then releases the slot.

When I look away, an "Interaction Invalidated" message fires and the keys/IMC are removed.

## Editor setup — interactable Mass entity (the main path)

This is the recipe for making a tree / container / station interactable.

### 1. Author a SmartObject Definition asset
- Create a **SmartObject Definition**.
- Add at least one **slot**.
- On the slot's **Definition Data**, add **`Arc Interaction Display Data`** (`FArcInteractionDisplayData`):
  - **Single option:** fill `DisplayName` (e.g. "Harvest") + `InteractionTag`. Leave `Options` empty.
  - **Multiple options:** add entries to **`Options`** (`FArcInteractionOption`), each with `DisplayName`, `OptionTag`, `InputAction`, and optionally an `OverrideBehavior` (a different behavior definition for that option). If `OverrideBehavior` is empty it uses the slot's default behavior.
- Add an **`Arc Interact Gameplay Interaction SmartObject Behavior Definition`** to the slot's behavior definitions and set its **`StateTreeReference`** to the interaction StateTree (see step 3). Its schema must be **Arc Gameplay Interactions**.

> Slot order matters: the SlotIndex sent from the client is the slot's index in the definition. Don't reorder slots casually.

### 2. Put the SmartObject on the Mass entity
- On the entity's Mass config / trait list, add **`Arc Smart Object Owner`** trait (`UArcMassSmartObjectOwnerTrait`, from ArcMass).
  - Set **`SmartObjectDefinition`** to the asset from step 1.
  - Set **`InteractionMappingContext`** to the IMC that contains the option Input Actions (this IMC is auto-pushed while the entity is targeted).
- Make sure the entity also gets the SmartObject tag/observer path so its SmartObject is actually created at runtime (the trait + `FArcSmartObjectTag` drive the observer-managed create/destroy).

### 3. Author the interaction StateTree
- Create a **StateTree** asset, schema = **Arc Gameplay Interactions** (`UArcGameplayInteractionStateTreeSchema`).
- Add tasks (see the table below). The `SmartObjectEntity` and `ContextEntity` handles are **auto-bound** by the schema — `SmartObjectEntity` = the thing being interacted with, `ContextEntity` = the interacting player's entity.
- Reference this StateTree from the behavior definition in step 1.

### 4. Wire the player
- On the **PlayerState** (Mass player-state extension), add:
  - **`UArcCoreInteractionSourceComponent`** — set its **`InteractionTargetingPreset`** to a targeting preset whose pipeline ends with the **`Filter: Interactable`** node.
  - **`UArcEntityInteractionComponent`** — no config; it just needs to exist so the server command can find it.
- The Input Actions referenced by the options must exist; they're bound automatically when a target is acquired (I don't bind them manually in a base IMC for the per-option path).
- For UI, listen for the **Interaction Acquired / Invalidated** async messages to show/hide the prompt widget.

## Editor setup — interactable Actor (alternative path)

If the interactable is a real Actor (not a Mass-only entity):
- Add **`UArcSmartObjectInteractionComponent`** to the actor.
- Fill its **`TargetConfigs`** (array of `InteractionTargetConfiguration`) — these are advertised to the targeting/UI system.
- The actor must have a `UMassAgentComponent` resolving to a Mass entity that owns the SmartObject (same SO setup as above).
- The same source component on the PlayerState discovers it via the `IInteractionTarget` interface and triggers `BeginInteraction`.

## Events & messages it sends

There are **two** separate event channels. Don't confuse them — one is for UI, one is internal to the StateTree.

### 1. Async gameplay messages — for the UI / HUD

Broadcast on the client via the **Async Message System** (`FAsyncGameplayMessageSystem`, reached through `UAsyncMessageWorldSubsystem`) whenever the targeted interaction changes. This is what the interaction prompt widget should listen to. Message structs live in **ArcCore** (`ArcCoreAsyncMessageTypes.h`).

| When | Message tag | Payload struct | Carries |
|------|-------------|----------------|---------|
| A new target is acquired (you look at something interactable) | `Arc.Interaction.Acquired` | `FArcInteractionAcquiredMessage` | `Target` (actor interface, if any), `EntityHandle` (entity target, if any), `Location` (world prompt anchor), `QueryResults` (the available options), `MappingContext` (the pushed IMC) |
| The current target is lost (you look away / it dies) | `Arc.Interaction.Invalidated` | `FArcInteractionInvalidatedMessage` | `PreviousTarget`, `PreviousQueryResults` |

The available interaction options inside `QueryResults.AvailableInteractions` are `FArcInteractionEntityOption` structs (`DisplayName`, `OptionTag`, `InputAction`, `SlotIndex`) — iterate these to draw one prompt row per option with the right key glyph.

**To consume (e.g. in a widget):** get the shared message system from `UAsyncMessageWorldSubsystem` and register a listener for the `Arc.Interaction.Acquired` / `Arc.Interaction.Invalidated` message ids. Show the prompt on Acquired, hide it on Invalidated.

> These messages are **client-side only** — the source component early-outs on a dedicated server, so nothing is broadcast there.

Note: `UArcCoreInteractionSourceComponent` also fires Blueprint hooks `OnInteractionChanged` (target changed) and `OnTriggerInteraction` (local trigger pressed, before server confirms — for cosmetic cues only). These are component delegates, not async messages.

### 2. StateTree events — inside the running interaction

Once a behavior is running, `FArcGameplayInteractionContext::SendEvent(Tag, Payload, Origin)` forwards an event **into the inner StateTree** (received on its next tick). Use this to drive transitions from gameplay code while an interaction is active. Within a tree, the **Enhanced Input Event** task also raises the `OnInputStarted` StateTree delegate when a bound `UInputAction` starts — wire that delegate to a transition to react to input mid-interaction.

## StateTree tasks available

Author these inside the interaction StateTree. All under the **"Gameplay Interactions"** category in the node picker.

| Task | What it does | Key fields |
|------|--------------|-----------|
| **Grant Items From Definitions** | Grants items defined on the SO entity into the player's store | `StoreType` (which item store fragment receives them) |
| **Grant Loot From Table** | Rolls a loot table on the SO entity into the player's store | `StoreType`, optional `LootContext` |
| **Transfer Loot Container Items** | Moves all items from a loot-container entity into the player's store | `StoreType` |
| **Destroy Entity** | Destroys the interacted entity (e.g. harvested node) | — |
| **Arc Bind Crafting Station** | Binds the crafting view model to the station for the duration of the state; opens no UI | `CraftingViewModelTag` |
| **Play Anim Montage Per Skeleton** | Plays the right montage for the actor's skeleton | `MontageMap` (Skeleton → Montage) |
| **Enhanced Input Event** | Waits for input actions, fires a StateTree delegate | `InputActions`, `OnInputStarted` |
| **Increment Integer** | Utility: increments a bound int | `Counter` (property ref) |

A typical "harvest" tree = **Grant Items / Grant Loot** → (optional **Play Montage**) → **Destroy Entity**. A "container" tree = **Transfer Loot Container Items**. A "crafting station" tree = **Arc Bind Crafting Station** next to **Arc Open RML Document** (ArcRmlUIMassGame) in one state; closing the document completes the state.

## What a task can read inside the tree (context & fragments)

The **Arc Gameplay Interactions** schema feeds every tree a fixed set of **context data** you can bind to in any task/condition (in the property binding dropdown):

| Context | Type | Meaning |
|---------|------|---------|
| `ContextEntityHandle` | Mass entity | the interacting **player's** entity |
| `SmartObjectEntityHandle` | Mass entity | the **interacted** entity (the SO) |
| `SlotTransform` | Transform | the claimed slot's world transform |
| `InteractionOptionTag` | Gameplay tag | which option the player chose (empty on the legacy single-trigger path) |
| `ContextActor` / `SmartObjectActor` | Actor | only set on the actor path; empty for entity-only interactions |
| `SmartObjectClaimedHandle`, `SlotEntranceHandle`, `AbortContext` | — | inherited SmartObject/GameplayInteraction context |

**Reading Mass fragments directly:** any task can declare a **Mass fragment** (regular, shared, or const-shared) as external data and the schema **auto-resolves it from `ContextEntityHandle`** — no manual lookup needed. That's how the grant/loot/transfer tasks read their source fragments. For a C++ task to show up in this schema's node picker it must derive from `Arc Gameplay Interaction Mass Task/Condition/Evaluator` base (or be a standard GameplayInteraction task). Plain UObject-based nodes are **not** allowed by this schema.

## Quick troubleshooting

- **No prompt appears** → targeting preset missing/`Filter: Interactable` not in the pipeline, or the entity has no `Arc Smart Object Owner` trait / its SmartObject wasn't created.
- **Prompt shows but key does nothing** → option's `InputAction` not set, or the IMC (`InteractionMappingContext` on the trait) doesn't contain that action.
- **Wrong behavior runs** → option's `OverrideBehavior` empty (falls back to slot default), or the behavior definition isn't the **Arc Interact** subclass the executor looks up.
- **"SlotIndex out of range" / "OptionTag not found" warnings** (LogGameplayInteractions) → slot order changed, or the OptionTag on the display data doesn't match what was sent.
- **"Trying to activate interaction with different root State Tree while the previous tree is still running"** → a new interaction was started before the previous one finished/released its slot.
- **"Expecting … schema" / "Expecting Actor to be of type …"** → the behavior's StateTree isn't using the **Arc Gameplay Interactions** schema, or the schema's Context Actor / SmartObject Actor class doesn't match the actor used.
- **"Missing Fragment / Shared Fragment / Const Shared Fragment"** → a task in the tree declared a Mass fragment as required external data that the interacted entity doesn't actually have.
- **Nothing happens on a dedicated server with no client** → the source component is client-only by design; targeting/trigger originate on the client.

**Where to look in the log:** the context + components log under engine category **`LogGameplayInteractions`**. Each task has its own category for granular filtering: `LogArcGIGrantItems`, `LogArcGIGrantLootFromTable`, `LogArcGITransferLoot`, `LogArcGIDestroyEntity`, `LogArcGIBindCraftingStation`. (This plugin defines no gameplay tags, console variables, or settings of its own — the `Arc.Interaction.*` tags live in ArcCore.)

## Related plugins
- **ArcMass** — `UArcMassSmartObjectOwnerTrait`, SO fragments, SO observer.
- **ArcCore** — `FArcInteractionDisplayData` / `FArcInteractionOption`, replicated command base.
- **ArcTargeting** — the targeting pipeline + `Filter: Interactable` node.
- **ArcMassItemsRuntime** — item stores / loot fragments the grant/transfer tasks read & write.
- **ArcFrontend / ArcUIModels** — crafting view model for the crafting task.
