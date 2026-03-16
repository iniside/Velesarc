# ArcGameSettings — MVVM Game Settings Framework

A data-driven settings system using UE5's ModelViewViewModel plugin. Settings are defined via a fluent builder API, stored in PropertyBags or forwarded to UGameUserSettings via getter/setter lambdas, and exposed to UMG widgets through projection-only ViewModels with FieldNotify bindings.

## Architecture

```
Widgets (UMG + MVVM bindings)
    │ FieldNotify
ViewModels (projection-only, no stored Model ref)
    │ Refresh()
Model (UArcSettingsModel — UGameInstanceSubsystem)
    │ owns descriptors, values, ViewModels
Storage (PropertyBag Local/Shared, UGameUserSettings, or custom lambdas)
```

## Quick Start — Registering Settings

Get the model from a UGameInstanceSubsystem or via OnModelReady:

```cpp
UArcSettingsModel* Model = GameInstance->GetSubsystem<UArcSettingsModel>();
```

Register settings with the fluent builder:

```cpp
// Discrete setting (dropdown/toggle) stored in Shared PropertyBag
Model->Discrete(TAG_MyFeature_Discrete_Difficulty, TAG_MyFeature)
    .Name(NSLOCTEXT("Settings", "Difficulty", "Difficulty"))
    .Desc(NSLOCTEXT("Settings", "Difficulty_Desc", "Game difficulty level"))
    .Shared("Difficulty")
    .Options({
        {TEXT("0"), NSLOCTEXT("Settings", "Easy",   "Easy")},
        {TEXT("1"), NSLOCTEXT("Settings", "Normal", "Normal")},
        {TEXT("2"), NSLOCTEXT("Settings", "Hard",   "Hard")}
    })
    .Default(1)
    .Done();

// Scalar setting (slider) stored in Local PropertyBag
Model->Scalar(TAG_MyFeature_Scalar_Volume, TAG_MyFeature)
    .Name(NSLOCTEXT("Settings", "Volume", "Volume"))
    .Local("MasterVolume")
    .Range(0.0, 1.0, 0.05)
    .Default(0.8)
    .Format(EArcSettingDisplayFormat::Percent)
    .Done();

// Action setting (button that opens a modal)
Model->Action(TAG_MyFeature_Action_Calibrate, TAG_MyFeature)
    .Name(NSLOCTEXT("Settings", "Calibrate", "Calibrate"))
    .ActionText(NSLOCTEXT("Settings", "Calibrate_Btn", "Start"))
    .ActionTag(TAG_MyFeature_Action_Calibrate)
    .Done();
```

## Tag Naming Convention

Setting tags encode the ViewModel type so widgets know which ViewModel class to use:

```
Settings.<Category>.<Type>.<SettingName>
```

Where `<Type>` is one of: `Discrete`, `Scalar`, `Action`.

Examples:
- `Settings.Video.Discrete.WindowMode` → `UArcDiscreteSettingViewModel`
- `Settings.Audio.Scalar.OverallVolume` → `UArcScalarSettingViewModel`
- `Settings.Video.Action.SafeZone` → `UArcActionSettingViewModel`

Category tags have no type segment: `Settings.Video`, `Settings.Audio`, etc.
Action trigger tags also have no type segment: `Settings.Action.OpenSafeZoneEditor`.

## Storage Types

| Method | Storage | Persistence |
| ------ | ------- | ----------- |
| `.Local("Name")` | FInstancedPropertyBag | Machine-specific, saved to file |
| `.Shared("Name")` | FInstancedPropertyBag | Cloud-safe, saved via USaveGame |
| `.Engine("Name")` | Categorization only | Must provide `.Getter()`/`.Setter()` |

For Engine settings or any setting needing side effects, provide lambdas:

```cpp
Model->Discrete(TAG_Video_Discrete_WindowMode, TAG_Video)
    .Name(NSLOCTEXT("Settings", "WindowMode", "Window Mode"))
    .Engine("FullscreenMode")
    .Options({...})
    .Getter([]() -> int32 {
        return (int32)GEngine->GetGameUserSettings()->GetFullscreenMode();
    })
    .Setter([](int32 V) {
        auto* S = GEngine->GetGameUserSettings();
        S->SetFullscreenMode(EWindowMode::Type(V));
        S->ApplyResolutionSettings(false);
    })
    .Done();
```

Lambdas override PropertyBag storage. If a Getter is provided, the Model calls it instead of reading the PropertyBag. Same for Setter.

## Dynamic Options

For settings whose options change at runtime (resolutions, audio devices):

