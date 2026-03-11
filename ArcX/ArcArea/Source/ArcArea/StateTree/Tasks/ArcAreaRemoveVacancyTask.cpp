// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaRemoveVacancyTask.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcKnowledgeSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAreaRemoveVacancyTask)

EStateTreeRunStatus FArcAreaRemoveVacancyTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bSucceeded = false;

	if (!InstanceData.VacancyHandle.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	const FMassStateTreeExecutionContext& MassCtx = static_cast<const FMassStateTreeExecutionContext&>(Context);
	UWorld* World = MassCtx.GetWorld();
	UArcKnowledgeSubsystem* KnowledgeSubsystem = World ? World->GetSubsystem<UArcKnowledgeSubsystem>() : nullptr;
	if (!KnowledgeSubsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	KnowledgeSubsystem->RemoveKnowledge(InstanceData.VacancyHandle);
	InstanceData.bSucceeded = true;

	return EStateTreeRunStatus::Succeeded;
}
