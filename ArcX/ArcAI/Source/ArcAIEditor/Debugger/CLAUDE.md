# TQS Rewind Debugger — Implementation Notes

## Architecture Overview

The TQS (Target Query System) Rewind Debugger integration follows Unreal's TraceServices pipeline to capture TQS query data at runtime and display it in the Animation Insights / Rewind Debugger UI.

### File Map

| File | Module | Role |
|------|--------|------|
| `ArcTQSTrace.h/cpp` | ArcAI (Runtime) | Trace emission — `TRACE_OBJECT`, `TRACE_INSTANCE`, `UE_TRACE_LOG` events |
| `ArcTQSTraceModule.h/cpp` | ArcAIEditor | TraceServices module — creates provider + analyzer on analysis begin |
| `ArcTQSTraceAnalyzer.h/cpp` | ArcAIEditor | Routes trace events from `ArcTQSDebugger` logger to the provider |
| `ArcTQSTraceProvider.h/cpp` | ArcAIEditor | Stores deserialized query records keyed by InstanceId |
| `ArcTQSTraceTypes.h` | ArcAIEditor | Data structs: `FArcTQSTraceQueryRecord`, `FArcTQSTraceStepRecord`, etc. |
| `ArcTQSRewindDebuggerTrack.h/cpp` | ArcAIEditor | Track creator + track widget (timeline view, details panel) |
| `ArcTQSRewindDebuggerExtensions.h/cpp` | ArcAIEditor | Recording extension (channel toggle) + Playback extension (viewport drawing) |

### Registration (ArcAIEditorModule.cpp)

Four modular features registered via `IModularFeatures::Get().RegisterModularFeature()`:
1. `FArcTQSTraceModule` — TraceServices module
2. `FArcTQSRewindDebuggerTrackCreator` — track creator (stored as `TPimplPtr`)
3. `FArcTQSRewindDebuggerPlaybackExtension` — viewport drawing on scrub
4. `FArcTQSRewindDebuggerRecordingExtension` — channel enable/disable

## Data Flow

```
Runtime (Game Thread)                    Analysis (Editor)
========================                 ========================
OutputQueryStartedEvent()
  TRACE_OBJECT(Subsystem)        --->    GameplayProvider stores object hierarchy
  TRACE_INSTANCE(Subsystem,...)  --->    GameplayProvider stores instance as sub-object of subsystem
  UE_TRACE_LOG(QueryStartedEvent)--->    ArcTQSTraceAnalyzer.OnEvent(RouteId=0)
                                           -> FArcTQSTraceProvider::OnQueryStarted()

OutputStepCompletedEvent()
  UE_TRACE_LOG(StepCompletedEvent)--->   ArcTQSTraceAnalyzer.OnEvent(RouteId=1)
                                           -> FArcTQSTraceProvider::OnStepCompleted()

OutputQueryCompletedEvent()
  UE_TRACE_LOG(QueryCompletedEvent)--->  ArcTQSTraceAnalyzer.OnEvent(RouteId=2)
                                           -> FArcTQSTraceProvider::OnQueryCompleted()
```

## Rewind Debugger Tree Discovery Chain

Understanding how the engine discovers and creates tracks is critical:

```
1. User selects debug target in Rewind Debugger dropdown
   (only objects registered via TRACE_OBJECT appear here)

2. FRewindDebuggerObjectTrack::UpdateInternal() runs:
   a. Gets ViewRange = IRewindDebugger::GetCurrentViewRange()
   b. GameplayProvider->EnumerateSubobjects(SelectedObjectId, ...)
   c. For each sub-object:
      - Check: !Intersection(Lifetime, ViewRange).IsEmpty()   <-- LIFETIME GATE
      - If passes: GetTypeHierarchyNames(ClassId, TypeNames)
      - For each registered IRewindDebuggerTrackCreator:
        - If TypeNames contains creator->GetTargetTypeName():  <-- TYPE MATCH
          - If creator->HasDebugInfo(SubObjectId):             <-- DATA CHECK
            - creator->CreateTrackInternal(SubObjectId)        <-- TRACK CREATED
```

## Key Lessons Learned

### 1. Two Time Bases — Never Mix Them

The engine has two distinct time bases:

