# ArcAbilityKit Plugin

Top-of-graph integration plugin. Bridges `ArcMassAbilities` + `ArcMassItems` + `ArcCore` + `ArcTargeting` into one cohesive layer. None of those upstream plugins depend on this one — ArcAbilityKit is a pure consumer of their substrates. All concrete cross-domain content that couples abilities, items, targeting, animation, and VFX in the same type lives here. Pure substrate types (e.g. `FArcAbilityCustomDataBase` base, `FArcAbilityCustomData` wrapper, `FArcActiveAbility`) remain in their owning plugin (ArcMassAbilities), not here.

## What Lives Here

### Sources/

| Type | Purpose |
|------|---------|
| `FArcAbilitySource_Item` | `FArcAbilityCustomDataBase` subclass that carries the source item (definition, item id, item data handle, slot) |
| `ArcAbilityKit::FindSourceItem()` | The one item-source resolver (`ArcSourceItemResolver.h`): ability overload reads the active ability's `FArcAbilitySource_Item`, effect overload reads `FArcEffectContext::SourceData`; both return the item's `FArcItemData` or null |
| `ArcAbilityKit::MakeItemSource()` | Builds the `FArcAbilitySource_Item` for an item and slot. Granted abilities get it as custom data and granted effects as their context `SourceData`, so both resolve the granting item |
| `FArcEffectTagSource_SourceItem` | AMA tag source: appends the `ItemTags` of the context's source item (`FindSourceItem`) to the spec's source tags. Translates the context only; never searches equipment. `FArcAbilityKitModule` registers it in `StartupModule` and unregisters it in `ShutdownModule` |
| `FArcTargetingItemContext` | Item-flavored extra-data struct for `FArcTargetingContext::ExtraData`; moved from `ArcCore/Targeting/ArcTargetingArcCoreContext.h` |

### Attributes/

Generic attribute handlers (`ArcResourceAttributeHandlers.h`), the meta-to-resource mechanism:

