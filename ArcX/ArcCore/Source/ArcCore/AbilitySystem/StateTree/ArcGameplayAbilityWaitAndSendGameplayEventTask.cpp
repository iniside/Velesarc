// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayAbilityWaitAndSendGameplayEventTask.h"

#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "StateTreeExecutionContext.h"

FArcGameplayAbilityWaitAndSendGameplayEventTask::FArcGameplayAbilityWaitAndSendGameplayEventTask()
{
}

EStateTreeRunStatus FArcGameplayAbilityWaitAndSendGameplayEventTask::EnterState(FStateTreeExecutionContext& Context
																				, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.CurrentTime = 0;

	InstanceData.bWaitTimeOver = false;

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcGameplayAbilityWaitAndSendGameplayEventTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.CurrentTime += DeltaTime;
	if (InstanceData.CurrentTime >= InstanceData.WaitTime)
	{
		InstanceData.bWaitTimeOver = true;
		UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(InstanceData.AbilitySystemComponent);
		if (ArcASC && InstanceData.EventTag.IsValid())
		{
			FGameplayEventData Payload;
			//UE_LOG(LogArcCore, Log, TEXT("Notify %s Time %.2f, CurrentTime %.2f"), *Notifies[Idx].Tag.ToString(), Notifies[Idx].Time, CurrentNotifyTime);
			InstanceData.AbilitySystemComponent->HandleGameplayEvent(InstanceData.EventTag, &Payload);
		}

		return EStateTreeRunStatus::Running;
	}

	return EStateTreeRunStatus::Running;
}

void FArcGameplayAbilityWaitAndSendGameplayEventTask::ExitState(FStateTreeExecutionContext& Context
	, const FStateTreeTransitionResult& Transition) const
{

}
