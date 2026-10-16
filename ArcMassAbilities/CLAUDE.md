# ArcMassAbilities Plugin

Mass-side ability system. Hybrid of GAS concepts (active abilities, granted tags, effects, attribute capture) running entirely on Mass entities, driven by per-ability StateTree task graphs.

Pure substrate types (`FArcAbilityCustomDataBase`, `FArcAbilityCustomData`, `FArcActiveAbility`) live here. Concrete cross-domain content that couples abilities with items, targeting, animation, VFX lives in `ArcAbilityKit`.

## Signal Protocol — `AbilityStateTreeTickRequired` and `AbilityInputReceived`

The ability pipeline has two distinct entry points driven by separate signals. Mixing them causes double-dispatch and ordering races.

- **`AbilityInputReceived`** — fired by `ArcAbilityFunctions::PushInput` / `ReleaseInput`. Wakes `UArcAbilityInputProcessor`, which decides whether to `Start()` a new ability and seeds its instance data. Do **not** also fire `AbilityStateTreeTickRequired` from input dispatch — the input processor is responsible for kicking the tree, and double-signaling races the input pass against the tick pass.
- **`AbilityStateTreeTickRequired`** — subscribed by `UArcAbilityStateTreeTickProcessor`. On signal, the processor runs `ExecContext.Tick(DeltaTime)` for every active ability on the entity. StateTree internally drives state transitions (Tick → ExitState → EnterState) inside that one call and then the processor tears down any ability that finished.

### When to fire `AbilityStateTreeTickRequired`

**Only from outside the Mass tick.** Specifically: native callbacks that mutate task state or call `FinishTask` when no tree tick is currently running. Anim `EndedDelegate` / `BlendingOutDelegate`, EnhancedInput `BindActionValueLambda`, async-resolver completions. Capture `Entity` into the lambda for the signal call.

**Do NOT signal from inside `Tick` / `EnterState` / `ExitState`.** Anything that happens there is already inside a tree tick — state changes and `BroadcastDelegate` calls propagate synchronously. A `Tick` that returns `Succeeded` / `Failed` runs the next state's `EnterState` in the same `ExecContext.Tick()` call; an extra signal is redundant work that obscures the data flow.

### Other input-processor rules

- **Don't tear down completed abilities in `UArcAbilityInputProcessor::SignalEntities`.** The tick processor's `SignalEntities` already removes granted tags, calls `Stop()`, and frees the StateTree instance handle after Tick returns Succeeded/Failed. Doing it on the input side races the cleanup.
- **Don't eager-`ExecContext.Tick(0.f)` right after `Start()`** — the signal pipeline ticks the tree on the next pass; eager-ticking duplicates the work and races the cleanup loop on the same frame.

## StateTree Task Sparse Fragments

See the "Sparse Fragment Lifecycle" section in the root `CLAUDE.md`. Every task that seeds a sparse fragment in `EnterState` and removes it in `ExitState` must use the **Set + idempotent seed + generation-fence** pattern, otherwise consecutive task instances (or two tasks claiming the same singleton) race on the per-entity slot.

## Attribute Capture Defs — construction, accessors, and scoped-modifier matching

Durable reference for how `FArcAttributeCaptureDef`s are built, exposed, read, and matched in the effect/execution pipeline. Established 2026-05-29 (capture-def macro refactor + scoped-modifier order-independence refactor).

### The type

`FArcAttributeCaptureDef` ([ArcAttributeCapture.h](Source/ArcMassAbilities/Effects/ArcAttributeCapture.h), module **ArcMassAbilities**) = the *identity* of one captured attribute:

- `FArcAttributeRef Attribute` — `(UScriptStruct* FragmentType, FName PropertyName)`; built by the `ARC_MASS_ATTRIBUTE_ACCESSORS(Owner, Attr)` macro's `static FArcAttributeRef Get##Attr##Attribute()`.
- `EArcCaptureSource CaptureSource` — `Source` (effect instigator) or `Target` (receiver).
- `bool bSnapshot` — `true` = resolve once at spec-build; `false` = re-read live each call.

`operator==` / `GetTypeHash` compare **all three** fields (`FArcAttributeRef::operator==` is `FragmentType` + `PropertyName`, ignoring its mutable `CachedProperty`). This full-triple equality is the match key used everywhere below — two captures of the same attribute under different source/snapshot are distinct defs.

### Building a def — the template engine

