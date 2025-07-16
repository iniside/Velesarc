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



#include "ArcAT_WaitActivationTimeWithAnimation.h"

#include "TimerManager.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "Items/ArcItemDefinition.h"

UArcAT_WaitActivationTimeWithAnimation* UArcAT_WaitActivationTimeWithAnimation::WaitActivationTimeWithAnimation(UGameplayAbility* OwningAbility
																												, FName TaskInstanceName
																												, bool bInWaitForInputConfirm
																												, bool bInBroadcastEventTagsOnInputConfirm
																												, FGameplayTagContainer InEventTags
																												, FGameplayTag InActivationTag
																												, FGameplayTagContainer InputConfirmedRequiredTags
																												, FGameplayTagContainer InputDenyRequiredTags)
{
	UArcAT_WaitActivationTimeWithAnimation* MyObj = NewAbilityTask<UArcAT_WaitActivationTimeWithAnimation>(OwningAbility, TaskInstanceName);

	MyObj->bWaitForInputConfirm = bInWaitForInputConfirm;
	MyObj->bBroadcastEventTagsOnInputConfirm = bInBroadcastEventTagsOnInputConfirm;
	MyObj->EventTags = InEventTags;
	MyObj->ActivationTag = InActivationTag;
	MyObj->InputConfirmedRequiredTags = InputConfirmedRequiredTags;
	MyObj->InputDenyRequiredTags = InputDenyRequiredTags;
	
	return MyObj;
}

void UArcAT_WaitActivationTimeWithAnimation::Activate()
{
	UArcCoreGameplayAbility* ArcCoreAbility = Cast<UArcCoreGameplayAbility>(Ability);
	const FArcItemFragment_ActivationTimeMontages* MontagesFragment = ArcCoreAbility->NativeGetSourceItemData()->FindFragment<FArcItemFragment_ActivationTimeMontages>();

	if (!MontagesFragment)
	{
		return;
	}
	
	ArcCoreAbility->OnActivationTimeChanged.AddUObject(this, &UArcAT_WaitActivationTimeWithAnimation::HandleActivationTimeChanged);
	
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);

	SetWaitingOnRemotePlayerData();
	SetWaitingOnAvatar();

	InputDelegateHandle = ArcASC->AddOnInputPressedMap(GetAbilitySpecHandle(), FArcAbilityInputDelegate::FDelegate::CreateUObject(this
			, &UArcAT_WaitActivationTimeWithAnimation::HandleOnInputPressed));
	
	FTimerManager& TimerManager = ArcASC->GetWorld()->GetTimerManager();

	EventHandle = ArcASC->AddGameplayEventTagContainerDelegate(EventTags
	, FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this
		, &UArcAT_WaitActivationTimeWithAnimation::OnGameplayEvent));

	bReachedActivationTime = false;
	float DesiredPlayTime = ArcCoreAbility->GetActivationTime();

	TimerManager.SetTimer(ActivationTimerHandle,
		FTimerDelegate::CreateUObject(this, &UArcAT_WaitActivationTimeWithAnimation::HandleOnActivationFinished), DesiredPlayTime, false);

	const float MontageLenght = MontagesFragment->StartMontage.Get()->GetSectionLength(0);

	float PlayRate = MontageLenght / DesiredPlayTime;
	if (MontagesFragment->LoopMontage.IsValid())
	{
		PlayRate = 1.f;
	}
	const UArcItemDefinition* ItemDef = ArcCoreAbility->NativeGetOwnerItemData();
	ArcASC->PlayAnimMontage(Ability, MontagesFragment->StartMontage.Get(), PlayRate, NAME_None, 0, false, ItemDef);

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	
	BlendingOutDelegate.BindUObject(this, &UArcAT_WaitActivationTimeWithAnimation::OnStartMontageEnded);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontagesFragment->StartMontage.Get());
}

void UArcAT_WaitActivationTimeWithAnimation::HandleOnActivationFinished()
{
	bReachedActivationTime = true;
	FGameplayEventData TempData;
	if (!bWaitForInputConfirm)
	{
		HandleFinished();
		TempData.EventTag = ActivationTag;
	
		OnEventReceived.Broadcast(ActivationTag, TempData);
		return;
	}

	OnActivationFinished.Broadcast(FGameplayTag(), TempData);
}

