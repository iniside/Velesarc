// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcMassAddItemCommand.h"

#include "Engine/World.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Operations/ArcMassItemOperations.h"

bool FArcMassAddItemCommand::CanSendCommand() const
{
	if (Entity.IsSet() == false)
	{
		return false;
	}
	if (Spec.GetItemDefinitionId().IsValid() == false)
	{
		return false;
	}
	return true;
}

bool FArcMassAddItemCommand::Execute(UObject* WorldContext)
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

	if (StoreType == nullptr)
	{
		UE_LOGF(LogArcReplicatedCommand, Warning,"%s: StoreType not set on %ls; falling back to FArcMassItemStoreFragment. "
				 "Use the parameterised constructor to set StoreType explicitly.",
			__FUNCTION__, *GetScriptStruct()->GetName());
	}
	UScriptStruct* ResolvedStoreType = StoreType ? StoreType.Get() : FArcMassItemStoreFragment::StaticStruct();

	const FArcItemId NewItemId = ArcMassItems::AddItem(Manager, Entity, ResolvedStoreType, Spec);
	return NewItemId.IsValid();
}
