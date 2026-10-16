// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcMassAddBagCommand.h"

#include "Bags/ArcBagDefinition.h"
#include "Bags/ArcBagId.h"
#include "Bags/ArcBagOperations.h"
#include "Engine/World.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"

bool FArcMassAddBagCommand::CanSendCommand() const
{
	if (Entity.IsSet() == false)
	{
		return false;
	}
	if (BagDefinition == nullptr)
	{
		return false;
	}
	return true;
}

bool FArcMassAddBagCommand::Execute(UObject* WorldContext)
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

	const FArcBagId NewBagId = ArcMassItems::Bags::AddBag(Manager, Entity, BagDefinition);
	return NewBagId.IsValid();
}
