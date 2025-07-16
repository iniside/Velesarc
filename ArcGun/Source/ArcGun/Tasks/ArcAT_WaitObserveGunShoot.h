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
#include "ArcAT_WaitObserveGunShoot.generated.h"

class UGameplayAbility;
class UArcTargetingObject;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FArcWaitObserveWeaponShootDelegate, class UGameplayAbility*, SourceAbilit, const FArcSelectedGun&, EquippedWeapon,  const FGameplayAbilityTargetDataHandle&, TargetData, class UArcTargetingObject*, TargetingObject);

/**
 * 
 */
UCLASS()
class ARCGUN_API UArcAT_WaitObserveGunShoot : public UAbilityTask
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(BlueprintAssignable)
	FArcWaitObserveWeaponShootDelegate OnShoot;

	FDelegateHandle WeaponShootDelegateHandle;
	
public:
	/**
	 * @brief Wait on gun shoots.
	 * @return
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability|Tasks|Weapon", meta = (
		HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_WaitObserveGunShoot* WaitObserveWeaponShoot(
		UGameplayAbility* OwningAbility
		, FName TaskInstanceName);

	virtual void OnDestroy(bool bInOwnerFinished) override;
	virtual void Activate() override;
	
private:
	void HandleWeaponShoot(UGameplayAbility* SourceAbility
						   , const FArcSelectedGun& EquippedWeapon
						   , const FGameplayAbilityTargetDataHandle& TargetData
						   , class UArcTargetingObject* TargetingObject
						   , const FGameplayAbilitySpecHandle& RequestHandle);
};
