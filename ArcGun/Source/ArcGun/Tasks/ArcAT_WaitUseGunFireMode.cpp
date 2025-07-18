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

#include "ArcAT_WaitUseGunFireMode.h"

#include "Abilities/GameplayAbility.h"
#include "TimerManager.h"

#include "ArcCore/AbilitySystem/ArcCoreGameplayAbility.h"
#include "ArcGunStateComponent.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"

DEFINE_LOG_CATEGORY(LogWaitWeaponAutoFire);

void UArcAT_WaitUseGunFireMode::OnReleaseCallbackSpec(FGameplayAbilitySpec& AbilitySpec)
{
	UArcGunStateComponent* GunComponent = UArcGunStateComponent::FindGunStateComponent(GetOwnerActor());
	GunComponent->StopFire();
	OnRelease.Broadcast(TimeHeld, true, TArray<FHitResult>());

	GunComponent->RemoveOnPreShoot(PreShootdHandle);
	GunComponent->RemoveOnStopFire(FireStopHandle);
	
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
	ArcASC->RemoveOnInputReleasedMap(GetAbilitySpecHandle(), InputReleaseHandle);
}

void UArcAT_WaitUseGunFireMode::HandleOnShootFired(const FArcGunShotInfo& ShotInfo)
{
	TimeHeld = ShotInfo.TimeHeld;
	OnPreShot.Broadcast(TimeHeld, ShotInfo.bEnded, ShotInfo.Targets);
}

void UArcAT_WaitUseGunFireMode::HandleOnStopFire(const FArcSelectedGun& InEquippedGun)
{
	OnRelease.Broadcast(TimeHeld, true, TArray<FHitResult>());
}

void UArcAT_WaitUseGunFireMode::Activate()
{
	Super::Activate();
	
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
	bool bIsServer = ArcASC->AbilityActorInfo->IsLocallyControlledPlayer() == false;

	if (bIsServer)
	{
		return;
	}
	
	UArcGunStateComponent* GunComponent = UArcGunStateComponent::FindGunStateComponent(GetOwnerActor());
	FGameplayAbilitySpecHandle SpecHandle = GetAbilitySpecHandle();
	
	if (bIsServer == false || ArcASC->GetNetMode() == NM_Standalone)
	{
		InputReleaseHandle = ArcASC->AddOnInputReleasedMap(
				GetAbilitySpecHandle()
				, FArcAbilityInputDelegate::FDelegate::CreateUObject(this, &UArcAT_WaitUseGunFireMode::OnReleaseCallbackSpec)
			);	
	}
	
	PreShootdHandle = GunComponent->AddOnPreShoot(FArcOnShootFired::FDelegate::CreateUObject(
		this, &UArcAT_WaitUseGunFireMode::HandleOnShootFired));

	FireStopHandle = GunComponent->AddOnStopFire(FArcGunDelegate::FDelegate::CreateUObject(
			this, &UArcAT_WaitUseGunFireMode::HandleOnStopFire));

	// TODO:: Targeting Handle. It safe to create from SpecHandle, but maybe we should use different way.
	GunComponent->StartFire(Ability, TargetingObject, SpecHandle);
}

UArcAT_WaitUseGunFireMode* UArcAT_WaitUseGunFireMode::WaitUseGunFireMode(UGameplayAbility* OwningAbility
	, UArcTargetingObject* InTargetingObject)
{
	UArcAT_WaitUseGunFireMode* Task = NewAbilityTask<UArcAT_WaitUseGunFireMode>(OwningAbility);
	Task->TargetingObject = InTargetingObject;
	return Task;
}
