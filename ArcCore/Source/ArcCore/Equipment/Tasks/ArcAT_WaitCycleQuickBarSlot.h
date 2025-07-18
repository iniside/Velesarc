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
#include "ArcAT_WaitCycleQuickBarSlot.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitCycleSlotDelegate);

/**
 *
 */
UCLASS()
class ARCCORE_API UArcAT_WaitCycleQuickBarSlot : public UAbilityTask
{
	GENERATED_BODY()

private:
	float ServerStartTime = 0;
	float TimeToChange = 0;

	FGameplayTag BarId;

	UPROPERTY(Transient)
	TObjectPtr<class UAnimMontage> Montage;

protected:
	UPROPERTY(BlueprintAssignable)
	FWaitCycleSlotDelegate OnSwap;

	UPROPERTY(BlueprintAssignable)
	FWaitCycleSlotDelegate OnFailed;

	UPROPERTY(BlueprintAssignable)
	FWaitCycleSlotDelegate InProgress;

public:
	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|Equipment|Tasks"
		, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_WaitCycleQuickBarSlot* WaitCycleQuickBarSlot(UGameplayAbility* OwningAbility
															   , FName TaskInstanceName
															   , FGameplayTag InBarId
															   , class UAnimMontage* InMontage
															   , float InTimeToChange);

	virtual void Activate() override;

	virtual void OnDestroy(bool bInOwnerFinished) override;

protected:
	UFUNCTION()
	void OnCycleComplete();

	UFUNCTION()
	void OnSignalCallback();
};
