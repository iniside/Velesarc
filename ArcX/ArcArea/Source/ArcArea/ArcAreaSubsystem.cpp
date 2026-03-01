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
		BroadcastOnSlotStateChanged(Handle, i, EArcAreaSlotState::Vacant);
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

		if (Slot.AssignedEntity.IsValid())
		{
			EntityAssignmentIndex.Remove(Slot.AssignedEntity);
			ClearNPCAssignmentFragment(Slot.AssignedEntity);
		}

		// Broadcast Disabled so listeners can clean up (e.g., remove vacancy postings)
		if (Slot.State != EArcAreaSlotState::Disabled)
		{
			BroadcastOnSlotStateChanged(Handle, i, EArcAreaSlotState::Disabled);
		}
	}

	Areas.Remove(Handle);
}

// ====================================================================
// Assignment
// ====================================================================

bool UArcAreaSubsystem::AssignToSlot(FArcAreaHandle AreaHandle, int32 SlotIndex, FMassEntityHandle Entity)
{
	FArcAreaData* Data = Areas.Find(AreaHandle);
	if (!Data || !Data->Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	FArcAreaSlotRuntime& Slot = Data->Slots[SlotIndex];
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
		UnassignFromSlot(ExistingSlot->AreaHandle, ExistingSlot->SlotIndex);
	}

	const FArcAreaSlotDefinition& SlotDef = Data->SlotDefinitions[SlotIndex];

	// Set assignment state
	Slot.State = EArcAreaSlotState::Assigned;
	Slot.AssignedEntity = Entity;

	// Add to reverse index
	FArcAreaSlotHandle SlotHandle;
	SlotHandle.AreaHandle = AreaHandle;
	SlotHandle.SlotIndex = SlotIndex;
	EntityAssignmentIndex.Add(Entity, SlotHandle);

	// Update NPC's fragment
	UpdateNPCAssignmentFragment(Entity, AreaHandle, SlotIndex, SlotDef.RoleTag);

	BroadcastOnSlotStateChanged(AreaHandle, SlotIndex, EArcAreaSlotState::Assigned);

	return true;
}

