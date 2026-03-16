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
#include "GameplayTagContainer.h"
#include "ArcAbilityAction_AddLooseTags.generated.h"

/**
 * Latent action that adds loose gameplay tags to the ability system component.
 * Tags are removed when the action is cancelled or the ability ends.
 *
 * LatentTag must be set for this action to function as a latent (cancelable) action.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Add Loose Tags"))
struct ARCCORE_API FArcAbilityAction_AddLooseTags : public FArcAbilityAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tags")
	FGameplayTagContainer Tags;

	virtual void Execute(FArcAbilityActionContext& Context) override;
	virtual void CancelLatent(FArcAbilityActionContext& Context) override;
};
