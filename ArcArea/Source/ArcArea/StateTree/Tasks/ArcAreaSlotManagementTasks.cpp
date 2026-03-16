// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaSlotManagementTasks.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcAreaSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAreaSlotManagementTasks)

EStateTreeRunStatus FArcAreaDisableSlotTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassCtx = static_cast<const FMassStateTreeExecutionContext&>(Context);
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.SlotHandle.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcAreaSubsystem* Subsystem = MassCtx.GetWorld()->GetSubsystem<UArcAreaSubsystem>();
	if (!Subsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	Subsystem->DisableSlot(InstanceData.SlotHandle);
	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FArcAreaEnableSlotTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassCtx = static_cast<const FMassStateTreeExecutionContext&>(Context);
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.SlotHandle.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcAreaSubsystem* Subsystem = MassCtx.GetWorld()->GetSubsystem<UArcAreaSubsystem>();
	if (!Subsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	Subsystem->EnableSlot(InstanceData.SlotHandle);
	return EStateTreeRunStatus::Succeeded;
}
