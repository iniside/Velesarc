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
#include "View/MVVMViewModelContextResolver.h"

#include "ArcSettingsViewModelResolver.generated.h"

/**
 * Resolves a ViewModel for a specific game setting by tag.
 * Set SettingTag on the resolver instance (via MVVM view configuration)
 * to specify which setting's ViewModel to return.
 */
UCLASS()
class ARCGAMESETTINGS_API UArcSettingsViewModelResolver : public UMVVMViewModelContextResolver
{
	GENERATED_BODY()

public:
	/** The setting tag this resolver provides a ViewModel for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Settings"))
	FGameplayTag SettingTag;

	virtual UObject* CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget, const UMVVMView* View) const override;
	virtual void DestroyInstance(UObject* ViewModel, const UMVVMView* View) const override;
};
