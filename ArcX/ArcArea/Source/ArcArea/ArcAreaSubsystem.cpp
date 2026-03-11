// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaSubsystem.h"
#include "ArcAreaDefinition.h"
#include "Mass/ArcAreaAssignmentFragments.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTypes.h"
#include "Engine/World.h"

// ====================================================================
// USubsystem
// ====================================================================

void UArcAreaSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UArcAreaSubsystem::Deinitialize()
{
	Areas.Empty();
	EntityAssignmentIndex.Empty();
	EntityAssignmentDelegates.Empty();
	Super::Deinitialize();
}

// ====================================================================
// Area Registration
// ====================================================================

FArcAreaHandle UArcAreaSubsystem::RegisterArea(
	const UArcAreaDefinition* Definition,
	FVector Location,
	FSmartObjectHandle SOHandle,
	FMassEntityHandle OwnerEntity)
{
	if (!Definition)
	{
		return FArcAreaHandle();
	}

	const FArcAreaHandle Handle = FArcAreaHandle::Make();

	FArcAreaData Data;
	Data.Handle = Handle;
	Data.DefinitionId = Definition->GetPrimaryAssetId();
	Data.DisplayName = Definition->DisplayName;
	Data.Location = Location;
	Data.AreaTags = Definition->AreaTags;
	Data.SmartObjectHandle = SOHandle;
	Data.OwnerEntity = OwnerEntity;
	Data.SlotDefinitions = Definition->Slots;

	// Initialize runtime slot state
	Data.Slots.SetNum(Definition->Slots.Num());

	Areas.Add(Handle, MoveTemp(Data));

	// Broadcast initial Vacant state for all slots
	for (int32 i = 0; i < Definition->Slots.Num(); ++i)
	{
		BroadcastOnSlotStateChanged(FArcAreaSlotHandle(Handle, i), EArcAreaSlotState::Vacant);
	}

	return Handle;
}

void UArcAreaSubsystem::UnregisterArea(FArcAreaHandle Handle)
{
	FArcAreaData* Data = Areas.Find(Handle);
	if (!Data)
	{
		return;
	}

	// Unassign all entities and broadcast Disabled for cleanup
	for (int32 i = 0; i < Data->Slots.Num(); ++i)
	{
		FArcAreaSlotRuntime& Slot = Data->Slots[i];
		const FMassEntityHandle PreviousEntity = Slot.AssignedEntity;

		if (PreviousEntity.IsValid())
		{
			EntityAssignmentIndex.Remove(PreviousEntity);
			ClearNPCAssignmentFragment(PreviousEntity);
		}

		// Broadcast Disabled so listeners can clean up (e.g., remove vacancy postings)
		if (Slot.State != EArcAreaSlotState::Disabled)
		{
			BroadcastOnSlotStateChanged(FArcAreaSlotHandle(Handle, i), EArcAreaSlotState::Disabled);
		}

		if (PreviousEntity.IsValid())
		{
			if (FArcAreaEntityDelegates* Delegates = EntityAssignmentDelegates.Find(PreviousEntity))
			{
				Delegates->OnUnassigned.Broadcast();
			}
		}
	}

	Areas.Remove(Handle);
}

// ====================================================================
// Assignment
// ====================================================================

bool UArcAreaSubsystem::AssignToSlot(FArcAreaSlotHandle SlotHandle, FMassEntityHandle Entity)
{
	FArcAreaData* Data = Areas.Find(SlotHandle.AreaHandle);
	if (!Data || !Data->Slots.IsValidIndex(SlotHandle.SlotIndex))
	{
		return false;
	}

	FArcAreaSlotRuntime& Slot = Data->Slots[SlotHandle.SlotIndex];
	if (Slot.State != EArcAreaSlotState::Vacant)
	{
		return false;
	}

	if (!Entity.IsValid())
	{
		return false;
	}

	// If the entity is already assigned elsewhere, unassign first
	if (const FArcAreaSlotHandle* ExistingSlot = EntityAssignmentIndex.Find(Entity))
	{
		UnassignFromSlot(*ExistingSlot);
	}

	// Set assignment state
	Slot.State = EArcAreaSlotState::Assigned;
	Slot.AssignedEntity = Entity;

	// Add to reverse index
	EntityAssignmentIndex.Add(Entity, SlotHandle);

	// Update NPC's fragment
	UpdateNPCAssignmentFragment(Entity, SlotHandle);

	BroadcastOnSlotStateChanged(SlotHandle, EArcAreaSlotState::Assigned);

	if (FArcAreaEntityDelegates* Delegates = EntityAssignmentDelegates.Find(Entity))
	{
		Delegates->OnAssigned.Broadcast();
	}

	return true;
}

