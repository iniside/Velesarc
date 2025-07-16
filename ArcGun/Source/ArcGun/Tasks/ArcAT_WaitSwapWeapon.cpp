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

#include "Tasks/ArcAT_WaitSwapWeapon.h"
#include "TimerManager.h"
#include "AbilitySystemComponent.h"

#include "ArcCore/Equipment/ArcItemAttachmentComponent.h"

#include "ArcGunStateComponent.h"
#include "Items/ArcItemData.h"

#include "QuickBar/ArcQuickBarComponent.h"


UArcAT_WaitSwapWeapon* UArcAT_WaitSwapWeapon::WaitSwapWeapon(
	UGameplayAbility* OwningAbility
	, FName TaskInstanceName
	, FGameplayTag InBarId
	, class UAnimMontage* InMontage
	, bool bInHolsterWeapon
)
{
	UArcAT_WaitSwapWeapon* Proxy = NewAbilityTask<UArcAT_WaitSwapWeapon>(OwningAbility, TaskInstanceName);
	Proxy->bHolsterWeapon = bInHolsterWeapon;
	Proxy->Montage = InMontage;
	Proxy->BarId = InBarId;
	return Proxy;
}

void UArcAT_WaitSwapWeapon::Activate()
{
	Super::Activate();
	FTimerHandle Unused;

	UArcGunStateComponent* ArcWC = UArcGunStateComponent::FindGunStateComponent(GetOwnerActor());
	if(ArcWC == nullptr)
	{
		EndTask();
	}
	UArcQuickBarComponent* QuickBar = UArcQuickBarComponent::GetQuickBar(GetOwnerActor());
	UArcItemAttachmentComponent* ItemAttachmentComp = UArcItemAttachmentComponent::FindItemAttachmentComponent(GetOwnerActor());
	const FArcSelectedGun& EquippedWeapon = ArcWC->GetSelectedGun();

	auto SlotCheck = [ItemAttachmentComp](const FArcItemData* InSlotData)-> bool
	{
		if (ItemAttachmentComp)
		{
			if (AActor* WeaponActor = ItemAttachmentComp->GetAttachedActor(InSlotData->GetItemId()))
			{
				return true;
			}
		}
		return false;
	};

	FGameplayTag NextSlot = QuickBar->CycleSlotForward(BarId, EquippedWeapon.Slot, SlotCheck);
	//bool bFoundNext = ArcWC->NextWeapon();
	if(NextSlot.IsValid() == false || NextSlot == EquippedWeapon.Slot)
	{
		OnFailed.Broadcast();
	}
	else
	{
		//ArcWC->SetActiveSlot(CycleSlotForward);
		if (Montage)
		{
			AbilitySystemComponent->PlayMontage(Ability, Ability->GetCurrentActivationInfo(), Montage, 1.0f);
		}
		GetWorld()->GetTimerManager().SetTimer(Unused, FTimerDelegate::CreateUObject(
			this, &UArcAT_WaitSwapWeapon::OnUnholsterComplete
		), 1.5, false);
	}
}

void UArcAT_WaitSwapWeapon::OnDestroy(bool bInOwnerFinished)
{
	Super::OnDestroy(bInOwnerFinished);
}

void UArcAT_WaitSwapWeapon::OnUnholsterComplete()
{
	UArcGunStateComponent* ArcWC = UArcGunStateComponent::FindGunStateComponent(GetOwnerActor());
	if(Montage == nullptr)
	{
		//ArcWC->AttachHolster();
		//ArcWC->AttachUnholster();
	}
	
	OnSwap.Broadcast();
}

void UArcAT_WaitSwapWeapon::OnHolsterComplete()
{
}

void UArcAT_WaitSwapWeapon::OnSignalCallback()
{
	EndTask();
}

