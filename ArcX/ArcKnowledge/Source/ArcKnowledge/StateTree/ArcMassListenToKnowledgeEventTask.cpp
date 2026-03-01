// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassListenToKnowledgeEventTask.h"
#include "MassStateTreeExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "AsyncMessageWorldSubsystem.h"
#include "AsyncMessageSystemBase.h"
#include "MassStateTreeDependency.h"
#include "StateTreeAsyncExecutionContext.h"
#include "ArcMass/ArcMassAsyncMessageEndpointFragment.h"
#include "StateTreeLinker.h"

FArcMassListenToKnowledgeEventTask::FArcMassListenToKnowledgeEventTask()
{
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = true;
}

bool FArcMassListenToKnowledgeEventTask::Link(FStateTreeLinker& Linker)
{
	return true;
}

void FArcMassListenToKnowledgeEventTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FArcMassAsyncMessageEndpointFragment>();
}

EStateTreeRunStatus FArcMassListenToKnowledgeEventTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Reset outputs
	InstanceData.bEventReceived = false;
	InstanceData.ReceivedEvent = FArcKnowledgeChangedEvent();

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcMassListenToKnowledgeEventTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Already bound — just keep running
	if (InstanceData.BoundListenerHandle.IsValid())
	{
		return EStateTreeRunStatus::Running;
	}

	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Running;
	}

	if (!InstanceData.MessageId.IsValid())
	{
		return EStateTreeRunStatus::Running;
	}

	// Get the message system
	TSharedPtr<FAsyncMessageSystemBase> MessageSystem = UAsyncMessageWorldSubsystem::GetSharedMessageSystem(World);
	if (!MessageSystem)
	{
		return EStateTreeRunStatus::Running;
	}

	// Get the entity's endpoint fragment
	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();
	const FMassEntityHandle Entity = MassCtx.GetEntity();

	const FArcMassAsyncMessageEndpointFragment* EndpointFrag = EntityManager.GetFragmentDataPtr<FArcMassAsyncMessageEndpointFragment>(Entity);
	if (!EndpointFrag || !EndpointFrag->Endpoint.IsValid())
	{
		return EStateTreeRunStatus::Running;
	}

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();

	// Capture filter data for the lambda
	const FGameplayTagQuery TagFilter = InstanceData.TagFilter;
	const TArray<EArcKnowledgeEventType> AcceptedTypes = InstanceData.AcceptedEventTypes;

	InstanceData.BoundListenerHandle = MessageSystem->BindListener(
		InstanceData.MessageId,
		[SignalSubsystem, WeakContext = Context.MakeWeakExecutionContext(), Entity, TagFilter, AcceptedTypes](const FAsyncMessage& Message)
		{
			const FArcKnowledgeChangedEvent* Event = Message.GetPayloadView().GetPtr<FArcKnowledgeChangedEvent>();
			if (!Event)
			{
				return;
			}

			// Apply event type filter
			if (!AcceptedTypes.IsEmpty() && !AcceptedTypes.Contains(Event->EventType))
			{
				return;
			}

			// Apply tag filter
			if (!TagFilter.IsEmpty() && !TagFilter.Matches(Event->Tags))
			{
				return;
			}

			FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FArcMassListenToKnowledgeEventTaskInstanceData* InstanceDataPtr =
				StrongContext.GetInstanceDataPtr<FArcMassListenToKnowledgeEventTaskInstanceData>();

			if (InstanceDataPtr)
			{
				InstanceDataPtr->ReceivedEvent = *Event;
				InstanceDataPtr->bEventReceived = true;

				StrongContext.BroadcastDelegate(InstanceDataPtr->OnEventReceived);

				if (SignalSubsystem)
				{
					SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
				}
			}
		},
		FAsyncMessageBindingOptions(),
		EndpointFrag->Endpoint);

	return EStateTreeRunStatus::Running;
}

void FArcMassListenToKnowledgeEventTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.BoundListenerHandle.IsValid())
	{
		UWorld* World = Context.GetWorld();
		if (World)
		{
			TSharedPtr<FAsyncMessageSystemBase> MessageSystem = UAsyncMessageWorldSubsystem::GetSharedMessageSystem(World);
			if (MessageSystem)
			{
				MessageSystem->UnbindListener(InstanceData.BoundListenerHandle);
			}
		}
		InstanceData.BoundListenerHandle = FAsyncMessageHandle::Invalid;
	}
}
