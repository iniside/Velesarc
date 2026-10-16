# ArcCraft Plugin

Crafting system with recipe-based item creation, output rarity, material-driven intrinsic stats, random modifier pools, and material property evaluation. Entity-based (Mass Framework) crafting path only.

## Module Structure

- `ArcCraft` (Runtime): Main crafting logic and the craft record types. Public deps include ArcCore, ArcMass, ArcMassItemsRuntime, ArcMassAbilities, ArcAbilityKit, ArcMassGameplayFramework, ArcMassReplicationRuntime, GameplayTags, Iris/IrisCore, MassEntity/MassCore/MassSignals, SlateIM, DeveloperSettings (`ArcCraft.Build.cs`)
- `ArcCraftEditor` (Editor): record editors for the craft tables, recipe graph. Depends on UnrealEd, ArcCraft, ArcJsonAssetsEditor
- `ArcCraftTest` (Separate plugin at `Plugins/ArcX/Tests/ArcCraftTest/`): Tests using **CQTest** framework. Depends on ArcCore, ArcCraft, ArcMassItemsRuntime, ArcMassAbilities, ArcSharedTestHarness, ArcMassItemsTest (editor: ArcCraftEditor)

## Directory Layout (Source/ArcCraft/)

- `Shared/` — Terminal modifier base & subtypes (`FArcCraftModifier`), record lookup (`ArcCraftRecords::Find<T>`), craft item tags (`ArcCraftItemTags::GetCraftTags`), `FArcItemFragment_ItemStatRanges`
- `Recipe/` — Recipe and random pool records, ingredients, modifier sources, craft context (`FArcCraftContext`), output builder, slot resolver
- `Rarity/` — Craft rules and stat catalog records, `UArcCraftSettings` (`Rules` record id), and the pure roll functions `ArcCraftRarity::*` (`ArcCraftRarity.h`)
- `Items/` — Definition fragment: `FArcItemFragment_RecipeUnlock`
- `StatTransfer/`, `Static/` — Stat transfer table and static modifier set records
- `MaterialCraft/` — Material property tables (`Part`: the equipment part, read only by tools and the editor), rules with TagQuery matching, the `MinAverageR` gate and a rule weight, evaluator
- `Station/` — Item sources, output delivery, recipe lookup
- `Mass/` — Entity-based crafting: fragments, station trait, tick processor, output resume subsystem, queue-changed signal
- `Commands/` — Replicated commands: `FArcCraftFromStationInputCommand` (moves the client's grouped input `Sources` into the station input, then queues), `FArcTransferItemFromStationCommand` (withdraw from the station input), `FArcWithdrawCraftOutputCommand`; shared auth gate `ArcCraftAuth.h`

## FInstancedStruct Hierarchies

All extensible data uses `FInstancedStruct` with `ExcludeBaseStruct` meta on UPROPERTY to enforce selecting subtypes only.

### Terminal Modifiers (applied to items)

`FArcCraftModifier` → `_Stats`, `_Abilities`, `_Effects`, `_AttributeModifier`, `_RandomPool`
- `_Stats`, `_Abilities`, `_Effects`, `_AttributeModifier` each hold one **singular** field: `StatId`, `AbilityEntry`, `Effect`, `Modifier` (not arrays)
- Common base fields: `MinR`, `TriggerTags`, `SlotTags` (chair tags; native `Modifier.Stat`, `Modifier.Passive`, `Modifier.Penalty` in `ArcCraftTags.h`; empty = unslotted)
- `ArcCraftModifiers::ConvertModifier` is the only modifier → result conversion (sources, nested pools, editor debug evals); each result carries the producing modifier's `SlotTags`
- `_Stats` resolves `StatId` in the rules' `StatCatalog`: a wearer `FArcDirectModifier` (catalog attribute, `Sign`, tag requirements), or an item stat `Multiply 1 + value` when the entry is `bItemSide`; with `Modifier.Penalty` it is `−value·Sign` / `Multiply 1 − value`. The value is `ArcCraftRarity::ComputeBandValue` on the catalog range (output rarity band, `BandBiasPerR`, `BandBiasPerSkillMargin`, `AverageR`, `SkillMargin`, one stream draw). Unknown id or no catalog: no result, warned once
- `_RandomPool` delegates to a random pool record (`PoolDefinition`, `SelectionMode`); pool entries hold only the other terminals
- Stored in: `FArcMaterialPropertyRule::Modifiers`, `FArcRandomPoolEntry::Modifiers`, `FArcStaticModifierSetData::Modifiers`

### Recipe Modifier Sources

`FArcRecipeModifierSource` → `_RandomPool`, `_MaterialProperties`, `_Static`, `_StatTransferTable`
- Stored in: `FArcRecipeDefinitionData::ModifierSources` (sources reference the table records by id)
- `Evaluate(..., const FArcCraftContext&)` returns `FArcCraftPendingModifier`s for slot resolution; the source carries `Weight` and `MinAverageR`, slot tags come from the modifiers
- Every pending result carries its group's provenance: the group is (`ModifierIndex` = source index, `GroupIndex`), `EffectiveWeight` is the group weight, `MatchedTags` the group's required tags the ingredients hold, `IngredientShare` the consumed amount (`SlotAmounts`) of the slots holding any required tag over the total consumed amount (0 without requirements)
- `_MaterialProperties`: every matched rule whose modifiers clear `MinR` is one group (`GroupIndex` = rule index), all its eligible modifiers convert, in table order. Rule weight (`FArcMaterialCraftEvaluator::ComputeRuleWeight`) = `ArcRandomPoolEval::ApplyWeightModifiers(Weight, WeightModifiers, combined slot AssetTags)` × `(1 + (AverageR − 1)·RWeightScaling)` × `(1 + BandWeightBonus)`, floored at 0; `BandWeightBonus` is the remedy bonus (extra ingredients, extra time). The required tags are the positive terms of `TagQuery` (`GetPositiveQueryTags`; tags under NOT are left out). `Match` (`EArcMaterialRuleMatch`): `Union` (default) matches `TagQuery` on the union of slot `AssetTags`; `SingleIngredient` matches only when one non-empty slot's own tags satisfy the whole query, and the rule's `MatchedTags` and `IngredientShare` then count only those slots (`FArcMaterialCraftEvaluator::GetRuleSlotTags`); `WeightModifiers` stay on the union
- `_RandomPool`: group = selected entry (`GroupIndex` = entry index), required tags = `RequiredIngredientTags`, weight = the source `Weight`. `_Static`: group = the set (`GroupIndex` 0), required tags = the union of its modifiers' `TriggerTags`, weight = the source `Weight`. Both match slot `AssetTags`. `_StatTransferTable` results are ungrouped (`GroupIndex` −1)
- `_StatTransferTable`: an entry reads the recipe slot `SlotIndex`, or every slot when it is `INDEX_NONE`; a read slot counts when its item matches the entry's `IngredientQuery` and has a range for the stat (an empty optional slot never counts). Each counted slot rolls `ArcCraftRarity::ComputeBandValue` on that item's `ItemStatRanges` range (output rarity band, R = that slot's rarity step, the entry's `RBias`, `BandBiasPerSkillMargin`, `SkillMargin`, one stream draw in entry × slot order) and adds roll × the entry's `Share` to one summed result per (stat, `OutputModType`), in first-contribution order. This is the sum of parts inside one item (D-107 §2); D-77 e (no same-stat stacking) is about worn pieces. `SlotIndex` lets one role in two template positions (main and reinforcement `Wire`) carry two shares without being counted twice. A stat in the rules' `RarityBandExempt` (weight) rolls on the full range with only the `RBias` bias, so skill never moves it. Results are unslotted and always apply

### Other Hierarchies

- **Ingredients**: `FArcRecipeIngredient` → `_ItemDef`, `_Tags`. `bOptional`: the slot never blocks a craft; it is filled when the input holds a fitting item after the required slots are matched
- **Item Sources**: `FArcCraftItemSource` → `_EntityStore` (reads `FArcCraftInputFragment`; deposit/withdraw helpers in `ArcCraftStationXfer`). Ingredients always live in the station's input. The crafting UI keeps a client-side input preview; `FArcCraftFromStationInputCommand` carries it as `Sources` (owner entity, store type, item id, stacks), validates it (owner = instigator, item store type, loose items, summed stacks, `AcceptsDeposits`) in `Execute` and again in its deferred lambda, moves it with `DepositItem_Mass` (one deposit per (owner, store type, item id), rows summed), then queues against the input. An `ItemId` names one item: a full deposit keeps it; moving part of a stack (deposit, input withdraw, output withdraw) is a split and the moved part gets `FArcItemId::Generate()`. A withdraw into the instigator may still merge into an existing stack or get new ids from bag spilling. The legacy `_InstigatorMassStore` / `_EntityThenInstigator` (hybrid) sources and the `EArcCraftSourcePolicy` were removed.
- **Output Delivery**: `FArcCraftOutputDelivery` → `_EntityStore`, `_EntityItemStore`, `_EntityBag`. `DeliverOutput_Mass` takes every spec of one finished craft. Store policies add them all; `_EntityBag` adds in order inside one deferred command, stops at the first spec that does not fit, holds the rest in `FArcCraftQueueEntry::HeldOutput` and blocks the entry (`bOutputBlocked`); the entry advances once, when nothing is left. A resumed craft re-delivers `HeldOutput` instead of rebuilding.
- **Pool Selection**: `FArcRandomPoolSelectionMode` → `_SimpleRandom`, `_Budget`
- **Slot Selection**: `FArcCraftSlotSelector` → `_HighestWeight`, `_WeightedRandom` (weighted draw without replacement, one `Stream.FRand` per pick while a positive weight remains), `_IngredientShare`, `_TagPriority` (ordered `Tags`; rank = first one in the candidate's `MatchedTags`, none = last). `Order(TArray<FArcCraftChairCandidate>&, const FRandomStream&)` sorts best first; candidates arrive in (`ModifierIndex`, `GroupIndex`, `Index`) order and ties keep it. Stored in `FArcRecipeDefinitionData::SlotSelector`; empty = HighestWeight

## Data Flow

Rolls, formulas, draw order and every rules-record number: [GameDesign/mechanics/crafting-rolls.md](../../../GameDesign/mechanics/crafting-rolls.md). Vectors: `Tools/CraftData/roll_vectors.json` (`craft_sim.py --selftest`) pin the Python mirror against the spec; C++ parity rests on code reading, no C++ test reads them.

1. The recipe record (`FArcRecipeDefinitionData`) holds ingredients (FInstancedStruct), `Outputs` (`FArcRecipeOutput`: item id, amount, level, `InstanceTags`, `bKeepIngredientRarity` (read on `Outputs[0]` only); `Outputs[0]` is the main output), modifier sources (FInstancedStruct), modifier slots (chairs), the slot selector, `GuaranteedRarity` (the start rarity; empty = `DefaultRarity`), `BaseRecipe` (the variant's base, `AllowedTypes = "ArcRecipe"`; empty on a base, i.e. any recipe without `BaseRecipe`; a base-less family points at its lowest-rarity member) and the requirement fields. `RequiredSkill` and `RequiredSkillLevel` are read for the skill margin (step 3); `bDefaultUnlock`, `Difficulty`, `XpMultiplier` are data only. `GetDisplayName()` is `RecipeName`, else the main output's `UIDescription::ItemName`, else empty; display code uses it, never `RecipeName` directly
2. `FArcCraftItemSource::MatchAndConsumeFromSpecs` matches ingredients (`DoesItemSatisfy()`): required slots first, then optional slots, each by first match from what is left; an optional slot with no fit stays empty (`OutSlotFilled`), weighs 0 in `ComputeAverageR` and adds nothing to a transfer sum. A slot may draw its amount from several specs of **one** definition and equal instance tags (`FArcItemFragment_InstanceTags`; rarity may differ, so an enchanted and a plain spec of one definition never share a slot); the slot step is the amount-weighted average of `ArcCraftRarity::GetRarityStep` (spec `FArcItemFragment_Rarity`, absent = rules `DefaultRarity`). Steps are cached on the queue entry at consume time (`SlotRaritySteps`, `CachedAverageR` from `ComputeAverageR`), because `ReconstructMatchedSpecs` rebuilds ingredient specs without rarity; the slot instance tags are cached too (`SlotInstanceTags`) and restored on the rebuilt specs
3. At completion `UArcCraftTickProcessor` builds `FArcCraftContext`: slot steps, `AverageR` and `SkillMargin = max(0, skill − RequiredSkillLevel)`, the skill read from the queue entry's `InstigatorEntity` (invalid instigator or unset skill = 0). It also fills `SlotAmounts` (the recipe ingredient's `Amount`, 0 when not `bConsumeOnCraft` or when the slot stayed empty) and, for a fresh build, seeds `Stream` (`FMath::Rand32`, logged at Log level with recipe and entry id); every craft roll draws from `Stream`, nothing else draws
4. `FArcCraftOutputBuilder::Build` decides the output rarity first (`DecideOutputRarity`: guarantee, Poor below `PoorBelowR`, upgrade roll with margin and R) unless the main output definition has a rules `NoRarityOutputTags` tag (consumables). With `bKeepIngredientRarity` it rolls nothing: the output takes the highest ladder rarity whose `Step` is at most the slot step of the first required ingredient (exact for a slot of one rarity; a mixed slot rounds down), so enchanting keeps a Rare sheet Rare
5. Chairs and selector come from one recipe. The start rarity is `GuaranteedRarity`, else `DefaultRarity`. When the output rarity equals it, the output rolls no rarity, or the main output has `bKeepIngredientRarity`, they are the crafted recipe's own. Otherwise `FArcRecipeLookup::FindRarityVariant` scans the recipe records for the family member (the base = `BaseRecipe`, else the crafted recipe's id, which `Build` takes from the tick processor; the base itself or a recipe whose `BaseRecipe` is it) whose start rarity is the output rarity. None found: no chairs, logged once per recipe id and rarity (Verbose for Poor = `Ladder[0]`, Warning otherwise). No index or cache.
6. Modifier sources (always the crafted recipe's) evaluate with that context → `FArcCraftPendingModifier`; `FArcCraftSlotResolver::Resolve` (chairs, selector, stream) decides what applies to `Outputs[0]`:
   - ungrouped results (transfer, one summed result per stat) and the non-penalty results of a group without bonus chair tags always apply;
   - a group (`ModifierIndex`, `GroupIndex`) competes with the union of its non-penalty slotted `SlotTags` and takes one whole bonus chair (a chair without `Modifier.Penalty` whose tags `HasAll` them), the narrowest free one, in selector order. Its unslotted results apply with it;
   - dedupe: a group sharing a bonus stat (item stat: attribute + `StatTags`; wearer modifier: attribute + source/target tag requirements) or an effect with a placed group replaces it only with a strictly higher absolute value (`|v|`, `|v − 1|` for a non-additive item stat), otherwise it is dropped; either way filling goes on;
   - penalty chairs (`Modifier.Penalty`) take, by the same selector, the `Modifier.Penalty` results of the placed groups, never on a stat a placed bonus carries, one penalty per stat (same dedupe);
   - no chairs = no slotted result. A pass with no chairs or no candidates does not call the selector, so it draws nothing
7. The rarity fragment is written on `Outputs[0]` only when the result ≠ `DefaultRarity`, so default outputs keep stacking with plain items. Any output with non-empty `InstanceTags` gets `FArcItemFragment_InstanceTags` with them (an enchanted material: `Material.Property.Magic` + `Enchant.*`); output delivery stacks through `IsSameStackKey`, which compares them. Byproducts carry no rarity or modifiers; invalid output ids are skipped. ArcWorldSim `UArcProductionProcessor` does not use the builder: it adds each output as a plain (Common) spec

## Records (JSON assets)

All craft data are JSON records; the data assets and the old per-type JSON loaders are gone. Format, index and editor rules: [ArcJsonAssets CLAUDE.md](../ArcJsonAssets/CLAUDE.md).

| Extension | Record | Type |
|---|---|---|
| `.arcrecipe` | `FArcRecipeDefinitionData` | `ArcRecipe` (a record may set `RecipeType`) |
| `.arcmaterialtable` | `FArcMaterialPropertyTableData` | `ArcMaterialPropertyTable` |
| `.arcrandompool` | `FArcRandomPoolDefinitionData` | `ArcRandomPool` |
| `.arcstattransfer` | `FArcStatTransferTableData` | `ArcStatTransferTable` |
| `.arcstaticmods` | `FArcStaticModifierSetData` | `ArcStaticModifierSet` |
| `.arccraftrules` | `FArcCraftRulesData` | `ArcCraftRules` (rarity ladder with band fractions, upgrade roll, `NoRarityOutputTags`, `RarityBandExempt`, `StatCatalog`, band biases; the active one is `UArcCraftSettings::Rules`, `DefaultGame.ini [/Script/ArcCraft.ArcCraftSettings]`) |
| `.arcstatcatalog` | `FArcStatCatalogData` | `ArcStatCatalog` (stat id → attribute, sign, category, tag requirements, range, `bItemSide`) |

- Registered in `ArcCraftModule.cpp`. References between craft data are `FArcNamedPrimaryAssetId` with `AllowedTypes` (modifier source tables, `FArcCraftModifier_RandomPool::PoolDefinition`, station `AvailableRecipes`, ArcWorldSim `FArcProductionConfigFragment::Recipes`).
- Code holds ids and resolves with `ArcCraftRecords::Find<T>(Id)` (wraps `UArcCoreAssetManager::FindJsonRecord`, game thread; a missing record warns once per id). A resolved pointer is used within one call, never stored. `UArcCraftTickProcessor` therefore runs on the game thread (todo: back to workers).
- `FArcRecipeLookup` finds recipes over the `.arcrecipe` records of every type (station/recipe tag gates, ingredient tags from `_Tags` ingredients, the rarity variant of a family).
- Editor: material, pool and static set tables open in their ArcCraftEditor toolkits (registered with `ArcJsonRecordEditing::RegisterRecordEditor`, edit the shared record edit object, undo, no Save As); recipes, stat transfer tables, craft rules and stat catalogs use the generic tab; the recipe graph reads recipe and item records (`.arcitem`) and creates both with `CreateRecordFile`.
- Game data: `ArcRPG/Content/Data/Crafting/` (`CraftRules`, `StatCatalog`, `StatTransfer/ST_*`); checked by `Tools/CraftData/check_records.py`, simulated by `Tools/CraftData/craft_sim.py`.
- `FArcItemFragment_RecipeUnlock` (`Items/ArcItemFragment_RecipeUnlock.h`): ordinary definition fragment listing the recipes (`Recipes`, `AllowedTypes = "ArcRecipe"`) an item teaches; data only, nothing reads it. In `.arcitem` JSON: `"_structType": "ArcItemFragment_RecipeUnlock"`.
- Item definitions are records too (`FArcItemDefinitionData`, ArcMassItems); recipe and ingredient item references are `FArcNamedPrimaryAssetId` with `AllowedTypes = "ArcItem"`.

## Testing (ArcCraftTest Plugin)

- Framework: **CQTest** (`#include "CQTest.h"`)
- Pattern: `TEST_CLASS(ClassName, "FilterPath")` + `TEST_METHOD(MethodName)` with `ASSERT_THAT()` matchers
- Test files are self-contained .cpp files (no headers except integration test helpers)
- Record fixtures: `ArcCraftTestRecordFiles.h` (`FArcCraftTestRecordFiles`) writes record files under the ArcCraftTest content root, indexes them and removes them; recipes and tables otherwise are in-memory `Data` structs
- Helper pattern: namespace with factory functions (`MakeStatModifier()`, `MakeStatEntry()`, `CreateTransientItemDef()`, etc.)
- Tests use `GetTransientPackage()` for transient UObjects, `RF_Transient` flag
- Each test file defines its own gameplay tags with unique prefixes to avoid ODR violations: `TAG_OutTest_*`, `TAG_JsonTest_*`, `TAG_PoolTest_*`, `TAG_IntTest_*`

## Key Conventions

- Terminal modifier fields are singular (one stat/ability/effect per modifier entry, use multiple modifier entries for multiple values)
- Material craft separates `BandEligibilityR` (threshold check) from `BandWeightBonus` (rule weight scaling)
- Per-slot contexts in material crafting track `PerSlotTags` independently for rule evaluation
- Every craft reader of ingredient tags (`_Tags` eligibility, `DenyTags` of `_Tags` and `_ItemDef`, `FArcMaterialCraftContext::Build` per-slot tags, the modifier sources' aggregated and per-slot tags, the transfer `IngredientQuery`) uses `ArcCraftItemTags::GetCraftTags` (`Shared/ArcCraftItemTags.h`): the definition's `AssetTags` plus the spec's instance tags; "slot `AssetTags`" above means these tags
- Entity-based stations persist data in fragments, surviving actor despawn
