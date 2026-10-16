# ArcMassMechanics Plugin

Layer L2: semi-generic gameplay mechanics built on top of ArcMassAbilities and ArcMassDamageSystem.

- No ArcRPG specifics. Game-specific rules live in ArcRPGMechanics (L3) or ArcRPG.
- Gameplay tags are native C++ (`UE_DEFINE_GAMEPLAY_TAG`) in `ArcMassMechanicsTags.{h,cpp}`, never `.ini`.
- Numbers are editor data (asset fields), not hardcoded constants. Exception: the pip cap `ArcResourcePips::Cap` (10) is a `constexpr` by owner decision (D-100).
- Log category: `LogArcMassMechanics`. Editor validators live in module `ArcMassMechanicsEditor`.
- Tests: `Tests/ArcMassMechanicsTest`.

Extension points are virtual structs or `FInstancedStruct` slots: L3 overrides a slot, L2 never names a game type.

## Resource flow of one change

A mechanic never writes a resource attribute directly. It writes a **meta** attribute; the meta's handler applies it to the resource, clamps it and resets the meta. The handler (`FArcAttributeHandler_MetaToResource`) is configured through ArcAbilityKit handler assets; L2 has no code dependency on ArcAbilityKit. Every meta write (costs, upkeep, resources) is an Execute op queued through `ArcAttributes::EnqueueInstantModifier`; the pending-ops drain applies it through the handler. Resources write through `FArcMetaWriter` (`Resources/ArcMetaWriter.h`), which warns once per config when the meta has no handler or the entity has no pending-ops fragment.

## Systems

| System | Files | Contract |
|---|---|---|
| Resources | `Resources/` | `UArcResourceConfig` (`Resources`: array of `FArcResourceDef`; `UpdateInterval`) + `UArcResourceTrait` ("Arc Resources") + `UArcResourceProcessor`. One target-seeking mechanism: per update a resource moves its `Value` relative to `FArcResourceTarget` (clamped to `[Min, upper bound]`) at `FArcResourceRate`, signed units per second, writing the signed step to its `Delta` meta (handler `MetaToResource`, Op `AddSigned`; any other Op drops a negative step). A positive rate moves toward the target and never passes it. A negative rate moves away from it, down to `Min` when the value is below the target, up to the upper bound when above; on a tie up when the target is at or below `Min`, else down; it never moves the value toward the target. Targets: `_Max`, `_Min`. Rates: `_Constant` (>= 0); `_Pips` (D-100): `clamp(floor(RegenPips) - floor(DegenPips), -ArcResourcePips::Cap, Cap) x max(0, PerPip)`, the three attributes read live, a missing Regen or PerPip gives 0, a missing Degen counts as 0; L3 adds attribute-scaled rates. Every `RateMultipliers` entry whose `Condition` the owner has multiplies the signed rate. `BlockedBy`: while the owner has any of these tags the resource makes no positive step (no move toward the target); a negative rate (degen) still steps; costs, effects and other meta writes still apply. An entity without `FArcOwnedTagsFragment` is never blocked (the processor warns once per config when a def sets `BlockedBy`). A value above the upper bound (`MaxAttribute`, else `Max`) drops to it in one update, ignoring rate, target and `BlockedBy` (plan 4 D49). There are no gates and no levels: a delay is an effect with a duration that grants a `BlockedBy` tag. Per-entity state: `FArcResourceStateFragment` (update accumulator). |
| Tiers | `Tiers/` | `FArcTierBands` (ascending thresholds, tier = 1-based index of the highest reached, 0 below the first). `UArcTierEffectTable` maps `(Key, Tier)` to an effect (`FindEffect`). Used by the threshold tier task, L3 Surge and the ArcEnviroEffects condition-delta handler (bands only). |
| Threshold tier task | `Tiers/ArcAbilityTask_EvalThresholdTier.h` | Ability task: `ValueAttribute` minus `ThresholdAttribute` when set, through `Bands`. Writes `Magnitude` and `Tier` to instance data, then applies `Table->FindEffect(Key, Tier)` once and keeps no handle (Instant allowed). A missing value or threshold attribute gives Magnitude 0, Tier 0 and no effect. |
| Costs | `Costs/ArcCostTypes.h` | `ArcCosts::` free functions for a concrete cost: `CheckResource` (the resource holds the amount and fills `FArcCostCheckResult`; a zero amount passes, an entity lacking the resource or meta fails `Unsatisfied`), `PayIntoMeta` (adds the amount to the meta as an Execute op). There is no resource cost base type. Concrete costs live in L3. `FArcCost_Set`: checks every cost before applying any, applies in order. |
| Statuses | `Statuses/`, `Aggregation/` | A status is an effect that grants its tags (`ArcCondition.*`, `ArcState.*`). `FArcEffectComp_InterruptAbilities` cancels running abilities selected by their own `AbilityTags` (`Abilities` requirements: With all, Without any; empty = every active ability). Removal by tag is the AMA `FArcEffectComp_RemoveEffects`. One-of-a-kind effects use AMA stacking Counter + Replace (the new application replaces the old one). Status duration is the effect's AMA duration calculation (L3 `FArcRPGStatusDurationCalculation`). Effect task `FArcEffectTask_Upkeep` (editable resource and meta, `UArcUpkeepValidator`). `FArcPolicy_ConditionalHighestWins`: on listed channels only the largest bonus per op counts, penalties stack. |
| Combat | `Combat/` | Hit chain, damage execution and true damage, below. |
| Triggers | `Triggers/` | Below. |

