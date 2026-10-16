// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaSubsystem.h"
#include "ArcAreaDefinition.h"
#include "Mass/ArcAreaAssignmentFragments.h"
#include "MassEntitySubsystem.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectTypes.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectSubsystem.h"
#include "Engine/World.h"
#include "ArcAreaModule.h"

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

bool UArcAreaSubsystem::RegisterArea(
	const UArcAreaDefinition* Definition,
	FVector Location,
	FSmartObjectHandle SOHandle,
	FMassEntityHandle OwnerEntity)
{
	if (!Definition)
	{
		return false;
	}

	if (!SOHandle.IsValid())
	{
		UE_LOGF(LogArcAreaDiag, Error,"RegisterArea '%ls': no SmartObject handle — an area IS a SmartObject, so it cannot be registered.",
			*Definition->GetName());
		return false;
	}

	UWorld* World = GetWorld();
	USmartObjectSubsystem* SOSubsystem = World ? World->GetSubsystem<USmartObjectSubsystem>() : nullptr;
	if (!SOSubsystem)
	{
		UE_LOGF(LogArcAreaDiag, Error,"RegisterArea '%ls': no USmartObjectSubsystem.", *Definition->GetName());
		return false;
	}

	TArray<FSmartObjectSlotHandle> SlotHandles;
	SOSubsystem->GetAllSlots(SOHandle, SlotHandles);
	if (SlotHandles.IsEmpty())
	{
		UE_LOGF(LogArcAreaDiag, Error,"RegisterArea '%ls': SmartObject %ls reports no slots — nothing can ever be staffed here.",
			*Definition->GetName(), *LexToString(SOHandle));
		return false;
	}

	FArcAreaData Data;
	Data.DefinitionId = Definition->GetPrimaryAssetId();
	Data.DisplayName = Definition->DisplayName;
	Data.Location = Location;
	Data.AreaTags = Definition->AreaTags;
	Data.SmartObjectHandle = SOHandle;
	Data.OwnerEntity = OwnerEntity;
	Data.Slots.Reserve(SlotHandles.Num());

	for (const FSmartObjectSlotHandle& SlotHandle : SlotHandles)
	{
		FArcAreaSlotRuntime& SlotRuntime = Data.Slots.Add(SlotHandle);

		SOSubsystem->ReadSlotData(SlotHandle, [&SlotRuntime](FConstSmartObjectSlotView SlotView)
		{
			SlotView.GetActivityTags(SlotRuntime.ActivityTags);

			const FSmartObjectSlotDefinition& SlotDefinition = SlotView.GetDefinition();
			const USmartObjectDefinition& ObjectDefinition = SlotView.GetSmartObjectDefinition();

			// Mirrors USmartObjectSubsystem::FindMatchingSlotDefinitionIndicesInternal: which of the two queries the
			// engine actually runs is decided by the policy, not by the slot query being non-empty.
			switch (ObjectDefinition.GetUserTagsFilteringPolicy())
			{
			case ESmartObjectTagFilteringPolicy::Combine:
				SlotRuntime.ObjectUserTagFilter = ObjectDefinition.GetUserTagFilter();
				SlotRuntime.SlotUserTagFilter = SlotDefinition.UserTagFilter;
				break;

			case ESmartObjectTagFilteringPolicy::Override:
				SlotRuntime.SlotUserTagFilter = SlotDefinition.UserTagFilter.IsEmpty()
					? ObjectDefinition.GetUserTagFilter()
					: SlotDefinition.UserTagFilter;
				break;

			case ESmartObjectTagFilteringPolicy::NoFilter:
				break;
			}
		});

		UE_CLOGF(SlotRuntime.ObjectUserTagFilter.IsEmpty() && SlotRuntime.SlotUserTagFilter.IsEmpty(),
			LogArcAreaDiag, Warning,"RegisterArea '%ls': slot %ls has no effective user tag query — it accepts any user.",
			*Definition->GetName(), *LexToString(SlotHandle));
	}

	UE_LOGF(LogArcAreaDiag, Log,"RegisterArea '%ls' at %ls: smartObject=%ls slots=%d",
		*Definition->GetName(), *Location.ToCompactString(), *LexToString(SOHandle), SlotHandles.Num());

	Areas.Add(SOHandle, MoveTemp(Data));

	for (const FSmartObjectSlotHandle& SlotHandle : SlotHandles)
	{
		BroadcastOnSlotStateChanged(SlotHandle, EArcAreaSlotState::Vacant);
	}

	return true;
}