void UArcAT_WaitActivationTimeWithAnimation::HandleFinished()
{
	UArcCoreGameplayAbility* ArcCoreAbility = Cast<UArcCoreGameplayAbility>(Ability);
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);

	const FArcItemFragment_ActivationTimeMontages* MontagesFragment = ArcCoreAbility->NativeGetSourceItemData()->FindFragment<FArcItemFragment_ActivationTimeMontages>();

	const UArcItemDefinition* ItemDef = ArcCoreAbility->NativeGetOwnerItemData();
	ArcASC->PlayAnimMontage(Ability, MontagesFragment->EndMontage.Get(), 1.f, NAME_None, 0, false, ItemDef);
}

void UArcAT_WaitActivationTimeWithAnimation::OnStartMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	BlendingOutDelegate.Unbind();

	UArcCoreGameplayAbility* ArcCoreAbility = Cast<UArcCoreGameplayAbility>(Ability);
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);

	const FArcItemFragment_ActivationTimeMontages* MontagesFragment = ArcCoreAbility->NativeGetSourceItemData()->FindFragment<FArcItemFragment_ActivationTimeMontages>();

	if (!MontagesFragment->LoopMontage.IsNull())
	{
		const UArcItemDefinition* ItemDef = ArcCoreAbility->NativeGetOwnerItemData();
		ArcASC->PlayAnimMontage(Ability, MontagesFragment->LoopMontage.Get(), 1.f, NAME_None, 0, false, ItemDef);	
	}
}

void UArcAT_WaitActivationTimeWithAnimation::HandleActivationTimeChanged(UArcCoreGameplayAbility* InAbility, float NewTime)
{
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);

	FTimerManager& TimerManager = ArcASC->GetWorld()->GetTimerManager();

	const float ElapsedTime = TimerManager.GetTimerElapsed(ActivationTimerHandle);
	const float DesiredPlayTime = NewTime - ElapsedTime;

	if (FMath::IsNearlyEqual(DesiredPlayTime, 0, 0.001f))
	{
		TimerManager.ClearTimer(ActivationTimerHandle);
		HandleOnActivationFinished();
		return;
	}

	UArcCoreGameplayAbility* ArcCoreAbility = Cast<UArcCoreGameplayAbility>(Ability);

	const FArcItemFragment_ActivationTimeMontages* MontagesFragment = ArcCoreAbility->NativeGetSourceItemData()->FindFragment<FArcItemFragment_ActivationTimeMontages>();
	const float MontageLenght = MontagesFragment->StartMontage.Get()->GetSectionLength(0);

	float PlayRate = MontageLenght / DesiredPlayTime;
	if (MontagesFragment->LoopMontage.IsValid())
	{
		PlayRate = 1.f;
	}
	
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();

	if (MontagesFragment->LoopMontage.IsNull())
	{
		AnimInstance->Montage_SetPlayRate(MontagesFragment->StartMontage.Get(), PlayRate);	
	}
	
	TimerManager.ClearTimer(ActivationTimerHandle);
	TimerManager.SetTimer(ActivationTimerHandle,
	FTimerDelegate::CreateUObject(this, &UArcAT_WaitActivationTimeWithAnimation::HandleOnActivationFinished), DesiredPlayTime, false);
}

void UArcAT_WaitActivationTimeWithAnimation::OnGameplayEvent(FGameplayTag EventTag
														, const FGameplayEventData* Payload)
{
	FGameplayEventData TempData = *Payload;
	TempData.EventTag = EventTag;
	OnEventReceived.Broadcast(EventTag, TempData);
}

void UArcAT_WaitActivationTimeWithAnimation::HandleOnInputPressed(FGameplayAbilitySpec& InSpec)
{
	
	if (InputConfirmedRequiredTags.Num() > 0 && !AbilitySystemComponent->HasAllMatchingGameplayTags(InputConfirmedRequiredTags))
	{
		return;
	}

	if (InputDenyRequiredTags.Num() > 0 && AbilitySystemComponent->HasAnyMatchingGameplayTags(InputDenyRequiredTags))
	{
		return;
	}
	
	if (bReachedActivationTime)
	{
		FGameplayEventData TempData;
		TempData.EventTag = ActivationTag;
		OnInputConfirmed.Broadcast(ActivationTag, TempData);
		HandleFinished();
		if (bBroadcastEventTagsOnInputConfirm)
		{
			OnEventReceived.Broadcast(ActivationTag, TempData);
		}
	}
}
