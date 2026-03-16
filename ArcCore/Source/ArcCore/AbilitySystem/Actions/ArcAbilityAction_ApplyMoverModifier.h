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
#include "Mover/ArcMoverTypes.h"
#include "ArcAbilityAction_ApplyMoverModifier.generated.h"

/**
 * Latent action that applies a movement modifier to limit max velocity and
 * force a specific gait. Uses FArcMovementModifier_MaxVelocity under the hood.
 * On cancel, the modifier is removed and original speed/gait are restored instantly.
 *
 * LatentTag must be set for this action to function as a latent (cancelable) action.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Apply Mover Modifier"))
struct ARCCORE_API FArcAbilityAction_ApplyMoverModifier : public FArcAbilityAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Mover")
	float MaxVelocity = 100.f;

	UPROPERTY(EditAnywhere, Category = "Mover")
	EArcMoverGaitType Gait = EArcMoverGaitType::Walk;

	// Runtime — populated during Execute
	FMovementModifierHandle ModifierHandle;

	virtual void Execute(FArcAbilityActionContext& Context) override;
	virtual void CancelLatent(FArcAbilityActionContext& Context) override;
};
