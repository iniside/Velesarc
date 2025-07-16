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

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayTagContainer.h"
#include "ArcGunTypes.h"
#include "ArcAT_WaitObserveReloadWeapon.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAuWaitObserveReloadWeaponDelegate);

/**
 * 
 */
UCLASS()
class ARCGUN_API UArcAT_WaitObserveReloadWeapon : public UAbilityTask
{
	GENERATED_BODY()
protected:

	UPROPERTY(BlueprintAssignable)
	FAuWaitObserveReloadWeaponDelegate ReloadStarted;

	UPROPERTY(BlueprintAssignable)
	FAuWaitObserveReloadWeaponDelegate ReloadFailed;

	UPROPERTY(BlueprintAssignable)
	FAuWaitObserveReloadWeaponDelegate ReloadFinish;

	FDelegateHandle ReloadStartDelegateHandle;
	FDelegateHandle ReloadEndDelegateHandle;
public:
	/**
	 * @brief Observe reload of current weapon.
	 * @return
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability|Tasks|Weapon", meta = (
		HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_WaitObserveReloadWeapon* WaitObserveReloadWeapon(
		UGameplayAbility* OwningAbility
		, FName TaskInstanceName);

	virtual void OnDestroy(bool bInOwnerFinished) override;
	virtual void Activate() override;
private:
	void HandleReloadStart(const FArcSelectedGun& InEquipedWeapon, float Reloadtime);
	void HandleReloadFinish(const FArcSelectedGun& InEquipedWeapon, float Reloadtime);
};