| Time Base | Source | APIs |
|-----------|--------|------|
| **Recording/Elapsed** | Seconds since recording started | `GetScrubTime()`, `GetCurrentViewRange()`, `GetWorldElapsedTime()` |
| **Profile/Trace** | `FPlatformTime::Cycles64()` converted via `FEventTime::AsSeconds()` | `CurrentTraceTime()`, `GetCurrentTraceRange()` |

Our provider stores timestamps in **trace time** (from `Context.EventTime.AsSeconds(Cycle)`), so all consumer code must use the trace-time APIs:
- Playback extension: `RewindDebugger->CurrentTraceTime()` (NOT `GetScrubTime()`)
- Timeline view: `GetCurrentTraceRange()` (NOT `GetCurrentViewRange()`)

### 2. Instance Lifetime Must Not Be Zero-Duration

TQS queries complete within a single frame (~0.002s). Calling `TRACE_INSTANCE_LIFETIME_END` immediately after `TRACE_INSTANCE` creates a near-zero lifetime like `[4.5785 .. 4.5785]`.

The Rewind Debugger's `UpdateInternal` checks `!Intersection(Lifetime, ViewRange).IsEmpty()`. A zero-duration lifetime (possibly a half-open range `[start, end)` where start==end) fails this overlap check, meaning the sub-object is **never discovered** as a potential track candidate.

**Fix**: Do NOT call `TRACE_INSTANCE_LIFETIME_END` for short-lived instances. Without it, the lifetime is `[creationTime, +inf)`, which always overlaps with any view range.

### 3. TRACE_INSTANCE Outer Determines Tree Placement

The `OuterId` parameter in `TRACE_INSTANCE(Outer, InstanceId, OuterId, Type, Name)` determines where the instance appears in the Rewind Debugger object hierarchy:

- **Outer = Subsystem**: Instance appears as child of subsystem. User must select subsystem in dropdown. All queries from all entities appear under one subsystem.
- **Outer = Actor**: Instance appears as child of actor. User selects the AI actor. Only that actor's queries appear. (Requires the querier to have a valid AActor — Mass entities without actors won't have tracks.)

We use **subsystem as outer** because:
- Not all querier entities have actors (`QuerierActor` can be null for pure Mass entities)
- Provides a single place to see all TQS activity
- Each query instance includes the actor name for identification

### 4. GetTargetTypeNameInternal Must Match TRACE_INSTANCE Type

`GetTargetTypeNameInternal()` returns an `FName` that the engine matches against `GetTypeHierarchyNames()` of each sub-object. This must exactly match the struct passed to `TRACE_INSTANCE`:

```cpp
// In trace emission:
TRACE_INSTANCE(..., FArcTQSQueryContext::StaticStruct(), ...);

// In track creator:
FName GetTargetTypeNameInternal() const override
{
    return FArcTQSQueryContext::StaticStruct()->GetFName(); // "ArcTQSQueryContext"
}
```

If these don't match, `HasDebugInfo` is never called.

### 5. StateTree Pattern (Reference)

StateTree's Rewind Debugger integration follows this pattern:
- `TRACE_OBJECT(Owner)` where Owner is the actor/component running the StateTree
- `TRACE_INSTANCE(Owner, InstanceId, OuterId, FStateTreeInstanceData::StaticStruct(), Name)`
- One instance per actor, lifetime spans the execution context
- Track creator returns `FStateTreeInstanceData::StaticStruct()->GetFName()`

This works well because each actor has exactly one StateTree component. TQS differs because queries are transient (many per entity, short-lived), so one instance per query with open-ended lifetime is the appropriate pattern.

## Diagnostic Infrastructure

The codebase includes diagnostic logging (can be removed once stable):
- `LogArcTQSTrace` — runtime trace emission
- `LogArcTQSAnalyzer` — analyzer event routing
- `LogArcTQSProvider` — provider record storage
- `LogArcTQSTrack` — track creator/track lifecycle
- `LogArcTQSPlayback` — playback extension + GameplayProvider diagnostic
- `LogArcTQSTraceModule` — module lifecycle

The GameplayProvider diagnostic in `ArcTQSRewindDebuggerExtensions.cpp` (the `[TQS Diag]` block) verifies:
- Whether TRACE_INSTANCE objects appear in GameplayProvider
- Whether the outer chain is intact (outer found, sub-object enumeration)
- The recording lifetime range (was critical for diagnosing the zero-duration bug)
