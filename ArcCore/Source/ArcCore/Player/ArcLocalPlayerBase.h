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

#pragma once

#include "AudioMixerBlueprintLibrary.h"
#include "CommonLocalPlayer.h"
#include "CoreMinimal.h"
#include "ArcLocalPlayerBase.generated.h"

class UArcSettingsLocalBase;
class UArcSettingsShared;
class UInputMappingContext;

DECLARE_DELEGATE_RetVal(class UGameUserSettings*
	, FArcGetLocalSettings)
DECLARE_DELEGATE_RetVal_OneParam(class USaveGame*
	, FArcGetSharedSettings
	, const class UArcLocalPlayerBase*)

/**
 * UArcLocalPlayerBase
 */
UCLASS()
class ARCCORE_API UArcLocalPlayerBase : public UCommonLocalPlayer
{
	GENERATED_BODY()

public:
	UArcLocalPlayerBase();

	virtual void PostInitProperties() override;

	virtual FString GetNickname() const override;

public:
	static FArcGetLocalSettings GetLocalSettingsDelegate;
	static FArcGetSharedSettings GetSharedSettingsDelegate;

	UFUNCTION()
	UGameUserSettings* GetLocalSettings() const;

	//UGameUserSettings* GetLocalSettings();
	
	UFUNCTION()
	USaveGame* GetSharedSettings() const;

	template <typename T>
	T* TGetSharedSettings() const
	{
		return Cast<T>(GetSharedSettings());
	}

	template <typename T>
	T* GetSharedSettingsTyped() const
	{
		return Cast<T>(GetSharedSettings());
	}

	template <typename T>
	T* GetLocalSettingsTyped() const
	{
		return Cast<T>(GetLocalSettings());
	}
	
protected:
	void OnAudioOutputDeviceChanged(const FString& InAudioOutputDeviceId);

	UFUNCTION()
	void OnCompletedAudioDeviceSwap(const FSwapAudioOutputResult& SwapResult);

private:
	UPROPERTY(Transient)
	mutable TObjectPtr<USaveGame> SharedSettings = nullptr;
};
