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

#include "AbilitySystem/ArcAbilityAction.h"
#include "ArcAbilityAction_ApplyEffectToSelf.generated.h"

class UGameplayEffect;

/**
 * Applies a gameplay effect to the owning actor's ability system component.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Apply Effect To Self"))
struct ARCCORE_API FArcAbilityAction_ApplyEffectToSelf : public FArcAbilityAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TSubclassOf<UGameplayEffect> GameplayEffectClass;

	UPROPERTY(EditAnywhere)
	float Level = 1.f;

	virtual void Execute(FArcAbilityActionContext& Context) override;
};
