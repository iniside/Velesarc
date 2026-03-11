// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassTickStateTreeTask.h"

#include "ArcMassStateTreeTickProcessor.h"
#include "MassCommands.h"
#include "MassStateTreeExecutionContext.h"

FArcMassTickStateTreeTask::FArcMassTickStateTreeTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcMassTickStateTreeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();

	EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Add>>([Entity = MassCtx.GetEntity()](FMassEntityManager& Mgr)
	{
		Mgr.AddSparseElementToEntity<FArcMassTickStateTreeTag>(Entity);
	});
	
	return EStateTreeRunStatus::Running;
}

void FArcMassTickStateTreeTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();

	EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Remove>>([Entity = MassCtx.GetEntity()](FMassEntityManager& Mgr)
	{
		Mgr.RemoveSparseElementFromEntity<FArcMassTickStateTreeTag>(Entity);
	});
}

#if WITH_EDITOR
FText FArcMassTickStateTreeTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return NSLOCTEXT("ArcAI", "TickStateTreeDesc", "Tick Child StateTree");
}
#endif