bool UArcAreaSubsystem::UnassignFromSlot(FArcAreaSlotHandle SlotHandle)
{
	FArcAreaData* Data = Areas.Find(SlotHandle.AreaHandle);
	if (!Data || !Data->Slots.IsValidIndex(SlotHandle.SlotIndex))
	{
		return false;
	}

	FArcAreaSlotRuntime& Slot = Data->Slots[SlotHandle.SlotIndex];
	if (Slot.State == EArcAreaSlotState::Vacant || Slot.State == EArcAreaSlotState::Disabled)
	{
		return false;
	}

	const FMassEntityHandle PreviousEntity = Slot.AssignedEntity;

	// Clear assignment
	Slot.State = EArcAreaSlotState::Vacant;
	Slot.AssignedEntity = FMassEntityHandle();

	// Remove from reverse index
	if (PreviousEntity.IsValid())
	{
		EntityAssignmentIndex.Remove(PreviousEntity);
		ClearNPCAssignmentFragment(PreviousEntity);
	}

	BroadcastOnSlotStateChanged(SlotHandle, EArcAreaSlotState::Vacant);

	if (PreviousEntity.IsValid())
	{
		if (FArcAreaEntityDelegates* Delegates = EntityAssignmentDelegates.Find(PreviousEntity))
		{
			Delegates->OnUnassigned.Broadcast();
		}
	}

	return true;
}

bool UArcAreaSubsystem::UnassignEntity(FMassEntityHandle Entity)
{
	if (const FArcAreaSlotHandle* SlotHandle = EntityAssignmentIndex.Find(Entity))
	{
		return UnassignFromSlot(*SlotHandle);
	}
	return false;
}

FArcAreaSlotHandle UArcAreaSubsystem::FindSlotForEntity(FArcAreaHandle AreaHandle, FMassEntityHandle Entity) const
{
	const FArcAreaData* Data = Areas.Find(AreaHandle);
	if (!Data)
	{
		return FArcAreaSlotHandle();
	}

	for (int32 i = 0; i < Data->Slots.Num(); ++i)
	{
		if (Data->Slots[i].AssignedEntity == Entity)
		{
			return FArcAreaSlotHandle(AreaHandle, i);
		}
	}

	return FArcAreaSlotHandle();
}

// ====================================================================
// Per-Entity Assignment Delegates
// ====================================================================

FArcAreaEntityDelegates& UArcAreaSubsystem::GetOrCreateEntityDelegates(FMassEntityHandle Entity)
{
	return EntityAssignmentDelegates.FindOrAdd(Entity);
}

FArcAreaEntityDelegates* UArcAreaSubsystem::FindEntityDelegates(FMassEntityHandle Entity)
{
	return EntityAssignmentDelegates.Find(Entity);
}

void UArcAreaSubsystem::RemoveEntityDelegates(FMassEntityHandle Entity)
{
	EntityAssignmentDelegates.Remove(Entity);
}

// ====================================================================
// SmartObject Bridge
// ====================================================================

void UArcAreaSubsystem::NotifySlotClaimed(FArcAreaSlotHandle SlotHandle)
{
	FArcAreaData* Data = Areas.Find(SlotHandle.AreaHandle);
	if (!Data || !Data->Slots.IsValidIndex(SlotHandle.SlotIndex))
	{
		return;
	}

	FArcAreaSlotRuntime& Slot = Data->Slots[SlotHandle.SlotIndex];
	if (Slot.State == EArcAreaSlotState::Assigned)
	{
		Slot.State = EArcAreaSlotState::Active;
		BroadcastOnSlotStateChanged(SlotHandle, EArcAreaSlotState::Active);
	}
}

void UArcAreaSubsystem::NotifySlotReleased(FArcAreaSlotHandle SlotHandle)
{
	FArcAreaData* Data = Areas.Find(SlotHandle.AreaHandle);
	if (!Data || !Data->Slots.IsValidIndex(SlotHandle.SlotIndex))
	{
		return;
	}

	FArcAreaSlotRuntime& Slot = Data->Slots[SlotHandle.SlotIndex];
	if (Slot.State == EArcAreaSlotState::Active)
	{
		Slot.State = EArcAreaSlotState::Assigned;
		BroadcastOnSlotStateChanged(SlotHandle, EArcAreaSlotState::Assigned);
	}
}

// ====================================================================
// Slot Management
// ====================================================================

