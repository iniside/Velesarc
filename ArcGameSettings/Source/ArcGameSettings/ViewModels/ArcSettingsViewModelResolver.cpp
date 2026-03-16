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

#include "ViewModels/ArcSettingsViewModelResolver.h"

#include "MVVMViewModelBase.h"
#include "Model/ArcSettingsModel.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSettingsViewModelResolver)

UObject* UArcSettingsViewModelResolver::CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget, const UMVVMView* View) const
{
	if (!UserWidget)
	{
		return nullptr;
	}

	UWorld* World = UserWidget->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	UArcSettingsModel* Model = GameInstance->GetSubsystem<UArcSettingsModel>();
	if (!Model || !SettingTag.IsValid())
	{
		return nullptr;
	}

	return Model->GetViewModel(SettingTag);
}

void UArcSettingsViewModelResolver::DestroyInstance(UObject* ViewModel, const UMVVMView* View) const
{
	// No-op — ViewModels are owned by the Model subsystem.
}