void UArcAreaSubsystem::UnregisterArea(FSmartObjectHandle Handle)
{
	FArcAreaData* Data = Areas.Find(Handle);
	if (!Data)
	{
		return;
	}

	TArray<FSmartObjectSlotHandle> SlotsToDisable;
	SlotsToDisable.Reserve(Data->Slots.Num());

	for (TPair<FSmartObjectSlotHandle, FArcAreaSlotRuntime>& Pair : Data->Slots)
	{
		const FMassEntityHandle PreviousEntity = Pair.Value.AssignedEntity;

		if (PreviousEntity.IsValid())
		{
			EntityAssignmentIndex.Remove(PreviousEntity);
			ClearNPCAssignmentFragment(PreviousEntity);
		}

		if (Pair.Value.State != EArcAreaSlotState::Disabled)
		{
			SlotsToDisable.Add(Pair.Key);
		}
	}

	Areas.Remove(Handle);

	for (const FSmartObjectSlotHandle& SlotHandle : SlotsToDisable)
	{
		BroadcastOnSlotStateChanged(SlotHandle, EArcAreaSlotState::Disabled);
	}
}

// ====================================================================
// Assignment
// ====================================================================

bool UArcAreaSubsystem::AssignToSlot(FSmartObjectSlotHandle SlotHandle, FMassEntityHandle Entity)
{
	FArcAreaData* Data = Areas.Find(SlotHandle.GetSmartObjectHandle());
	if (!Data)
	{
		return false;
	}

	FArcAreaSlotRuntime* Slot = Data->Slots.Find(SlotHandle);
	if (!Slot)
	{
		return false;
	}

	if (Slot->State != EArcAreaSlotState::Vacant && Slot->AssignedEntity != Entity)
	{
		return false;
	}

	if (!Entity.IsValid())
	{
		return false;
	}

	// If the entity is already assigned elsewhere, unassign first
	if (const FSmartObjectSlotHandle* ExistingSlot = EntityAssignmentIndex.Find(Entity))
	{
		UnassignFromSlot(*ExistingSlot);
	}

	// Set assignment state
	Slot->State = EArcAreaSlotState::Assigned;
	Slot->AssignedEntity = Entity;

	// Add to reverse index
	EntityAssignmentIndex.Add(Entity, SlotHandle);

	// Update NPC's fragment
	UpdateNPCAssignmentFragment(Entity, SlotHandle);

	BroadcastOnSlotStateChanged(SlotHandle, EArcAreaSlotState::Assigned);

	return true;
}

bool UArcAreaSubsystem::UnassignFromSlot(FSmartObjectSlotHandle SlotHandle)
{
	FArcAreaData* Data = Areas.Find(SlotHandle.GetSmartObjectHandle());
	if (!Data)
	{
		return false;
	}

	FArcAreaSlotRuntime* Slot = Data->Slots.Find(SlotHandle);
	if (!Slot)
	{
		return false;
	}

	if (Slot->State == EArcAreaSlotState::Vacant || Slot->State == EArcAreaSlotState::Disabled)
	{
		return false;
	}

	const FMassEntityHandle PreviousEntity = Slot->AssignedEntity;

	// Clear assignment
	Slot->State = EArcAreaSlotState::Vacant;
	Slot->AssignedEntity = FMassEntityHandle();

	// Remove from reverse index
	if (PreviousEntity.IsValid())
	{
		EntityAssignmentIndex.Remove(PreviousEntity);
		ClearNPCAssignmentFragment(PreviousEntity);
	}

	BroadcastOnSlotStateChanged(SlotHandle, EArcAreaSlotState::Vacant);

	return true;
}

bool UArcAreaSubsystem::UnassignEntity(FMassEntityHandle Entity)
{
	if (const FSmartObjectSlotHandle* SlotHandle = EntityAssignmentIndex.Find(Entity))
	{
		return UnassignFromSlot(*SlotHandle);
	}
	return false;
}

FSmartObjectSlotHandle UArcAreaSubsystem::FindSlotForEntity(FSmartObjectHandle AreaHandle, FMassEntityHandle Entity) const
{
	const FSmartObjectSlotHandle* SlotHandle = EntityAssignmentIndex.Find(Entity);
	if (SlotHandle && SlotHandle->GetSmartObjectHandle() == AreaHandle)
	{
		return *SlotHandle;
	}

	return FSmartObjectSlotHandle();
}

// ====================================================================
// SmartObject Bridge
// ====================================================================

void UArcAreaSubsystem::NotifySlotClaimed(FSmartObjectSlotHandle SlotHandle)
{
	FArcAreaData* Data = Areas.Find(SlotHandle.GetSmartObjectHandle());
	if (!Data)
	{
		return;
	}

	FArcAreaSlotRuntime* Slot = Data->Slots.Find(SlotHandle);
	if (Slot && Slot->State == EArcAreaSlotState::Assigned)
	{
		Slot->State = EArcAreaSlotState::Active;
		BroadcastOnSlotStateChanged(SlotHandle, EArcAreaSlotState::Active);
	}
}

