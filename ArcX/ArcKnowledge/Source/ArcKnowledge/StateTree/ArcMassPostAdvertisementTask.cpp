// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPostAdvertisementTask.h"
#include "MassStateTreeExecutionContext.h"
#include "MassEntityFragments.h"
#include "ArcKnowledgeSubsystem.h"
#include "StateTreeLinker.h"

bool FArcMassPostAdvertisementTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TransformHandle);
	return true;
}

EStateTreeRunStatus FArcMassPostAdvertisementTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UWorld* World = MassCtx.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcKnowledgeSubsystem* Subsystem = World->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!Subsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FTransformFragment& Transform = Context.GetExternalData(TransformHandle);
	const FMassEntityHandle Entity = MassCtx.GetEntity();

	FArcKnowledgeEntry AdvertisementEntry;
	AdvertisementEntry.Tags = InstanceData.Tags;
	AdvertisementEntry.Location = Transform.GetTransform().GetLocation();
	AdvertisementEntry.SourceEntity = Entity;
	AdvertisementEntry.Relevance = InstanceData.Priority;
	AdvertisementEntry.Payload = InstanceData.Payload;
	AdvertisementEntry.Instruction = InstanceData.Instruction;

	InstanceData.PostedHandle = Subsystem->PostAdvertisement(AdvertisementEntry);

	return EStateTreeRunStatus::Succeeded;
}

void FArcMassPostAdvertisementTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (!bRemoveOnExit)
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
		if (UArcKnowledgeSubsystem* Subsystem = World->GetSubsystem<UArcKnowledgeSubsystem>())
		{
			Subsystem->RemoveKnowledge(InstanceData.PostedHandle);
			InstanceData.PostedHandle = FArcKnowledgeHandle();
		}
	}
}
