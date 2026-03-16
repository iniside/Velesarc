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
#include "Items/ArcItemScalableFloat.h"
#include "ArcAbilityAction_ExecuteTargeting.generated.h"

class UArcTargetingObject;

/**
 * Runs a targeting preset synchronously and feeds the results into the ability context's
 * TargetData and HitResults. The targeting object can be set directly or falls back to
 * the item's FArcItemFragment_TargetingObjectPreset. Results are then available for
 * subsequent actions like ApplyEffectToTarget or SendTargetingResult.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Execute Targeting"))
struct ARCCORE_API FArcAbilityAction_ExecuteTargeting : public FArcAbilityAction
{
	GENERATED_BODY()

	/** Optional override; if null the targeting object is read from the item fragment. */
	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcTargetingObject> TargetingObjectOverride;

	UPROPERTY(EditAnywhere)
	FArcScalableCurveFloat TestScalableFloat;
	
	virtual void Execute(FArcAbilityActionContext& Context) override;
};
