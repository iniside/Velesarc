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

#include "Abilities/Tasks/AbilityTask.h"
#include "ArcDynamicDelegates.h"
#include "ArcAT_WaitDirectInputPressed.generated.h"

/**
 *
 */
UCLASS()
class ARCCORE_API UArcAT_WaitDirectInputPressed : public UAbilityTask
{
	GENERATED_BODY()

private:
	FDelegateHandle InputDelegateHandle;

	UPROPERTY(BlueprintAssignable)
	FArcFloatMulticastDynamic OnInputPressed;

public:
	/**
	 * @brief Waits for direct Input from ability, either enable globally or per ability
	 * using Replicate Input Directly.
	 * @param TaskInstanceName Instance name, sometimes used to recycle tasks
	 * @return Task Object
	 */
	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|Ability|Tasks"
		, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_WaitDirectInputPressed* WaitDirectInputPressed(UGameplayAbility* OwningAbility
																 , FName TaskInstanceName);

	virtual void Activate() override;

private:
	UFUNCTION()
	void HandleOnInputPressed(FGameplayAbilitySpec& InSpec);
};
