// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassRegisterKnowledgeTask.h"
#include "MassStateTreeExecutionContext.h"
#include "MassEntityFragments.h"
#include "ArcSettlementSubsystem.h"
#include "Mass/ArcSettlementFragments.h"
#include "StateTreeLinker.h"

bool FArcMassRegisterKnowledgeTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TransformHandle);
	return true;
}

EStateTreeRunStatus FArcMassRegisterKnowledgeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
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

	FArcKnowledgeEntry Entry;
	Entry.Tags = InstanceData.Tags;
	Entry.Location = Transform.GetTransform().GetLocation();
	Entry.SourceEntity = Entity;
	Entry.Relevance = InstanceData.Relevance;
	Entry.Payload = InstanceData.Payload;

	// Associate with settlement if entity is a member
	if (const FArcSettlementMemberFragment* MemberFrag = EntityManager.GetFragmentDataPtr<FArcSettlementMemberFragment>(Entity))
	{
		Entry.Settlement = MemberFrag->SettlementHandle;
	}

	InstanceData.RegisteredHandle = Subsystem->RegisterKnowledge(Entry);

	return EStateTreeRunStatus::Succeeded;
}

void FArcMassRegisterKnowledgeTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (!bRemoveOnExit)
	{
		return;
	}

	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.RegisteredHandle.IsValid())
	{
		return;
	}

	if (UWorld* World = MassCtx.GetWorld())
	{
		if (UArcSettlementSubsystem* Subsystem = World->GetSubsystem<UArcSettlementSubsystem>())
		{
			Subsystem->RemoveKnowledge(InstanceData.RegisteredHandle);
			InstanceData.RegisteredHandle = FArcKnowledgeHandle();
		}
	}
}
