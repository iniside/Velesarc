# ArcTactics Plugin

ArcTactics is the server-authoritative Mass utility-selection and ability-input
layer. Authored configurations compile into Global Tasks, standalone Actions,
Selectors, directional binding copy batches, reflected parameter defaults, and
scoped property bags.
ArcMassAbilities remains the ability execution authority.

## Local contracts

- `UArcTacticsConfig` authoring data is editor-only. Cooked/runtime execution
  consumes `FArcTacticsCompiledData`; it never resolves authoring GUIDs or
  compiles configuration in play.
- The compiled-format version and nonzero revision gate execution. Editing or
  changing bindings invalidates the artifact; save and the editor Compile
  command rebuild it. A failed compile leaves no executable old artifact.
- `UArcTacticsConfig` and its authoring arrays remain the editor's sole source
  of truth. `FArcTacticsEditorViewModel` owns selection and commands without
  copying authoring data; selection is a node kind plus stable GUID, and
  Details resolves property handles again after structural changes and undo.
- Structural editor commands mutate the config through one view model and one
  transaction. The same model owns compile diagnostics and derives artifact
  validity from `FArcTacticsCompiledData`; the toolkit keeps no parallel
  compile-success flag or legacy raw-array editor.
- Stable authoring IDs belong to binding roots. Reorder preserves them,
  duplication regenerates node (including the selector node), parameter, and
  bag IDs and remaps forward and output bindings. Invalid paths are removed when their
  roots or reflected types change.
- Engine PropertyBindingUtils owns path resolution, type compatibility, copy
  descriptions, and copy execution. ArcTactics owns only its context/node/branch
  handles, accessible scopes, and per-node copy batches.
- The Bindings panel uses the Engine binding collection view; selector `Items`
  are ordinary node bindings with no projection. Output rows display actual
  node-output-to-bag direction. Navigation always resolves the original
  authoritative path and stable GUID before focusing Details.
- A Selector row is its selector node (icon, description, type picker); Add
  Selector and Replace Type offer only the concrete Entity/Location/Target
  selectors, and the action slot starts empty until set on the Selector Action
  row. The Selector row shows the selector node; the Selector Action row shows the
  action node plus branch Parameters and Parameter Bag, and binding navigation to
  the selector's Parameters or Bag lands there. View-model commands and the clipboard use the selector node
  ID as the selector's scope ID; clipboard payload version is 13.
- `meta = (Hidden)` hides a node type from the editor pickers and
  `ListTacticsNodeTypes`; it can still be created from code/JSON.
- Bindable descriptors carry no Category, so the binding picker has no
  category submenu; menu sections come only from the descriptor's own section.
- `UArcTacticsToolset` (MCP: `ListTacticsNodeTypes`, `ExportTacticsConfig`,
  `ApplyTacticsConfig`, `CompileTacticsConfig`) is a string/JSON shell over
  `ArcTacticsConfigJson`. Apply builds a transient scratch config with the
  shared node preparation and exact-`Valid` binding validation, and touches the
  target (one transaction, structural notify, compile) only after the whole
  JSON succeeds. The JSON names nodes, never GUIDs.
- `ArcTacticsConfigLint::Run` adds static `lint[]` findings to Apply/Compile
  results after a successful compile. It scores through `ArcUtilityScoring`,
  so keep its per-type input sets in step with the consideration `Score`
  functions. See `docs/reference/arctactics.md` **Lint**.
- ArcTacticsEditor adapts StateTree's visual language and editor patterns only.
  It does not depend on StateTreeEditorModule, serialize StateTree data, or add
  a StateTree runtime executor; Slate dependencies stay in the editor module.
- Forward bindings read Context, available Global Parameters and completed
  Global Task outputs, plus the owning branch's Parameters and Parameter Bag
  where phase rules permit. A selector node's `Items` read only Context, Global
  Parameters, and completed Global Task outputs. Its `Current*` Output is
  readable only by that selector's action, considerations, branch Parameters,
  and Parameter Bag; it is never an output-binding target, and no other node
  of the branch becomes a readable source. Global initialization reads only
  Context; decision settings (`MinScore`, the only bindable setting) run after
  Global Tasks.
