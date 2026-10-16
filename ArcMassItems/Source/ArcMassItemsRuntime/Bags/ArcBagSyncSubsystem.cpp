// Copyright Lukasz Baran. All Rights Reserved.

#include "Bags/ArcBagSyncSubsystem.h"

#include "Bags/ArcBagStoreFragment.h"
#include "Bags/ArcBagEventSubsystem.h"
#include "Events/ArcMassItemEventSubsystem.h"

#include "Engine/World.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"

void UArcMassBagSyncSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UMassEntitySubsystem>();
	Collection.InitializeDependency<UArcMassItemEventSubsystem>();

	EventSubsystem = GetWorld()->GetSubsystem<UArcMassItemEventSubsystem>();
	if (!EventSubsystem)
	{
		return;
	}

	HandleItemRemoved = EventSubsystem->OnItemRemoved.AddUObject(
		this, &UArcMassBagSyncSubsystem::OnItemRemoved);

	HandleItemStacksChanged = EventSubsystem->OnItemStacksChanged.AddUObject(
		this, &UArcMassBagSyncSubsystem::OnItemStacksChanged);
}

void UArcMassBagSyncSubsystem::Deinitialize()
{
	if (EventSubsystem)
	{
		EventSubsystem->OnItemRemoved.Remove(HandleItemRemoved);
		EventSubsystem->OnItemStacksChanged.Remove(HandleItemStacksChanged);
		EventSubsystem = nullptr;
	}

	EntityManager = nullptr;

	Super::Deinitialize();
}

bool UArcMassBagSyncSubsystem::EnsureCached()
{
	if (!EntityManager)
	{
		UMassEntitySubsystem* MassSub = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
		if (MassSub)
		{
			EntityManager = &MassSub->GetMutableEntityManager();
		}
	}
	return EntityManager != nullptr;
}

void UArcMassBagSyncSubsystem::OnItemRemoved(
	FMassEntityHandle Entity, FArcItemId ItemId, const UScriptStruct* /*StoreType*/)
{
	// FArcItemId is globally unique — StoreType filtering is unnecessary; one bag
	// fragment per entity regardless of which store the item was removed from.

	if (!EnsureCached())
	{
		return;
	}
	if (!EntityManager->IsEntityValid(Entity))
	{
		return;
	}

	FMassEntityView View(*EntityManager, Entity);
	FArcMassBagStoreFragment* Frag = View.GetSparseFragmentDataPtr<FArcMassBagStoreFragment>();
	if (!Frag)
	{
		return;
	}

	UWorld* World = GetWorld();
	UArcMassBagEventSubsystem* BagEvents = World ? World->GetSubsystem<UArcMassBagEventSubsystem>() : nullptr;

	for (FArcBagInstance& Bag : Frag->Bags.Items)
	{
		bool bBagChanged = false;
		for (int32 SlotIdx = 0; SlotIdx < Bag.Slots.Num(); ++SlotIdx)
		{
			if (Bag.Slots[SlotIdx] == ItemId)
			{
				Bag.Slots[SlotIdx] = FArcItemId();
				bBagChanged = true;
			}
		}
		if (bBagChanged)
		{
			Frag->Bags.MarkItemDirty(Bag);
			if (BagEvents)
			{
				BagEvents->QueueBagSlotsChanged(Entity, Bag.BagId);
			}
		}
	}
}

void UArcMassBagSyncSubsystem::OnItemStacksChanged(
	FMassEntityHandle Entity, FArcItemId ItemId, int32 /*StackDelta*/, const UScriptStruct* /*StoreType*/)
{
	// FArcItemId is globally unique — StoreType filtering is unnecessary; one bag
	// fragment per entity regardless of which store the item lives in. An in-place
	// stack change keeps the same id slotted, so this purely re-broadcasts a refresh
	// for any bag holding the id — no slot/fragment mutation, no MarkItemDirty.

	if (!EnsureCached())
	{
		return;
	}
	if (!EntityManager->IsEntityValid(Entity))
	{
		return;
	}

	FMassEntityView View(*EntityManager, Entity);
	FArcMassBagStoreFragment* Frag = View.GetSparseFragmentDataPtr<FArcMassBagStoreFragment>();
	if (!Frag)
	{
		return;
	}

	UWorld* World = GetWorld();
	UArcMassBagEventSubsystem* BagEvents = World ? World->GetSubsystem<UArcMassBagEventSubsystem>() : nullptr;
	if (!BagEvents)
	{
		return;
	}

	for (const FArcBagInstance& Bag : Frag->Bags.Items)
	{
		bool bContainsItem = false;
		for (int32 SlotIdx = 0; SlotIdx < Bag.Slots.Num(); ++SlotIdx)
		{
			if (Bag.Slots[SlotIdx] == ItemId)
			{
				bContainsItem = true;
				break;
			}
		}
		if (bContainsItem)
		{
			BagEvents->QueueBagSlotsChanged(Entity, Bag.BagId);
		}
	}
}
