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



#include "ArcAT_WaitActivationTimeWithAnimation.h"

#include "ArcAN_SendGameplayEvent.h"
#include "ArcLogs.h"
#include "EnhancedInputSubsystems.h"
#include "TimerManager.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Input/ArcCoreInputComponent.h"
#include "Items/ArcItemDefinition.h"

UArcAT_WaitActivationTimeWithAnimation* UArcAT_WaitActivationTimeWithAnimation::WaitActivationTimeWithAnimation(UGameplayAbility* OwningAbility
																												, FName TaskInstanceName
																												, bool bInBroadcastEventTagsOnInputConfirm
																												, float EndTime
																												, FGameplayTag NotifyEndTimeTag
																												, FGameplayTagContainer InEventTags
																												, FGameplayTag InActivationTag
																												, FGameplayTagContainer InputConfirmedRequiredTags
																												, FGameplayTagContainer InputDenyRequiredTags)
{
	UArcAT_WaitActivationTimeWithAnimation* MyObj = NewAbilityTask<UArcAT_WaitActivationTimeWithAnimation>(OwningAbility, TaskInstanceName);

	MyObj->bBroadcastEventTagsOnInputConfirm = bInBroadcastEventTagsOnInputConfirm;
	MyObj->EndTime = EndTime;
	MyObj->NotifyEndTimeTag = NotifyEndTimeTag;
	
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
	
	Mode = MontagesFragment->Mode;
	
	ArcCoreAbility->OnActivationTimeChanged.AddUObject(this, &UArcAT_WaitActivationTimeWithAnimation::HandleActivationTimeChanged);
	
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);

	SetWaitingOnRemotePlayerData();
	SetWaitingOnAvatar();

	if (Mode == EArcActivationMontageMode::StartEndAndLoopMontage)
	{
		if (MontagesFragment->InputMappingContext && MontagesFragment->InputAction)
		{
			const APawn* Pawn = Cast<APawn>(GetAvatarActor());

			const APlayerController* PC = Pawn->GetController<APlayerController>();
			check(PC);

			const ULocalPlayer* LP = PC->GetLocalPlayer();
			check(LP);
	
			UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
			check(Subsystem);
		
			FModifyContextOptions Context;
			Context.bForceImmediately = true;
			Context.bIgnoreAllPressedKeysUntilRelease = false;
			Subsystem->AddMappingContext(MontagesFragment->InputMappingContext
				, MontagesFragment->InputPriority
				, Context);
		
			UArcCoreInputComponent* ArcIC = Pawn->FindComponentByClass<UArcCoreInputComponent>();
		
			UInputAction* InputAction = MontagesFragment->InputAction;
			TriggeredHandle = ArcIC->BindAction(MontagesFragment->InputAction
				, ETriggerEvent::Triggered
				, this
				, &UArcAT_WaitActivationTimeWithAnimation::HandleOnInputTriggered
				, InputAction).GetHandle();
		}
		else
		{
			InputDelegateHandle = ArcASC->AddOnInputPressedMap(GetAbilitySpecHandle(), FArcAbilityInputDelegate::FDelegate::CreateUObject(this
					, &UArcAT_WaitActivationTimeWithAnimation::HandleOnInputPressed));	
		}
	}
	
	FTimerManager& TimerManager = ArcASC->GetWorld()->GetTimerManager();

	EventHandle = ArcASC->AddGameplayEventTagContainerDelegate(EventTags
	, FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this
		, &UArcAT_WaitActivationTimeWithAnimation::OnGameplayEvent));

	bReachedActivationTime = false;
	float DesiredPlayTime = ArcCoreAbility->GetActivationTime();

	TimerManager.SetTimer(ActivationTimerHandle,
		FTimerDelegate::CreateUObject(this, &UArcAT_WaitActivationTimeWithAnimation::HandleOnActivationFinished), DesiredPlayTime, false);

	const float MontageLenght = MontagesFragment->StartMontage.Get()->GetSectionLength(0);

	const float StartLength = MontagesFragment->StartMontage.IsNull() ? 0.f : MontagesFragment->StartMontage->GetSectionLength(0);
	const float LoopLength = MontagesFragment->LoopMontage.IsNull() ? 0.f : MontagesFragment->LoopMontage->GetSectionLength(0);
	const float EndLength = MontagesFragment->EndMontage.IsNull() ? 0.f : MontagesFragment->EndMontage->GetSectionLength(0);
	const float PostEndLength = MontagesFragment->PostEndMontage.IsNull() ? 0.f : MontagesFragment->PostEndMontage->GetSectionLength(0);
	
	
	StartEndPlayRate = (StartLength + EndLength) / DesiredPlayTime;
	
	if (Mode == EArcActivationMontageMode::StartEndAndLoopMontage)
	{
		StartEndPlayRate = 1.f;
	}
	
	{
		FAnimNotifyContext Context;
		MontagesFragment->StartMontage->GetAnimNotifies(0, 30, Context);
		
		Notifies.Empty();
		
		for (const FAnimNotifyEventReference& AN : Context.ActiveNotifies)
		{
			const FAnimNotifyEvent* Event = AN.GetNotify();
			if (!Event)
			{
				continue;
			}
			
			if (UArcAnimNotify_SetPlayRate* SPR = Cast<UArcAnimNotify_SetPlayRate>(Event->Notify))
			{
				Notify NewNotify;
				float Time = Event->GetTime();
				NewNotify.OriginalTime = Event->GetTriggerTime();
				NewNotify.Time = Event->GetTriggerTime() / StartEndPlayRate;
				NewNotify.bChangePlayRate = true;
				NewNotify.PlayRate = SPR->GetNewPlayRate();
				Notifies.Add(NewNotify);
			}
			
			UArcAnimNotify_MarkGameplayEvent* MGE = Cast<UArcAnimNotify_MarkGameplayEvent>(Event->Notify);
			if (MGE)
			{
				Notify NewNotify;
				NewNotify.Tag = MGE->GetEventTag();
				float Time = Event->GetTime();
				NewNotify.OriginalTime = Event->GetTriggerTime();
				NewNotify.Time = Event->GetTriggerTime() / StartEndPlayRate;
				Notifies.Add(NewNotify);	
			}
		}
		
		bPlayNotifiesInLoop = false;
		
		CurrentNotifyTime = 0.f;
		if (Notifies.Num() > 0)
		{
			TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::HandleNotifiesTick));
		}
	}	
	
	CurrentMontage = MontagesFragment->StartMontage.Get();
	
	const UArcItemDefinition* ItemDef = ArcCoreAbility->NativeGetOwnerItemData();
	ArcASC->PlayAnimMontage(Ability, MontagesFragment->StartMontage.Get(), StartEndPlayRate, NAME_None, 0, false, ItemDef);

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	
	StartMontageEndedDelegate.BindUObject(this, &UArcAT_WaitActivationTimeWithAnimation::HandleOnStartMontageEnded);
	AnimInstance->Montage_SetEndDelegate(StartMontageEndedDelegate, MontagesFragment->StartMontage.Get());
	
	OnStartMontageStarted.Broadcast(FGameplayTag(), FGameplayEventData());
}

