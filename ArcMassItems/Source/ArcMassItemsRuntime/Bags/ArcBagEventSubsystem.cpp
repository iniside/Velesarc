// Copyright Lukasz Baran. All Rights Reserved.

#include "Bags/ArcBagEventSubsystem.h"
#include "Engine/World.h"
#include "Events/ArcEventSubsystemCore.h"
#include "MassEntitySubsystem.h"

// ---------------------------------------------------------------------------
// USubsystem lifecycle
// ---------------------------------------------------------------------------

void UArcMassBagEventSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UMassEntitySubsystem>();

	PostActorTickHandle = ArcEventSubsystemCore::BindPostActorTickDrain(this);
}

void UArcMassBagEventSubsystem::Deinitialize()
{
	ArcEventSubsystemCore::UnbindPostActorTickDrain(PostActorTickHandle);

	// Drain queues to discard any pending entries.
	{
		FArcQueuedBagListChange Discarded0;
		while (BagListChangeQueue.Dequeue(Discarded0)) {}
	}
	{
		FArcQueuedBagSlotsChange Discarded1;
		while (BagSlotsChangeQueue.Dequeue(Discarded1)) {}
	}

	PerEntityBagListChanged.Empty();
	PerEntityBagSlotsChanged.Empty();

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Per-entity bind / unbind
// ---------------------------------------------------------------------------

FDelegateHandle UArcMassBagEventSubsystem::AddBagListChangedForEntity(
	FMassEntityHandle Entity, FOnBagListChanged::FDelegate Delegate)
{
	return PerEntityBagListChanged.FindOrAdd(Entity).Add(MoveTemp(Delegate));
}

FDelegateHandle UArcMassBagEventSubsystem::AddBagSlotsChangedForEntity(
	FMassEntityHandle Entity, FOnBagSlotsChanged::FDelegate Delegate)
{
	return PerEntityBagSlotsChanged.FindOrAdd(Entity).Add(MoveTemp(Delegate));
}

void UArcMassBagEventSubsystem::RemoveForEntity(FMassEntityHandle Entity, FDelegateHandle Handle)
{
	// Key is a single FMassEntityHandle — direct-key removal helper, no scan needed.
	if (ArcEventSubsystemCore::TryRemoveHandle(PerEntityBagListChanged, Entity, Handle)) { return; }
	ArcEventSubsystemCore::TryRemoveHandle(PerEntityBagSlotsChanged, Entity, Handle);
}

// ---------------------------------------------------------------------------
// Producer API
// ---------------------------------------------------------------------------

void UArcMassBagEventSubsystem::QueueBagListChanged(FMassEntityHandle Entity)
{
	FArcQueuedBagListChange Item;
	Item.Entity = Entity;
	BagListChangeQueue.Enqueue(Item);
}

void UArcMassBagEventSubsystem::QueueBagSlotsChanged(FMassEntityHandle Entity, FArcBagId BagId)
{
	FArcQueuedBagSlotsChange Item;
	Item.Entity = Entity;
	Item.BagId  = BagId;
	BagSlotsChangeQueue.Enqueue(Item);
}

// ---------------------------------------------------------------------------
// Game-thread drain
// ---------------------------------------------------------------------------

void UArcMassBagEventSubsystem::DrainQueues()
{
	// -----------------------------------------------------------------------
	// 1. BagListChanged
	// -----------------------------------------------------------------------
	{
		FArcQueuedBagListChange Item;
		while (BagListChangeQueue.Dequeue(Item))
		{
			FOnBagListChanged* PerEntity = PerEntityBagListChanged.Find(Item.Entity);
			if (PerEntity != nullptr)
			{
				PerEntity->Broadcast(Item.Entity);
			}
			OnBagListChanged.Broadcast(Item.Entity);
		}
	}

	// -----------------------------------------------------------------------
	// 2. BagSlotsChanged
	// -----------------------------------------------------------------------
	{
		FArcQueuedBagSlotsChange Item;
		while (BagSlotsChangeQueue.Dequeue(Item))
		{
			FOnBagSlotsChanged* PerEntity = PerEntityBagSlotsChanged.Find(Item.Entity);
			if (PerEntity != nullptr)
			{
				PerEntity->Broadcast(Item.Entity, Item.BagId);
			}
			OnBagSlotsChanged.Broadcast(Item.Entity, Item.BagId);
		}
	}
}

void UArcMassBagEventSubsystem::DrainOnPostActorTick(
	UWorld* InWorld, ELevelTick /*TickType*/, float /*DeltaSeconds*/)
{
	// Guard: post-actor-tick fires for every world in the editor; only drain ours.
	if (InWorld != GetWorld())
	{
		return;
	}

	DrainQueues();
}

// ---------------------------------------------------------------------------
// Entity eviction
// ---------------------------------------------------------------------------

void UArcMassBagEventSubsystem::EvictEntity(FMassEntityHandle Entity)
{
	PerEntityBagListChanged.Remove(Entity);
	PerEntityBagSlotsChanged.Remove(Entity);
}
