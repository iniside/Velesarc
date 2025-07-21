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

#include "Tasks/ArcAT_WaitObserveGunShoot.h"

#include "AbilitySystemComponent.h"

#include "ArcGunStateComponent.h"

UArcAT_WaitObserveGunShoot* UArcAT_WaitObserveGunShoot::WaitObserveWeaponShoot(
	UGameplayAbility* OwningAbility
	, FName TaskInstanceName
)
{	
	UArcAT_WaitObserveGunShoot* Proxy = NewAbilityTask<UArcAT_WaitObserveGunShoot>(OwningAbility, TaskInstanceName);
	return Proxy;
}

void UArcAT_WaitObserveGunShoot::OnDestroy(bool bInOwnerFinished)
{
	UArcGunStateComponent* GunComponent = UArcGunStateComponent::FindGunStateComponent(GetOwnerActor());
	if (GunComponent != nullptr)
	{
		GunComponent->RemoveOnGunShoot(WeaponShootDelegateHandle);
	}
	
	Super::OnDestroy(bInOwnerFinished);
}

void UArcAT_WaitObserveGunShoot::Activate()
{
	UArcGunStateComponent* GunComponent = UArcGunStateComponent::FindGunStateComponent(GetOwnerActor());
	if (GunComponent == nullptr)
	{
		EndTask();
	}

	WeaponShootDelegateHandle = GunComponent->AddOnGunShoot(FArcGunFireHitDelegate::FDelegate::CreateUObject(
		this, &UArcAT_WaitObserveGunShoot::HandleWeaponShoot));
}

void UArcAT_WaitObserveGunShoot::HandleWeaponShoot(UGameplayAbility* SourceAbility
	, const FArcSelectedGun& EquippedWeapon
	, const FGameplayAbilityTargetDataHandle& TargetData
	, UArcTargetingObject* TargetingObject
	, const FGameplayAbilitySpecHandle& RequestHandle)
{
	OnShoot.Broadcast(SourceAbility, EquippedWeapon, TargetData, TargetingObject);
}