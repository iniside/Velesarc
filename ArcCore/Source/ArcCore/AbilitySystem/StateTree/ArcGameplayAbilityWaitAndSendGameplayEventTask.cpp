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

#if WITH_EDITOR
FText FArcGameplayAbilityWaitAndSendGameplayEventTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData && InstanceData->EventTag.IsValid())
		{
			return FText::Format(NSLOCTEXT("ArcCore", "WaitAndSendEventDesc", "Wait {0}s → {1}"), FText::AsNumber(InstanceData->WaitTime), FText::FromString(InstanceData->EventTag.ToString()));
		}
	}
	return NSLOCTEXT("ArcCore", "WaitAndSendEventDescDefault", "Wait And Send Event");
}
#endif
