// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaSlotManagementTasks.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcAreaSubsystem.h"
#include "ArcArea/Mass/ArcAreaFragments.h"
#include "MassEntityView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAreaSlotManagementTasks)

namespace UE::ArcArea::Private
{
	static FArcAreaHandle GetAreaHandleFromEntity(const FMassStateTreeExecutionContext& MassCtx)
	{
		const FMassEntityView EntityView(MassCtx.GetEntityManager(), MassCtx.GetEntity());
		if (const FArcAreaFragment* Fragment = EntityView.GetFragmentDataPtr<FArcAreaFragment>())
		{
			return Fragment->AreaHandle;
		}
		return {};
	}
}

EStateTreeRunStatus FArcAreaDisableSlotTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassCtx = static_cast<const FMassStateTreeExecutionContext&>(Context);
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const FArcAreaHandle AreaHandle = UE::ArcArea::Private::GetAreaHandleFromEntity(MassCtx);
	if (!AreaHandle.IsValid() || InstanceData.SlotIndex == INDEX_NONE)
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcAreaSubsystem* Subsystem = MassCtx.GetWorld()->GetSubsystem<UArcAreaSubsystem>();
	if (!Subsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	Subsystem->DisableSlot(AreaHandle, InstanceData.SlotIndex);
	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FArcAreaEnableSlotTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassCtx = static_cast<const FMassStateTreeExecutionContext&>(Context);
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const FArcAreaHandle AreaHandle = UE::ArcArea::Private::GetAreaHandleFromEntity(MassCtx);
	if (!AreaHandle.IsValid() || InstanceData.SlotIndex == INDEX_NONE)
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcAreaSubsystem* Subsystem = MassCtx.GetWorld()->GetSubsystem<UArcAreaSubsystem>();
	if (!Subsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	Subsystem->EnableSlot(AreaHandle, InstanceData.SlotIndex);
	return EStateTreeRunStatus::Succeeded;
}