## Combat

A damage type is a leaf tag under `ArcEffect.Damage` (`ArcMechCombat::IsLeafDamageType`); its category comes from the tag hierarchy. One damage effect is one hit of one damage type (D-96): the type is the effect's own `ArcEffect.Damage` tag (`ArcMechCombat::FindEffectDamageType`); several damage types are several effects. Hit chain data is attributes only, no config asset. Attribute sets: `FArcAttributeSet_HitChain` (`MissChance` from the attacker; `DodgeChance`, `ParryChance`, `BlockChance` from the defender), `FArcAttributeSet_Crit` (`CritChance`, and `CritMultiplier`, the full crit multiplier: the base is data, e.g. 1.5, bonuses are modifiers; read by the L3 damage execution, not by the gate), `FArcAttributeSet_Mitigation` (global `Armor`, `Resistance`, `DamageReduction`; read with the hit's captured source tags, which hold the definition's damage type; a per-type value is a modifier whose `SourceTagRequirements` require that type, like GAS). Sets are added through the attributes trait.

**Hit gate** (`FArcMechHitGate`, an effect component, no config): on pre-application rolls Miss (the attacker's `MissChance`), then Dodge, Parry, Block (the defender's chances), in that order; the first success vetoes the effect. Each chance is read only after the previous roll failed, through the aggregator with the spec's captured source and target tags, so a defense rule is a tag-qualified modifier on the chance attribute, in data (a source or target requirement, `Override` 0 to switch a defense off). A chance of 0 never rolls. Rules:
- Attack class tags (`ArcRPG.AttackClass.*`) are plain tags describing the attack; the gate does not read them.
- From behind (outside the defender's `constexpr` 90 degree front half-angle) Parry and Block are not rolled. There is no per-effect switch.
- Convention: no positive `Override` on a chance attribute. Bonuses use `Add`, so an `Override 0` always wins regardless of order.
- A vetoing outcome publishes `ArcMech.Hit.Outcome.{Miss,Dodge,Parry,Block}` with `FArcHitResolvedPayload` (outcome, definition; Magnitude 0) and the spec's captured tags in the event's `SourceTags`/`TargetTags`. The gate publishes no Hit.
- One gate per effect definition (`UArcHitGateValidator`). The gate rolls no crit and no zone.
- A target immune to the effect is rejected by AMA before any component runs: no roll and no hit event.

**Hit and damage events**: the damage execution publishes, on every execution (application and every periodic tick, 0 damage included), `ArcMech.Hit.Outcome.Hit` (`ArcMechCombat::PublishHitEvent`: crit, zone, damage type, final damage as Magnitude; the spec's captured tags, unchanged, in the event's `SourceTags`/`TargetTags`), then `ArcMech.Damage.Dealt` with `FArcDamageDealtPayload` when the final amount is above 0 (`ArcMechCombat::PublishDamageEvent`). An effect without a damage execution, or a stack increment, publishes no Hit. At most one damage execution per effect (`UArcDamageTypeValidator`).

**Damage execution**: L2 has only the empty base `FArcDamageExecutionBase` (editor-only `GatherDataWarnings` for the validator); the execution is L3 `FArcRPGDamageExecution`. Shared helpers in `ArcMechCombat` read no attribute; the caller passes every value:
- `AddFlatAmount(Flat, Sign, IfAmount, InOutValue)`: adds a flat bonus plus the If flat amount, floored at 0.
- `ApplyIncomingBonus(Context, Flat, Percent, MaxPercent, IfTakenFlat, IfTakenMultiplier, InOutValue)`: the target's incoming bonus, then the If Taken values, before mitigation.
- `Mitigate(Context, Value, Curve, ArmorK, Armor, DamageReduction, Resistance)`: armor through `FArcArmorCurve` (a null curve or K <= 0 skips it), then `DamageReduction`, then `Resistance`. The caller passes final armor and resistance.
- `PublishHitEvent`, `PublishDamageEvent`.

**True damage and heal**: an effect with a SetByCaller modifier on the damage (or heal) meta, keyed `ArcMech.SBC.TrueDamage` / `ArcMech.SBC.TrueHeal`, and no execution. It skips mitigation. `ArcMechCombat::ApplyEffectWithSetByCaller` applies it; ArcWeather ice damage uses it.

## Triggers

Three independent effect tasks on a Duration or Infinite stored effect (not valid on an instant effect). They share no base; each holds its own fields and runtime state inside the active effect. Callbacks and deferred commands re-find the task by (effect handle, task index) through `ArcEffects::FindActiveEffect`; a removed effect is not found. Shared logic is free functions in `ArcTriggers` that take the task's fields.
- **On** (`FArcEffectTask_TriggerOn`, `Triggers/ArcTriggerOn.h`):
  - Subscribes to one exact bus channel (`EventTag`) on the owner, delivered one frame late. Channels include every AMA channel, hit events and `ArcMech.Damage.Dealt`; a parent tag matches nothing.
  - On `ArcEvent.Tag.Added`/`Removed` it also needs `Tag` and hears only that tag's edges; there, `Role` is ignored.
  - `Role` is Instigator, Target or Any.
  - Conditions read the event's `SourceTags`/`TargetTags`: Owner's side when the owner is the instigator, else the target's side.
  - `Chance` is a plain authored float in (0, 1] with no default.
  - Without an `EventTag` it never subscribes.
- **While** (`FArcEffectTask_TriggerWhile`): fires effects when all conditions become true and removes the effects it applied when they stop holding.
  - Threshold conditions subscribe to their attribute (and `MaxAttribute`) on the condition's party.
  - Tag conditions subscribe by (entity, tag) to every tag their requirements name, plus that tag's child tags, as registered when the effect is applied.
  - Every evaluation runs in a deferred command, so no effect stack changes inside a bus callback.
- **If** (`FArcEffectTask_TriggerIf`, D-97): a conditional modifier of the hit's damage through six fixed channels (`EArcIfChannel`: Done multiplier, flat, crit chance, crit damage; Taken multiplier, flat), on application only. The damage execution builds an `ArcTriggers::FArcIfHit`, reads each channel with `ArcTriggers::GetIfModifier` at its formula point (multipliers multiply as `1 + Value`, flat and crit values add, no highest-wins) and calls `ArcTriggers::CommitIf` once after the damage is final. `GetIfModifier` walks the source's (Done) and the target's (Taken) active effects for If tasks, gated by `Role`. A contributing If reserves its charge on first use. `SpendOn`: `AnyContributingHit` pays on every hit it contributed to; `OnlyCrit` refunds on a non-crit hit and skips `Effects` and the host-removal outcome. `CommitIf` runs, in one deferred command per If, its `Effects` (only on a landed hit) and the host-stack and remove-host-when-spent outcome. `_HitResult` Crit and Zones filters are unmet while the crit is not rolled, so they never apply to a `CritChance` modifier.

Conditions (`FArcTriggerCondition`): `_Tags`, `_AttributeThreshold`, `_EventMagnitude`, `_HitResult`, `_EffectTags`, `_Ability`. Effects (`FArcTriggerEffect`): `_ApplyEffect`, `_CastAbility` (pushes the input tag through `ArcAbilities::PushInput`; the activation carries the trigger origin). Conditions and effects validate per task in `ValidateInOn` / `ValidateInWhile` / `ValidateInIf`. Charge modes (On, If): Unlimited, Trigger (own count, optional remove-host-when-spent for this task), HostStack (consumes a host stack).

`_ApplyEffect` builds its context with `ArcEffects::MakeChildContext` from the host effect's context: it keeps the host's SourceData (item), ability and origin tags, and adds `ArcMech.Trigger.Origin`. `Instigator` sets the source entity. `Duration`, when set, is the base duration passed to the applied effect's duration calculation; unset uses the effect's own duration.

**Procs never trigger procs.** Effects applied by a trigger, and abilities cast by one, carry `ArcMech.Trigger.Origin` in their `OriginTags`. An event with that tag in its `SourceTags` fires no On trigger: effect, hit, damage and ability events. The hits of such effects fire no If trigger effects. Tag and attribute events carry no origin. On such hits an If trigger with `Modifiers` still applies them and pays its charge, but its `Effects` do not fire; an If trigger without `Modifiers` is skipped (`ArcTriggers::IsTriggerOriginated`).

Everything is torn down when the host effect is removed, which resets item triggers on unequip. `UArcTriggerValidator` checks per task: an instant host; On `Chance` in (0, 1]; On `Tag` set on the tag channels and only there; Trigger charge mode needs charges; While needs conditions; If needs `Modifiers` or `Effects`, no Done channel on `Role = Target` or Taken channel on `Role = Instigator`, no multiplier `Value` below -1, no `CritChance` modifier beside a `_HitResult` Crit or Zones filter; plus each condition's and effect's `ValidateIn*` (e.g. Apply Effect `Duration` above 0).

## Validators (editor module)

- `UArcAbilityCostValidator`: a Cost Set has no empty entry.
- `UArcDamageTypeValidator`: an effect carries at most one `ArcEffect.Damage` tag and it is a leaf; an effect with an execution derived from `FArcDamageExecutionBase` carries one; at most one damage execution per effect; reports the execution's `GatherDataWarnings`.
- `UArcHitGateValidator`: at most one Hit Gate per effect.
- `UArcResourceConfigValidator`: a resource has Value, Rate and Target; a Pips rate has `RegenPips` and `PerPip`; warns on a missing Delta.
- `UArcTriggerValidator`: see Triggers.
- `UArcUpkeepValidator`: an Upkeep task has both resource and meta.

## Native tags

`ArcMassMechanicsTags`: `ArcCondition.*`, `ArcState.*` (incl. `ArcState.Stun.Frozen`), `ArcEffect.Type.*`, `ArcMech.SBC.{TrueDamage,TrueHeal}`, `ArcMech.Hit.Zone` (root of hit zone tags), `ArcMech.Hit.Outcome.{Hit,Miss,Dodge,Parry,Block}`, `ArcEffect.Damage` (damage type root), `ArcMech.Damage.Dealt`, `ArcMech.Damage.Crit` (a damage execution adds it to the Source tags of a critical `Damage` output through `FArcEffectModifierOutput::SourceTags`), `ArcMech.Trigger.Origin`.
