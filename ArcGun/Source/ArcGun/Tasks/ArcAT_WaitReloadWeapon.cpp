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

#include "Tasks/ArcAT_WaitReloadWeapon.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffectExecutionCalculation.h"
#include "ArcCore/AbilitySystem/ArcCoreGameplayAbility.h"
#include "TimerManager.h"

#include "ArcGunStateComponent.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"

UArcAT_WaitReloadWeapon* UArcAT_WaitReloadWeapon::WaitReloadWeapon(
	UGameplayAbility* OwningAbility
	, FName TaskInstanceName
	, class UAnimMontage* InMontage
)
{	
	UArcAT_WaitReloadWeapon* Proxy = NewAbilityTask<UArcAT_WaitReloadWeapon>(OwningAbility, TaskInstanceName);
	Proxy->Montage = InMontage;
	return Proxy;
}

void UArcAT_WaitReloadWeapon::OnDestroy(bool bInOwnerFinished)
{
	double CurrentReloadTime = GetWorld()->GetTimeSeconds() - ReloadStartTime;
	UArcGunStateComponent* GunComponent = UArcGunStateComponent::FindGunStateComponent(GetOwnerActor());
	if(bSuccess == false
		&& bActivationFailed == true
		&& CurrentReloadTime >= ReloadTime)
	{
		GunComponent->BroadcastOnReloadAborted(GunComponent->GetSelectedGun(), ReloadTime);
		ReloadFailed.Broadcast();
	}
	if (GunComponent->GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
		UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
		if (AnimInstance != nullptr)
		{
			if(AnimInstance->Montage_IsPlaying(Montage))
			{
				ArcASC->StopAnimMontage(Montage);
			}
		}
	}
	Super::OnDestroy(bInOwnerFinished);
}

void UArcAT_WaitReloadWeapon::Activate()
{
	Super::Activate();

	UArcGunStateComponent* GunComponent = UArcGunStateComponent::FindGunStateComponent(GetOwnerActor());

	FArcGunInstanceBase* GunInstance = GunComponent->GetGunInstance<FArcGunInstanceBase>();
	ReloadTime = GunInstance->GetReloadTime(GunComponent->GetSelectedGun().GunItemPtr, GunComponent);
	
	ReloadStartTime = GetWorld()->GetTimeSeconds();
	bSuccess = false;
	bActivationFailed = false;
	const FArcItemData* E = GunComponent->GetSelectedGun().GunItemPtr;
	if(E == nullptr)
	{
		bActivationFailed = true;
		ReloadFailed.Broadcast();
		return;
	}
	GunComponent->BroadcastOnReloadStart(GunComponent->GetSelectedGun(), ReloadTime);
	FTimerHandle Unused;
	GetWorld()->GetTimerManager().SetTimer(Unused
		, FTimerDelegate::CreateUObject(
			this, &UArcAT_WaitReloadWeapon::OnReloadFinish
		)
		, ReloadTime
		, false);
	
	if(GunComponent->GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
		UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
		if (AnimInstance != nullptr)
		{
			const float MontageLenght = Montage->GetPlayLength();
			const float PlayRate = MontageLenght / ReloadTime;
			ArcASC->PlayAnimMontage(Ability, Montage, PlayRate, NAME_None, 0, true);
		}
	}
}

void UArcAT_WaitReloadWeapon::OnReloadFinish()
{
	bSuccess = true;
	UArcGunStateComponent* GunComponent = UArcGunStateComponent::FindGunStateComponent(GetOwnerActor());
	GunComponent->BroadcastOnReloadEnd(GunComponent->GetSelectedGun(), ReloadTime);
	if (GunComponent->GetOwner()->HasAuthority())
	{
		const FArcItemData* E = GunComponent->GetSelectedGun().GunItemPtr;
		FArcGunInstanceBase* GunInstance = GunComponent->GetGunInstance<FArcGunInstanceBase>();
		
		GunInstance->Reload(E, GunComponent);
		ReloadSuccess.Broadcast();
	}
}