`Arc::Capture::MakeCaptureDef<AttrGetter, Source, bSnapshot>()` (in `ArcAttributeCapture.h`). The attribute getter is a **non-type template parameter** (`FArcAttributeRef (*)()`), so each `(getter, source, snapshot)` triple is a distinct instantiation. Returns **by value** — the def is tiny and consumers copy it into `Captures` regardless.

```cpp
template <FArcAttributeRef (*AttrGetter)(), EArcCaptureSource InSource, bool bInSnapshot>
FArcAttributeCaptureDef MakeCaptureDef();
```

Why NTTP, not a runtime arg: a getter passed as a *function argument* with a `static` local would make all calls sharing `<Source,Snap>` collide on one cached def (returning the first attribute's). NTTP keys each instantiation uniquely. Returning by value sidesteps the cache entirely.

`MakeCaptureDef<>` is the **mandatory** way to build a def for a compile-time attribute in tests/code that needs to *match* a registered capture — never hand-initialize an `FArcAttributeCaptureDef` inline, because the type defaults `bSnapshot = true` and a forgotten `=false` silently fails the `operator==` match (the capture won't be found and the read/modifier no-ops).

For an attribute chosen at runtime (registry-driven, no compile-time getter), use `Arc::Capture::MakeRuntimeCaptureDef(FArcAttributeRef, EArcCaptureSource, bool bSnapshot)`; it sets all three identity fields explicitly. Do not use it for compile-time attributes, where `MakeCaptureDef<>` and the `GET_CAPTURE_*` accessors apply.

### Exposing named accessors on an attribute set — declaration macros

An attribute set co-locates its capture-def accessors next to its `ARC_MASS_ATTRIBUTE_ACCESSORS` block, via one declaration macro per policy. These mint a `static` member forwarding to the template:

```cpp
ARC_DECLARE_CAPTURE_SOURCE_SNAPSHOT(FArcAttributeSet_DamageModifiers, OutgoingBonus)
// -> static FArcAttributeCaptureDef GetOutgoingBonusSourceSnapshotCaptureDef()
ARC_DECLARE_CAPTURE_SOURCE_LIVE(Owner, Attr)     // ...SourceLiveCaptureDef()
ARC_DECLARE_CAPTURE_TARGET_SNAPSHOT(Owner, Attr) // ...TargetSnapshotCaptureDef()
ARC_DECLARE_CAPTURE_TARGET_LIVE(Owner, Attr)     // ...TargetLiveCaptureDef()
```

The set's header must `#include "Effects/ArcAttributeCapture.h"` **before** its `*.generated.h` line. No cycle: attribute sets live downstream of ArcMassAbilities (e.g. `FArcAttributeSet_Damage`/`_DamageModifiers` in ArcMassDamageSystem, the RPG sets in ArcRPGRuntime — both PublicDependency-link ArcMassAbilities).

Macros are the *only* preprocessor piece — they exist solely for identifier synthesis (templates can't mint `Get##Attr##…` names). All logic stays in `MakeCaptureDef<>`.

### Terse call sites — getter macros

`GET_CAPTURE_{SOURCE,TARGET}_{SNAPSHOT,LIVE}(Owner, Attr)` expands to the named accessor, so the screen-spanning `FArcAttributeSet_DamageModifiers::GetIncomingBonusTargetSnapshotCaptureDef()` is never hand-typed:

```cpp
Captures.Add(GET_CAPTURE_SOURCE_SNAPSHOT(FArcAttributeSet_DamageModifiers, OutgoingBonus));
```

### Registering + reading in an execution

- **Constructor:** `Captures.Add(GET_CAPTURE_*(...))` once per captured attribute. Order is *no longer* semantically load-bearing (see matching below) — but keep it readable.
- **`GatherCaptures`:** `FArcEffectExecution::GatherCaptures(OutCaptures, OutExtras)` is how an execution declares captures beyond its constructor lists. The default appends `Captures` / `ExtraCaptures`; an override calls `Super::GatherCaptures` or replaces them, and may add runtime-built defs. `BuildCaptureLayout`, scoped-modifier resolution and the editor dropdown all read captures through it, never from the `Captures` member directly.
- **Execute():** read via the context wrapper, never the raw declare-then-call:

  ```cpp
  const float OutBonus = Context.CalcCapturedMagnitude(
      GET_CAPTURE_SOURCE_SNAPSHOT(FArcAttributeSet_DamageModifiers, OutgoingBonus), EvalParams);
  ```

  `FArcEffectExecutionContext::CalcCapturedMagnitude(Def, EvalParams, Default=0.f)` returns the captured magnitude, or `Default` **on miss only** (a hit always overwrites; a miss leaves the seed). Pass a non-zero default where the capture is a multiplier-style value (e.g. a cap that must read `1.f` when absent).

`AttemptCalculateCapturedAttributeMagnitude(Def, …)` resolves by scanning `CapturedAttributes` for a def-equal entry — so reads are matched by identity, not position.

### Scoped modifiers — match by identity, not index

`FArcScopedModifier` selects which capture it modifies via a stored `FArcAttributeCaptureDef CaptureDef` (NOT a positional index). `ArcEffectSpec.cpp` resolves it against the `GatherCaptures` set with `FindByPredicate(D == ScopedMod.CaptureDef)`; not-found warns and skips. This makes scoped-modifier resolution independent of `Captures.Add` order.

A scoped modifier's `SourceTagRequirements` / `TargetTagRequirements` are copied onto `FArcResolvedScopedMod` and checked when the capture is read (`FArcEffectExecutionContext::ComputeCapturedMagnitude`) against the `EvalParams.SourceTags` / `TargetTags` the execution passes, like GAS scoped modifiers. Tags the execution adds (zone) and a rebound target count; spec build does not filter.

(The legacy `int32 CaptureIndex` remains as a deprecated, serialized-but-hidden field, migrated to `CaptureDef` in `UArcEffectDefinition::PostLoad` — slated for removal once legacy assets are re-saved.)

The editor picker (`ArcScopedModifierCustomization`) calls `GatherCaptures` on the execution instance and stores the selected entry's full def via leaf property handles, keeping a name-based dropdown.

### Rules of thumb

- Build defs with `MakeCaptureDef<>` (compile-time attributes) or `MakeRuntimeCaptureDef` (registry-chosen); never hand-init (the `bSnapshot` default is a silent-mismatch trap).
- Declare named accessors on the attribute set, call them via `GET_CAPTURE_*`.
- Read via `CalcCapturedMagnitude`; match (reads & scoped mods) is always by full-def `operator==`.
- `Captures.Add` order is for readability only — nothing resolves by position anymore.

Source: [capture-def macro refactor](../../../../docs/summaries/2026-05-29-1034-arcrpg-capture-def-macros-summary.md), [scoped-mod identity](../../../../docs/summaries/2026-05-29-1339-arcmass-scoped-mod-by-identity-summary.md).

## Effect / Attribute Semantics — 2026-06 review fixes

### Stacking — executions never re-run on stack increment

Stack increments (Aggregate / Instance policy on an already-active effect) rebuild aggregator modifiers and fire `OnStackChanged` only. `FArcEffectExecution::Execute` runs on first application and on each period tick — never on a stack increment. This is an intentional divergence from GAS (Overwatch-style stacking). Do not change this behavior; tests in `ArcEffectTests.cpp` pin it.

### Channel-layered aggregator evaluation

`FArcAggregator` evaluates modifiers channel-by-channel in ascending channel order; each channel's full formula output (including its `AddFinal`) is the next channel's base input. Within a channel:

- **Override** — last qualifying mod wins its channel (divergence from GAS, which uses first-wins). Override short-circuits the remaining ops of *that channel only*; subsequent channels still evaluate with the override value as their base.
- **Else** — standard formula per channel: `(Base + SumAdd) × (1 + SumMulAdd) / Divisor × ProdCompound + SumAddFinal`, where `Divisor = 1 + SumDivAdd`; near-zero divisor clamps to 1.
- All-channel-0 content reproduces the flat (single-channel) formula exactly — no asset migration needed.

Core implementation: `ArcAggregatorEval::EvaluateChannels` in `Attributes/ArcAggregator.cpp`.

### `FArcAttribute` — `SetBaseValue` vs `InitValue`

- **`SetBaseValue(float)`** — writes `BaseValue` only. `CurrentValue` refreshes on the next `AttributeRecalculate` signal (or never, for attributes without an aggregator). Use for runtime gameplay writes where you want the aggregator to reconcile Current on the next tick.
- **`InitValue(float)`** — writes both `BaseValue` and `CurrentValue` immediately. Use when seeding an attribute for the first time (trait setup, test fixtures, reset-to-zero) where there is no aggregator pass scheduled yet.
- The `ARC_MASS_ATTRIBUTE_ACCESSORS` macro: `Set##Attr` → `SetBaseValue` (base-only); `Init##Attr` → `InitValue` (both). Seeding code that wants both fields must use `Init##Attr` / `InitValue`.

### `TryApplyEffect` — `FArcEffectApplyResult`

`TryApplyEffect` returns `FArcEffectApplyResult { bool bApplied; FArcActiveEffectHandle Handle; }`. No `IsValid()` / `operator bool` sugar — choose the field that answers your question:

- `r.bApplied` — did the effect pass all gates and run? True for instant effects, true for stored effects that were admitted, false for rejections (immunity, gate, DenyNew, overflow-Deny).
- `r.Handle.IsValid()` — do I hold a removable active effect? Valid only for stored (duration/infinite) effects; always invalid for instant effects.

Instant-effect callers must not test `Handle.IsValid()` as a proxy for "did it apply" — use `bApplied` instead.

Pre-built-spec overload: `ArcEffects::RebindSpecTarget` rebinds the target half (target entity, target tag sources, Target-side snapshot captures and extras with `GetCaptureSide() == Target`) to the `Target` parameter before immunity and pre-application. Resolved magnitudes and the duration result are not recomputed.

### Modifier tag requirements — GAS split, stored by value

`FArcEffectModifier::SourceTagRequirements` / `TargetTagRequirements` go into `FArcAggregatorMod::SourceTagReqs` / `TargetTagReqs` on both `AddMod` and `AddLiveMod` (`ArcEffects::Private::PushModsToAggregator`). Source requirements test the source tags of an evaluation (an incoming effect's damage type, attack class); target requirements test the holder's tags. In an execution read (`EvaluateWithTags`) the sides are the containers the execution passes; in a plain `Evaluate` the source side is the mod's source entity's owned tags (`FArcAggregator::UpdateQualifications`).

`FArcAggregatorMod::SourceTagReqs` / `TargetTagReqs` are stored by value (not as pointers). This prevents dangling pointers after TMap rehash (`ApplyInfinite` ×N) or asset reload. The `AddMod` overloads still accept `const FArcGameplayTagRequirements*` (nullable = "no requirements"); the value is copied in. `OwnedModifiers` in `ApplyInfinite` is kept for `RemoveModifier` bookkeeping and debugger reads only — nothing references its memory for tag-qualification.

### Ability teardown — `StopAndFreeAbilityTree`

All StateTree Stop+Free paths go through `ArcAbilities::Private::StopAndFreeAbilityTree`. `Stop()` runs without `SetContextRequirements`; ExitState tasks must use `GetContextDataPtr`-style null-safe reads, not checked getters, for any external context data.

## Extension points for mechanics plugins (Plan 1, 2026-10)

AMA exposes hooks; game rules (CC blocking abilities, procs, damage pipelines, concrete stack resolvers) live in mechanics plugins, never here. Plan: `docs/plans/2026-10-04-1408-arc-mechanics-plan1-ama-extensions-plan.md`.

- **Game event bus**: `UArcGameEventSubsystem` (`Events/`) is the one event bus.
  - `Queue(FArcGameEvent)` works from any thread. Delivery happens in the post-actor-tick drain. Events queued during a drain are delivered in the next drain, so a chain advances one step per frame.
  - A channel is an exact tag: no parent matching.
  - Subscriptions (game thread):
    - `Subscribe(Tag, Entity|global, EArcEventRole, delegate)`.
    - `Subscribe(Entity, FArcAttributeRef, delegate)` on `ArcEvent.Attribute.Changed`. Changes are coalesced per entity and attribute between drains: first Old, last New.
    - `Subscribe(ArcEvent.Tag.Added|Removed, Entity, ChangedTag, delegate)`: edges of exactly that tag.
  - Dead entities are evicted lazily.
  - The envelope `FArcGameEvent` has Tag, Instigator (source), Target (receiver), Magnitude, `SourceTags`, `TargetTags` and Payload.
    - Every event captures Source and Target tags the way a spec does. Effect events copy the spec's captured tags. Other events gather through `ArcGameEvents::GatherEnvelopeTags`: ability events with the ability's effect context (`ArcAbilities::MakeAbilityEffectContext`: origin tags, item SourceData); single-entity events with Instigator = Target = the entity.
  - Channels:
    - `ArcEvent.Effect.{Applied,Executed,Removed,Rejected}`. The payload carries the spec (`FSharedStruct`); read stack count from the payload, not from the spec.
    - `ArcEvent.Ability.{Activated,Ended,CostFailed,CooldownFailed,CooldownExpired}`.
    - `ArcEvent.Tag.{Added,Removed}`: count edges 0 -> >0 and >0 -> 0 only.
    - `ArcEvent.Attribute.Changed`.
  - Payload headers: `Events/Arc{Effect,Ability,Tag,Attribute}EventPayloads.h`.
- **Removal** — every removal goes through `ArcEffects::RemoveEffects(EM, Target, FArcEffectRemovalQuery)` and the single `CleanupActiveEffect`; hooks receive `EArcEffectRemovalReason`. Effects are extracted from the stack before hooks run.
- **Pre-application** — order: context → `MakeSpec` → immunity (ref-counted) → `OnPreApplication(const FArcEffectApplyRequest&, const FArcEffectSpec&, FArcEffectPreApplicationResult&)` once. Components only veto (with reason tag); after a veto they may apply a child effect built with `ArcEffects::MakeChildContext`. No definition swap, no duration or SetByCaller writes.
- **Child context** — `ArcEffects::MakeChildContext(Parent, ExtraOriginTags, ExcludedOriginTags)` is the one way to build the context of an effect applied on behalf of another: it copies source entity, SourceData, ability and handle, and OriginTags (minus the excluded, plus the extra). Users: `FArcEffectComp_ApplyOtherEffect`, the `TryApplyEffect(Effect, Context)` overload, the effect tasks `FArcEffectTask_OnAttributeChanged` and `FArcEffectTask_OnAbilityActivated` (from the owning effect's context), and mechanics plugins (triggers, status lowering).
- **Tag sources** — `ArcEffectTagSources` registry (`Effects/ArcEffectTagSource.h`): `MakeSpec` fills `CapturedSourceTags`/`CapturedTargetTags` only from registered sources (built-ins: owned tags, definition, ability, origin). Modules register in `StartupModule` and must unregister in `ShutdownModule`.
- **Duration calculation** — optional `UArcEffectDefinition::DurationCalculation` (`FArcEffectDurationCalculation`): base (request `DurationOverride`, else policy) → final `Spec.Duration`; its captures join the definition's capture layout.
- **Execution trigger** — `FArcEffectExecutionContext::Trigger` is `Application` or `PeriodicTick`.
- **Execution output tags** — `FArcEffectModifierOutput::SourceTags` are appended to the spec's captured Source tags of that one output's op, on both sides; an empty container changes nothing.
- **Identity tags** — components contribute via `GatherIdentityTags`; `GetEffectTags` returns the definition's cached set.
- **Stacking** — Invariant: an effect's mods are in the aggregator iff `!bInhibited`; `ArcEffects::Private::RebuildEffectMods` is the only push path. `ConsumeStack`, `SetEffectDecayRate` are public. Removal by tag: `FArcEffectComp_RemoveEffects` (`FGameplayTagQuery` on the effect's identity tags, `MaxCount`, `Order`; an empty query removes nothing). Effect task `FArcEffectTask_OnTagChanged` applies an effect to its owner when `RequiredTag` is added or removed on its `Party`: Target is the effect's owner; Source is the effect's source entity. It subscribes by (entity, tag). The effect tasks `OnTagChanged`, `OnAttributeChanged` and `OnAbilityActivated` re-find themselves by (effect handle, task index); a removed effect fires nothing. An execution output with `Side = Source` writes to the effect's source entity through `ArcAttributes::EnqueueInstantModifier`. Attribute handlers get their own attribute in `FArcAttributeHandlerContext::Attribute` and declare observed inputs in `GatherInputs`.
- **Owned tags** — mutate only through `ArcAbilities::AddOwnedTags` / `RemoveOwnedTags` (fragment mutators are private).
- **Ability end** — `CancelAbility(..., EArcAbilityEndReason, Instigator)`; `Interrupted` is available for mechanics that interrupt casts.
- **Live magnitudes** — `FArcLiveMagnitude` as a Custom magnitude makes a same-entity attribute-dependent modifier re-evaluate when its source attribute changes; cycles fall back to the snapshot with a warning.
- **Captures** — executions declare captures through `GatherCaptures`; registry-chosen attributes use `Arc::Capture::MakeRuntimeCaptureDef`.

Known risk (A6, dropped): effect StateTrees run inline (apply, periodic tick, event drain) without declaring their external data to the Mass scheduler.
