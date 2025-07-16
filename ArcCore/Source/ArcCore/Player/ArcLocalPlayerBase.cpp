/**
 * This file is part of ArcX.
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

#include "ArcLocalPlayerBase.h"
#include "Input/ArcInputActionConfig.h"
#include "AudioMixerBlueprintLibrary.h"
#include "InputMappingContext.h"
#include "Net/OnlineEngineInterface.h"
#include "Engine/World.h"

FArcGetLocalSettings UArcLocalPlayerBase::GetLocalSettingsDelegate = FArcGetLocalSettings();
FArcGetSharedSettings UArcLocalPlayerBase::GetSharedSettingsDelegate = FArcGetSharedSettings();

UArcLocalPlayerBase::UArcLocalPlayerBase()
{
}

void UArcLocalPlayerBase::PostInitProperties()
{
	Super::PostInitProperties();

	// if (UArcSettingsLocalBase* LocalSettings = GetLocalSettings())
	//{
	//	LocalSettings->OnAudioOutputDeviceChanged.AddUObject(this,
	//&UArcLocalPlayerBase::OnAudioOutputDeviceChanged);
	// }
}

FString UArcLocalPlayerBase::GetNickname() const
{
	//FUniqueNetIdRepl UniqueId = GetPreferredUniqueNetId();
	//FString PlayerNetId = UniqueId.ToString();
	//FName T = UniqueId.GetType();
	//if (T.IsNone() == false && T != TEXT("NULL"))
	//{
	//	return Super::GetNickname();
	//}

	return TEXT("");
}

UGameUserSettings* UArcLocalPlayerBase::GetLocalSettings() const
{
	if (GetLocalSettingsDelegate.IsBound())
	{
		return GetLocalSettingsDelegate.Execute();
	}
	return nullptr;
}

USaveGame* UArcLocalPlayerBase::GetSharedSettings() const
{
	if (GetSharedSettingsDelegate.IsBound() && SharedSettings == nullptr)
	{
		SharedSettings = GetSharedSettingsDelegate.Execute(this);
	}

	return SharedSettings;
}

void UArcLocalPlayerBase::OnAudioOutputDeviceChanged(const FString& InAudioOutputDeviceId)
{
	FOnCompletedDeviceSwap DevicesSwappedCallback;
	DevicesSwappedCallback.BindUFunction(this
		, FName("OnCompletedAudioDeviceSwap"));
	UAudioMixerBlueprintLibrary::SwapAudioOutputDevice(GetWorld()
		, InAudioOutputDeviceId
		, DevicesSwappedCallback);
}

void UArcLocalPlayerBase::OnCompletedAudioDeviceSwap(const FSwapAudioOutputResult& SwapResult)
{
	if (SwapResult.Result == ESwapAudioOutputDeviceResultState::Failure)
	{
	}
}
