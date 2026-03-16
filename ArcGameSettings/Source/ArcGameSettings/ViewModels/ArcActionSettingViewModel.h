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

#pragma once

#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "Model/ArcSettingsTypes.h"

#include "ArcActionSettingViewModel.generated.h"

class UArcSettingsModel;

UCLASS()
class ARCGAMESETTINGS_API UArcActionSettingViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void InitializeWithTag(FGameplayTag InTag) { SettingTag = InTag; }
	void Refresh(const UArcSettingsModel& InModel);

	UFUNCTION(BlueprintCallable)
	void ExecuteAction(UArcSettingsModel* InModel);

	FGameplayTag GetSettingTag() const { return SettingTag; }

private:
	FGameplayTag SettingTag;

	UPROPERTY(FieldNotify, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	UPROPERTY(FieldNotify, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FText Description;

	UPROPERTY(FieldNotify, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FText ActionText;

	UPROPERTY(FieldNotify, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	EArcSettingVisibility Visibility = EArcSettingVisibility::Visible;
};