void UArcAT_WaitActivationTimeWithAnimation::HandleOnStartMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	StartMontageEndedDelegate.Unbind();
	
	OnStartMontageEnded.Broadcast(FGameplayTag(), FGameplayEventData());
	
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	UArcCoreGameplayAbility* ArcCoreAbility = Cast<UArcCoreGameplayAbility>(Ability);
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);

	const FArcItemFragment_ActivationTimeMontages* MontagesFragment = ArcCoreAbility->NativeGetSourceItemData()->FindFragment<FArcItemFragment_ActivationTimeMontages>();
	if (Mode == EArcActivationMontageMode::StartEndAndLoopMontage)
	{
		FAnimNotifyContext Context;
		MontagesFragment->LoopMontage->GetAnimNotifies(0, 30, Context);
		
		Notifies.Empty();
		
		for (const FAnimNotifyEventReference& AN : Context.ActiveNotifies)
		{
			const FAnimNotifyEvent* Event = AN.GetNotify();
			if (!Event)
			{
				continue;
			}
			
			UArcAnimNotify_MarkGameplayEvent* MGE = Cast<UArcAnimNotify_MarkGameplayEvent>(Event->Notify);
			if (!MGE)
			{
				continue;
			}
			
			Notify NewNotify;
			NewNotify.Tag = MGE->GetEventTag();
			float Time = Event->GetTime();
			NewNotify.Time = Event->GetTriggerTime();
			Notifies.Add(NewNotify);
		}
		
		bPlayNotifiesInLoop = true;
		CurrentNotifyTime = 0.f;
		if (Notifies.Num() > 0 && !TickerHandle.IsValid())
		{
			TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::HandleNotifiesTick));	
		}
		
		CurrentMontage = MontagesFragment->LoopMontage.Get();
		
		const UArcItemDefinition* ItemDef = ArcCoreAbility->NativeGetOwnerItemData();
		ArcASC->PlayAnimMontage(Ability, MontagesFragment->LoopMontage.Get(), 1.f, NAME_None, 0, false, ItemDef);
	}
	else if (Mode == EArcActivationMontageMode::StartAndEndMontage)
	{
		FAnimNotifyContext Context;
		MontagesFragment->EndMontage->GetAnimNotifies(0, 30, Context);
		
		Notifies.Empty();
		
		for (const FAnimNotifyEventReference& AN : Context.ActiveNotifies)
		{
			const FAnimNotifyEvent* Event = AN.GetNotify();
			if (!Event)
			{
				continue;
			}
			
			UArcAnimNotify_MarkGameplayEvent* MGE = Cast<UArcAnimNotify_MarkGameplayEvent>(Event->Notify);
			if (!MGE)
			{
				continue;
			}
			
			Notify NewNotify;
			NewNotify.Tag = MGE->GetEventTag();
			float Time = Event->GetTime();
			NewNotify.Time = Event->GetTriggerTime();
			Notifies.Add(NewNotify);
		}
		
		bPlayNotifiesInLoop = true;
		CurrentNotifyTime = 0.f;
		if (Notifies.Num() > 0 && !TickerHandle.IsValid())
		{
			TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::HandleNotifiesTick));	
		}
		
		CurrentMontage = MontagesFragment->EndMontage.Get();
		
		const UArcItemDefinition* ItemDef = ArcCoreAbility->NativeGetOwnerItemData();
		
		ArcASC->PlayAnimMontage(Ability, MontagesFragment->EndMontage.Get(), StartEndPlayRate, NAME_None, 0, false, ItemDef);
		
		EndMontageEndedDelegate.BindUObject(this, &UArcAT_WaitActivationTimeWithAnimation::HandleOnEndMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndMontageEndedDelegate, MontagesFragment->EndMontage.Get());
		
		OnEndMontageStarted.Broadcast(FGameplayTag(), FGameplayEventData());
	}
}

