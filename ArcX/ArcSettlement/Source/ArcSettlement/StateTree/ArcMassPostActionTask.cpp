// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPostActionTask.h"
#include "MassStateTreeExecutionContext.h"
#include "MassEntityFragments.h"
#include "ArcSettlementSubsystem.h"
#include "Mass/ArcSettlementFragments.h"
#include "StateTreeLinker.h"

bool FArcMassPostActionTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TransformHandle);
	return true;
}

EStateTreeRunStatus FArcMassPostActionTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UWorld* World = MassCtx.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcSettlementSubsystem* Subsystem = World->GetSubsystem<UArcSettlementSubsystem>();
	if (!Subsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FTransformFragment& Transform = Context.GetExternalData(TransformHandle);
	const FMassEntityHandle Entity = MassCtx.GetEntity();
	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();

	FArcKnowledgeEntry ActionEntry;
	ActionEntry.Tags = InstanceData.Tags;
	ActionEntry.Location = Transform.GetTransform().GetLocation();
	ActionEntry.SourceEntity = Entity;
	ActionEntry.Relevance = InstanceData.Priority;
	ActionEntry.Payload = InstanceData.ActionData;

	if (const FArcSettlementMemberFragment* MemberFrag = EntityManager.GetFragmentDataPtr<FArcSettlementMemberFragment>(Entity))
	{
		ActionEntry.Settlement = MemberFrag->SettlementHandle;
	}

	InstanceData.PostedHandle = Subsystem->PostAction(ActionEntry);

	return EStateTreeRunStatus::Succeeded;
}

void FArcMassPostActionTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (!bCancelOnExit)
	{
		return;
	}

	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.PostedHandle.IsValid())
	{
		return;
	}

	if (UWorld* World = MassCtx.GetWorld())
	{
		if (UArcSettlementSubsystem* Subsystem = World->GetSubsystem<UArcSettlementSubsystem>())
		{
			Subsystem->RemoveKnowledge(InstanceData.PostedHandle);
			InstanceData.PostedHandle = FArcKnowledgeHandle();
		}
	}
}