```cpp
Model->Discrete(TAG_Audio_OutputDevice, TAG_Audio)
    .Options({{"default", "Default"}})   // fallback
    .DynamicOptions([]() -> TArray<FArcSettingOption> {
        // Enumerate audio devices from platform API
        return {...};
    })
    .Done();
```

`DynamicOptions` is called during ViewModel `Refresh()`. For push-based updates (e.g., device hot-plug):

```cpp
Model->UpdateOptions(TAG_Audio_OutputDevice, NewOptionList);
```

## Edit Conditions

Control visibility per-setting with a lambda:

```cpp
.EditCondition([](const UArcSettingsModel&) -> EArcSettingVisibility {
    return ICommonUIModule::GetSettings().GetPlatformTraits()
        .HasTag(FGameplayTag::RequestGameplayTag(TEXT("Platform.Trait.XXX")))
        ? EArcSettingVisibility::Visible
        : EArcSettingVisibility::Hidden;
})
```

Conditions that depend on other settings:

```cpp
.DependsOn({TAG_Video_WindowMode})
.EditCondition([](const UArcSettingsModel& M) -> EArcSettingVisibility {
    return M.GetDiscreteIndex(TAG_Video_WindowMode) == 0  // Fullscreen
        ? EArcSettingVisibility::Visible
        : EArcSettingVisibility::Disabled;
})
```

## Cross-Setting Coordination

Use `RegisterOnChanged` to propagate changes between settings:

```cpp
Model->RegisterOnChanged(TAG_Video_OverallQuality,
    [](UArcSettingsModel& M, FGameplayTag) {
        int32 Idx = M.GetDiscreteIndex(TAG_Video_OverallQuality);
        M.SetDiscreteIndex(TAG_Video_Shadows, Idx);
        M.SetDiscreteIndex(TAG_Video_Textures, Idx);
    });
```

OnChanged callbacks fire at depth 1 only. Nested `SetDiscreteIndex` calls (depth 2) store values and refresh ViewModels but do NOT trigger further OnChanged callbacks. This prevents infinite loops.

**Rule:** Side effects that must run at ALL depths go in the Setter lambda. OnChanged is for cross-setting coordination only.

## Modular Registration

External modules register settings by ensuring their subsystem initializes after UArcSettingsModel:

```cpp
void UMySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UArcSettingsModel>();

    UArcSettingsModel* Model = GetGameInstance()->GetSubsystem<UArcSettingsModel>();
    // Register settings...
}
```

GameFeature plugins can unregister on deactivation:

```cpp
Model->UnregisterSetting(TAG_MyFeature_Option);
```

## ViewModel Resolution (for Widget Blueprints)

In Widget Blueprint MVVM view settings:

1. Add a source of type `UArcDiscreteSettingViewModel` (or Scalar/Action)
2. Set the resolver to `UArcSettingsViewModelResolver`
3. Set the resolver's `SettingTag` property to the desired setting tag

The resolver looks up the pre-existing ViewModel from the Model's collection. ViewModels are created at registration time and owned by the Model.

Alternatively, entry widgets can bypass the resolver and call `Model->GetViewModel(Tag)` directly in `OnListItemObjectSet`.

## Dirty Tracking

```cpp
Model->SnapshotCurrentState();  // Call when settings screen opens
Model->ApplyChanges();          // Persist to disk
Model->RestoreToInitial();      // Revert to snapshot
```

## Key Types

| Type | Purpose |
| ---- | ------- |
| `FArcSettingDescriptor` | Base descriptor (tag, name, storage) |
| `FArcDiscreteSettingDescriptor` | Adds Options, DefaultIndex |
| `FArcScalarSettingDescriptor` | Adds Min/Max/Step/Default/Format |
| `FArcActionSettingDescriptor` | Adds ActionText, ActionTag |
| `FArcSettingOption` | {FString Value, FText DisplayText} |
| `UArcSettingsModel` | Game instance subsystem, owns everything |
| `UArcDiscreteSettingViewModel` | FieldNotify VM for discrete settings |
| `UArcScalarSettingViewModel` | FieldNotify VM for scalar settings |
| `UArcActionSettingViewModel` | FieldNotify VM for action settings |
| `UArcSettingsViewModelResolver` | MVVM resolver, returns VM by SettingTag |
| `EArcSettingStorage` | Local, Shared, Engine |
| `EArcSettingVisibility` | Visible, Disabled, Hidden |
| `EArcSettingDisplayFormat` | Percent, Integer, Float, Custom |