void UArcAreaSubsystem::DisableSlot(FArcAreaSlotHandle SlotHandle)
{
	FArcAreaData* Data = Areas.Find(SlotHandle.AreaHandle);
	if (!Data || !Data->Slots.IsValidIndex(SlotHandle.SlotIndex))
	{
		return;
	}

	FArcAreaSlotRuntime& Slot = Data->Slots[SlotHandle.SlotIndex];
	if (Slot.State == EArcAreaSlotState::Disabled)
	{
		return;
	}

	// Clear assignment directly (skip intermediate Vacant broadcast)
	const FMassEntityHandle PreviousEntity = Slot.AssignedEntity;
	if (PreviousEntity.IsValid())
	{
		Slot.AssignedEntity = FMassEntityHandle();

		EntityAssignmentIndex.Remove(PreviousEntity);
		ClearNPCAssignmentFragment(PreviousEntity);
	}

	Slot.State = EArcAreaSlotState::Disabled;
	BroadcastOnSlotStateChanged(SlotHandle, EArcAreaSlotState::Disabled);

	if (PreviousEntity.IsValid())
	{
		if (FArcAreaEntityDelegates* Delegates = EntityAssignmentDelegates.Find(PreviousEntity))
		{
			Delegates->OnUnassigned.Broadcast();
		}
	}
}

void UArcAreaSubsystem::EnableSlot(FArcAreaSlotHandle SlotHandle)
{
	FArcAreaData* Data = Areas.Find(SlotHandle.AreaHandle);
	if (!Data || !Data->Slots.IsValidIndex(SlotHandle.SlotIndex))
	{
		return;
	}

	FArcAreaSlotRuntime& Slot = Data->Slots[SlotHandle.SlotIndex];
	if (Slot.State != EArcAreaSlotState::Disabled)
	{
		return;
	}

	Slot.State = EArcAreaSlotState::Vacant;
	BroadcastOnSlotStateChanged(SlotHandle, EArcAreaSlotState::Vacant);
}

// ====================================================================
// Queries
// ====================================================================

const FArcAreaData* UArcAreaSubsystem::GetAreaData(FArcAreaHandle Handle) const
{
	return Areas.Find(Handle);
}

EArcAreaSlotState UArcAreaSubsystem::GetSlotState(FArcAreaSlotHandle SlotHandle) const
{
	const FArcAreaData* Data = Areas.Find(SlotHandle.AreaHandle);
	if (!Data || !Data->Slots.IsValidIndex(SlotHandle.SlotIndex))
	{
		return EArcAreaSlotState::Disabled;
	}
	return Data->Slots[SlotHandle.SlotIndex].State;
}

TArray<FArcAreaHandle> UArcAreaSubsystem::FindAreasByTags(const FGameplayTagQuery& TagQuery) const
{
	TArray<FArcAreaHandle> Result;
	for (const auto& Pair : Areas)
	{
		if (TagQuery.Matches(Pair.Value.AreaTags))
		{
			Result.Add(Pair.Key);
		}
	}
	return Result;
}

bool UArcAreaSubsystem::HasVacancy(FArcAreaHandle Handle) const
{
	const FArcAreaData* Data = Areas.Find(Handle);
	if (!Data)
	{
		return false;
	}

	for (const FArcAreaSlotRuntime& Slot : Data->Slots)
	{
		if (Slot.State == EArcAreaSlotState::Vacant)
		{
			return true;
		}
	}
	return false;
}

TArray<FArcAreaSlotHandle> UArcAreaSubsystem::GetVacantSlots(FArcAreaHandle Handle) const
{
	TArray<FArcAreaSlotHandle> Result;
	const FArcAreaData* Data = Areas.Find(Handle);
	if (!Data)
	{
		return Result;
	}

	for (int32 i = 0; i < Data->Slots.Num(); ++i)
	{
		if (Data->Slots[i].State == EArcAreaSlotState::Vacant)
		{
			Result.Emplace(Handle, i);
		}
	}
	return Result;
}

// ====================================================================
// NPC Fragment Management (Private)
// ====================================================================

void UArcAreaSubsystem::UpdateNPCAssignmentFragment(FMassEntityHandle Entity, FArcAreaSlotHandle SlotHandle)
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
	if (!EntityManager.IsEntityValid(Entity))
	{
		return;
	}

	if (FArcAreaAssignmentFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcAreaAssignmentFragment>(Entity))
	{
		Fragment->SlotHandle = SlotHandle;
	}
}

void UArcAreaSubsystem::ClearNPCAssignmentFragment(FMassEntityHandle Entity)
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
	if (!EntityManager.IsEntityValid(Entity))
	{
		return;
	}

	if (FArcAreaAssignmentFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcAreaAssignmentFragment>(Entity))
	{
		Fragment->SlotHandle = FArcAreaSlotHandle();
	}
}
