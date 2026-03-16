/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "Descriptors/ArcSettingDescriptor.h"
#include "Model/ArcSettingsModel.h"
#include "NativeGameplayTags.h"
#include "CommonInputBaseTypes.h"
#include "CommonUISettings.h"
#include "ICommonUIModule.h"
#include "Descriptors/ArcSettingBuilder.h"

// ============================================================
// Tags — categories
// ============================================================

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_MouseKeyboard, "Settings.MouseKeyboard");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Gamepad,       "Settings.Gamepad");

// ============================================================
// Tags — Mouse & Keyboard settings
// ============================================================

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_MouseKeyboard_Scalar_SensitivityX,         "Settings.MouseKeyboard.Scalar.SensitivityX");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_MouseKeyboard_Scalar_SensitivityY,         "Settings.MouseKeyboard.Scalar.SensitivityY");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_MouseKeyboard_Scalar_TargetingMultiplier,  "Settings.MouseKeyboard.Scalar.TargetingMultiplier");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_MouseKeyboard_Discrete_InvertVerticalAxis,   "Settings.MouseKeyboard.Discrete.InvertVerticalAxis");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_MouseKeyboard_Discrete_InvertHorizontalAxis, "Settings.MouseKeyboard.Discrete.InvertHorizontalAxis");

// ============================================================
// Tags — Gamepad settings
// ============================================================

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Gamepad_Discrete_Vibration,                  "Settings.Gamepad.Discrete.Vibration");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Gamepad_Discrete_InvertVerticalAxis,         "Settings.Gamepad.Discrete.InvertVerticalAxis");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Gamepad_Discrete_InvertHorizontalAxis,       "Settings.Gamepad.Discrete.InvertHorizontalAxis");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Gamepad_Discrete_LookSensitivityPreset,      "Settings.Gamepad.Discrete.LookSensitivityPreset");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Gamepad_Discrete_LookSensitivityPresetAds,   "Settings.Gamepad.Discrete.LookSensitivityPresetAds");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Gamepad_Scalar_MoveStickDeadZone,            "Settings.Gamepad.Scalar.MoveStickDeadZone");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Gamepad_Scalar_LookStickDeadZone,            "Settings.Gamepad.Scalar.LookStickDeadZone");

// ============================================================
// Implementation namespace (file-scoped)
// ============================================================

