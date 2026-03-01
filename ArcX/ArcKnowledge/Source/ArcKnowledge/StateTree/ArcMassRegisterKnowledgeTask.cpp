// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassRegisterKnowledgeTask.h"
#include "MassStateTreeExecutionContext.h"
#include "MassEntityFragments.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntryDefinition.h"
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

	UArcKnowledgeSubsystem* Subsystem = World->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!Subsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FTransformFragment& Transform = Context.GetExternalData(TransformHandle);
	const FMassEntityHandle Entity = MassCtx.GetEntity();
	const FVector Location = Transform.GetTransform().GetLocation();

	if (Definition)
	{
		// Register all entries from the definition
		Subsystem->RegisterFromDefinition(Definition, Location, Entity, InstanceData.AllRegisteredHandles);

		// Set the primary handle output (first handle if available)
		if (!InstanceData.AllRegisteredHandles.IsEmpty())
		{
			InstanceData.RegisteredHandle = InstanceData.AllRegisteredHandles[0];
		}
	}
	else
	{
		// Inline registration from instance data parameters
		FArcKnowledgeEntry Entry;
		Entry.Tags = InstanceData.Tags;
		Entry.Location = Location;
		Entry.SourceEntity = Entity;
		Entry.Relevance = InstanceData.Relevance;
		Entry.Payload = InstanceData.Payload;

		InstanceData.RegisteredHandle = Subsystem->RegisterKnowledge(Entry);
		InstanceData.AllRegisteredHandles.Add(InstanceData.RegisteredHandle);
	}

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

	if (InstanceData.AllRegisteredHandles.IsEmpty())
	{
		return;
	}

	if (UWorld* World = MassCtx.GetWorld())
	{
		if (UArcKnowledgeSubsystem* Subsystem = World->GetSubsystem<UArcKnowledgeSubsystem>())
		{
			for (const FArcKnowledgeHandle& Handle : InstanceData.AllRegisteredHandles)
			{
				if (Handle.IsValid())
				{
					Subsystem->RemoveKnowledge(Handle);
				}
			}
			InstanceData.AllRegisteredHandles.Empty();
			InstanceData.RegisteredHandle = FArcKnowledgeHandle();
		}
	}
}
