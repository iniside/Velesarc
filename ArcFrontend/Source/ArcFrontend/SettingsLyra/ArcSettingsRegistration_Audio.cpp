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
#include "CommonInputBaseTypes.h"
#include "CommonUISettings.h"
#include "ICommonUIModule.h"
#include "Descriptors/ArcSettingBuilder.h"

// ---- Category ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Audio, "Settings.Audio");

// ---- Volume ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Audio_Scalar_OverallVolume,   "Settings.Audio.Scalar.OverallVolume");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Audio_Scalar_MusicVolume,     "Settings.Audio.Scalar.MusicVolume");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Audio_Scalar_SFXVolume,       "Settings.Audio.Scalar.SFXVolume");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Audio_Scalar_DialogueVolume,  "Settings.Audio.Scalar.DialogueVolume");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Audio_Scalar_VoiceChatVolume, "Settings.Audio.Scalar.VoiceChatVolume");

// ---- Subtitles ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Audio_Discrete_Subtitles,               "Settings.Audio.Discrete.Subtitles");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Audio_Discrete_SubtitleTextSize,         "Settings.Audio.Discrete.SubtitleTextSize");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Audio_Discrete_SubtitleTextColor,        "Settings.Audio.Discrete.SubtitleTextColor");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Audio_Discrete_SubtitleTextBorder,       "Settings.Audio.Discrete.SubtitleTextBorder");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Audio_Discrete_SubtitleBackgroundOpacity,"Settings.Audio.Discrete.SubtitleBackgroundOpacity");

// ---- Sound ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Audio_Discrete_OutputDevice,    "Settings.Audio.Discrete.OutputDevice");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Audio_Discrete_BackgroundAudio, "Settings.Audio.Discrete.BackgroundAudio");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Audio_Discrete_HeadphoneMode,   "Settings.Audio.Discrete.HeadphoneMode");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Audio_Discrete_HDRAudioMode,    "Settings.Audio.Discrete.HDRAudioMode");

// ---------------------------------------------------------------------------

