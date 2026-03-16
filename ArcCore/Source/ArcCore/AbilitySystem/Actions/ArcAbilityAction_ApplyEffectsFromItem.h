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
#include "ArcAbilityAction_ApplyEffectsFromItem.generated.h"

/**
 * Applies gameplay effects defined on the item's FArcItemFragment_GrantedGameplayEffects.
 * If EffectTag is set, only effects matching that tag are applied; if empty, all effects
 * from the fragment are applied. Effects are applied to the owning actor's ASC.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Apply Effects From Item"))
struct ARCCORE_API FArcAbilityAction_ApplyEffectsFromItem : public FArcAbilityAction
{
	GENERATED_BODY()

	/** If empty, apply all effects from the item. */
	UPROPERTY(EditAnywhere)
	FGameplayTag EffectTag;

	virtual void Execute(FArcAbilityActionContext& Context) override;
};
