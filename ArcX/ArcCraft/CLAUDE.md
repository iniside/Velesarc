# ArcCraft Plugin

Crafting system with recipe-based item creation, quality scaling, random modifier pools, and material property evaluation. Supports both actor-based (UArcCraftStationComponent) and entity-based (Mass Framework) crafting paths.

## Module Structure

- `ArcCraft` (Runtime): Main crafting logic. Depends on ArcCore, ArcMass, ArcJson, GameplayTags, GameplayAbilities, Iris, MassEntity
- `ArcCraftEditor` (Editor): JSON import/export tools, asset factories. Depends on UnrealEd, AssetTools, ArcCraft
- `ArcCraftTest` (Separate plugin at `Plugins/ArcX/ArcCraftTest/`): Tests using **CQTest** framework. Depends on ArcCore, ArcCraft

## Directory Layout (Source/ArcCraft/)

- `Shared/` — Terminal modifier base & subtypes (`FArcCraftModifier`), JSON utils (`ArcCraftJsonUtils`)
- `Recipe/` — Recipe definitions, ingredients, output modifiers, quality tiers, random pools, slot resolver, JSON loaders
- `MaterialCraft/` — Material property tables, rules with TagQuery matching, quality bands, evaluator
- `Station/` — Actor-based crafting station component, item sources, output delivery, recipe lookup
- `Mass/` — Entity-based crafting: fragments, traits, tick processor, vis-entity sync
- `Commands/` — GAS craft command (`ArcStartRecipeCraftCommand`)
- `Debug/` — ImGui debug window

## FInstancedStruct Hierarchies

All extensible data uses `FInstancedStruct` with `ExcludeBaseStruct` meta on UPROPERTY to enforce selecting subtypes only.

### Terminal Modifiers (applied to items, no recursion)

`FArcCraftModifier` → `_Stats`, `_Abilities`, `_Effects`
- Each holds a **singular** field: `BaseStat`, `AbilityToGrant`, `EffectToGrant` (not arrays)
- Common base fields: `MinQualityThreshold`, `TriggerTags`, `Weight`, `QualityScalingFactor`
- Stored in: `FArcMaterialQualityBand::Modifiers`, `FArcRandomPoolEntry::Modifiers`

### Recipe Output Modifiers (can trigger sub-evaluations)

`FArcRecipeOutputModifier` → `_Stats`, `_Abilities`, `_Effects`, `_TransferStats`, `_Random` (ChooserTable), `_RandomPool`, `_MaterialProperties`
- Stored in: `UArcRecipeDefinition::OutputModifiers`
- **Dual API**: `ApplyToOutput()` (immediate) vs `Evaluate()` (deferred → `FArcCraftPendingModifier` for slot resolution)

### Other Hierarchies

- **Ingredients**: `FArcRecipeIngredient` → `_ItemDef`, `_Tags`
- **Item Sources**: `FArcCraftItemSource` → `_InstigatorStore`, `_StationStore`, `_EntityStore`
- **Output Delivery**: `FArcCraftOutputDelivery` → `_StoreOnStation`, `_ToInstigator`, `_EntityStore`
- **Pool Selection**: `FArcRandomPoolSelectionMode` → `_SimpleRandom`, `_Budget`

## Data Flow

1. `UArcRecipeDefinition` holds ingredients (FInstancedStruct), output modifiers (FInstancedStruct), quality tier table, modifier slots
2. Ingredients matched via `FArcRecipeIngredient::DoesItemSatisfy()` with quality multiplier from `UArcQualityTierTable`
3. Average quality computed across matched ingredients
4. Output modifiers evaluated: immediate apply or deferred via `Evaluate()` → `FArcCraftPendingModifier`
5. Slot resolution (`FArcCraftSlotResolver`) picks highest-weight modifiers per slot tag
6. `FArcCraftOutputBuilder` shared by both actor-based (`UArcCraftStationComponent`) and entity-based (`FArcCraftTickProcessor`) paths

## JSON System

- **Loaders**: `UArcRecipeJsonLoader`, `UArcQualityTierTableJsonLoader`, `UArcRandomPoolJsonLoader`, `UArcMaterialPropertyTableJsonLoader`
- All loaders are UObject with static `ParseJson()` methods
- Support reimport (clears + replaces data)
- Round-trip: `ParseJson()` ↔ `ExportToJson()` on data assets
- JSON keys use **singular** forms for terminal modifier fields: `"stat"`, `"ability"`, `"effect"` (each is a single object, not array)
- Shared utils in `ArcCraftJsonUtils.h`: `ParseCraftModifier()`, `SerializeCraftModifier()`, `ParseOutputModifier()`, `SerializeOutputModifier()`

## Testing (ArcCraftTest Plugin)

- Framework: **CQTest** (`#include "CQTest.h"`)
- Pattern: `TEST_CLASS(ClassName, "FilterPath")` + `TEST_METHOD(MethodName)` with `ASSERT_THAT()` matchers
- Test files are self-contained .cpp files (no headers except integration test helpers)
- Integration test helpers in `ArcCraftStationIntegrationTests.h`: testable subclass `UArcCraftStationTestComponent`, transient item/recipe factories
- Helper pattern: namespace with factory functions (`MakeStatModifier()`, `MakeStatEntry()`, `CreateTransientItemDef()`, etc.)
- Tests use `GetTransientPackage()` for transient UObjects, `RF_Transient` flag
- Each test file defines its own gameplay tags with unique prefixes to avoid ODR violations: `TAG_OutTest_*`, `TAG_JsonTest_*`, `TAG_PoolTest_*`, `TAG_IntTest_*`

## Key Conventions

- Terminal modifier fields are singular (one stat/ability/effect per modifier entry, use multiple modifier entries for multiple values)
- Material craft separates `BandEligibilityQuality` (threshold check) from `BandWeightBonus` (weight scaling)
- Per-slot contexts in material crafting track `PerSlotTags` independently for rule evaluation
- Iris fast array serializer used for queue replication
- Entity-based stations persist data in fragments, surviving actor despawn
