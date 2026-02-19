// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassTickStateTreeTask.h"

#include "ArcMassStateTreeTickProcessor.h"
#include "MassEntitySubsystem.h"
#include "MassStateTreeExecutionContext.h"

FArcMassTickStateTreeTask::FArcMassTickStateTreeTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcMassTickStateTreeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	
	UMassEntitySubsystem* MassSubsystem = MassCtx.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	
	EntityManager.Defer().AddTag<FArcMassTickStateTreeTag>(MassCtx.GetEntity());
	
	return EStateTreeRunStatus::Running;
}

void FArcMassTickStateTreeTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	
	UMassEntitySubsystem* MassSubsystem = MassCtx.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	
	EntityManager.Defer().RemoveTag<FArcMassTickStateTreeTag>(MassCtx.GetEntity());
}
