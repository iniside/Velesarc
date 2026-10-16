// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcMassAddNewItemToBagCommand.h"

#include "Bags/ArcBagOperations.h"
#include "Engine/World.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"

bool FArcMassAddNewItemToBagCommand::CanSendCommand() const
{
	if (Entity.IsSet() == false)
	{
		return false;
	}
	if (ItemDefinitionId.IsValid() == false)
	{
		return false;
	}
	// A specific bag without a specific slot is malformed — the specific-slot op needs an index,
	// and silently falling back to first-free-slot would ignore the caller's named bag.
	if (BagId.IsValid() && SlotIndex == INDEX_NONE)
	{
		return false;
	}
	return true;
}

bool FArcMassAddNewItemToBagCommand::Execute(UObject* WorldContext)
{
	UWorld* ResolvedWorld = ResolveWorld(WorldContext);
	if (ResolvedWorld == nullptr)
	{
		return false;
	}

	UMassEntitySubsystem* Subsystem = ResolvedWorld->GetSubsystem<UMassEntitySubsystem>();
	if (Subsystem == nullptr)
	{
		return false;
	}

	FMassEntityManager& Manager = Subsystem->GetMutableEntityManager();
	if (Entity.IsSet() == false || Manager.IsEntityActive(Entity) == false)
	{
		return false;
	}

	if (FArcItemDefinitionData::Find(ItemDefinitionId) == nullptr)
	{
		return false;
	}

	const FArcItemSpec Spec = FArcItemSpec::NewItem(ItemDefinitionId, Level, Amount);

	UE_LOGF(LogArcReplicatedCommand, Log,"AddNewItemToBag Execute: Def=%ls Level=%d Amount=%d Spec.Amount=%d Bag=%ls Slot=%d",
		*ItemDefinitionId.ToString(), static_cast<int32>(Level), Amount,
		static_cast<int32>(Spec.Amount), *BagId.ToString(), SlotIndex);

	// The bag ops derive the target store from FArcMassBagStoreFragment::TargetStoreType —
	// the inherited StoreType is deliberately unused here.
	if (BagId.IsValid() == false)
	{
		const FArcItemId Result = ArcMassItems::Bags::AddItemToFirstFreeSlot(Manager, Entity, Spec);
		UE_LOGF(LogArcReplicatedCommand, Log,"AddNewItemToBag Execute: first-free-slot result=%ls",
			Result.IsValid() ? *Result.ToString() : TEXT("INVALID"));
		return Result.IsValid();
	}
	const FArcItemId Result = ArcMassItems::Bags::AddItemToSlot(Manager, Entity, Spec, BagId, SlotIndex);
	UE_LOGF(LogArcReplicatedCommand, Log,"AddNewItemToBag Execute: specific-slot result=%ls",
		Result.IsValid() ? *Result.ToString() : TEXT("INVALID"));
	return Result.IsValid();
}
