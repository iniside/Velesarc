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

#include "ArcDynamicDelegates.h"
#include "Abilities/Tasks/AbilityTask.h"

#include "ArcAT_WaitEnhancedInputAction.generated.h"

class UInputMappingContext;
class UInputAction;

/**
 *
 */
UCLASS()
class ARCCORE_API UArcAT_WaitEnhancedInputAction : public UAbilityTask
{
	GENERATED_BODY()

private:
	FDelegateHandle InputDelegateHandle;

	UPROPERTY(BlueprintAssignable)
	FArcFloatMulticastDynamic OnInputPressed;

	/*
	 * Triggered is separate from Pressed (although depending on Modifier they may occur
	 * at the same time).
	 */
	UPROPERTY(BlueprintAssignable)
	FArcFloatMulticastDynamic OnInputTriggered;

	UPROPERTY(BlueprintAssignable)
	FArcFloatMulticastDynamic OnInputReleased;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> InputAction;

	int32 PressedHandle;
	int32 TriggeredHandle;
	int32 ReleasedHandle;

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
	static UArcAT_WaitEnhancedInputAction* WaitEnhancedInputAction(UGameplayAbility* OwningAbility
																   , class UInputMappingContext* InMappingContext
																   , class UInputAction* InInputAction
																   , FName TaskInstanceName);

	virtual void Activate() override;

	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	UFUNCTION()
	void HandleOnInputPressed(UInputAction* InInputAction);

	UFUNCTION()
	void HandleOnInputTriggered(UInputAction* InInputAction);

	UFUNCTION()
	void HandleOnInputReleased(UInputAction* InInputAction);
};
