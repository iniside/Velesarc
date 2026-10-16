// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcMassPlaceItemInBagSlotCommand.h"

#include "Bags/ArcBagOperations.h"
#include "Engine/World.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"

bool FArcMassPlaceItemInBagSlotCommand::CanSendCommand() const
{
	if (Entity.IsSet() == false)
	{
		return false;
	}
	if (BagId.IsValid() == false)
	{
		return false;
	}
	if (ItemId.IsValid() == false)
	{
		return false;
	}
	return true;
}

bool FArcMassPlaceItemInBagSlotCommand::Execute(UObject* WorldContext)
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

	return ArcMassItems::Bags::PlaceItemInSlot(Manager, Entity, BagId, SlotIndex, ItemId);
}
