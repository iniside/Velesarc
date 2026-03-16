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

#include "Model/ArcSettingsModel.h"
#include "NativeGameplayTags.h"
#include "GameFramework/GameUserSettings.h"
#include "CommonInputBaseTypes.h"
#include "CommonUISettings.h"
#include "ICommonUIModule.h"
#include "Descriptors/ArcSettingBuilder.h"
#include "Engine/Engine.h"
#include "Math/UnrealMathUtility.h"

// ============================================================
// Tags — category
// ============================================================

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video, "Settings.Video");

// ============================================================
// Tags — settings
// ============================================================

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Discrete_WindowMode,               "Settings.Video.Discrete.WindowMode");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Discrete_Resolution,               "Settings.Video.Discrete.Resolution");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Scalar_Brightness,                 "Settings.Video.Scalar.Brightness");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Action_SafeZone,                   "Settings.Video.Action.SafeZone");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Discrete_OverallQuality,           "Settings.Video.Discrete.OverallQuality");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Discrete_GlobalIlluminationQuality,"Settings.Video.Discrete.GlobalIlluminationQuality");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Discrete_Shadows,                  "Settings.Video.Discrete.Shadows");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Discrete_AntiAliasing,             "Settings.Video.Discrete.AntiAliasing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Discrete_ViewDistance,             "Settings.Video.Discrete.ViewDistance");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Discrete_TextureQuality,           "Settings.Video.Discrete.TextureQuality");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Discrete_VisualEffectQuality,      "Settings.Video.Discrete.VisualEffectQuality");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Discrete_ReflectionQuality,        "Settings.Video.Discrete.ReflectionQuality");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Discrete_PostProcessingQuality,    "Settings.Video.Discrete.PostProcessingQuality");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Discrete_VSync,                    "Settings.Video.Discrete.VSync");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Discrete_FrameRateLimit_OnBattery,     "Settings.Video.Discrete.FrameRateLimit.OnBattery");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Discrete_FrameRateLimit_InMenu,        "Settings.Video.Discrete.FrameRateLimit.InMenu");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Discrete_FrameRateLimit_WhenBackgrounded, "Settings.Video.Discrete.FrameRateLimit.WhenBackgrounded");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Video_Discrete_FrameRateLimit_Always,        "Settings.Video.Discrete.FrameRateLimit.Always");

// ============================================================
// Tags — actions
// ============================================================

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Action_OpenSafeZoneEditor, "Settings.Action.OpenSafeZoneEditor");

// ============================================================
// Implementation namespace (file-scoped)
// ============================================================

