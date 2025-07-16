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

#include "Equipment/Tasks/ArcAT_WaitCycleQuickBarSlot.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "ArcCore/Equipment/ArcItemAttachmentComponent.h"

#include "QuickBar/ArcQuickBarComponent.h"

UArcAT_WaitCycleQuickBarSlot* UArcAT_WaitCycleQuickBarSlot::WaitCycleQuickBarSlot(UGameplayAbility* OwningAbility
																				  , FName TaskInstanceName
																				  , FGameplayTag InBarId
																				  , class UAnimMontage* InMontage
																				  , float InTimeToChange)
{
	UArcAT_WaitCycleQuickBarSlot* Proxy = NewAbilityTask<UArcAT_WaitCycleQuickBarSlot>(OwningAbility
		, TaskInstanceName);
	Proxy->Montage = InMontage;
	Proxy->BarId = InBarId;
	Proxy->TimeToChange = InTimeToChange;
	return Proxy;
}

void UArcAT_WaitCycleQuickBarSlot::Activate()
{
	Super::Activate();

	bool bSuccess = false;
	if (AbilitySystemComponent->AbilityActorInfo->IsLocallyControlledPlayer() == true)
	{
		FTimerHandle Unused;
		UArcQuickBarComponent* QuickBar = UArcQuickBarComponent::GetQuickBar(GetOwnerActor());
		UArcItemAttachmentComponent* ItemAttachmentComp = UArcItemAttachmentComponent::FindItemAttachmentComponent(
			GetOwnerActor());

		auto SlotCheck = [ItemAttachmentComp] (const FArcItemData* InSlotData) -> bool
		{
			return true;
		};

		TArray<FGameplayTag> ActiveSlots = QuickBar->GetActiveSlots(BarId);

		// TODO: In Slot Cycling case there should only one active slot at the time.
		FGameplayTag CurrentSlot = ActiveSlots.Num() > 0 ? ActiveSlots[0] : FGameplayTag::EmptyTag;

		FGameplayTag NextSlot = QuickBar->CycleSlotForward(BarId
			, CurrentSlot
			, SlotCheck);
	
		if (NextSlot.IsValid() == false || NextSlot == CurrentSlot)
		{
			OnFailed.Broadcast();
		}
		else
		{
			// TODO:: Need to add more customization here
			// Right new montage play it's own way, and timer goes it's own way.
		

			if (TimeToChange > 0)
			{
				GetWorld()->GetTimerManager().SetTimer(Unused
					, FTimerDelegate::CreateUObject(this
						, &UArcAT_WaitCycleQuickBarSlot::OnCycleComplete)
					, TimeToChange
					, false);
			}
			else
			{
				OnSwap.Broadcast();
			}
		}
		bSuccess = true;
	}
	else
	{
		bSuccess = true;
	}
	
	//if (Montage && bSuccess)
	//{
	//	AbilitySystemComponent->PlayMontage(Ability
	//		, Ability->GetCurrentActivationInfo()
	//		, Montage
	//		, 1.0f);
	//}
}

void UArcAT_WaitCycleQuickBarSlot::OnDestroy(bool bInOwnerFinished)
{
	Super::OnDestroy(bInOwnerFinished);
}

void UArcAT_WaitCycleQuickBarSlot::OnCycleComplete()
{
	OnSwap.Broadcast();
}

void UArcAT_WaitCycleQuickBarSlot::OnSignalCallback()
{
	EndTask();
}
