// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcMassRemoveItemFromQuickSlotCommand.h"

#include "Engine/World.h"
#include "Commands/ArcMassItemReplicatedCommand.h"
#include "Items/ArcItemId.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Operations/ArcMassItemOperations.h"
#include "QuickBar/ArcMassQuickBarConfig.h"
#include "QuickBar/ArcMassQuickBarOperations.h"

bool FArcMassRemoveItemFromQuickSlotCommand::CanSendCommand() const
{
	if (Entity.IsSet() == false)
	{
		return false;
	}

	if (QuickBar.IsValid() == false || QuickSlot.IsValid() == false)
	{
		return false;
	}

	return true;
}

bool FArcMassRemoveItemFromQuickSlotCommand::Execute(UObject* WorldContext)
{
	UWorld* ResolvedWorld = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
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

	const FArcMassQuickBarLocation Location = ArcMassItems::QuickBar::FindBar(Manager, Entity, QuickBar);
	if (Location.Config == nullptr || Location.Config->StoreType == nullptr)
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "RemoveItemFromQuickSlot: no quick bar config for Bar=%ls", *QuickBar.ToString());
		return false;
	}
	if (Location.Bar->ParentBarId.IsValid())
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "RemoveItemFromQuickSlot: Bar=%ls is a child bar, emptied only with its parent item", *QuickBar.ToString());
		return false;
	}
	const UScriptStruct* StoreType = Location.Config->StoreType;

	const FArcItemId AssignedItem = ArcMassItems::QuickBar::GetAssignedItem(Manager, Entity, QuickBar, QuickSlot);
	if (AssignedItem.IsValid() == false)
	{
		return false;
	}

	ArcMassItems::QuickBar::RemoveSlot(Manager, Entity, QuickBar, QuickSlot);
	ArcMassItems::RemoveItemFromSlot(Manager, Entity, StoreType, AssignedItem);

	return true;
}
