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
#include "GameplayTagContainer.h"
#include "ArcAT_WaitReloadWeapon.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNxWaitReloadWeaponDelegate);

/**
 * 
 */
UCLASS()
class ARCGUN_API UArcAT_WaitReloadWeapon : public UAbilityTask
{
	GENERATED_BODY()
protected:
	UPROPERTY(Transient)
	TObjectPtr<class UAnimMontage> Montage;

	UPROPERTY(BlueprintAssignable)
	FNxWaitReloadWeaponDelegate ReloadStarted;

	UPROPERTY(BlueprintAssignable)
	FNxWaitReloadWeaponDelegate ReloadFailed;

	UPROPERTY(BlueprintAssignable)
	FNxWaitReloadWeaponDelegate ReloadSuccess;

	double ReloadStartTime = 0;
	float ReloadTime = 0;
	bool bSuccess = false;
	bool bActivationFailed = false;
public:

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability|Tasks|Weapon", meta = (
		HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_WaitReloadWeapon* WaitReloadWeapon(
		UGameplayAbility* OwningAbility
		, FName TaskInstanceName
		, class UAnimMontage* InMontage);

	virtual void OnDestroy(bool bInOwnerFinished) override;
	virtual void Activate() override;
private:
	void OnReloadFinish();
};
