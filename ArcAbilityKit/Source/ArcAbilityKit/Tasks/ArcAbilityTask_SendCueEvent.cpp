// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_SendCueEvent.h"

#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "ArcCues/ArcCueFunctions.h"

EStateTreeRunStatus FArcAbilityTask_SendCueEvent::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	UWorld* World = AbilityContext.GetMassContext().GetWorld();

	if (World == nullptr || !InstanceData.Handle.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	ArcCues::SendCueEvent(World, InstanceData.Handle, InstanceData.Event);
	return EStateTreeRunStatus::Succeeded;
}
