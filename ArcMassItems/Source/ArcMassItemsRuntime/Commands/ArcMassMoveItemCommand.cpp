// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcMassMoveItemCommand.h"

#include "Engine/World.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Operations/ArcMassItemOperations.h"

bool FArcMassMoveItemCommand::CanSendCommand() const
{
	if (Entity.IsSet() == false)
	{
		return false;
	}
	if (ToEntity.IsSet() == false)
	{
		return false;
	}
	if (ItemId.IsValid() == false)
	{
		return false;
	}
	return true;
}

bool FArcMassMoveItemCommand::Execute(UObject* WorldContext)
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
	if (ToEntity.IsSet() == false || Manager.IsEntityActive(ToEntity) == false)
	{
		return false;
	}

	if (StoreType == nullptr)
	{
		UE_LOGF(LogArcReplicatedCommand, Warning,"%s: StoreType not set on %ls; falling back to FArcMassItemStoreFragment. "
				 "Use the parameterised constructor to set StoreType explicitly.",
			__FUNCTION__, *GetScriptStruct()->GetName());
	}
	UScriptStruct* ResolvedFromStoreType = StoreType ? StoreType.Get() : FArcMassItemStoreFragment::StaticStruct();

	if (ToStoreType == nullptr)
	{
		UE_LOGF(LogArcReplicatedCommand, Warning,"%s: ToStoreType not set on %ls; falling back to FArcMassItemStoreFragment. "
				 "Use the parameterised constructor to set ToStoreType explicitly.",
			__FUNCTION__, *GetScriptStruct()->GetName());
	}
	UScriptStruct* ResolvedToStoreType = ToStoreType ? ToStoreType.Get() : FArcMassItemStoreFragment::StaticStruct();

	return ArcMassItems::MoveItem(Manager, Entity, ToEntity, ItemId, ResolvedFromStoreType, ResolvedToStoreType);
}