namespace ArcSettingsRegistration_Audio_Impl
{

static void RegisterAudioSettings_Impl(UArcSettingsModel& Model)
{
    // -------------------------------------------------------------------------
    // Volume — 5 scalars, Local storage, 0-1 range, 0.01 step, default 1.0, Percent
    // -------------------------------------------------------------------------
    Model.Scalar(TAG_Settings_Audio_Scalar_OverallVolume, TAG_Settings_Audio)
        .Name(NSLOCTEXT("Settings", "Audio_OverallVolume_Name", "Overall Volume"))
        .Desc(NSLOCTEXT("Settings", "Audio_OverallVolume_Desc", "Adjusts the volume of everything."))
        .Local(TEXT("OverallVolume"))
        .Range(0.0, 1.0, 0.01)
        .Default(1.0)
        .Format(EArcSettingDisplayFormat::Percent)
        .Done();

    Model.Scalar(TAG_Settings_Audio_Scalar_MusicVolume, TAG_Settings_Audio)
        .Name(NSLOCTEXT("Settings", "Audio_MusicVolume_Name", "Music Volume"))
        .Desc(NSLOCTEXT("Settings", "Audio_MusicVolume_Desc", "Adjusts the volume of music."))
        .Local(TEXT("MusicVolume"))
        .Range(0.0, 1.0, 0.01)
        .Default(1.0)
        .Format(EArcSettingDisplayFormat::Percent)
        .Done();

    Model.Scalar(TAG_Settings_Audio_Scalar_SFXVolume, TAG_Settings_Audio)
        .Name(NSLOCTEXT("Settings", "Audio_SFXVolume_Name", "Sound Effects Volume"))
        .Desc(NSLOCTEXT("Settings", "Audio_SFXVolume_Desc", "Adjusts the volume of sound effects."))
        .Local(TEXT("SFXVolume"))
        .Range(0.0, 1.0, 0.01)
        .Default(1.0)
        .Format(EArcSettingDisplayFormat::Percent)
        .Done();

    Model.Scalar(TAG_Settings_Audio_Scalar_DialogueVolume, TAG_Settings_Audio)
        .Name(NSLOCTEXT("Settings", "Audio_DialogueVolume_Name", "Dialogue Volume"))
        .Desc(NSLOCTEXT("Settings", "Audio_DialogueVolume_Desc", "Adjusts the volume of dialogue and voice overs."))
        .Local(TEXT("DialogueVolume"))
        .Range(0.0, 1.0, 0.01)
        .Default(1.0)
        .Format(EArcSettingDisplayFormat::Percent)
        .Done();

    Model.Scalar(TAG_Settings_Audio_Scalar_VoiceChatVolume, TAG_Settings_Audio)
        .Name(NSLOCTEXT("Settings", "Audio_VoiceChatVolume_Name", "Voice Chat Volume"))
        .Desc(NSLOCTEXT("Settings", "Audio_VoiceChatVolume_Desc", "Adjusts the volume of voice chat."))
        .Local(TEXT("VoiceChatVolume"))
        .Range(0.0, 1.0, 0.01)
        .Default(1.0)
        .Format(EArcSettingDisplayFormat::Percent)
        .Done();

    // -------------------------------------------------------------------------
    // Subtitles — 5 discrete, Shared storage
    // -------------------------------------------------------------------------
    Model.Discrete(TAG_Settings_Audio_Discrete_Subtitles, TAG_Settings_Audio)
        .Name(NSLOCTEXT("Settings", "Audio_Subtitles_Name", "Subtitles"))
        .Desc(NSLOCTEXT("Settings", "Audio_Subtitles_Desc", "Turns subtitles on or off."))
        .Shared(TEXT("Subtitles"))
        .Options({
            { TEXT("0"), NSLOCTEXT("Settings", "Audio_Off", "Off") },
            { TEXT("1"), NSLOCTEXT("Settings", "Audio_On",  "On")  },
        })
        .Default(1)
        .Done();

    Model.Discrete(TAG_Settings_Audio_Discrete_SubtitleTextSize, TAG_Settings_Audio)
        .Name(NSLOCTEXT("Settings", "Audio_SubtitleTextSize_Name", "Subtitle Text Size"))
        .Desc(NSLOCTEXT("Settings", "Audio_SubtitleTextSize_Desc", "Choose the size of subtitle text."))
        .Shared(TEXT("SubtitleTextSize"))
        .Options({
            { TEXT("0"), NSLOCTEXT("Settings", "SubtitleSize_ExtraSmall", "Extra Small") },
            { TEXT("1"), NSLOCTEXT("Settings", "SubtitleSize_Small",      "Small")       },
            { TEXT("2"), NSLOCTEXT("Settings", "SubtitleSize_Medium",     "Medium")      },
            { TEXT("3"), NSLOCTEXT("Settings", "SubtitleSize_Large",      "Large")       },
            { TEXT("4"), NSLOCTEXT("Settings", "SubtitleSize_ExtraLarge", "Extra Large") },
        })
        .Default(2)
        .Done();

    Model.Discrete(TAG_Settings_Audio_Discrete_SubtitleTextColor, TAG_Settings_Audio)
        .Name(NSLOCTEXT("Settings", "Audio_SubtitleTextColor_Name", "Subtitle Text Color"))
        .Desc(NSLOCTEXT("Settings", "Audio_SubtitleTextColor_Desc", "Choose the color of subtitle text."))
        .Shared(TEXT("SubtitleTextColor"))
        .Options({
            { TEXT("0"), NSLOCTEXT("Settings", "SubtitleColor_White",  "White")  },
            { TEXT("1"), NSLOCTEXT("Settings", "SubtitleColor_Yellow", "Yellow") },
        })
        .Default(0)
        .Done();

    Model.Discrete(TAG_Settings_Audio_Discrete_SubtitleTextBorder, TAG_Settings_Audio)
        .Name(NSLOCTEXT("Settings", "Audio_SubtitleTextBorder_Name", "Subtitle Text Border"))
        .Desc(NSLOCTEXT("Settings", "Audio_SubtitleTextBorder_Desc", "Choose the border style for subtitle text."))
        .Shared(TEXT("SubtitleTextBorder"))
        .Options({
            { TEXT("0"), NSLOCTEXT("Settings", "SubtitleBorder_None",       "None")        },
            { TEXT("1"), NSLOCTEXT("Settings", "SubtitleBorder_Outline",    "Outline")     },
            { TEXT("2"), NSLOCTEXT("Settings", "SubtitleBorder_DropShadow", "Drop Shadow") },
        })
        .Default(0)
        .Done();

    Model.Discrete(TAG_Settings_Audio_Discrete_SubtitleBackgroundOpacity, TAG_Settings_Audio)
        .Name(NSLOCTEXT("Settings", "Audio_SubtitleBackgroundOpacity_Name", "Subtitle Background Opacity"))
        .Desc(NSLOCTEXT("Settings", "Audio_SubtitleBackgroundOpacity_Desc", "Choose the background opacity for subtitles."))
        .Shared(TEXT("SubtitleBackgroundOpacity"))
        .Options({
            { TEXT("0"), NSLOCTEXT("Settings", "SubtitleBg_Clear",  "Clear")  },
            { TEXT("1"), NSLOCTEXT("Settings", "SubtitleBg_Low",    "Low")    },
            { TEXT("2"), NSLOCTEXT("Settings", "SubtitleBg_Medium", "Medium") },
            { TEXT("3"), NSLOCTEXT("Settings", "SubtitleBg_High",   "High")   },
            { TEXT("4"), NSLOCTEXT("Settings", "SubtitleBg_Solid",  "Solid")  },
        })
        .Default(2)
        .Done();

    // -------------------------------------------------------------------------
    // Sound — 4 discrete
    // -------------------------------------------------------------------------

    // OutputDevice — Local, with DynamicOptions and EditCondition
    Model.Discrete(TAG_Settings_Audio_Discrete_OutputDevice, TAG_Settings_Audio)
        .Name(NSLOCTEXT("Settings", "Audio_OutputDevice_Name", "Audio Output Device"))
        .Desc(NSLOCTEXT("Settings", "Audio_OutputDevice_Desc", "Changes the audio output device for game audio (not voice chat)."))
        .Local(TEXT("OutputDevice"))
        .Options({
            { TEXT("default"), NSLOCTEXT("Settings", "Audio_OutputDevice_Default", "Default") },
        })
        .Default(0)
        .DynamicOptions([]() -> TArray<FArcSettingOption>
        {
            // Real device enumeration to be implemented later.
            return { { TEXT("default"), NSLOCTEXT("Settings", "Audio_OutputDevice_Default", "Default") } };
        })
        .EditCondition([](const UArcSettingsModel&) -> EArcSettingVisibility
        {
            return ICommonUIModule::GetSettings().GetPlatformTraits()
                .HasTag(FGameplayTag::RequestGameplayTag(TEXT("Platform.Trait.SupportsChangingAudioOutputDevice")))
                ? EArcSettingVisibility::Visible
                : EArcSettingVisibility::Hidden;
        })
        .Done();

    // BackgroundAudio — Shared, with EditCondition
    Model.Discrete(TAG_Settings_Audio_Discrete_BackgroundAudio, TAG_Settings_Audio)
        .Name(NSLOCTEXT("Settings", "Audio_BackgroundAudio_Name", "Background Audio"))
        .Desc(NSLOCTEXT("Settings", "Audio_BackgroundAudio_Desc", "Turns game audio on or off when the game runs in the background."))
        .Shared(TEXT("BackgroundAudio"))
        .Options({
            { TEXT("0"), NSLOCTEXT("Settings", "Audio_BackgroundAudio_Off",       "Off")        },
            { TEXT("1"), NSLOCTEXT("Settings", "Audio_BackgroundAudio_AllSounds", "All Sounds") },
        })
        .Default(0)
        .EditCondition([](const UArcSettingsModel&) -> EArcSettingVisibility
        {
            return ICommonUIModule::GetSettings().GetPlatformTraits()
                .HasTag(FGameplayTag::RequestGameplayTag(TEXT("Platform.Trait.SupportsBackgroundAudio")))
                ? EArcSettingVisibility::Visible
                : EArcSettingVisibility::Hidden;
        })
        .Done();

    // HeadphoneMode — Local
    Model.Discrete(TAG_Settings_Audio_Discrete_HeadphoneMode, TAG_Settings_Audio)
        .Name(NSLOCTEXT("Settings", "Audio_HeadphoneMode_Name", "3D Headphones"))
        .Desc(NSLOCTEXT("Settings", "Audio_HeadphoneMode_Desc", "Enables binaural audio for 3D sound spatialization. Recommended for stereo headphones only."))
        .Local(TEXT("HeadphoneMode"))
        .Options({
            { TEXT("0"), NSLOCTEXT("Settings", "Audio_Off", "Off") },
            { TEXT("1"), NSLOCTEXT("Settings", "Audio_On",  "On")  },
        })
        .Default(0)
        .Done();

    // HDRAudioMode — Local
    Model.Discrete(TAG_Settings_Audio_Discrete_HDRAudioMode, TAG_Settings_Audio)
        .Name(NSLOCTEXT("Settings", "Audio_HDRAudioMode_Name", "High Dynamic Range Audio"))
        .Desc(NSLOCTEXT("Settings", "Audio_HDRAudioMode_Desc", "Enables high dynamic range audio processing for a more cinematic experience."))
        .Local(TEXT("HDRAudioMode"))
        .Options({
            { TEXT("0"), NSLOCTEXT("Settings", "Audio_Off", "Off") },
            { TEXT("1"), NSLOCTEXT("Settings", "Audio_On",  "On")  },
        })
        .Default(0)
        .Done();
}

} // namespace ArcSettingsRegistration_Audio_Impl

void RegisterAudioSettings(UArcSettingsModel& Model)
{
    ArcSettingsRegistration_Audio_Impl::RegisterAudioSettings_Impl(Model);
}
