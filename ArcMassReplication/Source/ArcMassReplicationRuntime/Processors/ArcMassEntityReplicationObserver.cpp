// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcMassEntityReplicationObserver.h"

#include "Engine/World.h"
#include "Fragments/ArcMassNetIdFragment.h"
#include "Fragments/ArcMassReplicatedTag.h"
#include "Fragments/ArcMassReplicationConfigFragment.h"
#include "Iris/ReplicationSystem/NetRefHandle.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "Subsystem/ArcMassEntityReplicationSubsystem.h"

// --- Start Observer ---

UArcMassEntityReplicationStartObserver::UArcMassEntityReplicationStartObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcMassEntityReplicatedTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
	// Do NOT set bAutoRegisterWithProcessingPhases (observers register through
	// FMassObserverManager, not the processing phase system).
	// DO set ExecutionFlags so the observer is only registered on worlds with
	// server authority. FMassObserverManager consults ShouldExecute() at register
	// time. Without this, replication-tagged entities created on a client world
	// (e.g. by the factory's local-entity spawn) would re-fire RegisterEntity and
	// cascade.
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcMassEntityReplicationStartObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddConstSharedRequirement<FArcMassEntityReplicationConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddRequirement<FArcMassEntityNetHandleFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddTagRequirement<FArcMassEntityReplicatedTag>(EMassFragmentPresence::All);
}

void UArcMassEntityReplicationStartObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (World == nullptr)
	{
		return;
	}
	UArcMassEntityReplicationSubsystem* Subsystem = World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
	if (Subsystem == nullptr)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context, [Subsystem](FMassExecutionContext& InnerContext)
	{
		const FArcMassEntityReplicationConfigFragment& ConfigFragment =
			InnerContext.GetConstSharedFragment<FArcMassEntityReplicationConfigFragment>();
		UMassEntityConfigAsset* Config = ConfigFragment.EntityConfigAsset;
		if (Config == nullptr)
		{
			return;
		}

		const TArrayView<FArcMassEntityNetHandleFragment> NetHandles =
			InnerContext.GetMutableFragmentView<FArcMassEntityNetHandleFragment>();

		const int32 NumEntities = InnerContext.GetNumEntities();
		UE_LOG(LogTemp, Log, TEXT("ArcMassReplication[Observer.Start]: chunk fired NumEntities=%d Config=%s World=%s"),
			NumEntities, *Config->GetName(), *Subsystem->GetWorld()->GetName());
		for (int32 Index = 0; Index < NumEntities; ++Index)
		{
			const FMassEntityHandle Entity = InnerContext.GetEntity(Index);
			UE_LOG(LogTemp, Log, TEXT("ArcMassReplication[Observer.Start]: RegisterEntity Entity=(Idx=%d,Ser=%d) Config=%s"),
				Entity.Index, Entity.SerialNumber, *Config->GetName());
			const UE::Net::FNetRefHandle Handle = Subsystem->RegisterEntity(Entity, Config);
			UE_LOG(LogTemp, Log, TEXT("ArcMassReplication[Observer.Start]: RegisterEntity returned Handle=%s"),
				*Handle.ToString());
			NetHandles[Index].NetHandle = Handle;
		}
	});
}

// --- Stop Observer ---

UArcMassEntityReplicationStopObserver::UArcMassEntityReplicationStopObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcMassEntityReplicatedTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
	// Symmetric with start observer: only the authority world unregisters server-side
	// replication state.
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcMassEntityReplicationStopObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// No fragment requirements — at Remove time the entity may have already lost
	// dependent fragments. We only need the entity handle, which is always available
	// via the execution context.
	ObserverQuery.AddTagRequirement<FArcMassEntityReplicatedTag>(EMassFragmentPresence::All);
}

void UArcMassEntityReplicationStopObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (World == nullptr)
	{
		return;
	}
	UArcMassEntityReplicationSubsystem* Subsystem = World->GetSubsystem<UArcMassEntityReplicationSubsystem>();
	if (Subsystem == nullptr)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context, [Subsystem](FMassExecutionContext& InnerContext)
	{
		const int32 NumEntities = InnerContext.GetNumEntities();
		for (int32 Index = 0; Index < NumEntities; ++Index)
		{
			const FMassEntityHandle Entity = InnerContext.GetEntity(Index);
			Subsystem->UnregisterEntity(Entity);
		}
	});
}