- Inputs and Parameters are forward targets. An Output writes only to the
  owning global or branch Parameter Bag after successful node execution. The
  compiler rejects missing endpoints, incompatible reflected types,
  inaccessible or forward dependencies, invalid roles, and missing required
  inputs.
- The Details role chip of a top-level node Input also shows Required or
  Optional, derived from `ArcTacticsOptional` via `ArcTacticsNode::IsRequiredInput`,
  the same rule the compiler and ListTypes use.
- Global Tasks run in authored order before candidate evaluation. `Running`
  pauses later tasks and scoring, `Failed` publishes no selection, and cancel
  reaches the outstanding task. Empty successful outputs remain successful.
- The built-in tasks collect own data, perception/shared targets as plain
  `Targets` entity handles plus `TargetInfos` (`FArcTacticsTarget` entity and
  freshest `LastLocation`), and all passing TQS points as `Positions`. Freshness,
  attitude, and tag filtering stay internal to the perception task.
- Run Tactical Query and Generate Patrol Points share one TQS submit, complete,
  and cancel path (`FArcTacticsTaskQueryState`). Generate Patrol Points queries
  from the entity's own transform with the definition's selection, and outputs `Points`. Its held point lives in ArcAI NPC memory
  (`FArcMassMemoryFragment`) under its `MemoryKey` parameter (default
  `AI.Memory.PatrolPoint`, a Vector), never in node instance data. The
  query's centre is not read from memory by the asset: the config binds
  `TargetQuery.Parameters.Value.Center`. `Points` only ever holds the held point: until the entity is
  within `ArrivalRange` (3D, `ArcPathFollow::IsArrived`, default 250 cm;
  authors keep it at least the patrol MoveTo `ArrivalRange` plus any
  transform-to-navmesh vertical offset so Patrol ends and stamps its cooldown;
  nothing enforces it) it outputs the held point;
  arrival, NaN, or an empty or failed query forgets it, and a successful query
  holds and outputs its first result alone. Entities using the task need
  `UArcMassMemoryTrait`; nothing validates this at spawn (the memory fragment is
  sparse), and without it the task fails with diagnostic
  `PatrolPointsMissingMemoryFragment`.
- Property functions sit in bindings, as in TQS (`FArcTQSPropertyFunction`).
  The function node lives in the owning `UArcTacticsConfig::EditorBindings`
  entry; its inputs are bindings targeting the function ID. A function input has
  the access of the function's root (the node, branch Parameters/Bag or setting
  its output finally feeds), including selector `Current*`. Output bindings never
  take a function. The compiler validates one output, one owner, no cycles and
  required inputs, and batches carry a `PropertyFunctionsBegin/End` range.
  Each package's `FArcTacticsInstanceData` owns `FunctionInstanceData`; before a
  batch's copies, each function resets to its compiled default, applies its input
  batch with the batch's scope, then runs `Execute` with a
  `FArcTQSFunctionContext` (`EntityManager`, `World`, `Self` = the deciding
  entity). `ApplyBindings` is not `const` and every caller passes the context.
  Editor copy, paste, duplicate and step move carry function nodes and regenerate
  their IDs. A field with `meta=(ArcMemoryAccess=...)` (a memory key) is
  literal-only: it can never be a binding target.
