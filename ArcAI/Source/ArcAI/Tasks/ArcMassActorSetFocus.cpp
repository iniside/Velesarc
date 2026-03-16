#include "ArcMassActorSetFocus.h"

#include "AIController.h"
#include "StateTreeExecutionContext.h"

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

#if WITH_EDITOR
FText FArcMassActorSetFocusTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return NSLOCTEXT("ArcAI", "SetFocusDesc", "Set Focus on Target");
}
#endif
