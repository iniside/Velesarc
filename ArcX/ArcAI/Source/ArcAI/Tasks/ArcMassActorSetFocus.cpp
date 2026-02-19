#include "ArcMassActorSetFocus.h"

#include "AIController.h"

FArcMassActorSetFocusTask::FArcMassActorSetFocusTask()
{
	bShouldCallTick = true;
}

EStateTreeRunStatus FArcMassActorSetFocusTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	if (!InstanceData.AIController)
	{
		return EStateTreeRunStatus::Running;
	}
	
	InstanceData.AIController->SetFocus(InstanceData.FocusTarget, EAIFocusPriority::Gameplay);
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcMassActorSetFocusTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController)
	{
		return EStateTreeRunStatus::Running;
	}
	
	InstanceData.AIController->SetFocus(InstanceData.FocusTarget, EAIFocusPriority::Gameplay);
	
	return EStateTreeRunStatus::Running;
}

void FArcMassActorSetFocusTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	if (!InstanceData.AIController)
	{
		return;
	}
	
	InstanceData.AIController->ClearFocus(EAIFocusPriority::Gameplay);
}
