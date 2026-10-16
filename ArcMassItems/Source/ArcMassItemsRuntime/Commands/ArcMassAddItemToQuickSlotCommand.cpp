// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcMassAddItemToQuickSlotCommand.h"

#include "ArcMassItemsNativeTags.h"
#include "Engine/World.h"
#include "Commands/ArcMassItemReplicatedCommand.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Operations/ArcMassItemOperations.h"
#include "QuickBar/ArcMassQuickBarConfig.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "QuickBar/ArcMassQuickBarTypes.h"

bool FArcMassAddItemToQuickSlotCommand::CanSendCommand() const
{
	if (Entity.IsSet() == false)
	{
		return false;
	}

	if (ItemId.IsValid() == false)
	{
		return false;
	}

	if (QuickBar.IsValid() == false || QuickSlot.IsValid() == false)
	{
		return false;
	}

	return true;
}

bool FArcMassAddItemToQuickSlotCommand::Execute(UObject* WorldContext)
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
	if (Location.Config == nullptr || Location.Bar == nullptr || Location.Config->StoreType == nullptr)
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "AddItemToQuickSlot: no quick bar config for Bar=%ls", *QuickBar.ToString());
		return false;
	}

	if (Location.Bar->ParentBarId.IsValid())
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "AddItemToQuickSlot: Bar=%ls is a child bar, filled only by its parent item", *QuickBar.ToString());
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
		UE_LOGF(LogArcReplicatedCommand, Warning, "AddItemToQuickSlot: no slot config for Bar=%ls Slot=%ls (Config=%ls)",
			*QuickBar.ToString(), *QuickSlot.ToString(), *GetNameSafe(Location.Config));
		return false;
	}

	// The quick slot is checked before the item moves, so a rejected command leaves the item untouched.
	const FArcItemId Occupant = ArcMassItems::QuickBar::GetAssignedItem(Manager, Entity, QuickBar, QuickSlot);
	if (Occupant.IsValid())
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "AddItemToQuickSlot: Bar=%ls Slot=%ls already holds %ls; rejected %ls",
			*QuickBar.ToString(), *QuickSlot.ToString(), *Occupant.ToString(), *ItemId.ToString());
		return false;
	}

	// Per spec: only fall back to TAG_SlotId_Active when ItemSlotId is unset.
	// DefaultItemSlotId is intentionally NOT consulted here.
	const FGameplayTag ItemSlot = SlotConfig->ItemSlotId.IsValid() ? SlotConfig->ItemSlotId : TAG_SlotId_Active;

	UE_LOGF(LogArcReplicatedCommand, Log, "AddItemToQuickSlot: Bar=%ls Slot=%ls Item=%ls -> ItemSlot=%ls",
		*QuickBar.ToString(), *QuickSlot.ToString(), *ItemId.ToString(), *ItemSlot.ToString());

	// Without an ItemSlotId the Quick Bar items share SlotId.Active and the Quick Bar tracks their activation.
	const bool bSlotted = SlotConfig->ItemSlotId.IsValid()
		? ArcMassItems::AddItemToSlot(Manager, Entity, StoreType, ItemId, ItemSlot)
		: ArcMassItems::AddItemToSharedSlot(Manager, Entity, StoreType, ItemId, ItemSlot);
	if (bSlotted == false)
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "AddItemToQuickSlot: AddItemToSlot failed for Item=%ls ItemSlot=%ls",
			*ItemId.ToString(), *ItemSlot.ToString());
		return false;
	}

	ArcMassItems::QuickBar::AddAndActivateSlot(Manager, Entity, QuickBar, QuickSlot, ItemId);

	return true;
}