- NPC memory is typed (`FArcMassMemoryFragment::Values`, tag to `FInstancedStruct`;
  see ArcAI). The memory steps (`ArcTacticsMemorySteps.h`: Write Memory
  Location/Entity/Float/Bool/Tag with literal `Key` and `Value` Input, `bValue` for Bool, Forget Memory
  with `Key`) are Sequence-only, utility-lane steps. `Enter` initializes scratch;
  `Tick` writes or forgets and succeeds, or fails with `FailureReason`
  `MemoryFragmentMissing` when the entity has no memory fragment. A key field is
  `meta=(ArcTacticsParameter, ArcMemoryAccess="Read|Write|Forget",
  ArcMemoryType="<struct path>")`. The compiler rejects one key used with two
  different `ArcMemoryType` values in one config, and an empty key on node
  data or a function node (`ValidateMemoryKeys`; the TQS compiler rejects
  empty keys too). Both compilers and the Memory tab find key fields with one
  walker, `ArcTQS::ForEachMemoryKey` (`ArcTQSBindings.h`, editor-only);
  conflicts across packages or with the trait are caught only by the ArcAI
  runtime warning. The config editor's Memory tab (`SArcTacticsMemory`, third
  bottom-panel tab) lists one row per key found in node data and function nodes:
  type, readers and writers, each navigating to its node (a function to its root);
  a key never written in the config reads "written elsewhere".
- Standalone Actions each contribute one candidate. A Selector is a typed
  `FArcTacticsSelectorNode` (role Selector) plus one action, considerations,
  branch Parameters, and a Parameter Bag. Its node ID and Name are the
  selector's. Its instance data binds an `Items` Input array and exposes the
  current item as an ordinary Output: Entity Selector (`TArray<FMassEntityHandle>`
  to `CurrentEntity`), Location Selector (`TArray<FVector>` to
  `CurrentLocation`), Target Selector (`TArray<FArcTacticsTarget>` to
  `CurrentTarget`). The selector node's forward batch runs once per decision;
  every item is then selected in turn and contributes one candidate. It never
  picks a private winner. All candidates enter one shared selection pool.
- Candidate identity is action definition plus the entity (Entity Selector,
  Target Selector `Entity`), the vector (Location Selector), or none for a
  standalone Action. It never uses an array index or pointer address.
- Global parameters reset to authored defaults at decision start and keep
  successful task publications through that decision. Branch parameters and
  bags reset for every standalone or selector candidate, and node instance data
  resets before its input bindings. Binding failure rejects that task or
  candidate and cannot reuse values from an earlier cycle or item.
- Considerations are mutable reflected instance data populated through
  bindings. Scoring uses `BeginPair`, `AccumulateConsideration`, and
  `FinishPair`; the shared `ScorePair` wrapper elsewhere retains the same
  curves, minimum score, and veto semantics. Considerations and TQS steps have
  no weight. A consideration is shaped only by its response curve. Every TQS
  Score step passes its raw value through its `ResponseCurve` (default Linear,
  identity), applied by the pipeline; Filter steps use the raw value.
