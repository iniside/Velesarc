// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_SendCueStateTreeEvent.h"

#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "ArcCues/ArcCueFunctions.h"

EStateTreeRunStatus FArcAbilityTask_SendCueStateTreeEvent::EnterState(
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

	ArcCues::SendCueStateTreeEvent(World, InstanceData.Handle, InstanceData.Event);
	return EStateTreeRunStatus::Succeeded;
}
