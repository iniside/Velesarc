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


#include "ArcGunTypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "UObject/Object.h"
#include "ArcAT_WaitObserverGunPreShot.generated.h"

/**************************************************************************************
 * Class Name: UArcAT_WaitObserverGunPreShot
 * File Name: ArcAT_WaitObserverGunPreShot.h
 *
 * Description:
 *   This class is a custom ability task that waits for the gun to shoot before
 *   continuing execution.
 *
 **************************************************************************************/
UCLASS()
class ARCGUN_API UArcAT_WaitObserverGunPreShot : public UAbilityTask
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(BlueprintAssignable)
	FArcOnShootFiered OnPreShot;

	FDelegateHandle GunPreShotDelegateHandle;
	
public:
	/**
	 * @brief Wait on gun shoots.
	 * @return
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability|Tasks|Weapon", meta = (
		HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_WaitObserverGunPreShot* WaitObserverGunPreShot(
		UGameplayAbility* OwningAbility
		, FName TaskInstanceName);

	virtual void OnDestroy(bool bInOwnerFinished) override;
	virtual void Activate() override;
	
private:
	void HandleOnShootFired(const FArcGunShotInfo& ShotInfo);
};
