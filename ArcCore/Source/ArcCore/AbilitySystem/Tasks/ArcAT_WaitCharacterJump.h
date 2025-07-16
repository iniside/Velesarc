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

#include "Abilities/Tasks/AbilityTask.h"
#include "ArcCore/AbilitySystem/ArcAbilityTargetingComponent.h"
#include "ArcCore/AbilitySystem/Tasks/ArcAbilityTask.h"
#include "CoreMinimal.h"
#include "ArcDynamicDelegates.h"

#include "ArcAT_WaitCharacterJump.generated.h"

/**
 *
 */
UCLASS()
class ARCCORE_API UArcAT_WaitCharacterJump : public UArcAbilityTask
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintAssignable)
	FArcGenericMulticastDynamic OnJumpApex;

	UPROPERTY(BlueprintAssignable)
	FArcGenericMulticastDynamic OnLanded;

public:
	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|Ability|Tasks"
		, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_WaitCharacterJump* WaitCharacterJump(UGameplayAbility* OwningAbility
													   , FName TaskInstanceName);

	virtual void Activate() override;

	virtual void OnDestroy(bool bInOwnerFinished) override;

protected:
	UFUNCTION()
	void OnApex();

	UFUNCTION()
	void Landed(const FHitResult& HitResult);
};