| Type | Purpose |
|------|---------|
| `FArcAttributeHandler_MetaToResource` | Assign to a meta attribute. In `PostExecute` applies the meta to `Resource` with `EArcResourceOp` (`Add`/`Subtract` discard non-positive meta values, `AddSigned` keeps the sign), clamps to `[MinConst, min(MaxAttribute, MaxConst)]`, writes with `SetAttributeDirectAndNotify`, resets the meta to 0. When `EventTag` is set, queues an `FArcGameEvent` with `FArcResourceChangedPayload` (Magnitude = new minus old). The meta is reset even when the resource set is missing. |
| `FArcAttributeHandler_HealthDamage` | Assign only to the `Damage` meta. Same fields, write and clamp as `MetaToResource`, no `EventTag`. When the applied change is not 0, queues `ArcEvent.Health.Changed` (native, `ArcAbilityKitTags`) with `FArcHealthChangedPayload` (`OldValue`, `NewValue`, `Requested`, `TargetLocation` from the target's `FTransformFragment`, zero without one). Envelope: Instigator = modifier source entity, Target = damaged entity, Magnitude = applied signed change, `SourceTags`/`TargetTags` = the op's captured tags (damage type `ArcEffect.Damage.*`, `ArcMech.Damage.Crit` on a crit). Server-side only, like the bus. `DegenDamage` and heals stay on `MetaToResource`. |
| `FArcAttributeHandler_ClampRange` | Assign to the clamped attribute. `PreChange` clamps to `[MinConst, min(MaxAttribute, MaxConst)]`; with neither max set only `MinConst` applies. |

`ArcAbilityKit::FindSourceItem` (Sources/) is the only item-source resolver; do not add per-plugin copies.

### Captures/

| Type | Purpose |
|------|---------|
| `FArcExtraCaptureSource_Item` | Extra capture source ("Item Stat"): the context source item's Item Stats value for `Attribute`, read through `FArcItemInstance_ItemStats::GetStatValue` with the evaluation source tags |

### Tasks/

| Type | Purpose |
|------|---------|
| `UArcAbilityTask_ApplyEffectsFromItem` | StateTree task — applies Mass effects from the item referenced in `FArcAbilitySource_Item` |
| `UArcAbilityTask_ExecuteTargeting` | StateTree task — launches targeting and resolves via item targeting presets |
| `UArcAbilityTask_WaitTargetingResult` | StateTree task — waits for targeting to complete and extracts results |

### Targeting/

| Type | Purpose |
|------|---------|
| `ArcAbilityKit::MakeAoEShapeFromItemData()` | Free function — builds an AoE shape from item targeting fragment data (`ArcAoEShapeFromItem.h`) |
| `UArcTT_ItemBoxTrace` | Targeting task — box trace driven by item targeting shape fragment |
| `UArcTT_ItemLineTrace` | Targeting task — line trace driven by item targeting trace fragment |
| `UArcTT_ItemSphereTrace` | Targeting task — sphere trace driven by item targeting shape fragment |

### ItemFragments/

Mass-bridge and targeting fragments that couple item definitions to ability/effect/targeting/animation/VFX systems:

| Fragment | Purpose |
|----------|---------|
| `FArcItemFragment_GrantedMassAbilities` | Mass abilities granted to the owner while this item is active (ArcMassItems activation), with the item source as custom data |
| `FArcItemFragment_GrantedMassEffects` | Mass effects applied to the owner while this item is active, with the item source as context `SourceData` |
| `FArcItemFragment_MassAbilityEffects` | Effects applied when a Mass ability fires |
| `FArcItemFragment_MassAttributeModifier` | Attribute modifier applied via Mass ability system |
| `FArcItemFragment_TargetingObjectPreset` | References a targeting preset asset object |
| `FArcItemFragment_TargetingPreset` | Inline targeting preset configuration |
| `FArcItemFragment_GlobalTargeting` | Global targeting configuration for the item |
| `FArcItemFragment_TargetingShapeConfig` | Geometric shape parameters for targeting (range, radius, etc.) |
| `FArcScalableFloatItemFragment_TargetingShape` | Scalable-float variant of targeting shape parameters |
| `FArcScalableFloatItemFragment_TargetingTrace` | Scalable-float trace distances for item targeting |
| `FArcItemFragment_AbilityMontages` | Animation montages played when abilities fire |
| `FArcItemFragment_AnimMontage` | Single-montage reference |
| `FArcItemFragment_NiagaraSystem` | VFX Niagara system reference |
| `FArcItemFragment_NiagaraSystemMap` | Tag-keyed map of Niagara VFX references |
| `FArcItemFragment_ItemStats` | Item-intrinsic stats keyed by `(Stat, StatTags)`; creates `FArcItemInstance_ItemStats`. See Item Stats below |

### Item Stats

`FArcItemFragment_ItemStats` holds stats intrinsic to one item (D25); anything applied to the owner goes through an effect.
- `Recalculate` (`ArcItemStats::BuildEntries`) merges this item's and its socketed children's entries into one `FArcItemStatReplicated` per `(Stat, StatTags)` key, with one replicated value per modifier kind (`Additive`, `Multiply`, `Division`). The same key twice keeps the highest value per kind (D-77 e).
- `GetStatValue(Stat, Tags)` (`ArcItemStats::ReadStat`) reads every entry of `Stat` whose `StatTags` are all in `Tags`, so an untagged (general) entry always matches. Per kind, positives take the max of the specific and general entries (D-77 b, D42); penalties (Additive below 0, Multiply below 1, Division above 1) add up. Result: `Additive x Multiply / Division`.
- `GetStatEntries` returns the merged entries.

## What Does NOT Belong Here

- **Pure item-domain fragments** (`FArcItemFragment_Tags`, `_Stacks`, `_SocketSlots`, `_Droppable`, `_UIDescription`, etc.) — live in **ArcMassItems**
- **Equipment visuals** (mesh, cloth, actor attachment fragments under `ArcCore/Equipment/`) — item presentation, not ability/targeting bridges; live in **ArcCore**
- **Generic engine wrappers** (anim notifies, anim node libraries) — live in **ArcCore** (existing) or their own dedicated plugin
- **Abstract custom data mechanism** (`FArcAbilityCustomDataBase` base, `FArcAbilityCustomData` wrapper, `FArcActiveAbility::CustomData` array) — live in **ArcMassAbilities**; the kit only houses concrete custom data subclasses
- **Domain-plugin item fragments** (gun fire-mode fragments, craft recipe fragments, builder fragments) — live in their respective domain plugin (`ArcGun`, `ArcCraft`, `ArcBuilder`)

## Adding a New Ability Source Type

If the new source bridges two or more plugins (e.g. condition-effect → ability, knowledge → ability):

1. Subclass `FArcAbilityCustomDataBase` (defined in `ArcMassAbilities/Fragments/ArcAbilityCustomDataBase.h`).
2. Place the concrete subclass in this plugin (`ArcAbilityKit/Sources/ArcAbilitySource_<Domain>.h`).
3. Add a companion StateTree task in `ArcAbilityKit/Tasks/` if source-specific task logic is needed.
4. Add this plugin as a dep to any plugin that needs to read the new source type.

If the new source bridges only a single plugin to ArcMassAbilities (e.g. a gun-specific ability source that stays entirely within ArcGun), keep it in that plugin — do not pull it into the kit.

## Build.cs Public Dependencies

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core", "CoreUObject", "Engine",
    "AsyncMessageSystem", "GameplayTags",
    "MassEntity", "MassCore", "MassSpawner", "MassActors", "MassSignals",
    "StateTreeModule", "TargetingSystem", "Niagara", "Mover",
    "ArcMover", "ArcCore", "ArcMass", "ArcMassAbilities", "ArcMassItemsRuntime", "ArcTargeting", "ArcCues",
});
```
