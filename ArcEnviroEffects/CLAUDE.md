# ArcEnviroEffects Plugin

Wet and Temperature conditions for Mass entities (D-66, D-99). Runtime module `ArcEnviroEffects`; no content. Depends on ArcMassAbilities, ArcAbilityKit, ArcMassMechanics. No dependency on ArcWeather.

- Gameplay tags are native C++ in `ArcEnviroEffectsTags.{h,cpp}`: `ArcEnviro.Hypothermia`, `ArcEnviro.Heatstroke`. There are no level tags; the level is an attribute (D-99).
- Numbers are editor data on the handler assets. Log category: `LogArcEnviroEffects`. Tests: `Tests/ArcEnviroEffectsTest`.
- No processor until weather: nothing moves Wet or Temperature over time. They change only through effects writing the delta metas.

## Inventory

| Area | Types | Contract |
|---|---|---|
| Attributes | `FArcEnviroAttributes` | Protection: `RainProtection`, `ColdProtection`, `HeatProtection` (fractional degrees, authored to one decimal place). Values: `Wet` (0..100), `Temperature` (-100..100). Levels 0..3: `WetLevel`, `ChilledLevel`, `HotLevel`. Metas: `WetDelta`, `TemperatureDelta`. |
| Condition delta | `FArcEnviroAttributeHandler_ConditionDelta` (`ArcEnviroAttributeHandlers.h`) | Assigned to a delta meta. The same shape as AK `MetaToResource` (`Resource`, `MinConst`, `MaxConst`, `Op`): applies the meta to `Resource`, clamps it, writes it with `SetAttributeDirectAndNotify` and resets the meta. Then sets `Level` to the `Bands` tier of the new value and `NegativeLevel` to the tier of the negated value, each only when it differs. Wet: `Resource` Wet, `Level` WetLevel. Temperature: `Resource` Temperature, `Level` HotLevel, `NegativeLevel` ChilledLevel. Bands 25/50/75. |
| Volumes | `AArcEnviroVolume`, `UArcEnviroEffectsSubsystem::GetOverridesAtLocation` (`FArcEnviroLevelOverrides`) | A box overriding the Rain, Cold or Heat degree; highest `Priority` wins per axis, at equal priority the lower level. No consumer until weather. |

## Contracts

- The level attributes have no handler of their own; only the condition-delta handler writes them. A write to `Wet` or `Temperature` that bypasses the delta meta leaves the level stale.
- `ChilledLevel` and `HotLevel` come from one `Temperature` value, so with a first threshold above 0 at most one of them is above 0.
- Hypothermia/Heatstroke (time at level 3), weather-driven targets and drift, and how protection enters the effective degree come with the weather plan.
