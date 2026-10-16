// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcMassAddNewItemToQuickSlotCommand.h"

#include "ArcMassItemsNativeTags.h"
#include "Engine/World.h"
#include "Commands/ArcMassItemReplicatedCommand.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Operations/ArcMassItemOperations.h"
#include "QuickBar/ArcMassQuickBarConfig.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "QuickBar/ArcMassQuickBarTypes.h"

bool FArcMassAddNewItemToQuickSlotCommand::CanSendCommand() const
{
	if (!Entity.IsValid())
	{
		return false;
	}
	if (!ItemDefinitionId.IsValid())
	{
		return false;
	}
	if (!QuickBar.IsValid() || !QuickSlot.IsValid())
	{
		return false;
	}
	return true;
}

bool FArcMassAddNewItemToQuickSlotCommand::Execute(UObject* WorldContext)
{
	if (!Entity.IsValid())
	{
		return false;
	}
	if (!ItemDefinitionId.IsValid())
	{
		return false;
	}
	if (!QuickBar.IsValid() || !QuickSlot.IsValid())
	{
		return false;
	}

	UWorld* WorldPtr = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	if (WorldPtr == nullptr)
	{
		return false;
	}

	UMassEntitySubsystem* Subsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(WorldPtr);
	if (Subsystem == nullptr)
	{
		return false;
	}

	FMassEntityManager& Manager = Subsystem->GetMutableEntityManager();
	if (!Manager.IsEntityValid(Entity))
	{
		return false;
	}

	if (FArcItemDefinitionData::Find(ItemDefinitionId) == nullptr)
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "AddNewItemToQuickSlot: no item definition %ls", *ItemDefinitionId.ToString());
		return false;
	}

	// Resolve quickbar config + slot up-front so we don't add an item that we'd
	// then be unable to slot.
	const FArcMassQuickBarLocation Location = ArcMassItems::QuickBar::FindBar(Manager, Entity, QuickBar);
	if (Location.Config == nullptr || Location.Bar == nullptr || Location.Config->StoreType == nullptr)
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "AddNewItemToQuickSlot: no quick bar config for Bar=%ls", *QuickBar.ToString());
		return false;
	}

	if (Location.Bar->ParentBarId.IsValid())
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "AddNewItemToQuickSlot: Bar=%ls is a child bar, filled only by its parent item", *QuickBar.ToString());
		return false;
	}

	const UScriptStruct* StoreType = Location.Config->StoreType;
	const FArcMassQuickBarSlot* SlotConfig = Location.Bar->Slots.FindByPredicate(
		[this](const FArcMassQuickBarSlot& Slot)
		{
			return Slot.QuickBarSlotId == QuickSlot;
		});
	if (SlotConfig == nullptr)
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "AddNewItemToQuickSlot: no slot config for Bar=%ls Slot=%ls (Config=%ls)",
			*QuickBar.ToString(), *QuickSlot.ToString(), *GetNameSafe(Location.Config));
		return false;
	}

	// The quick slot is checked before the item is created, so a rejected command adds nothing.
	const FArcItemId Occupant = ArcMassItems::QuickBar::GetAssignedItem(Manager, Entity, QuickBar, QuickSlot);
	if (Occupant.IsValid())
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "AddNewItemToQuickSlot: Bar=%ls Slot=%ls already holds %ls",
			*QuickBar.ToString(), *QuickSlot.ToString(), *Occupant.ToString());
		return false;
	}

	const FGameplayTag ItemSlot = SlotConfig->ItemSlotId.IsValid()
		? SlotConfig->ItemSlotId
		: TAG_SlotId_Active;

	const FArcItemId NewItemId = ArcMassItems::AddItem(
		Manager, Entity, StoreType,
		FArcItemSpec::NewItem(ItemDefinitionId, 1, 1));
	if (!NewItemId.IsValid())
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "AddNewItemToQuickSlot: AddItem failed for %ls", *ItemDefinitionId.ToString());
		return false;
	}

	// Without an ItemSlotId the Quick Bar items share SlotId.Active and the Quick Bar tracks their activation.
	const bool bSlotted = SlotConfig->ItemSlotId.IsValid()
		? ArcMassItems::AddItemToSlot(Manager, Entity, StoreType, NewItemId, ItemSlot)
		: ArcMassItems::AddItemToSharedSlot(Manager, Entity, StoreType, NewItemId, ItemSlot);
	if (!bSlotted)
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "AddNewItemToQuickSlot: slotting %ls into %ls failed", *NewItemId.ToString(), *ItemSlot.ToString());
		return false;
	}

	ArcMassItems::QuickBar::AddAndActivateSlot(
		Manager, Entity, QuickBar, QuickSlot, NewItemId);

	return true;
}