void UArcAT_WaitActivationTimeWithAnimation::HandleOnEndMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{	
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	EndMontageEndedDelegate.Unbind();

	UArcCoreGameplayAbility* ArcCoreAbility = Cast<UArcCoreGameplayAbility>(Ability);
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
	const FArcItemFragment_ActivationTimeMontages* MontagesFragment = ArcCoreAbility->NativeGetSourceItemData()->FindFragment<FArcItemFragment_ActivationTimeMontages>();
	const UArcItemDefinition* ItemDef = ArcCoreAbility->NativeGetOwnerItemData();
	
	if (!MontagesFragment->PostEndMontage.IsNull())
	{	
		ArcASC->PlayAnimMontage(Ability, MontagesFragment->PostEndMontage.Get(), 1.f, NAME_None, 0, false, ItemDef);
	
		PostEndMontageEndedDelegate.BindUObject(this, &UArcAT_WaitActivationTimeWithAnimation::HandleOnPostEndMontageEnded);
		AnimInstance->Montage_SetEndDelegate(PostEndMontageEndedDelegate, MontagesFragment->PostEndMontage.Get());	
	}
	
	OnEndMontageEnded.Broadcast(FGameplayTag(), FGameplayEventData());
}

void UArcAT_WaitActivationTimeWithAnimation::HandleOnPostEndMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	OnPostEndMontageEnded.Broadcast(FGameplayTag(), FGameplayEventData());
}

