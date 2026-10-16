// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcMassReorderBagCommand.h"

#include "Bags/ArcBagOperations.h"
#include "Engine/World.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"

bool FArcMassReorderBagCommand::CanSendCommand() const
{
	if (Entity.IsSet() == false)
	{
		return false;
	}
	if (BagId.IsValid() == false)
	{
		return false;
	}
	return true;
}

bool FArcMassReorderBagCommand::Execute(UObject* WorldContext)
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

	return ArcMassItems::Bags::ReorderBag(Manager, Entity, BagId, NewDisplayIndex);
}
