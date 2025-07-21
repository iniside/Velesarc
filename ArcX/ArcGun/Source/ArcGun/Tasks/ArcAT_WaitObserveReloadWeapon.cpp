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

#include "Tasks/ArcAT_WaitObserveReloadWeapon.h"

#include "AbilitySystemComponent.h"

#include "ArcGunStateComponent.h"

UArcAT_WaitObserveReloadWeapon* UArcAT_WaitObserveReloadWeapon::WaitObserveReloadWeapon(
	UGameplayAbility* OwningAbility
	, FName TaskInstanceName
)
{	
	UArcAT_WaitObserveReloadWeapon* Proxy = NewAbilityTask<UArcAT_WaitObserveReloadWeapon>(OwningAbility, TaskInstanceName);
	return Proxy;
}

void UArcAT_WaitObserveReloadWeapon::OnDestroy(bool bInOwnerFinished)
{
	Super::OnDestroy(bInOwnerFinished);
}

void UArcAT_WaitObserveReloadWeapon::Activate()
{
	UArcGunStateComponent* ArcWC = UArcGunStateComponent::FindGunStateComponent(GetOwnerActor());
	ReloadStartDelegateHandle = ArcWC->AddOnReloadStart(FArcGunReloadDelegate::FDelegate::CreateUObject(
		this, &UArcAT_WaitObserveReloadWeapon::HandleReloadStart));

	ReloadEndDelegateHandle = ArcWC->AddOnReloadEnd(FArcGunReloadDelegate::FDelegate::CreateUObject(
		this, &UArcAT_WaitObserveReloadWeapon::HandleReloadFinish));

}

void UArcAT_WaitObserveReloadWeapon::HandleReloadStart(const FArcSelectedGun& InEquipedWeapon, float Reloadtime)
{
	if (AbilitySystemComponent->GetOwner()->HasAuthority())
	{
		ReloadStarted.Broadcast();
	}
}

void UArcAT_WaitObserveReloadWeapon::HandleReloadFinish(const FArcSelectedGun& InEquipedWeapon, float Reloadtime)
{
	if(AbilitySystemComponent->GetOwner()->HasAuthority())
	{
		ReloadFinish.Broadcast();
	}
}