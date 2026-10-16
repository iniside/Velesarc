// Copyright Lukasz Baran. All Rights Reserved.

#include "Equipment/ArcMassEquipmentSyncSubsystem.h"

#include "Equipment/ArcMassEquipmentFragment.h"
#include "Events/ArcMassItemEventSubsystem.h"

#include "Engine/World.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"

void UArcMassEquipmentSyncSubsystem::Initialize(FSubsystemCollectionBase& Collection)
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
		this, &UArcMassEquipmentSyncSubsystem::OnItemRemoved);
	HandleSlotCleared = EventSubsystem->OnItemSlotCleared.AddUObject(
		this, &UArcMassEquipmentSyncSubsystem::OnItemSlotCleared);
}

void UArcMassEquipmentSyncSubsystem::Deinitialize()
{
	if (EventSubsystem)
	{
		EventSubsystem->OnItemRemoved.Remove(HandleItemRemoved);
		EventSubsystem->OnItemSlotCleared.Remove(HandleSlotCleared);
		EventSubsystem = nullptr;
	}

	EntityManager = nullptr;

	Super::Deinitialize();
}

bool UArcMassEquipmentSyncSubsystem::EnsureCached()
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

void UArcMassEquipmentSyncSubsystem::EvictItemFromEquipment(FMassEntityHandle Entity, FArcItemId ItemId)
{
	if (!EnsureCached())
	{
		return;
	}
	if (!EntityManager->IsEntityValid(Entity))
	{
		return;
	}

	FMassEntityView View(*EntityManager, Entity);
	FArcMassEquipmentFragment* EquipFragment = View.GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();
	if (!EquipFragment)
	{
		return;
	}

	TArray<FGameplayTag> KeysToRemove;
	for (const TPair<FGameplayTag, FArcItemId>& Pair : EquipFragment->EquippedItems)
	{
		if (Pair.Value == ItemId)
		{
			KeysToRemove.Add(Pair.Key);
		}
	}
	for (const FGameplayTag& Key : KeysToRemove)
	{
		EquipFragment->EquippedItems.Remove(Key);
	}
}

void UArcMassEquipmentSyncSubsystem::OnItemRemoved(
	FMassEntityHandle Entity, FArcItemId ItemId, const UScriptStruct* /*StoreType*/)
{
	EvictItemFromEquipment(Entity, ItemId);
}

void UArcMassEquipmentSyncSubsystem::OnItemSlotCleared(
	FMassEntityHandle Entity, FArcItemId ItemId, FGameplayTag /*OldSlot*/, const UScriptStruct* /*StoreType*/)
{
	EvictItemFromEquipment(Entity, ItemId);
}
