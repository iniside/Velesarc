// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCraft/Mass/ArcCraftOutputResumeSubsystem.h"

#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Mass/ArcCraftQueueChangedSignal.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"

#include "Bags/ArcBagEventSubsystem.h"

#include "Engine/World.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"

void UArcCraftOutputResumeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UArcMassBagEventSubsystem>();

	BagEventSubsystem = GetWorld()->GetSubsystem<UArcMassBagEventSubsystem>();
	if (!BagEventSubsystem)
	{
		return;
	}

	BagSlotsChangedHandle = BagEventSubsystem->OnBagSlotsChanged.AddUObject(
		this, &UArcCraftOutputResumeSubsystem::OnBagSlotsChanged);
}

void UArcCraftOutputResumeSubsystem::Deinitialize()
{
	if (BagEventSubsystem)
	{
		BagEventSubsystem->OnBagSlotsChanged.Remove(BagSlotsChangedHandle);
		BagEventSubsystem = nullptr;
	}
	BagSlotsChangedHandle.Reset();

	BlockedByBagOwner.Empty();

	Super::Deinitialize();
}

void UArcCraftOutputResumeSubsystem::RegisterBlocked(FMassEntityHandle BagOwner, FMassEntityHandle Station)
{
	// Add the pair only if not already present (avoid duplicate (BagOwner, Station) mappings).
	if (BlockedByBagOwner.FindPair(BagOwner, Station) != nullptr)
	{
		return;
	}
	BlockedByBagOwner.Add(BagOwner, Station);
}

void UArcCraftOutputResumeSubsystem::EvictEntity(FMassEntityHandle Entity)
{
	// Remove Entity as a key.
	BlockedByBagOwner.Remove(Entity);

	// Remove Entity from every value list. Two-pass: collect the (key, value) pairs to
	// remove first, then bulk-remove — never RemoveAt while iterating.
	TArray<FMassEntityHandle> KeysToRemove;
	for (TMultiMap<FMassEntityHandle, FMassEntityHandle>::TConstIterator It(BlockedByBagOwner); It; ++It)
	{
		if (It.Value() == Entity)
		{
			KeysToRemove.Add(It.Key());
		}
	}
	for (const FMassEntityHandle& Key : KeysToRemove)
	{
		BlockedByBagOwner.RemoveSingle(Key, Entity);
	}
}

void UArcCraftOutputResumeSubsystem::OnBagSlotsChanged(FMassEntityHandle BagOwner, FArcBagId /*BagId*/)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem)
	{
		return;
	}
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();

	// Snapshot the mapped stations before mutating fragments / firing signals.
	TArray<FMassEntityHandle> Stations;
	BlockedByBagOwner.MultiFind(BagOwner, Stations);

	for (const FMassEntityHandle& Station : Stations)
	{
		if (!EntityManager.IsEntityValid(Station))
		{
			// Stale station; the mapping is removed wholesale below.
			continue;
		}

		FArcCraftQueueFragment* Queue = EntityManager.GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
		if (!Queue)
		{
			continue;
		}

		// Clear bOutputBlocked on whichever entry currently carries it. Re-read the live
		// entries — the blocked entry may have been cancelled/changed since the block. No-op
		// if none is blocked. Leave HeldOutput intact: the processor needs it next tick to
		// deliver the held items.
		for (FArcCraftQueueEntry& Entry : Queue->Entries)
		{
			if (Entry.bOutputBlocked)
			{
				Entry.bOutputBlocked = false;
			}
		}

		// Wake the station so the AutoTick processor re-attempts delivery next tick.
		if (SignalSubsystem)
		{
			SignalSubsystem->SignalEntity(ArcCraftSignals::QueueChanged, Station);
		}
	}

	// Remove ALL mappings for BagOwner after processing, including stale/invalid stations —
	// they self-heal (re-block next completion, re-register).
	BlockedByBagOwner.Remove(BagOwner);
}