void UArcAT_WaitActivationTimeWithAnimation::OnDestroy(bool bInOwnerFinished)
{
	UArcCoreGameplayAbility* ArcCoreAbility = Cast<UArcCoreGameplayAbility>(Ability);
	if (!ArcCoreAbility)
	{
		Super::OnDestroy(bInOwnerFinished);
		return;
	}
	
	const FArcItemFragment_ActivationTimeMontages* MontagesFragment = ArcCoreAbility->NativeGetSourceItemData()->FindFragment<FArcItemFragment_ActivationTimeMontages>();
	
	if (!MontagesFragment)
	{
		Super::OnDestroy(bInOwnerFinished);
		return;
	}
	
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
	
	ArcASC->RemoveGameplayEventTagContainerDelegate(EventTags, EventHandle);
	if (Mode == EArcActivationMontageMode::StartEndAndLoopMontage)
	{
		if (MontagesFragment->InputMappingContext && MontagesFragment->InputAction)
		{
			const APawn* Pawn = Cast<APawn>(GetAvatarActor());

			const APlayerController* PC = Pawn->GetController<APlayerController>();
			if (PC == nullptr)
			{
				return;
			}
			check(PC);

			const ULocalPlayer* LP = PC->GetLocalPlayer();
			check(LP);
			UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
			check(Subsystem);

			if (MontagesFragment->InputAction)
			{
				UArcCoreInputComponent* ArcIC = Pawn->FindComponentByClass<UArcCoreInputComponent>();
				ArcIC->RemoveBindingByHandle(TriggeredHandle);
			}
	
			FModifyContextOptions Context;
			Context.bForceImmediately = true;
			Context.bIgnoreAllPressedKeysUntilRelease = false;
			Subsystem->RemoveMappingContext(MontagesFragment->InputMappingContext, Context);
		}
		else
		{
			ArcASC->RemoveOnInputPressedMap(GetAbilitySpecHandle(), InputDelegateHandle);
		}
	}

	Super::OnDestroy(bInOwnerFinished);
}

bool UArcAT_WaitActivationTimeWithAnimation::HandleNotifiesTick(float DeltaTime)
{
	if (!AbilitySystemComponent.IsValid())
	{
		CurrentNotifyTime = 0.f;
		return false;
	}
	for (int32 Idx = Notifies.Num() - 1; Idx >= 0; --Idx)
	{
		if (CurrentNotifyTime >= Notifies[Idx].Time)
		{
			FGameplayEventData Payload;
			UE_LOG(LogArcCore, Log, TEXT("Notify %s Time %.2f, CurrentTime %.2f"), *Notifies[Idx].Tag.ToString(), Notifies[Idx].Time, CurrentNotifyTime);
			AbilitySystemComponent->HandleGameplayEvent(Notifies[Idx].Tag, &Payload);
			
			if (Notifies[Idx].bResetPlayRate)
			{
				const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
				UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
				for (int32 NIdx = 0; NIdx < Notifies.Num(); ++NIdx)
				{
					Notifies[NIdx].Time = Notifies[NIdx].OriginalTime;
				}
				
				AnimInstance->Montage_SetPlayRate(CurrentMontage.Get(), 1.f);
			}
			
			if (Notifies[Idx].bChangePlayRate)
			{
				const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
				UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
				for (int32 NIdx = 0; NIdx < Notifies.Num(); ++NIdx)
				{
					Notifies[NIdx].Time = Notifies[NIdx].OriginalTime;
				}
				AnimInstance->Montage_SetPlayRate(CurrentMontage.Get(), Notifies[Idx].PlayRate);
			}
			
			if (!bPlayNotifiesInLoop)
			{
				Notifies.RemoveAt(Idx);
			}
		}
	}
	
	if (Notifies.Num() == 0)
	{
		CurrentNotifyTime = 0.f;
		return false;
	}
	
	CurrentNotifyTime += DeltaTime;
	return true;
}