void UArcAreaSubsystem::NotifySlotReleased(FSmartObjectSlotHandle SlotHandle)
{
	FArcAreaData* Data = Areas.Find(SlotHandle.GetSmartObjectHandle());
	if (!Data)
	{
		return;
	}

	FArcAreaSlotRuntime* Slot = Data->Slots.Find(SlotHandle);
	if (Slot && Slot->State == EArcAreaSlotState::Active)
	{
		Slot->State = EArcAreaSlotState::Assigned;
		BroadcastOnSlotStateChanged(SlotHandle, EArcAreaSlotState::Assigned);
	}
}

// ====================================================================
// Slot Management
// ====================================================================

void UArcAreaSubsystem::DisableSlot(FSmartObjectSlotHandle SlotHandle)
{
	FArcAreaData* Data = Areas.Find(SlotHandle.GetSmartObjectHandle());
	if (!Data)
	{
		return;
	}

	FArcAreaSlotRuntime* Slot = Data->Slots.Find(SlotHandle);
	if (!Slot || Slot->State == EArcAreaSlotState::Disabled)
	{
		return;
	}

	// Clear assignment directly (skip intermediate Vacant broadcast)
	const FMassEntityHandle PreviousEntity = Slot->AssignedEntity;
	if (PreviousEntity.IsValid())
	{
		Slot->AssignedEntity = FMassEntityHandle();

		EntityAssignmentIndex.Remove(PreviousEntity);
		ClearNPCAssignmentFragment(PreviousEntity);
	}

	Slot->State = EArcAreaSlotState::Disabled;
	BroadcastOnSlotStateChanged(SlotHandle, EArcAreaSlotState::Disabled);
}

void UArcAreaSubsystem::EnableSlot(FSmartObjectSlotHandle SlotHandle)
{
	FArcAreaData* Data = Areas.Find(SlotHandle.GetSmartObjectHandle());
	if (!Data)
	{
		return;
	}

	FArcAreaSlotRuntime* Slot = Data->Slots.Find(SlotHandle);
	if (!Slot || Slot->State != EArcAreaSlotState::Disabled)
	{
		return;
	}

	Slot->State = EArcAreaSlotState::Vacant;
	BroadcastOnSlotStateChanged(SlotHandle, EArcAreaSlotState::Vacant);
}

// ====================================================================
// Queries
// ====================================================================

const FArcAreaData* UArcAreaSubsystem::GetAreaData(FSmartObjectHandle Handle) const
{
	return Areas.Find(Handle);
}

EArcAreaSlotState UArcAreaSubsystem::GetSlotState(FSmartObjectSlotHandle SlotHandle) const
{
	const FArcAreaData* Data = Areas.Find(SlotHandle.GetSmartObjectHandle());
	if (!Data)
	{
		return EArcAreaSlotState::Disabled;
	}

	const FArcAreaSlotRuntime* Slot = Data->Slots.Find(SlotHandle);
	return Slot ? Slot->State : EArcAreaSlotState::Disabled;
}

TArray<FSmartObjectHandle> UArcAreaSubsystem::FindAreasByTags(const FGameplayTagQuery& TagQuery) const
{
	TArray<FSmartObjectHandle> Result;
	for (const TPair<FSmartObjectHandle, FArcAreaData>& Pair : Areas)
	{
		if (TagQuery.Matches(Pair.Value.AreaTags))
		{
			Result.Add(Pair.Key);
		}
	}
	return Result;
}

TArray<FSmartObjectSlotHandle> UArcAreaSubsystem::GetVacantSlots(FSmartObjectHandle Handle) const
{
	TArray<FSmartObjectSlotHandle> Result;
	const FArcAreaData* Data = Areas.Find(Handle);
	if (!Data)
	{
		return Result;
	}

	for (const TPair<FSmartObjectSlotHandle, FArcAreaSlotRuntime>& Pair : Data->Slots)
	{
		if (Pair.Value.State == EArcAreaSlotState::Vacant)
		{
			Result.Add(Pair.Key);
		}
	}
	return Result;
}

// ====================================================================
// NPC Fragment Management (Private)
// ====================================================================

void UArcAreaSubsystem::UpdateNPCAssignmentFragment(FMassEntityHandle Entity, FSmartObjectSlotHandle SlotHandle)
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
		Fragment->SlotHandle = FSmartObjectSlotHandle();
	}
}