namespace ArcSettingsRegistration_Input_Impl
{

static TArray<FArcSettingOption> MakeOffOnOptions()
{
	return {
		{TEXT("0"), NSLOCTEXT("Settings", "Toggle_Off", "Off")},
		{TEXT("1"), NSLOCTEXT("Settings", "Toggle_On",  "On")},
	};
}

static TArray<FArcSettingOption> MakeGamepadSensitivityOptions()
{
	return {
		{TEXT("1"),  NSLOCTEXT("Settings", "GPSens_1",  "1 - Slow")},
		{TEXT("2"),  NSLOCTEXT("Settings", "GPSens_2",  "2 - Slower")},
		{TEXT("3"),  NSLOCTEXT("Settings", "GPSens_3",  "3 - Slow-ish")},
		{TEXT("4"),  NSLOCTEXT("Settings", "GPSens_4",  "4 - Normal")},
		{TEXT("5"),  NSLOCTEXT("Settings", "GPSens_5",  "5 - Normal-ish")},
		{TEXT("6"),  NSLOCTEXT("Settings", "GPSens_6",  "6 - Fast-ish")},
		{TEXT("7"),  NSLOCTEXT("Settings", "GPSens_7",  "7 - Fast")},
		{TEXT("8"),  NSLOCTEXT("Settings", "GPSens_8",  "8 - Faster")},
		{TEXT("9"),  NSLOCTEXT("Settings", "GPSens_9",  "9 - Very Fast")},
		{TEXT("10"), NSLOCTEXT("Settings", "GPSens_10", "10 - Insane")},
	};
}

static EArcSettingVisibility MouseAndKeyboardVisibility(const UArcSettingsModel&)
{
	return ICommonUIModule::GetSettings().GetPlatformTraits()
		.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Platform.Trait.Input.SupportsMouseAndKeyboard")))
		? EArcSettingVisibility::Visible : EArcSettingVisibility::Hidden;
}

static EArcSettingVisibility GamepadVisibility(const UArcSettingsModel&)
{
	return ICommonUIModule::GetSettings().GetPlatformTraits()
		.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Platform.Trait.Input.SupportsGamepad")))
		? EArcSettingVisibility::Visible : EArcSettingVisibility::Hidden;
}

static void RegisterInputSettings(UArcSettingsModel& Model)
{
	// ============================================================
	// Mouse & Keyboard
	// ============================================================

	// ---- Mouse Sensitivity X ----
	Model.Scalar(TAG_Settings_MouseKeyboard_Scalar_SensitivityX, TAG_Settings_MouseKeyboard)
		.Name(NSLOCTEXT("Settings", "MouseSensX_Name", "Mouse Sensitivity X"))
		.Shared(TEXT("MouseSensitivityX"))
		.Range(0.01, 10.0, 0.01)
		.Default(1.0)
		.Format(EArcSettingDisplayFormat::Float)
		.EditCondition(&MouseAndKeyboardVisibility)
		.Done();

	// ---- Mouse Sensitivity Y ----
	Model.Scalar(TAG_Settings_MouseKeyboard_Scalar_SensitivityY, TAG_Settings_MouseKeyboard)
		.Name(NSLOCTEXT("Settings", "MouseSensY_Name", "Mouse Sensitivity Y"))
		.Shared(TEXT("MouseSensitivityY"))
		.Range(0.01, 10.0, 0.01)
		.Default(1.0)
		.Format(EArcSettingDisplayFormat::Float)
		.EditCondition(&MouseAndKeyboardVisibility)
		.Done();

	// ---- Targeting Multiplier (ADS) ----
	Model.Scalar(TAG_Settings_MouseKeyboard_Scalar_TargetingMultiplier, TAG_Settings_MouseKeyboard)
		.Name(NSLOCTEXT("Settings", "MouseTargetingMult_Name", "Targeting Sensitivity Multiplier"))
		.Desc(NSLOCTEXT("Settings", "MouseTargetingMult_Desc", "Sensitivity multiplier when aiming down sights"))
		.Shared(TEXT("TargetingMultiplier"))
		.Range(0.01, 10.0, 0.01)
		.Default(0.5)
		.Format(EArcSettingDisplayFormat::Float)
		.EditCondition(&MouseAndKeyboardVisibility)
		.Done();

	// ---- Invert Vertical Axis ----
	Model.Discrete(TAG_Settings_MouseKeyboard_Discrete_InvertVerticalAxis, TAG_Settings_MouseKeyboard)
		.Name(NSLOCTEXT("Settings", "MouseInvertV_Name", "Invert Vertical Axis"))
		.Shared(TEXT("InvertVerticalMouse"))
		.Options(MakeOffOnOptions())
		.Default(0)
		.EditCondition(&MouseAndKeyboardVisibility)
		.Done();

	// ---- Invert Horizontal Axis ----
	Model.Discrete(TAG_Settings_MouseKeyboard_Discrete_InvertHorizontalAxis, TAG_Settings_MouseKeyboard)
		.Name(NSLOCTEXT("Settings", "MouseInvertH_Name", "Invert Horizontal Axis"))
		.Shared(TEXT("InvertHorizontalMouse"))
		.Options(MakeOffOnOptions())
		.Default(0)
		.EditCondition(&MouseAndKeyboardVisibility)
		.Done();

	// ============================================================
	// Gamepad
	// ============================================================

	// ---- Vibration ----
	Model.Discrete(TAG_Settings_Gamepad_Discrete_Vibration, TAG_Settings_Gamepad)
		.Name(NSLOCTEXT("Settings", "GPVibration_Name", "Vibration"))
		.Shared(TEXT("ForceFeedback"))
		.Options(MakeOffOnOptions())
		.Default(1)
		.EditCondition(&GamepadVisibility)
		.Done();

	// ---- Invert Vertical Axis ----
	Model.Discrete(TAG_Settings_Gamepad_Discrete_InvertVerticalAxis, TAG_Settings_Gamepad)
		.Name(NSLOCTEXT("Settings", "GPInvertV_Name", "Invert Vertical Axis"))
		.Shared(TEXT("InvertVerticalGamepad"))
		.Options(MakeOffOnOptions())
		.Default(0)
		.EditCondition(&GamepadVisibility)
		.Done();

	// ---- Invert Horizontal Axis ----
	Model.Discrete(TAG_Settings_Gamepad_Discrete_InvertHorizontalAxis, TAG_Settings_Gamepad)
		.Name(NSLOCTEXT("Settings", "GPInvertH_Name", "Invert Horizontal Axis"))
		.Shared(TEXT("InvertHorizontalGamepad"))
		.Options(MakeOffOnOptions())
		.Default(0)
		.EditCondition(&GamepadVisibility)
		.Done();

	// ---- Look Sensitivity Preset ----
	Model.Discrete(TAG_Settings_Gamepad_Discrete_LookSensitivityPreset, TAG_Settings_Gamepad)
		.Name(NSLOCTEXT("Settings", "GPLookSens_Name", "Look Sensitivity"))
		.Shared(TEXT("GamepadLookSensitivity"))
		.Options(MakeGamepadSensitivityOptions())
		.Default(3)
		.EditCondition(&GamepadVisibility)
		.Done();

	// ---- Look Sensitivity Preset (ADS) ----
	Model.Discrete(TAG_Settings_Gamepad_Discrete_LookSensitivityPresetAds, TAG_Settings_Gamepad)
		.Name(NSLOCTEXT("Settings", "GPLookSensAds_Name", "Look Sensitivity (Aiming)"))
		.Desc(NSLOCTEXT("Settings", "GPLookSensAds_Desc", "Sensitivity when aiming down sights"))
		.Shared(TEXT("GamepadTargetingSensitivity"))
		.Options(MakeGamepadSensitivityOptions())
		.Default(3)
		.EditCondition(&GamepadVisibility)
		.Done();

	// ---- Move Stick Dead Zone ----
	Model.Scalar(TAG_Settings_Gamepad_Scalar_MoveStickDeadZone, TAG_Settings_Gamepad)
		.Name(NSLOCTEXT("Settings", "GPMoveDeadZone_Name", "Move Stick Dead Zone"))
		.Shared(TEXT("GamepadMoveDeadZone"))
		.Range(0.05, 0.95, 0.01)
		.Default(0.15)
		.Format(EArcSettingDisplayFormat::Percent)
		.EditCondition(&GamepadVisibility)
		.Done();

	// ---- Look Stick Dead Zone ----
	Model.Scalar(TAG_Settings_Gamepad_Scalar_LookStickDeadZone, TAG_Settings_Gamepad)
		.Name(NSLOCTEXT("Settings", "GPLookDeadZone_Name", "Look Stick Dead Zone"))
		.Shared(TEXT("GamepadLookDeadZone"))
		.Range(0.05, 0.95, 0.01)
		.Default(0.15)
		.Format(EArcSettingDisplayFormat::Percent)
		.EditCondition(&GamepadVisibility)
		.Done();
}

} // namespace ArcSettingsRegistration_Input_Impl

// ============================================================
// Public trampoline
// ============================================================

void ArcSettingsRegistration_Input_Register(UArcSettingsModel& Model)
{
	ArcSettingsRegistration_Input_Impl::RegisterInputSettings(Model);
}