void UArcAT_WaitActivationTimeWithAnimation::HandleOnActivationFinished()
{
	if (UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent))
	{
		if (UArcCoreGameplayAbility* ArcCoreAbility = Cast<UArcCoreGameplayAbility>(Ability))
		{
			ArcCoreAbility->CallOnActionTimeFinished();
		}
	}
	
	bReachedActivationTime = true;
	FGameplayEventData TempData;
	TempData.EventTag = ActivationTag;
	
	OnEventReceived.Broadcast(ActivationTag, TempData);

	OnActivationFinished.Broadcast(FGameplayTag(), TempData);
}

void UArcAT_WaitActivationTimeWithAnimation::HandleFinished()
{
	UArcCoreGameplayAbility* ArcCoreAbility = Cast<UArcCoreGameplayAbility>(Ability);
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);

	const FArcItemFragment_ActivationTimeMontages* MontagesFragment = ArcCoreAbility->NativeGetSourceItemData()->FindFragment<FArcItemFragment_ActivationTimeMontages>();

	if (Mode == EArcActivationMontageMode::StartEndAndLoopMontage)
	{
		FAnimNotifyContext Context;
		MontagesFragment->EndMontage->GetAnimNotifies(0, 30, Context);
		
		Notifies.Empty();
		float LastNotifyTime = 0.f;
		for (const FAnimNotifyEventReference& AN : Context.ActiveNotifies)
		{
			const FAnimNotifyEvent* Event = AN.GetNotify();
			if (!Event)
			{
				continue;
			}
			
			UArcAnimNotify_MarkGameplayEvent* MGE = Cast<UArcAnimNotify_MarkGameplayEvent>(Event->Notify);
			if (!MGE)
			{
				continue;
			}
			
			Notify NewNotify;
			NewNotify.Tag = MGE->GetEventTag();
			NewNotify.OriginalTime = Event->GetTriggerTime();
			NewNotify.bResetPlayRate = MGE->GetResetPlayeRate();
			
			if (NotifyEndTimeTag.IsValid())
			{
				if (NotifyEndTimeTag == MGE->GetEventTag())
				{
					LastNotifyTime = Event->GetTriggerTime();
				}
			}
			else
			{
				if (Event->GetTriggerTime() > LastNotifyTime)
				{
					LastNotifyTime = Event->GetTriggerTime();
				}	
			}
			
			Notifies.Add(NewNotify);
		}
	
		float PlayRate = 1.f;
		if (EndTime > 0)
		{
			PlayRate = LastNotifyTime / EndTime;
		}
		
		for (Notify& Notify : Notifies)
		{
			Notify.Time = Notify.OriginalTime / PlayRate;
		}
		
		bPlayNotifiesInLoop = false;
		CurrentNotifyTime = 0.f;
		if (Notifies.Num() > 0 && !TickerHandle.IsValid())
		{
			TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::HandleNotifiesTick));	
		}
		
		CurrentMontage = MontagesFragment->EndMontage.Get();
		
		const UArcItemDefinition* ItemDef = ArcCoreAbility->NativeGetOwnerItemData();
		ArcASC->PlayAnimMontage(Ability, MontagesFragment->EndMontage.Get(), PlayRate, NAME_None, 0, false, ItemDef);
		
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		
		UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
		
		FGameplayEventData TempData;
		TempData.EventTag = ActivationTag;
		
		EndMontageEndedDelegate.BindUObject(this, &UArcAT_WaitActivationTimeWithAnimation::HandleOnEndMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndMontageEndedDelegate, MontagesFragment->EndMontage.Get());
		
		OnEndMontageStarted.Broadcast(FGameplayTag(), FGameplayEventData());
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
	
	FGameplayEventData TempData;
	TempData.EventMagnitude = NewTime;
	OnActivationTimeChanged.Broadcast(FGameplayTag(), TempData);
	
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
	if (bFinished)
	{
		return;
	}
	
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
		bFinished = true;
		if (bBroadcastEventTagsOnInputConfirm)
		{
			OnEventReceived.Broadcast(ActivationTag, TempData);
		}
	}
}

void UArcAT_WaitActivationTimeWithAnimation::HandleOnInputTriggered(UInputAction* InInputAction)
{
	if (bFinished)
	{
		return;
	}
	
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
		bFinished = true;
		
		if (bBroadcastEventTagsOnInputConfirm)
		{
			OnEventReceived.Broadcast(ActivationTag, TempData);
		}
	}
}
