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
#include "ArcAbilityAction_ApplyCooldown.generated.h"

/**
 * Applies the ability's cooldown gameplay effect, putting the ability on cooldown.
 * The cooldown effect class and duration are defined on the ability itself.
 * Use standalone when cost and cooldown need to be applied at different points
 * in the ability flow; otherwise prefer CommitAbility which applies both atomically.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Apply Cooldown"))
struct ARCCORE_API FArcAbilityAction_ApplyCooldown : public FArcAbilityAction
{
	GENERATED_BODY()

	virtual void Execute(FArcAbilityActionContext& Context) override;
};
