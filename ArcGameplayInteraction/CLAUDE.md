# ArcGameplayInteraction Plugin

Extensions on top of Epic's **GameplayInteractions** / **SmartObjects** / **StateTree** plugins. Drives player→world interactions (open container, harvest, craft, sit, loot) against **Mass entities** (no actor) and **actor IInteractionTarget** components. Non-GAS: trigger flows through a replicated command, not an ability (the old `UArcGA_Interact` ability is retired).

Module is **flat** (no Public/Private). Source root: `Source/ArcGameplayInteraction/`. Includes are module-relative.

## Mental Model — two execution paths

A SmartObject behavior is run by an `FArcGameplayInteractionContext`, which owns an inner **StateTree** (the SO behavior definition's `StateTreeReference`). Two components drive that context:

| Driver | Target | Owner | Entry point |
|--------|--------|-------|-------------|
| `UArcEntityInteractionComponent` | Mass entity (no actor) | `PlayerState` (source side) | `ExecuteEntityInteraction(Entity, CtxData, OptionTag, SlotIndex)` |
| `UArcSmartObjectInteractionComponent` | Actor that *is* a SmartObject | the actor (target side) | `BeginInteraction()` / `StartInteraction(Pawn)` |

Both: resolve SO handle → `MarkSlotAsClaimed` (High) → `GetBehaviorDefinition<UArcInteractGameplayInteractionSmartObjectBehaviorDefinition>` → `Context.Activate(Def)` → tick via `FTSTicker` until the inner StateTree finishes → `SOSubsystem->Release(ClaimHandle)`.

## Trigger pipeline (entity path — the primary one)

1. `UArcCoreInteractionSourceComponent` (on PlayerState, client-only) runs a **continuous** `UArcTargetingPreset` request loop (`StartAsync` with `bRequeueOnCompletion=true`).
2. On completion `HandleTargetingCompleted` picks `Candidates[0]`, resolves an actor `IInteractionTarget` *or* falls back to the candidate's `FMassEntityHandle`.
3. On a new entity target it reads `FArcSmartObjectDefinitionSharedFragment` (from **ArcMass**) → builds `FArcInteractionEntityOption`s from each slot's `FArcInteractionDisplayData` (from **ArcCore**), broadcasts `Interaction_Acquired` async message (carries query results, entity, location, `InteractionMappingContext`), and **pushes the per-target IMC + binds each option's InputAction** (`SetupInteractionInput`).
4. Player presses an option key → `HandleInteractionOptionTriggered(OptionTag, SlotIndex)` → `Arcx::SendServerCommand<FArcInteractCommand>(...)` capturing pawn, entity, eye location/direction, OptionTag, SlotIndex.
5. Server `FArcInteractCommand::Execute` → finds `UArcEntityInteractionComponent` on PlayerState → `ExecuteEntityInteraction`. OptionTag selects a per-option `OverrideBehavior`; else slot default. SlotIndex (`INDEX_NONE` ⇒ 0) picks which slot to claim.

Targeting filter that gates candidates: `FArcTargetingFilter_Interactable` (keeps actors implementing `IInteractionTarget`, or entities carrying a valid `FArcSmartObjectDefinitionSharedFragment`).

Async message ids: `Arcx::InteractionMessages::Interaction_Acquired` / `Interaction_Invalidated` (UI listens to these).

## Key types

- `FArcGameplayInteractionContext` (`ArcGameplayInteractionContext.h`) — wraps StateTree exec. `Activate/Tick/Deactivate(MassContext*)`, `SendEvent`. Setters for claimed handle, slot transform, context/SO entity, OptionTag, owner. `Activate` builds an `FMassStateTreeExecutionContext` when a `FMassExecutionContext*` is passed (enables Mass nodes/fragment binding in the inner tree).
- `UArcGameplayInteractionStateTreeSchema` — schema for interaction trees; collects `FMassStateTreeDependency`. Filters which structs/classes/external items the tree may use.
- `UArcGameplayInteractionSmartObjectBehaviorDefinition` (+ `…AI…`, `…Interact…` subclasses) — `USmartObjectBehaviorDefinition` holding the `FStateTreeReference`. The component looks up the `…Interact…` subclass specifically.
- `FArcInteractCommand` (`Commands/`) — self-contained `FArcReplicatedCommand` (rules: [ArcCore/CLAUDE.md](../ArcCore/CLAUDE.md)). Carries pawn/target/entity/location/direction/OptionTag/SlotIndex.
- `FArcMassCustomInputAction_TriggerInteraction` — `FArcMassCustomInputAction` whose `OnTriggered` reads the source component and sends the command (legacy single-trigger path; per-option path binds dynamically instead).
- `FArcInteractionStructModel` (`Interaction/ArcInteractionStructModel.h`) — local-only MVVM struct model (`VM_Interaction`, `UArcInteractionViewModel` in ArcUIModels). Each tick reads the PlayerState's `UArcCoreInteractionSourceComponent`: entity target → top-centre of the entity's own `FArcMassPhysicsBodyFragment::Body` bounds (never the cached `HitResult.PhysicsObject`), actor target → top-centre of the actor's bounds, plus `AnchorOffset`; otherwise `bHasTarget = false`.
- Schema base markers: `FArcGameplayInteractionMassTaskBase/ConditionBase/EvaluatorBase` — Mass StateTree nodes must derive from these to appear in the Arc schema picker.

## Schema & context binding — the core authoring contract

`FArcGameplayInteractionContext::Activate/Tick/Deactivate/SendEvent` all take an **optional `FMassExecutionContext*`**. When passed, `FExecutionContextHolder` builds an `FMassStateTreeExecutionContext` (with `SetEntity(ContextEntity)`) so the inner tree gets Mass nodes + fragment binding; otherwise a plain `FStateTreeExecutionContext`. Both component drivers currently call the **no-MassContext** overload (they tick from `FTSTicker`, not a processor) — fragments still resolve via the collect-external-data callback below, which holds its own `FMassEntityManager*`.

**Context data the schema exposes** (`UArcGameplayInteractionStateTreeSchema` ctor → `ContextDataDescs`, populated in `SetContextRequirements`):

| Name | Type | Source | Req |
|------|------|--------|-----|
| `ContextEntityHandle` | `FMassEntityHandle` | interacting player's entity | Required |
| `SmartObjectEntityHandle` | `FMassEntityHandle` | interacted (SO) entity | Required |
| `SlotTransform` | `FTransform` | claimed slot transform | Required |
| `InteractionOptionTag` | `FGameplayTag` | chosen option (empty on legacy path) | **Optional** |
| `ContextActor`, `SmartObjectActor`, `SmartObjectClaimedHandle`, `SlotEntranceHandle`, `AbortContext` | — | inherited from `UGameplayInteractionStateTreeSchema` | (SmartObjectActor Optional) |

**External-data auto-resolution** (`SetContextRequirements` installs a `SetCollectExternalDataCallback`): a node external-data dependency is resolved against the **ContextEntity** when it's a `FMassFragment` / `FMassSharedFragment` / `FMassConstSharedFragment` (via `FMassEntityView`), against the world for `UWorldSubsystem`, or against `ContextActor` for `AActor` / `UActorComponent`. Missing **Required** deps log `LogGameplayInteractions` errors; the rest still bind. This is why a task can declare `TStateTreeExternalData<FSomeFragment>` and read it off the SO entity with no manual plumbing.

**Schema gating:** `IsStructAllowed` = GameplayInteraction tasks/conditions + StateTree common bases + the three `FArcGameplayInteractionMass*Base` markers. `IsClassAllowed` = **false** (no UObject nodes). `IsExternalItemAllowed` = parent + `UWorldSubsystem` + the three Mass fragment kinds. `Link` walks all nodes (incl. linked sub-trees) collecting `FMassStateTreeDependency` via `FStateTreeDependencyBuilder`.

**Activate guards** (all `LogGameplayInteractions` Error): re-activating a *different* root tree while the previous is still Running; `!IsValid()` (no claimed handle / owner); null StateTree on the definition; reentrant Activate/Tick; schema mismatch (`ContextActor`/`SmartObjectActor` not of `Schema->GetContextActorClass()`/`GetSmartObjectActorClass()`). Activate writes `FGameplayInteractionSlotUserData::UserActor` into the claimed slot (cleared on Deactivate) — only when a `ContextActor` is present.

## Two trigger input paths

- **Legacy single-trigger:** `FArcMassCustomInputAction_TriggerInteraction::OnTriggered` reads the source component's current target and sends `FArcInteractCommand` with **empty OptionTag + `INDEX_NONE`** (server claims slot 0, slot-default behavior). One bind, authored in the input action config.
- **Per-option dynamic:** `UArcCoreInteractionSourceComponent::SetupInteractionInput` binds each acquired option's `InputAction` at acquire time and pushes the per-target IMC (priority `100`, below ability IMCs at `999`); `HandleInteractionOptionTriggered` sends the command with that option's OptionTag + SlotIndex. Torn down on target loss / EndPlay.

## Logging

No native tags / cvars / log category defined in this plugin. Context + components log under engine `LogGameplayInteractions`. Each task owns a static category: `LogArcGIGrantItems`, `LogArcGIGrantLootFromTable`, `LogArcGITransferLoot`, `LogArcGIDestroyEntity`, `LogArcGIBindCraftingStation`. The `Arc.Interaction.Acquired/Invalidated` tags are defined in **ArcCore**, not here.

## StateTree tasks (authored inside SO behavior trees)

| Task | Header | Does |
|------|--------|------|
| Grant Items From Definitions | `Tasks/ArcGI_GrantItemsTask.h` | Reads `FArcItemGrantFragment` on SO entity → grants into dest entity's `FArcMassItemStoreFragment` (`StoreType` selects sub-store) |
| Grant Loot From Table | `Tasks/ArcGI_GrantLootFromTableTask.h` | Rolls `FArcLootTableConfigFragment` (+ optional `FArcLootContext`) into dest store |
| Transfer Loot Container Items | `Tasks/ArcGI_TransferLootContainerTask.h` | Moves `FArcLootContainerFragment` items → dest store |
| Destroy Entity | `Tasks/ArcGI_DestroyEntityTask.h` | Destroys the SO entity |
| Arc Bind Crafting Station | `Tasks/ArcGI_BindCraftingStationTask.h` | Binds `UArcCraftingViewModel` (by `CraftingViewModelTag`) to the station until ExitState; no UI. The document is opened by `FArcRmlOpenDocumentTask` (ArcRmlUIMassGame) in the same state |
| Play Anim Montage Per Skeleton | `ArcGI_PlayAnimMontagePerSkeletonTask.h` | `TMap<USkeleton*, UAnimMontage*>` montage by actor skeleton |
| Enhanced Input Event | `ArcGI_EnhancedInputEventTask.h` | Waits on `UInputAction`s, fires `OnInputStarted` StateTree delegate |
| Increment Integer | `ArcGI_IncrementInteger.h` | `TStateTreePropertyRef<int32>` demo/util |

Tasks deriving from `FGameplayInteractionStateTreeTask` use `SmartObjectEntity`/`ContextEntity` instance-data handles that the schema **auto-binds**. Per [[feedback_statetree_param_category]]: manual-bind fields use `Category="Parameter"`, auto-bound context uses `Category="Context"`.

## Data setup dependencies (live in OTHER plugins)

- **ArcMass**: `UArcMassSmartObjectOwnerTrait` adds `FArcSmartObjectOwnerFragment` (runtime handle) + `FArcSmartObjectDefinitionSharedFragment` (SO definition asset + `InteractionMappingContext`). `FArcSmartObjectTag` opts the entity into observer-driven SO create/destroy. See [ArcMass/Source/ArcMass/SmartObject/ArcMassSmartObjectFragments.h](../ArcMass/Source/ArcMass/SmartObject/ArcMassSmartObjectFragments.h).
- **ArcCore**: `FArcInteractionDisplayData` (slot `DefinitionData`) + `FArcInteractionOption` (per-option DisplayName/OptionTag/InputAction/OverrideBehavior). See [ArcCore/Source/ArcCore/Interaction/ArcInteractionDisplayData.h](../ArcCore/Source/ArcCore/Interaction/ArcInteractionDisplayData.h).

## Working-on-this-plugin notes

- **Mass rules apply**: deferred commands, no runtime fragment removal/recomposition, sparse-fragment API — see root [CLAUDE.md](../../../../CLAUDE.md) + [mass-patterns](../../../../docs/reference/mass-patterns.md).
- Reading SO shared fragment: `EM.GetConstSharedFragmentDataPtr<FArcSmartObjectDefinitionSharedFragment>(Entity)`; owner fragment: `EM.GetFragmentDataPtr<FArcSmartObjectOwnerFragment>(Entity)`.
- `SlotIndex` from the client **must** match server `GetAllSlots()` ordering — the acquire loop iterates *every* definition slot (incl. ones without display data) to keep indices aligned. Don't "compact" that loop.
- Build verified via `UnrealBuildTool ArcGameEditor` (link `UnrealEditor-ArcGameplayInteraction.dll`). Deps are heavy (ArcFrontend, ArcUIModels, ArcTargeting, ArcMassItemsRuntime) — check Build.cs before assuming a symbol is reachable.
- Commit scope: `arcgameplayinteraction`.
- C++ nav: LSP/clangd first ([clang-nav](C:/Users/lukas/.claude/skills/clang-nav/SKILL.md)), grep is a lower bound.