- Branch `Priority` (int, bindable) forms buckets. With no active action,
  selection runs only over the eligible candidates of the highest priority
  present. With an active action, its own branch (same package, branch index
  from that package's compiled data) is always vetoed and every candidate of
  any package at or below its priority is vetoed `BelowActivePriority` (before
  the cooldown veto); only a selected higher candidate replaces it, and a
  decision with no selection never cancels or clears it. There is no score
  hysteresis.
- The selection mode comes from the trait `Settings.Selection`. Within the
  bucket, `HighestScore` sorts score descending with authored candidate order
  as the deterministic tie break. `RandomFromTopPercent`
  samples a sorted candidate-count prefix. `WeightedRandom` samples positive
  final scores. Each slot owns its seeded random stream.
- Selected action data is copied away from evaluation scratch and holds no
  pointers into resizable arrays. A decision never re-selects or adopts the
  active action. The option stores the winning item index and re-selects it in
  its captured node data, so the active action's bindings read that item's
  `Current*` Output.
- `FArcTacticsWaitAction` (Wait) is a standalone, step-free, utility-lane
  action: it succeeds after its `Duration` Parameter, wakes at its end time,
  and never blocks decisions. Configs use it as the always-eligible
  lowest-bucket Idle action (Idle contract).
- `FArcTacticsSequenceAction` is one selectable action and a thin container: its
  data is only `StepIndex`, `RequestGeneration`, `RequestDecisionGeneration`
  and `bActiveStepsEntered`, and it has no Inputs. Its steps are child nodes of role `Step`
  (`ArcTacticsMoveToStep`, `ArcTacticsUseAbilityStep`,
  `ArcTacticsUseAbilityFocusStep`) run in order; there is no branching, backward
  step, or action-local condition graph. A Sequence with zero steps, or steps
  under a non-Sequence action, is a compile error.
- Active steps (role `ActiveStep`, base `FArcTacticsActiveStepNode`, not a
  `FArcTacticsStepNode`) are a separate list, `ActiveSteps`, on actions and
  selector actions, shown in the editor as the "While Active" group with its
  own picker. The Sequence applies their inputs once and enters them in order
  before step 0. It exits them in reverse before returning Succeeded and in
  every `Cancel` (replace, failure, teardown); `bActiveStepsEntered` makes exit
  run once. Active steps have no outputs. They are allowed only under a
  Sequence. `ArcTacticsFocusActiveStep` (Focus) never changes composition
  immediately: Enter and Exit push `FMassDeferredSetCommand`s, so they flush in
  push order. Enter adds the sparse `FArcMassFocusFragment` if absent and sets
  its target and `OwnerSerial` (the action serial); Exit removes it only when
  `OwnerSerial` is still its own. The Mover input producer reads it sparse and
  turns it into Aim mode and facing toward the target.
- Step inputs: MoveTo has optional `Target` entity, `TargetLocation`, and
  `Destination`; UseAbility and UseAbilityFocus have optional `Target` and
  `TargetLocation`. Parameters are `bUseTargetLocation`, `bUseDestination`
  (MoveTo), `AbilityDefinition` (ability steps), plus each step's movement,
  ability, and query settings. Shared target-location rule:
  `bUseTargetLocation` selects `TargetLocation`, else the `Target` entity's
  location. MoveTo `bUseDestination` selects `Destination`. There is no
  compiler-set presence flag. Cost and Cooldown considerations take an `Entity`
  Input and an `AbilityDefinition` Parameter and require AbilityCollection.
  Distance To Entity reads the bound entity's transform; Distance To Location
  binds a vector; both measure from the querier's own `FTransformFragment` and
  require Transform.
- Step boundaries are owned by the Sequence inside `Tick`. On success: publish
  the step's outputs, reset its scratch, advance `StepIndex`, apply the next
  step's inputs, run the next step's `Enter`, then hand off the lane
  (`RequestExecution` and return Running) if the next step's lane differs. Each
  Tick applies the current step's inputs before it ticks and publishes after.
  Sequence `Enter` applies step 0's inputs and runs its `Enter`. A step `Enter`
  only initializes scratch; it never touches lane resources (queries, path,
  hold, abilities start in `Tick`).
- Step scope follows branch-node rules: a step reads Context, globals, the
  selector's `Current*`, and its own branch Parameters/Bag; it writes only its
  branch Bag; siblings are never readable. Data passes between steps only
  through the branch Parameter Bag. Known limits, relevant only once a
  step has an Output (none do today): step scratch is reset before the step's
  outputs publish.
- Abilities are resolved by definition: exactly one grant of the definition on
  the entity with a unique input tag, on Begin and on every wake. An ability
  step keeps a runtime-only handle captured at activation for input release and
  cancel; a wake whose resolved grant no longer matches it fails the step. The
  steps also own action-local TQS, movement/repath state, input ownership,
  start and execution deadlines, and durable hold ownership. Ability
  Activated/Ended delegates are wakeups.
- TQS queries are `FArcTQSQueryReference` fields (`MoveTo.DestinationQuery`,
  RunTQS/Patrol `TargetQuery`). Forward bindings may target
  `<Node>.<Ref>.Parameters.Value.<Prop>` (role from the root Parameter).
  `CompileEditorData` syncs every embedded reference (`ArcTacticsNode::ForEachQueryReference`)
  before refreshing bindings, and a reference to a non-executable definition is
  a compile error. See `docs/reference/arctqs.md`.
- `FArcTacticsCompiledData::ResolvePaths` patches every bag descriptor and bag
  initialization batch target from the saved default bags, then refreshes path
  segments from the compiled defaults, before resolving (UPropertyBag structs
  are transient and save as null; the `UStateTree::PatchBindings` rule). Load and
  compile share this path.
- `MoveTo.DestinationQuery` is an action-local query. It must complete
  successfully before pathing and never contributes utility candidates.
  `UseAbilityFocus` completes facing before ability activation. ArcTactics does
  not grant abilities or replace gameplay ability execution.
- MoveTo arrival: the path stops short by `ArrivalRange` (`QueryApproachDistance`
  with a `DestinationQuery`) through `ArcPathFollow` `StopShortDistance`;
  steering slows into the end because the final segment of a complete nav path
  sets `IntentAtGoal` Stand. MoveTo succeeds when the path is done and the
  destination has not moved by `RepathDistance`, or immediately when the path
  is already within the stop distance. There is no straight-line range check;
  existing assets change meaning without migration. See
  `docs/reference/arctactics.md` **Tactical Action Sequence**.
- Combat slots: `ArcAI/Combat/ArcCombatSlots` stores claims in a sparse
  `FArcCombatSlotsFragment` on the target, written only through deferred Set
  commands that validate separation and `MaxClaimers` at flush. MoveTo
  `bClaimCombatSlot` claims the query result, then follows the slot point
  (target transform + yaw × radius) without re-querying. The TQS filter
  `Combat Slot Free` and the consideration `Combat Slots Available` read the
  claims. See `docs/reference/arctactics.md` **Combat slots**.
- Editor: steps appear under their action before its considerations. Add Step
  and per-step delete buttons edit them; reorder is only within the owning
  action; copy/paste moves steps across actions with scope remapping.
- `UArcTacticsInstanceSubsystem` is the sole owner of per-entity mutable
  instance storage. `FArcTacticsInstanceHandle` is an index/generation handle;
  freed slots advance generation and exhausted generations retire permanently.
- Decision packages: an entity runs a list of configs. The slot
  (`FArcTacticsInstanceSlot`) owns the entity-level state: Context (Self,
  History), trait settings, one candidate list, selected and active action,
  cooldowns, random stream, action serials, movement hold, delegates and
  decision request state (`bDecisionRequested`, `NextDecisionTime`). Each
  `FArcTacticsPackage` (unique pointer, stable address) owns one config's
  shared compiled definitions, `SeenRevision`, `StaticCount`/`RuntimeCount`,
  a slot-unique `Serial` and its `FArcTacticsInstanceData` (global
  parameters, branch parameters and bags, node data, task cursor, `MinScore`,
  diagnostic); its Context bindings read the slot Context through
  `SharedContext`. Options carry `PackageIndex` and `PackageSerial`.
  `AddReferencedObjects` traverses every package's config, definitions and
  storage plus the slot's candidates, selected and active action. No UObject
  is created per NPC.
- `AddConfig`/`RemoveConfig` grant and revoke runtime packages (counted per
  config; removing a trait-only or never-added config logs an error and
  fails). Adding an active package cancels a running decision and requests
  one; removing the last count cancels a running decision, cancels the
  package's active action (`ReleaseHold`, end reason `PackageRemoved`),
  removes it and requests a decision. While the slot is inside
  `RunDecision`, `TickAction`, `CancelSlot` or an async completion
  (`FArcTacticsExecutionScope`, `ExecutionDepth`), both calls are validated
  at once and queued, then applied in call order when the outermost scope
  ends, so the package list never changes during a callback.
- Async callbacks carry weak subsystem, entity, slot generation, decision
  generation, action serial, and request generation where applicable. They
  resolve the slot again on the game thread before writing.
- `UArcTacticsLifecycleProcessor` runs before utility for every tactics
  archetype. It allocates missing slots (also for an empty `Configs` list),
  reinitializes when any package's `SeenRevision` differs from its config's
  compiled revision (runtime packages and their counts survive; the action
  serial counter carries over), and frees disabled instances. It is the only
  allocation site (the trait has no `InitializeEntity`). A failed allocation
  (invalid settings) logs once and clears `bEnabled`, so it is not retried
  every tick. Its navigation tuple is optional so non-movement tactics still
  receive lifecycle cleanup.
- The trait adds only `FArcTacticsTag`, the instance fragment and the config
  const shared fragment; it never adds or requires fragments from node
  requirements. `ValidateTemplate` fails for invalid `Settings`
  (logging an error) and for a missing or non-executable listed config.
  `ArcTacticsRequirements::Gather` derives a config's requirement flags and
  `FindMissing` checks the entity's archetype for the dense and const shared
  fragments they need (sparse requirements and subsystem flags are not
  checked). Every package build (allocation, reinit, `AddConfig`) runs it; a
  missing requirement or a non-executable config logs an error and leaves the
  package listed but inactive. It never recomposes a live archetype. The
  instance fragment contains only the handle and enable state; the trait
  `Configs` and `Settings` (`FArcTacticsDecisionSettings`: `DecisionInterval`,
  `Selection`, `HistorySize`; not bindable) are a const shared fragment,
  copied into the slot at allocation.
- Tag removal, disable, recompile, entity destruction, and world teardown
  cancel decisions, tasks, active execution, queries, input, and owned movement
  before freeing the slot.
- Lifecycle, utility, and movement require the game thread, run in
  `PostPhysics` for `Standalone|Server`, and never execute decisions on clients.
  Lifecycle runs before utility; movement runs after utility. State is not
  replicated.
- Utility owns scheduling and action flow. Ordered tasks resume through
  `TaskWakeup`; utility-lane actions use `ActionWakeup`; ability events use
  `AbilityWakeup`; movement-lane actions tick in the movement processor.
- Decisions are event-driven; no decision runs without a request. A request
  sets `bDecisionRequested`, consumed when a decision begins. Sources: slot
  init/reinit, every active-action end path except teardown, cooldown expiry
  (delayed `DecisionDue` at the stamp's expiry), ability `OnCooldownExpired`,
  the ArcAI `PerceptionChanged` signal (deferred, from sight/hearing processors
  and destroy observers), and `UArcTacticsInstanceSubsystem::RequestDecision`.
  A received `DecisionDue` is a request. The trait `Settings.DecisionInterval`
  is the minimum time between decisions; requests inside it coalesce into one
  decision at its end.
  A request waits while the active action `BlocksDecision`. A failed decision
  with no active action requests one retry after `DecisionInterval`; a
  successful decision never schedules itself. A slot with no active package
  runs no decision, stays `Idle`, schedules no retry and emits no trace.
- One decision runs every active package's global tasks in list order
  (`PackageCursor` resumes a `Running` task; cancel and async completion use
  the package at the cursor), then evaluates each package's `MinScore` and
  candidates into one pool with a global candidate `Order`, and selects once
  with the trait selection. A package whose reset, global task or `MinScore`
  binding fails is excluded with its own diagnostic; the decision fails only
  when every active package was excluded.
- Idle contract: a config needs an always-eligible lowest-bucket action; its
  end is the next request. Conditions without events (combat slots, shared
  targets ageing, distance) are re-checked only on the next request.
- `arc.Tactics.Dump <EntityIndex> <EntitySerial>` and ArcGameplayDebugger's
  Tactics Live tab share the capture target and `FArcTacticsDebugSnapshot`
  (`BuildEntitySnapshot`; `FormatSnapshot` is the dump formatter). They report
  pool validity, decision status, diagnostic and request state (`requested=`,
  `next=`), selection settings, history size and cooldowns with their config,
  then one section per package (config, `SeenRevision`, active/excluded,
  counts, diagnostic, global task state, `MinScore`, branches), the active
  action's package and identity, `priority=` and lane, hold ownership, the
  last ended action (`last_end=` package/name, status, reason, `failure=` step failure reason, time), the entity's NPC memory (`memory <key> (<type>) <value>` lines; `memory=None` without a fragment), and the
  capture target's last decision: `SelectedOrder`, status, diagnostic and
  every candidate (no cap) with package, priority, outcome, reason,
  consideration scores and `selected=`. Scores exist only after a decision ran
  while the entity was the capture target. The debugger is read-only.
- The removed usage-row schema, row candidate builders, and row-owned pending
  query maps have no compatibility path. Existing ArcTactics assets must be
  reauthored and recompiled for compiled format version 15.
- Consideration curves are `FArcResponseCurve` (formula types plus Custom). See
  `docs/reference/arctactics.md` **Response curves**.
- The editor Simulator tab (`ArcTacticsEditor/Simulator`) scores sliders through
  `ArcUtilityScoring` and `ComputeSelectionChances`; keep that function in step
  with `SelectByMode`. See `docs/reference/arctactics.md` **Decision simulator**.
- Compiled nodes carry their authored `Name`, or the node display name when
  it has none.
- Decision history: `ArcTactics/Debugger/ArcTacticsTrace` emits `ArcTacticsChannel`
  events: Candidate (with Package, resolved Priority and entity target
  location), DecisionPackage (one per package right before its Decision, keyed
  by entity and DecisionGeneration: ConfigName, MinScore, Active, Excluded,
  Diagnostic), Decision (with SelectionMode, SelectedOrder, status,
  diagnostic, entity location), ActionEntered (with Package and target
  location), ActionStepChanged (every Sequence step
  with name and type name), PathRequested (start, destination, path points),
  TQSQuery (accepted TQS query: all items with scores, selected/applied
  flags and per-step scores, chunked under the trace array limit) and
  ActionEnded (with Package).
  The emitters are `AddCandidate`, `AddRejectedSelectorCandidate`, the
  `RunTasks` exits and the other exits that leave Running, `RecordEndedAction`
  (Status and Reason from the caller), the replace and enter sites, the
  Sequence action's step entry, the approach path requests, the subsystem's `CompleteTaskQuery` and
  `CompleteActionQuery` (after the node reported acceptance through
  `FArcTacticsQueryReport` on its context), and `CancelSlot`.
  ArcGameplayDebugger records and reads them, and
  `ArcMCP` `ExportTacticsRecording` writes a recording as JSON; recordings
  from before these fields are not supported. See `docs/reference/arctactics.md`
  **Decision history** and **Live view**.
- Action cooldowns and history:
  - Branch Parameters carry `Priority`, `Cooldown`, `CooldownStart` (`OnEnd`
    default, `OnEnter`) and `HistoryTag`.
  - A stamp stores `CooldownEndTimes[(config, branch)] = now + captured
    Cooldown` on the slot, so removing and re-adding a package keeps its
    cooldowns; a slot reinit clears them.
    `OnEnd` stamps at every end (success, failure, replacement); `OnEnter`
    stamps only when the action starts (inputs applied, `Enter` returns Running
    or Succeeded, outputs published), and the end does not stamp again.
  - It appends `HistoryTag` to the slot `Context.History` on success only,
    capped by the trait `Settings.HistorySize`.
  - `AddCandidate` vetoes `ActionCooldown` while `now < CooldownEndTime`.
  - The considerations Action History Recent, Within Time and Absent bind
    `History <- Context.History`.

## Integration prerequisites

The ArcTactics trait must be placed on an entity whose other traits satisfy the
requirements declared by its compiled tasks, actions, and considerations; the
trait adds none of them.
Path-follow actions require the transform, agent dimensions, desired movement,
move target, navmesh path fragments, movement parameters, steering, smooth
orientation, and navigation setup. Perception, team, owned-tag, ability,
knowledge, TQS, and ability subsystems are required only by nodes that declare
them.

Authoring and diagnostics are documented in
[`docs/reference/arctactics.md`](../../../../docs/reference/arctactics.md).
