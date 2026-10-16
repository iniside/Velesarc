// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcMassAddNewItemCommand.h"

#include "Engine/World.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Operations/ArcMassItemOperations.h"

bool FArcMassAddNewItemCommand::CanSendCommand() const
{
	if (Entity.IsSet() == false)
	{
		return false;
	}
	if (ItemDefinitionId.IsValid() == false)
	{
		return false;
	}
	return true;
}

bool FArcMassAddNewItemCommand::Execute(UObject* WorldContext)
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

	if (FArcItemDefinitionData::Find(ItemDefinitionId) == nullptr)
	{
		return false;
	}

	const FArcItemId NewItemId = ArcMassItems::AddItem(
		Manager, Entity, ResolvedStoreType, FArcItemSpec::NewItem(ItemDefinitionId, 1, 1));
	return NewItemId.IsValid();
}
