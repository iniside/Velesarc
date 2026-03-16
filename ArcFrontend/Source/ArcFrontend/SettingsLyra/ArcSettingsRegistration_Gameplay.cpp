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
#include "Descriptors/ArcSettingBuilder.h"

// ---- Category ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Gameplay, "Settings.Gameplay");

// ---- Settings ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Gameplay_Discrete_Language,          "Settings.Gameplay.Discrete.Language");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Gameplay_Discrete_ColorBlindMode,    "Settings.Gameplay.Discrete.ColorBlindMode");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Gameplay_Discrete_ColorBlindStrength,"Settings.Gameplay.Discrete.ColorBlindStrength");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Gameplay_Discrete_RecordReplay,      "Settings.Gameplay.Discrete.RecordReplay");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Settings_Gameplay_Discrete_ReplayKeepLimit,   "Settings.Gameplay.Discrete.ReplayKeepLimit");

// ---------------------------------------------------------------------------

namespace ArcSettingsRegistration_Gameplay_Impl
{

static TArray<FArcSettingOption> MakeColorBlindStrengthOptions()
{
    TArray<FArcSettingOption> Options;
    Options.Reserve(11);
    for (int32 i = 0; i <= 10; ++i)
    {
        Options.Add({ FString::FromInt(i), FText::AsNumber(i) });
    }
    return Options;
}

static TArray<FArcSettingOption> MakeReplayKeepLimitOptions()
{
    TArray<FArcSettingOption> Options;
    Options.Reserve(21);
    for (int32 i = 0; i <= 20; ++i)
    {
        Options.Add({ FString::FromInt(i), FText::AsNumber(i) });
    }
    return Options;
}

static void RegisterGameplaySettings_Impl(UArcSettingsModel& Model)
{
    // -------------------------------------------------------------------------
    // Language — discrete, Shared, dynamic options (placeholder)
    // -------------------------------------------------------------------------
    Model.Discrete(TAG_Settings_Gameplay_Discrete_Language, TAG_Settings_Gameplay)
        .Name(NSLOCTEXT("Settings", "Gameplay_Language_Name", "Language"))
        .Desc(NSLOCTEXT("Settings", "Gameplay_Language_Desc", "The language used for the game."))
        .Shared(TEXT("Language"))
        .Options({
            { TEXT("en"), NSLOCTEXT("Settings", "Gameplay_Language_English", "English") },
        })
        .Default(0)
        .DynamicOptions([]() -> TArray<FArcSettingOption>
        {
            // Real culture enumeration to be implemented later.
            return { { TEXT("en"), NSLOCTEXT("Settings", "Gameplay_Language_English", "English") } };
        })
        .Done();

    // -------------------------------------------------------------------------
    // ColorBlindMode — discrete, Shared
    // -------------------------------------------------------------------------
    Model.Discrete(TAG_Settings_Gameplay_Discrete_ColorBlindMode, TAG_Settings_Gameplay)
        .Name(NSLOCTEXT("Settings", "Gameplay_ColorBlindMode_Name", "Color Blind Mode"))
        .Desc(NSLOCTEXT("Settings", "Gameplay_ColorBlindMode_Desc", "Applies a color blind correction filter to the game."))
        .Shared(TEXT("ColorBlindMode"))
        .Options({
            { TEXT("0"), NSLOCTEXT("Settings", "ColorBlind_Off",         "Off")         },
            { TEXT("1"), NSLOCTEXT("Settings", "ColorBlind_Deuteranope", "Deuteranope") },
            { TEXT("2"), NSLOCTEXT("Settings", "ColorBlind_Protanope",   "Protanope")   },
            { TEXT("3"), NSLOCTEXT("Settings", "ColorBlind_Tritanope",   "Tritanope")   },
        })
        .Default(0)
        .Done();

    // -------------------------------------------------------------------------
    // ColorBlindStrength — discrete, Shared, options 0-10, default 10
    // -------------------------------------------------------------------------
    Model.Discrete(TAG_Settings_Gameplay_Discrete_ColorBlindStrength, TAG_Settings_Gameplay)
        .Name(NSLOCTEXT("Settings", "Gameplay_ColorBlindStrength_Name", "Color Blind Strength"))
        .Desc(NSLOCTEXT("Settings", "Gameplay_ColorBlindStrength_Desc", "Sets the strength of the color blind correction filter."))
        .Shared(TEXT("ColorBlindStrength"))
        .Options(MakeColorBlindStrengthOptions())
        .Default(10)
        .Done();

    // -------------------------------------------------------------------------
    // RecordReplay — discrete, Local, Off/On, default 0
    // -------------------------------------------------------------------------
    Model.Discrete(TAG_Settings_Gameplay_Discrete_RecordReplay, TAG_Settings_Gameplay)
        .Name(NSLOCTEXT("Settings", "Gameplay_RecordReplay_Name", "Record Replays"))
        .Desc(NSLOCTEXT("Settings", "Gameplay_RecordReplay_Desc", "Automatically records game replays. This is an experimental feature."))
        .Local(TEXT("RecordReplay"))
        .Options({
            { TEXT("0"), NSLOCTEXT("Settings", "Gameplay_Off", "Off") },
            { TEXT("1"), NSLOCTEXT("Settings", "Gameplay_On",  "On")  },
        })
        .Default(0)
        .Done();

    // -------------------------------------------------------------------------
    // ReplayKeepLimit — discrete, Local, options 0-20, default 5
    // -------------------------------------------------------------------------
    Model.Discrete(TAG_Settings_Gameplay_Discrete_ReplayKeepLimit, TAG_Settings_Gameplay)
        .Name(NSLOCTEXT("Settings", "Gameplay_ReplayKeepLimit_Name", "Keep Replay Limit"))
        .Desc(NSLOCTEXT("Settings", "Gameplay_ReplayKeepLimit_Desc", "Number of saved replays to keep. Set to 0 for unlimited."))
        .Local(TEXT("ReplayKeepLimit"))
        .Options(MakeReplayKeepLimitOptions())
        .Default(5)
        .Done();
}

} // namespace ArcSettingsRegistration_Gameplay_Impl

void RegisterGameplaySettings(UArcSettingsModel& Model)
{
    ArcSettingsRegistration_Gameplay_Impl::RegisterGameplaySettings_Impl(Model);
}