namespace ArcSettingsRegistration_Video_Impl
{

static TArray<FArcSettingOption> MakeQualityOptions_LowToEpic()
{
	return {
		{TEXT("0"), NSLOCTEXT("Settings", "Quality_Low",    "Low")},
		{TEXT("1"), NSLOCTEXT("Settings", "Quality_Medium", "Medium")},
		{TEXT("2"), NSLOCTEXT("Settings", "Quality_High",   "High")},
		{TEXT("3"), NSLOCTEXT("Settings", "Quality_Epic",   "Epic")},
	};
}

static TArray<FArcSettingOption> MakeQualityOptions_OffToEpic()
{
	return {
		{TEXT("0"), NSLOCTEXT("Settings", "Quality_Off",    "Off")},
		{TEXT("1"), NSLOCTEXT("Settings", "Quality_Medium", "Medium")},
		{TEXT("2"), NSLOCTEXT("Settings", "Quality_High",   "High")},
		{TEXT("3"), NSLOCTEXT("Settings", "Quality_Epic",   "Epic")},
	};
}

static TArray<FArcSettingOption> MakeFrameRateOptions()
{
	return {
		{TEXT("0"),   NSLOCTEXT("Settings", "FPS_Unlimited", "Unlimited")},
		{TEXT("30"),  NSLOCTEXT("Settings", "FPS_30",        "30 FPS")},
		{TEXT("60"),  NSLOCTEXT("Settings", "FPS_60",        "60 FPS")},
		{TEXT("120"), NSLOCTEXT("Settings", "FPS_120",       "120 FPS")},
		{TEXT("144"), NSLOCTEXT("Settings", "FPS_144",       "144 FPS")},
	};
}

static EArcSettingVisibility WindowModeVisibility(const UArcSettingsModel&)
{
	return ICommonUIModule::GetSettings().GetPlatformTraits()
		.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Platform.Trait.SupportsWindowedMode")))
		? EArcSettingVisibility::Visible : EArcSettingVisibility::Hidden;
}

static EArcSettingVisibility BrightnessVisibility(const UArcSettingsModel&)
{
	return ICommonUIModule::GetSettings().GetPlatformTraits()
		.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Platform.Trait.NeedsBrightnessAdjustment")))
		? EArcSettingVisibility::Visible : EArcSettingVisibility::Hidden;
}

static void RegisterVideoSettings(UArcSettingsModel& Model)
{
	// ---- Window Mode ----
	Model.Discrete(TAG_Settings_Video_Discrete_WindowMode, TAG_Settings_Video)
		.Name(NSLOCTEXT("Settings", "WindowMode_Name", "Window Mode"))
		.Engine(TEXT("FullscreenMode"))
		.Options({
			{TEXT("0"), NSLOCTEXT("Settings", "WindowMode_Fullscreen",         "Fullscreen")},
			{TEXT("1"), NSLOCTEXT("Settings", "WindowMode_WindowedFullscreen",  "Windowed Fullscreen")},
			{TEXT("2"), NSLOCTEXT("Settings", "WindowMode_Windowed",            "Windowed")},
		})
		.Default(0)
		.Getter([]() -> int32 {
			return static_cast<int32>(GEngine->GetGameUserSettings()->GetFullscreenMode());
		})
		.Setter([](int32 V) {
			UGameUserSettings* S = GEngine->GetGameUserSettings();
			S->SetFullscreenMode(static_cast<EWindowMode::Type>(V));
			S->ApplyResolutionSettings(false);
		})
		.EditCondition(&WindowModeVisibility)
		.Done();

	// ---- Resolution ----
	Model.Discrete(TAG_Settings_Video_Discrete_Resolution, TAG_Settings_Video)
		.Name(NSLOCTEXT("Settings", "Resolution_Name", "Resolution"))
		.Engine(TEXT("ResolutionSizeX"))
		.DynamicOptions([]() -> TArray<FArcSettingOption> {
			// Placeholder — real implementation enumerates screen resolutions
			return {{TEXT("auto"), NSLOCTEXT("Settings", "Res_Auto", "Auto")}};
		})
		.Default(0)
		.Done();

	// ---- Brightness ----
	// TODO:: Clanker add HDR settings here.

	// ---- Safe Zone ----
	Model.Action(TAG_Settings_Video_Action_SafeZone, TAG_Settings_Video)
		.Name(NSLOCTEXT("Settings", "SafeZone_Name", "Safe Zone"))
		.ActionText(NSLOCTEXT("Settings", "SafeZone_Action", "Adjust"))
		.ActionTag(TAG_Settings_Action_OpenSafeZoneEditor)
		.Done();

	// ---- Overall Quality ----
	Model.Discrete(TAG_Settings_Video_Discrete_OverallQuality, TAG_Settings_Video)
		.Name(NSLOCTEXT("Settings", "OverallQuality_Name", "Overall Quality"))
		.Engine(TEXT("OverallScalabilityLevel"))
		.Options({
			{TEXT("0"), NSLOCTEXT("Settings", "Quality_Low",      "Low")},
			{TEXT("1"), NSLOCTEXT("Settings", "Quality_Medium",   "Medium")},
			{TEXT("2"), NSLOCTEXT("Settings", "Quality_High",     "High")},
			{TEXT("3"), NSLOCTEXT("Settings", "Quality_Epic",     "Epic")},
			{TEXT("4"), NSLOCTEXT("Settings", "Quality_Cinematic","Cinematic")},
		})
		.Default(2)
		.Getter([]() -> int32 {
			return GEngine->GetGameUserSettings()->GetOverallScalabilityLevel();
		})
		.Setter([](int32 V) {
			GEngine->GetGameUserSettings()->SetOverallScalabilityLevel(V);
		})
		.Done();

	// ---- Global Illumination Quality ----
	Model.Discrete(TAG_Settings_Video_Discrete_GlobalIlluminationQuality, TAG_Settings_Video)
		.Name(NSLOCTEXT("Settings", "GIQuality_Name", "Global Illumination Quality"))
		.Engine(TEXT("GlobalIlluminationQuality"))
		.Options(MakeQualityOptions_LowToEpic())
		.Default(2)
		.DependsOn({TAG_Settings_Video_Discrete_OverallQuality})
		.Getter([]() -> int32 {
			return GEngine->GetGameUserSettings()->GetGlobalIlluminationQuality();
		})
		.Setter([](int32 V) {
			GEngine->GetGameUserSettings()->SetGlobalIlluminationQuality(V);
			GEngine->GetGameUserSettings()->ApplyNonResolutionSettings();
		})
		.Done();

	// ---- Shadows ----
	Model.Discrete(TAG_Settings_Video_Discrete_Shadows, TAG_Settings_Video)
		.Name(NSLOCTEXT("Settings", "Shadows_Name", "Shadows"))
		.Engine(TEXT("ShadowQuality"))
		.Options(MakeQualityOptions_OffToEpic())
		.Default(2)
		.DependsOn({TAG_Settings_Video_Discrete_OverallQuality})
		.Getter([]() -> int32 {
			return GEngine->GetGameUserSettings()->GetShadowQuality();
		})
		.Setter([](int32 V) {
			GEngine->GetGameUserSettings()->SetShadowQuality(V);
			GEngine->GetGameUserSettings()->ApplyNonResolutionSettings();
		})
		.Done();

	// ---- Anti-Aliasing ----
	Model.Discrete(TAG_Settings_Video_Discrete_AntiAliasing, TAG_Settings_Video)
		.Name(NSLOCTEXT("Settings", "AntiAliasing_Name", "Anti-Aliasing"))
		.Engine(TEXT("AntiAliasingQuality"))
		.Options(MakeQualityOptions_OffToEpic())
		.Default(2)
		.DependsOn({TAG_Settings_Video_Discrete_OverallQuality})
		.Getter([]() -> int32 {
			return GEngine->GetGameUserSettings()->GetAntiAliasingQuality();
		})
		.Setter([](int32 V) {
			GEngine->GetGameUserSettings()->SetAntiAliasingQuality(V);
			GEngine->GetGameUserSettings()->ApplyNonResolutionSettings();
		})
		.Done();

	// ---- View Distance ----
	Model.Discrete(TAG_Settings_Video_Discrete_ViewDistance, TAG_Settings_Video)
		.Name(NSLOCTEXT("Settings", "ViewDistance_Name", "View Distance"))
		.Engine(TEXT("ViewDistanceQuality"))
		.Options(MakeQualityOptions_LowToEpic())
		.Default(2)
		.DependsOn({TAG_Settings_Video_Discrete_OverallQuality})
		.Getter([]() -> int32 {
			return GEngine->GetGameUserSettings()->GetViewDistanceQuality();
		})
		.Setter([](int32 V) {
			GEngine->GetGameUserSettings()->SetViewDistanceQuality(V);
			GEngine->GetGameUserSettings()->ApplyNonResolutionSettings();
		})
		.Done();

	// ---- Texture Quality ----
	Model.Discrete(TAG_Settings_Video_Discrete_TextureQuality, TAG_Settings_Video)
		.Name(NSLOCTEXT("Settings", "TextureQuality_Name", "Texture Quality"))
		.Engine(TEXT("TextureQuality"))
		.Options(MakeQualityOptions_LowToEpic())
		.Default(2)
		.DependsOn({TAG_Settings_Video_Discrete_OverallQuality})
		.Getter([]() -> int32 {
			return GEngine->GetGameUserSettings()->GetTextureQuality();
		})
		.Setter([](int32 V) {
			GEngine->GetGameUserSettings()->SetTextureQuality(V);
			GEngine->GetGameUserSettings()->ApplyNonResolutionSettings();
		})
		.Done();

	// ---- Visual Effects Quality ----
	Model.Discrete(TAG_Settings_Video_Discrete_VisualEffectQuality, TAG_Settings_Video)
		.Name(NSLOCTEXT("Settings", "VisualEffectQuality_Name", "Visual Effects Quality"))
		.Engine(TEXT("VisualEffectQuality"))
		.Options(MakeQualityOptions_LowToEpic())
		.Default(2)
		.DependsOn({TAG_Settings_Video_Discrete_OverallQuality})
		.Getter([]() -> int32 {
			return GEngine->GetGameUserSettings()->GetVisualEffectQuality();
		})
		.Setter([](int32 V) {
			GEngine->GetGameUserSettings()->SetVisualEffectQuality(V);
			GEngine->GetGameUserSettings()->ApplyNonResolutionSettings();
		})
		.Done();

	// ---- Reflection Quality ----
	Model.Discrete(TAG_Settings_Video_Discrete_ReflectionQuality, TAG_Settings_Video)
		.Name(NSLOCTEXT("Settings", "ReflectionQuality_Name", "Reflection Quality"))
		.Engine(TEXT("ReflectionQuality"))
		.Options(MakeQualityOptions_LowToEpic())
		.Default(2)
		.DependsOn({TAG_Settings_Video_Discrete_OverallQuality})
		.Getter([]() -> int32 {
			return GEngine->GetGameUserSettings()->GetReflectionQuality();
		})
		.Setter([](int32 V) {
			GEngine->GetGameUserSettings()->SetReflectionQuality(V);
			GEngine->GetGameUserSettings()->ApplyNonResolutionSettings();
		})
		.Done();

	// ---- Post Processing Quality ----
	Model.Discrete(TAG_Settings_Video_Discrete_PostProcessingQuality, TAG_Settings_Video)
		.Name(NSLOCTEXT("Settings", "PostProcessingQuality_Name", "Post Processing Quality"))
		.Engine(TEXT("PostProcessingQuality"))
		.Options(MakeQualityOptions_LowToEpic())
		.Default(2)
		.DependsOn({TAG_Settings_Video_Discrete_OverallQuality})
		.Getter([]() -> int32 {
			return GEngine->GetGameUserSettings()->GetPostProcessingQuality();
		})
		.Setter([](int32 V) {
			GEngine->GetGameUserSettings()->SetPostProcessingQuality(V);
			GEngine->GetGameUserSettings()->ApplyNonResolutionSettings();
		})
		.Done();

	// ---- Register OverallQuality cascade ----
	Model.RegisterOnChanged(TAG_Settings_Video_Discrete_OverallQuality, [](UArcSettingsModel& M, FGameplayTag)
	{
		const int32 Idx = M.GetDiscreteIndex(TAG_Settings_Video_Discrete_OverallQuality);
		const int32 Sub = FMath::Min(Idx, 3);
		M.SetDiscreteIndex(TAG_Settings_Video_Discrete_GlobalIlluminationQuality, Sub);
		M.SetDiscreteIndex(TAG_Settings_Video_Discrete_Shadows,                   Sub);
		M.SetDiscreteIndex(TAG_Settings_Video_Discrete_AntiAliasing,              Sub);
		M.SetDiscreteIndex(TAG_Settings_Video_Discrete_ViewDistance,              Sub);
		M.SetDiscreteIndex(TAG_Settings_Video_Discrete_TextureQuality,            Sub);
		M.SetDiscreteIndex(TAG_Settings_Video_Discrete_VisualEffectQuality,       Sub);
		M.SetDiscreteIndex(TAG_Settings_Video_Discrete_ReflectionQuality,         Sub);
		M.SetDiscreteIndex(TAG_Settings_Video_Discrete_PostProcessingQuality,     Sub);
	});

	// ---- VSync ----
	Model.Discrete(TAG_Settings_Video_Discrete_VSync, TAG_Settings_Video)
		.Name(NSLOCTEXT("Settings", "VSync_Name", "VSync"))
		.Engine(TEXT("bUseVSync"))
		.Options({
			{TEXT("0"), NSLOCTEXT("Settings", "VSync_Off", "Off")},
			{TEXT("1"), NSLOCTEXT("Settings", "VSync_On",  "On")},
		})
		.Default(0)
		.Getter([]() -> int32 {
			return GEngine->GetGameUserSettings()->IsVSyncEnabled() ? 1 : 0;
		})
		.Setter([](int32 V) {
			GEngine->GetGameUserSettings()->SetVSyncEnabled(V != 0);
			GEngine->GetGameUserSettings()->ApplyNonResolutionSettings();
		})
		.Done();

	// ---- Frame Rate Limits ----
	const TArray<FArcSettingOption> FrameRateOptions = MakeFrameRateOptions();

	Model.Discrete(TAG_Settings_Video_Discrete_FrameRateLimit_OnBattery, TAG_Settings_Video)
		.Name(NSLOCTEXT("Settings", "FRLimit_OnBattery_Name", "Frame Rate Limit (On Battery)"))
		.Local(TEXT("FrameRateLimit_OnBattery"))
		.Options(FrameRateOptions)
		.Default(0)
		.Done();

	Model.Discrete(TAG_Settings_Video_Discrete_FrameRateLimit_InMenu, TAG_Settings_Video)
		.Name(NSLOCTEXT("Settings", "FRLimit_InMenu_Name", "Frame Rate Limit (In Menu)"))
		.Local(TEXT("FrameRateLimit_InMenu"))
		.Options(FrameRateOptions)
		.Default(0)
		.Done();

	Model.Discrete(TAG_Settings_Video_Discrete_FrameRateLimit_WhenBackgrounded, TAG_Settings_Video)
		.Name(NSLOCTEXT("Settings", "FRLimit_WhenBackgrounded_Name", "Frame Rate Limit (When Backgrounded)"))
		.Local(TEXT("FrameRateLimit_WhenBackgrounded"))
		.Options(FrameRateOptions)
		.Default(0)
		.Done();

	Model.Discrete(TAG_Settings_Video_Discrete_FrameRateLimit_Always, TAG_Settings_Video)
		.Name(NSLOCTEXT("Settings", "FRLimit_Always_Name", "Frame Rate Limit"))
		.Local(TEXT("FrameRateLimit_Always"))
		.Options(FrameRateOptions)
		.Default(0)
		.Done();
}

} // namespace ArcSettingsRegistration_Video_Impl

// ============================================================
// Public trampoline
// ============================================================

void ArcSettingsRegistration_Video_Register(UArcSettingsModel& Model)
{
	ArcSettingsRegistration_Video_Impl::RegisterVideoSettings(Model);
}
