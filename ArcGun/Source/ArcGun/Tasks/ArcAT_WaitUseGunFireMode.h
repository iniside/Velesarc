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
#include "ArcGunTypes.h"
#include "AbilitySystem/Targeting/ArcTargetingObject.h"
#include "ArcAT_WaitUseGunFireMode.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogWaitWeaponAutoFire, Log, Log);

UCLASS()
class ARCGUN_API UArcAT_WaitUseGunFireMode : public UAbilityTask
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UArcTargetingObject> TargetingObject;
	
	float TimeHeld = 0;
	FDelegateHandle InputReleaseHandle;

	FDelegateHandle PreShootdHandle;

	FDelegateHandle FireStopHandle;
	
	UPROPERTY(BlueprintAssignable)
	FArcOnShootFiered OnRelease;

	UPROPERTY(BlueprintAssignable)
	FArcOnShootFiered OnPreShot;

	UPROPERTY(BlueprintAssignable)
	FArcOnShootFiered OnEnded;
	
	UFUNCTION()
	void OnReleaseCallbackSpec(FGameplayAbilitySpec& AbilitySpec);

	void HandleOnShootFired(const FArcGunShotInfo& ShotInfo);
	void HandleOnStopFire(const FArcSelectedGun& InEquippedGun);
	
	virtual void Activate() override;

	/** Repeats action untill user releases input. Either from input binding or calling release event directly. */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability|Tasks|Weapon", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_WaitUseGunFireMode* WaitUseGunFireMode(UGameplayAbility* OwningAbility
		, UArcTargetingObject* InTargetingObject);
};