bool UArcAreaSubsystem::UnassignFromSlot(FArcAreaHandle AreaHandle, int32 SlotIndex)
{
	FArcAreaData* Data = Areas.Find(AreaHandle);
	if (!Data || !Data->Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	FArcAreaSlotRuntime& Slot = Data->Slots[SlotIndex];
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

	BroadcastOnSlotStateChanged(AreaHandle, SlotIndex, EArcAreaSlotState::Vacant);

	return true;
}

bool UArcAreaSubsystem::UnassignEntity(FMassEntityHandle Entity)
{
	if (const FArcAreaSlotHandle* SlotHandle = EntityAssignmentIndex.Find(Entity))
	{
		return UnassignFromSlot(SlotHandle->AreaHandle, SlotHandle->SlotIndex);
	}
	return false;
}

int32 UArcAreaSubsystem::FindSlotForEntity(FArcAreaHandle AreaHandle, FMassEntityHandle Entity) const
{
	const FArcAreaData* Data = Areas.Find(AreaHandle);
	if (!Data)
	{
		return INDEX_NONE;
	}

	for (int32 i = 0; i < Data->Slots.Num(); ++i)
	{
		if (Data->Slots[i].AssignedEntity == Entity)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

// ====================================================================
// SmartObject Bridge
// ====================================================================

void UArcAreaSubsystem::NotifySlotClaimed(FArcAreaHandle AreaHandle, int32 SlotIndex)
{
	FArcAreaData* Data = Areas.Find(AreaHandle);
	if (!Data || !Data->Slots.IsValidIndex(SlotIndex))
	{
		return;
	}

	FArcAreaSlotRuntime& Slot = Data->Slots[SlotIndex];
	if (Slot.State == EArcAreaSlotState::Assigned)
	{
		Slot.State = EArcAreaSlotState::Active;
		BroadcastOnSlotStateChanged(AreaHandle, SlotIndex, EArcAreaSlotState::Active);
	}
}

void UArcAreaSubsystem::NotifySlotReleased(FArcAreaHandle AreaHandle, int32 SlotIndex)
{
	FArcAreaData* Data = Areas.Find(AreaHandle);
	if (!Data || !Data->Slots.IsValidIndex(SlotIndex))
	{
		return;
	}

	FArcAreaSlotRuntime& Slot = Data->Slots[SlotIndex];
	if (Slot.State == EArcAreaSlotState::Active)
	{
		Slot.State = EArcAreaSlotState::Assigned;
		BroadcastOnSlotStateChanged(AreaHandle, SlotIndex, EArcAreaSlotState::Assigned);
	}
}

// ====================================================================
// Slot Management
// ====================================================================

void UArcAreaSubsystem::DisableSlot(FArcAreaHandle AreaHandle, int32 SlotIndex)
{
	FArcAreaData* Data = Areas.Find(AreaHandle);
	if (!Data || !Data->Slots.IsValidIndex(SlotIndex))
	{
		return;
	}

	FArcAreaSlotRuntime& Slot = Data->Slots[SlotIndex];
	if (Slot.State == EArcAreaSlotState::Disabled)
	{
		return;
	}

	// Clear assignment directly (skip intermediate Vacant broadcast)
	if (Slot.AssignedEntity.IsValid())
	{
		const FMassEntityHandle PreviousEntity = Slot.AssignedEntity;
		Slot.AssignedEntity = FMassEntityHandle();

		EntityAssignmentIndex.Remove(PreviousEntity);
		ClearNPCAssignmentFragment(PreviousEntity);
	}

	Slot.State = EArcAreaSlotState::Disabled;
	BroadcastOnSlotStateChanged(AreaHandle, SlotIndex, EArcAreaSlotState::Disabled);
}

void UArcAreaSubsystem::EnableSlot(FArcAreaHandle AreaHandle, int32 SlotIndex)
{
	FArcAreaData* Data = Areas.Find(AreaHandle);
	if (!Data || !Data->Slots.IsValidIndex(SlotIndex))
	{
		return;
	}

	FArcAreaSlotRuntime& Slot = Data->Slots[SlotIndex];
	if (Slot.State != EArcAreaSlotState::Disabled)
	{
		return;
	}

	Slot.State = EArcAreaSlotState::Vacant;
	BroadcastOnSlotStateChanged(AreaHandle, SlotIndex, EArcAreaSlotState::Vacant);
}

// ====================================================================
// Queries
// ====================================================================

const FArcAreaData* UArcAreaSubsystem::GetAreaData(FArcAreaHandle Handle) const
{
	return Areas.Find(Handle);
}

EArcAreaSlotState UArcAreaSubsystem::GetSlotState(FArcAreaHandle Handle, int32 SlotIndex) const
{
	const FArcAreaData* Data = Areas.Find(Handle);
	if (!Data || !Data->Slots.IsValidIndex(SlotIndex))
	{
		return EArcAreaSlotState::Disabled;
	}
	return Data->Slots[SlotIndex].State;
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

TArray<int32> UArcAreaSubsystem::GetVacantSlotIndices(FArcAreaHandle Handle) const
{
	TArray<int32> Result;
	const FArcAreaData* Data = Areas.Find(Handle);
	if (!Data)
	{
		return Result;
	}

	for (int32 i = 0; i < Data->Slots.Num(); ++i)
	{
		if (Data->Slots[i].State == EArcAreaSlotState::Vacant)
		{
			Result.Add(i);
		}
	}
	return Result;
}

// ====================================================================
// NPC Fragment Management (Private)
// ====================================================================

void UArcAreaSubsystem::UpdateNPCAssignmentFragment(FMassEntityHandle Entity, FArcAreaHandle AreaHandle, int32 SlotIndex, FGameplayTag RoleTag)
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
		Fragment->AreaHandle = AreaHandle;
		Fragment->SlotIndex = SlotIndex;
		Fragment->RoleTag = RoleTag;
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
		Fragment->AreaHandle = FArcAreaHandle();
		Fragment->SlotIndex = INDEX_NONE;
		Fragment->RoleTag = FGameplayTag();
	}
}
