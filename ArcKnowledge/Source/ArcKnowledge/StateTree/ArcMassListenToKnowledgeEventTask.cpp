// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassListenToKnowledgeEventTask.h"
#include "MassStateTreeExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "AsyncMessageWorldSubsystem.h"
#include "AsyncMessageSystemBase.h"
#include "MassStateTreeDependency.h"
#include "StateTreeAsyncExecutionContext.h"
#include "ArcMass/Messaging/ArcMassAsyncMessageEndpointFragment.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
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

	// Reset all outputs
	InstanceData.KnowledgeHandle = FArcKnowledgeHandle();
	InstanceData.EventType = EArcKnowledgeEventType::Registered;
	InstanceData.Tags.Reset();
	InstanceData.Location = FVector::ZeroVector;
	InstanceData.bEntryAvailable = false;
	InstanceData.SourceEntity = FArcMassEntityHandleWrapper();
	InstanceData.Payload.Reset();
	InstanceData.Relevance = 1.0f;
	InstanceData.Timestamp = 0.0;
	InstanceData.Lifetime = 0.0f;
	InstanceData.bClaimed = false;
	InstanceData.ClaimedBy = FArcMassEntityHandleWrapper();

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

	TSharedPtr<FAsyncMessageSystemBase> MessageSystem = UAsyncMessageWorldSubsystem::GetSharedMessageSystem(World);
	if (!MessageSystem)
	{
		return EStateTreeRunStatus::Running;
	}

	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();
	const FMassEntityHandle Entity = MassCtx.GetEntity();

	const FArcMassAsyncMessageEndpointFragment* EndpointFrag = EntityManager.GetFragmentDataPtr<FArcMassAsyncMessageEndpointFragment>(Entity);
	if (!EndpointFrag || !EndpointFrag->Endpoint.IsValid())
	{
		return EStateTreeRunStatus::Running;
	}

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	UArcKnowledgeSubsystem* KnowledgeSubsystem = World->GetSubsystem<UArcKnowledgeSubsystem>();

	InstanceData.BoundListenerHandle = MessageSystem->BindListener(
		InstanceData.MessageId,
		[SignalSubsystem, KnowledgeSubsystem, WeakContext = Context.MakeWeakExecutionContext(), Entity](const FAsyncMessage& Message)
		{
			const FArcKnowledgeChangedEvent* Event = Message.GetPayloadView().GetPtr<FArcKnowledgeChangedEvent>();
			if (!Event)
			{
				return;
			}

			FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FArcMassListenToKnowledgeEventTaskInstanceData* InstanceDataPtr =
				StrongContext.GetInstanceDataPtr<FArcMassListenToKnowledgeEventTaskInstanceData>();

			if (!InstanceDataPtr)
			{
				return;
			}

			// Populate event-derived outputs
			InstanceDataPtr->KnowledgeHandle = Event->Handle;
			InstanceDataPtr->EventType = Event->EventType;
			InstanceDataPtr->Tags = Event->Tags;
			InstanceDataPtr->Location = Event->Location;

			// Look up full entry from subsystem
			InstanceDataPtr->bEntryAvailable = false;
			if (KnowledgeSubsystem)
			{
				const FArcKnowledgeEntry* Entry = KnowledgeSubsystem->GetKnowledgeEntry(Event->Handle);
				if (Entry)
				{
					InstanceDataPtr->bEntryAvailable = true;
					InstanceDataPtr->SourceEntity.EntityHandle = Entry->SourceEntity;
					InstanceDataPtr->Payload = Entry->Payload;
					InstanceDataPtr->Relevance = Entry->Relevance;
					InstanceDataPtr->Timestamp = Entry->Timestamp;
					InstanceDataPtr->Lifetime = Entry->Lifetime;
					InstanceDataPtr->bClaimed = Entry->bClaimed;
					InstanceDataPtr->ClaimedBy.EntityHandle = Entry->ClaimedBy;
				}
			}

			// Fire the matching typed dispatcher
			switch (Event->EventType)
			{
			case EArcKnowledgeEventType::Registered:
				StrongContext.BroadcastDelegate(InstanceDataPtr->OnRegistered);
				break;
			case EArcKnowledgeEventType::Updated:
				StrongContext.BroadcastDelegate(InstanceDataPtr->OnUpdated);
				break;
			case EArcKnowledgeEventType::Removed:
				StrongContext.BroadcastDelegate(InstanceDataPtr->OnRemoved);
				break;
			case EArcKnowledgeEventType::AdvertisementPosted:
				StrongContext.BroadcastDelegate(InstanceDataPtr->OnAdvertisementPosted);
				break;
			case EArcKnowledgeEventType::AdvertisementClaimed:
				StrongContext.BroadcastDelegate(InstanceDataPtr->OnAdvertisementClaimed);
				break;
			case EArcKnowledgeEventType::AdvertisementCompleted:
				StrongContext.BroadcastDelegate(InstanceDataPtr->OnAdvertisementCompleted);
				break;
			}

			if (SignalSubsystem)
			{
				SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
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
