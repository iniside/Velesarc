# ArcAI Plugin

ArcAI is the perception, navigation and shared-knowledge layer beneath the decision plugins. It senses, it publishes what it senses, and it costs paths. It does not decide (ArcTactics) and does not store general world knowledge (ArcKnowledge).

## Local contracts

### Shared target knowledge

Sight perception is private: `UArcMassSightPerceptionProcessor` writes each perceiver's own `FArcMassSightPerceptionResult` and nothing else. `UArcSharedTargetPublishProcessor` is the bridge that makes a sighting available to a whole side, and it is the only writer of these entries. Unlike perception, it runs Standalone and Server only — clients build no shared store, matching the ArcTactics processors that read it.

- **The entry convention lives in one place.** `ArcSharedTargetKnowledge::PublishSighting` decides everything about a shared-target entry: the target goes in `FArcKnowledgeEntry::SourceEntity` (so `GetKnowledgeBySource` is an indexed lookup rather than the subsystem's linear tag scan), the marker tag `TAG_AI_Knowledge_SharedTarget` identifies the kind, `FArcSharedTargetPayload::TeamId` carries the side, `Timestamp` carries age, and `Lifetime` carries reclamation. Readers must go through `ArcSharedTargetKnowledge::FindTeamEntry` and never re-derive that filter.
- **`GetKnowledgeBySource` returns a mixed bucket.** Overloading `SourceEntity` with the target means that call yields the target's own knowledge entries *plus* every side's records about it. Filtering on the marker tag **and** the payload team is mandatory, not defensive. `FArcAreaDebugger` consequently lists these records under an entity that did not author them.
- **Freshest wins — there is no precedence between own memory and an ally's report.** `FArcMassSightPerceptionResult::PerceivedEntities` is *memory*, not sight: entries survive until `ForgetTime`. An own-first rule would let a nine-second-old memory beat an ally who is looking at the target right now, which is the failure this whole layer exists to remove. Both readers compare timestamps and take the newer.
- **Nobody publishes an ally.** The publisher skips a perceived entity resolving `Friendly` — which by D2 includes the same team, but now also an authored cross-team friendly pair that the old bare `==` would have published. A teamless perceiver publishes nothing at all. A `Neutral` **is** published: worth knowing about, not worth attacking, so the published set is deliberately wider than the targetable set. Candidate production skips records about the observer itself, and it is the only place that does — nothing re-validates a bound target, so do not remove that check believing a second one exists. Sight perception applies no team filter of its own, and target tags default empty, so without both gates an NPC would read the record an ally published about it and act on itself.
- **Retention is a property of the side's memory, not of anyone's eyes.** `UArcSharedTargetKnowledgeSettings::RetentionSeconds` is the single authored value, read through `ArcSharedTargetKnowledge::GetRetentionSeconds`. It must not be derived from a perceiver's `ForgetTime` — several allies with different sight configs write the same entry, and a derived lifetime would flip with whoever wrote last. A retention of zero is clamped to the shortest finite lifetime, because `FArcKnowledgeEntry::Lifetime == 0` means *never expires* to the sweep.
- **Publishing rides perception's cadence.** The publisher skips a perceiver whose result was not refreshed this frame, which it detects by `TimeSinceLastUpdate == 0.0f`. That is an identity test against a literal the sight processor assigns on the tick it samples — if that assignment ever becomes a `-=` idiom, this gate silently stops working.
- Known and accepted: with N allies watching one moving target, that entry takes N writes per perception period. Bounded by the cadence gate; reducing it further means changing the entry model.

### Team attitudes

`ArcTeamAttitude::Resolve(ObserverTeamId, TargetTeamId)` is the only thing that decides what one side makes of another. Everything that asks about a relation goes through it; nobody compares `TeamId`s directly.

- **The resolution order is the contract, and it is total.** Either side teamless ⇒ `Neutral`; same team ⇒ `Friendly`, decided *before* the settings object is read so it can never be overridden by an authored row; otherwise the first matching row in authored order, matched in either direction; otherwise `Neutral`. No branch assumes a validator ran — a `UDeveloperSettings` CDO is not an asset, never sees `IsDataValid`, and a hand-edited ini has no hook at all.
- **An unlisted pair is Neutral, deliberately.** With a hostile default every new faction is born at war and has to be written out of every relation. Hostility is a deliberate act; a relation row's `Attitude` also defaults to `Neutral` so a malformed ini row is inert rather than hostile.
- **`Neutral` is no longer "not a valid target".** An ArcTactics row authors the attitude it wants, so a neutral can be targeted deliberately; what `Neutral` still means is that nothing happens *by default*. It does not flee, warn, or change stance — those are behaviours, and behaviours are usage rows. Grudges and retaliation do not exist: an attacked neutral does not turn hostile, because that needs per-entity state with a lifetime.
- The call is **directional** although the table is symmetric today. That is the seam a one-way grudge lands in later, without touching a single call site.

### TQS StateTree task

- TQS property functions `Perceived Last Location` and the memory functions
  `Memory Location`, `Memory Entity`, `Memory Float`, `Memory Bool` and
  `Memory Tag` (`TargetQuery/Functions/ArcTQSFunctions_Gameplay.*`) read
  perception results and NPC memory, and write their `Fallback` when absent.
- `FArcTQSRunQueryTask::Query` is an `FArcTQSQueryReference`; there is no inline
  query. StateTree binds `Query.Parameters.Value.<Prop>` like any instance-data
  field. See `docs/reference/arctqs.md`.

### Utility considerations

- **Curves are `FArcResponseCurve`** (ArcCore `Math/ArcResponseCurve.h`), used
  by considerations, TimeOfDay and TQS steps. Formula types sanitise their
  output to 0..1. Custom keeps rich-curve behaviour and can still produce the
  `InvalidResponseCurve` veto. See `docs/reference/arctactics.md`
  **Response curves**.

- **One consideration type, and it declares what it reads.** Every consideration overrides
  `GetRequirements()` — inside `WITH_EDITOR`, where the base puts both the enum and the virtual —
  so `UArcTacticsConfig::IsDataValid` can catch a target-reading consideration authored on a
  targetless row. Before that they all reported `None` and such a row scored zero at runtime
  instead.
- **`EntityHealth` reads whichever subject it is authored for.** `Subject` defaults to `Target`;
  set to `Querier` it scores the entity doing the asking, which is what a retreat or self-heal row
  needs. Both early-out guards test the resolved subject, not the target handle — a querier row's
  target handle is unset by construction.
- **Unresolvable health rejects rather than scoring zero.** Under a geometric mean a zero is an
  invisible veto with no reason attached, so an unset attribute ref used to read as merely
  unattractive. It now names itself: `HealthUnresolved`, or `NoMaxHealth`. The null-manager and
  invalid-entity guards still accept zero — those are framework invariants, not authoring errors.

### NPC memory

`FArcMassMemoryFragment` is one NPC's private memory: typed values, each under a
gameplay-tag key. It is sparse, so only entities carrying `UArcMassMemoryTrait` pay for it, and
it dies with its entity.

- **Typed storage.** `TMap<FGameplayTag, FInstancedStruct> Values`: one value per key, and a
  key holds one type at a time. `TArcMemoryValueTraits<T>` maps a C++ type to its stored struct:
  `FVector`, `FMassEntityHandle` and `FGameplayTag` store themselves, `float` stores
  `FArcMemoryFloat`, `bool` stores `FArcMemoryBool`. Add a type with a traits
  specialization (and a one-field USTRUCT when no engine struct fits).
- **API (`ArcMassMemory::`).** `Write<T>(Fragment, Key, Value)`, `Read<T>(Fragment, Key, Out)`
  and `Forget(Fragment, Key)`, each with an entity overload that returns false when the entity
  has no fragment. `Read` returns false, leaving `Out` untouched, when the key is absent or holds
  another type.
- **A type mismatch warns once.** `WarnTypeMismatch` logs once per (key, stored type,
  requested type) from a game-thread-only set, so a read of the wrong type is visible without
  flooding the log. This, not the compiler, catches conflicts across configs or with the trait.
- **Native keys.** `TAG_AI_Memory_Anchor` (Vector, written by the trait at spawn) and
  `TAG_AI_Memory_PatrolPoint` (Vector, held by Generate Patrol Points) are native tags exported
  from ArcAI.
- **Key metadata.** A key field carries `meta=(ArcMemoryAccess="Read|Write|Forget",
  ArcMemoryType="<struct path>")`. The editors read it for the ArcTactics Memory tab, the compile
  type check and the node type listings. A key field is literal-only: it is never a binding
  target.
- **No sweeper and no expiry.** An entry lives until `Forget` or until the entity dies. Nothing
  reads a timestamp, so none is stored; add one when a reader needs it.
- **An absent key is an answer, not an error.** The memory read functions read `Context.Self`
  (the querier in TQS, the deciding entity in ArcTactics) and have no `Entity` input: memory is
  private to its owner. They write their `Fallback` when the key is missing or holds another
  type, the entity has no memory, or there is no querier (spawner queries). The patrol config
  binds `Memory Location(Key = AI.Memory.Anchor, Fallback <- Entity Location(Entity <- Context.Self))`,
  so an unanchored NPC circles where it stands rather than walking to the world origin. Readers
  are expected to have a sane answer for absence.
- **Writers.** ArcAI only stores. ArcTactics owns the write nodes (the Write Memory and Forget
  Memory steps) and the patrol task.
- **It is not `UArcKnowledgeSubsystem`.** That store is world-global with no owner scoping
  (`SourceEntity` is provenance, and `GetKnowledgeBySource` returns a mixed bucket), sweeps
  every entry in the world on a timer, and broadcasts spatially on write. Private working state
  that only its owner reads belongs on the entity.
- **The anchor is written in `InitializeEntity`, because Mass does not fire observers for
  sparse elements at all** - `ObserverRegistry` ensures against registering one and the
  dispatch path never reaches them. For the same class of reason the trait derives from
  `UArcMassEntityTraitBase`: on the engine base neither `BuildSparseFragments` nor
  `InitializeEntity` is called and the fragment silently never exists.

### Perception has no occlusion test

`UArcMassSightPerceptionProcessor` is a spatial-hash cone or radius query plus a gameplay-tag filter. There is no trace, no visibility channel, nothing. Entities are seen through walls, and shared knowledge now propagates a through-wall sighting to a whole side. Movement is wall-aware (the tiled influence grid's visibility channel); target selection is not.
