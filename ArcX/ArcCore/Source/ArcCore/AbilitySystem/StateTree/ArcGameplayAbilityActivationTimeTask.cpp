// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayAbilityActivationTimeTask.h"

#include "StateTreeExecutionContext.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"

FArcGameplayAbilityActivationTimeTask::FArcGameplayAbilityActivationTimeTask()
{
}

EStateTreeRunStatus FArcGameplayAbilityActivationTimeTask::EnterState(FStateTreeExecutionContext& Context
	, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.CurrentTime = 0;
	InstanceData.bWaitTimeOver = false;
	
	if (UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(InstanceData.Ability))
	{
		
	}
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcGameplayAbilityActivationTimeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(InstanceData.Ability))
	{
		const float ActivationTime = ArcAbility->GetActivationTime();
		InstanceData.CurrentTime += DeltaTime;
		if (InstanceData.CurrentTime >= ActivationTime && !InstanceData.bWaitTimeOver)
		{
			InstanceData.bWaitTimeOver = true;
			Context.BroadcastDelegate(InstanceData.OnActivationCompleted);
			return EStateTreeRunStatus::Running;
		}
		
	}
	
	return EStateTreeRunStatus::Failed;
}

void FArcGameplayAbilityGetActivationTimePropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(InstanceData.Ability))
	{
		InstanceData.ActivationTime = ArcAbility->GetActivationTime();
		return;
	}
	
	InstanceData.ActivationTime = 0;
}

void FArcGameplayAbilityGetItemScalableFloatPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(InstanceData.Ability))
	{
		InstanceData.Value = ArcAbility->FindItemScalableValue(InstanceData.ScalableFloatStruct, InstanceData.ScalableFloatName);
		return;
	}
	
	InstanceData.Value = 0;
}
