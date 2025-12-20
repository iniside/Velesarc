// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayAbilityPlayMontageTask.h"

#include "ArcAN_SendGameplayEvent.h"
#include "StateTreeExecutionContext.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "AbilitySystem/Tasks/Animation/ArcAT_PlayMontageAndWaitForEvent.h"

FArcGameplayAbilityPlayMontageTask::FArcGameplayAbilityPlayMontageTask()
{
}

EStateTreeRunStatus FArcGameplayAbilityPlayMontageTask::EnterState(FStateTreeExecutionContext& Context
																   , const FStateTreeTransitionResult& Transition) const
{

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.CurrentTime = 0;
	InstanceData.NotifyEvents.Empty();
	
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(InstanceData.AbilitySystemComponent);
	UAnimInstance* AnimInstance = ArcASC->AbilityActorInfo.IsValid() ? ArcASC->AbilityActorInfo->GetAnimInstance() : nullptr;
	
	//ArcASC->PlayAnimMontage(Ability, MontagesFragment->StartMontage.Get(), StartEndPlayRate, NAME_None, 0, false, ItemDef);
	
	float PlayRate = 1.f;
	if (InstanceData.bUseCustomDuration)
	{
		const float Length = InstanceData.MontageToPlay->GetPlayLength();
		PlayRate = Length / InstanceData.Duration;
	}
	
	if (InstanceData.bGatherNotifies)
	{
		FAnimNotifyContext AnimContext;
		InstanceData.MontageToPlay->GetAnimNotifies(0, 30, AnimContext);
	
		for (const FAnimNotifyEventReference& AN : AnimContext.ActiveNotifies)
		{
			const FAnimNotifyEvent* Event = AN.GetNotify();
			if (!Event)
			{
				continue;
			}
			
			UArcAnimNotify_MarkGameplayEvent* MGE = Cast<UArcAnimNotify_MarkGameplayEvent>(Event->Notify);
			if (MGE)
			{
				FArcNotifyEvent NewNotify;
				NewNotify.Tag = MGE->GetEventTag();
				float Time = Event->GetTime();
				NewNotify.OriginalTime = Event->GetTriggerTime();
				NewNotify.Time = Event->GetTriggerTime() / PlayRate;
				InstanceData.NotifyEvents.Add(NewNotify);	
			}
		}
	}
	
	InstanceData.PlayedMontageLength = AnimInstance->Montage_Play(InstanceData.MontageToPlay
		, PlayRate
		, EMontagePlayReturnType::MontageLength
		, 0);
	
	if (InstanceData.bUseCustomDuration)
	{
		InstanceData.PlayedMontageLength = InstanceData.Duration;
	}
	
	if (!InstanceData.SectionName.IsNone())
	{
		AnimInstance->Montage_JumpToSection(InstanceData.SectionName, InstanceData.MontageToPlay);
	}
	
	InstanceData.MontageEndedDelegate.BindLambda([WeakContext = Context.MakeWeakExecutionContext()](class UAnimMontage* InMontage, bool bInterrupted)
		{
			auto StrongContext = WeakContext.MakeStrongExecutionContext();
			FInstanceDataType* InstanceData = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			if (InstanceData)
			{
				UAnimInstance* AnimInstance = InstanceData->AbilitySystemComponent->AbilityActorInfo.IsValid() ? InstanceData->AbilitySystemComponent->AbilityActorInfo->GetAnimInstance() : nullptr;
				AnimInstance->Montage_Stop(0.03f, InMontage);
				if (InstanceData->EndEventTag.IsValid())
				{
					FGameplayEventData Payload;
					InstanceData->AbilitySystemComponent->HandleGameplayEvent(InstanceData->EndEventTag, &Payload);	
				}
				
				StrongContext.BroadcastDelegate(InstanceData->OnMontageEnded);
			}
		});
	
	//AnimInstance->Montage_SetEndDelegate(InstanceData.MontageEndedDelegate, InstanceData.MontageToPlay);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcGameplayAbilityPlayMontageTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.CurrentTime += DeltaTime;
	
	for (int32 Idx = InstanceData.NotifyEvents.Num() - 1; Idx >= 0; --Idx)
	{
		if (InstanceData.CurrentTime >= InstanceData.NotifyEvents[Idx].Time)
		{
			FGameplayEventData Payload;
			//UE_LOG(LogArcCore, Log, TEXT("Notify %s Time %.2f, CurrentTime %.2f"), *Notifies[Idx].Tag.ToString(), Notifies[Idx].Time, CurrentNotifyTime);
			InstanceData.AbilitySystemComponent->HandleGameplayEvent(InstanceData.NotifyEvents[Idx].Tag, &Payload);
			
			InstanceData.NotifyEvents.RemoveAt(Idx);
		}
	}
	
	if (!InstanceData.bIsLooping)
	{
		if (InstanceData.PlayedMontageLength > 0 && InstanceData.CurrentTime >= (InstanceData.PlayedMontageLength))
		{
			UAnimInstance* AnimInstance = InstanceData.AbilitySystemComponent->AbilityActorInfo.IsValid() ? InstanceData.AbilitySystemComponent->AbilityActorInfo->GetAnimInstance() : nullptr;
			AnimInstance->Montage_Stop(InstanceData.EndBlendTime, InstanceData.MontageToPlay);
			if (InstanceData.EndEventTag.IsValid())
			{
				FGameplayEventData Payload;
				InstanceData.AbilitySystemComponent->HandleGameplayEvent(InstanceData.EndEventTag, &Payload);	
			}
			
			Context.BroadcastDelegate(InstanceData.OnMontageEnded);
			return EStateTreeRunStatus::Succeeded;
		}
	}
	
	return EStateTreeRunStatus::Running;
}

void FArcGameplayAbilityPlayMontageTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	if (InstanceData.bIsLooping)
	{
		UAnimInstance* AnimInstance = InstanceData.AbilitySystemComponent->AbilityActorInfo.IsValid() ? InstanceData.AbilitySystemComponent->AbilityActorInfo->GetAnimInstance() : nullptr;
		AnimInstance->Montage_Stop(InstanceData.EndBlendTime, InstanceData.MontageToPlay);
	}
}
