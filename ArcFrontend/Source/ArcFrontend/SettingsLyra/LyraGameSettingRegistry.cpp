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

#include "LyraGameSettingRegistry.h"

#include "GameSettingCollection.h"
#include "LyraSettingsLocal.h"
#include "LyraSettingsShared.h"
#include "Player/ArcLocalPlayerBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameSettingRegistry)

DEFINE_LOG_CATEGORY(LogLyraGameSettingRegistry);

#define LOCTEXT_NAMESPACE "Lyra"

//--------------------------------------
// ULyraGameSettingRegistry
//--------------------------------------

ULyraGameSettingRegistry::ULyraGameSettingRegistry()
{
}

ULyraGameSettingRegistry* ULyraGameSettingRegistry::Get(UArcLocalPlayerBase* InLocalPlayer)
{
	ULyraGameSettingRegistry* Registry = FindObject<ULyraGameSettingRegistry>(InLocalPlayer, TEXT("LyraGameSettingRegistry"), EFindObjectFlags::ExactClass);
	if (Registry == nullptr)
	{
		Registry = NewObject<ULyraGameSettingRegistry>(InLocalPlayer, TEXT("LyraGameSettingRegistry"));
		Registry->Initialize(InLocalPlayer);
	}

	return Registry;
}

bool ULyraGameSettingRegistry::IsFinishedInitializing() const
{
	if (Super::IsFinishedInitializing())
	{
		if (UArcLocalPlayerBase* LocalPlayer = Cast<UArcLocalPlayerBase>(OwningLocalPlayer))
		{
			if (LocalPlayer->GetSharedSettings() == nullptr)
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

void ULyraGameSettingRegistry::OnInitialize(ULocalPlayer* InLocalPlayer)
{
	UArcLocalPlayerBase* LyraLocalPlayer = Cast<UArcLocalPlayerBase>(InLocalPlayer);

	VideoSettings = InitializeVideoSettings(LyraLocalPlayer);
	InitializeVideoSettings_FrameRates(VideoSettings, LyraLocalPlayer);
	RegisterSetting(VideoSettings);

	AudioSettings = InitializeAudioSettings(LyraLocalPlayer);
	RegisterSetting(AudioSettings);

	GameplaySettings = InitializeGameplaySettings(LyraLocalPlayer);
	RegisterSetting(GameplaySettings);

	MouseAndKeyboardSettings = InitializeMouseAndKeyboardSettings(LyraLocalPlayer);
	RegisterSetting(MouseAndKeyboardSettings);

	GamepadSettings = InitializeGamepadSettings(LyraLocalPlayer);
	RegisterSetting(GamepadSettings);
}

void ULyraGameSettingRegistry::SaveChanges()
{
	Super::SaveChanges();
	
	if (UArcLocalPlayerBase* LocalPlayer = Cast<UArcLocalPlayerBase>(OwningLocalPlayer))
	{
		// Game user settings need to be applied to handle things like resolution, this saves indirectly
		LocalPlayer->GetLocalSettings()->ApplySettings(false);
		
		LocalPlayer->GetSharedSettingsTyped<ULyraSettingsLocal>()->ApplySettings(false);
		LocalPlayer->GetSharedSettingsTyped<ULyraSettingsLocal>()->SaveSettings();
	}
}

#undef LOCTEXT_NAMESPACE

