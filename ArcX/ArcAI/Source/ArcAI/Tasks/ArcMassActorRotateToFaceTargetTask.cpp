#include "ArcMassActorRotateToFaceTargetTask.h"

#include "AIController.h"
#include "StateTreeExecutionContext.h"

FArcMassActorRotateToFaceTargetTask::FArcMassActorRotateToFaceTargetTask()
{
}

bool FArcMassActorRotateToFaceTargetTask::Link(FStateTreeLinker& Linker)
{
	return FMassStateTreeTaskBase::Link(Linker);
}

void FArcMassActorRotateToFaceTargetTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	FMassStateTreeTaskBase::GetDependencies(Builder);
}

EStateTreeRunStatus FArcMassActorRotateToFaceTargetTask::EnterState(FStateTreeExecutionContext& Context
	, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	APawn* P = Cast<APawn>(InstanceData.InputActor);
	if (!P)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	AAIController* AIController = P->GetController<AAIController>();
	if (!AIController)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.Target)
	{
		return EStateTreeRunStatus::Failed;
	}

	AIController->SetFocus(InstanceData.Target, EAIFocusPriority::Gameplay);

	return EStateTreeRunStatus::Running;
}

void FArcMassActorRotateToFaceTargetTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	APawn* P = Cast<APawn>(InstanceData.InputActor);
	if (!P)
	{
		return;
	}
	
	AAIController* AIController = P->GetController<AAIController>();
	if (!AIController)
	{
		return;
	}

	AIController->ClearFocus(EAIFocusPriority::Gameplay);
}
