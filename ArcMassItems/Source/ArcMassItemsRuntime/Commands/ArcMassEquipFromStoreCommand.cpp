// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcMassEquipFromStoreCommand.h"

#include "Engine/World.h"
#include "Equipment/ArcMassEquipmentFragment.h"
#include "Equipment/ArcMassEquipmentUtils.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "MassSignalSubsystem.h"
#include "Signals/ArcMassItemSignals.h"

bool FArcMassEquipFromStoreCommand::CanSendCommand() const
{
	if (Entity.IsSet() == false)
	{
		return false;
	}
	if (ItemId.IsValid() == false)
	{
		return false;
	}
	if (ItemSlotId.IsValid() == false)
	{
		return false;
	}
	if (EquipmentSlotId.IsValid() == false)
	{
		return false;
	}
	return true;
}

bool FArcMassEquipFromStoreCommand::Execute(UObject* WorldContext)
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
		UE_LOGF(LogArcReplicatedCommand, Warning,
			"%s: StoreType not set on %ls; falling back to FArcMassItemStoreFragment. "
			"Use the parameterised constructor to set StoreType explicitly.",
			__FUNCTION__, *GetScriptStruct()->GetName());
	}
	UScriptStruct* ResolvedStoreType = StoreType ? StoreType.Get() : FArcMassItemStoreFragment::StaticStruct();

	if (Arc::MassEquipment::CanEquip(Manager, Entity, ResolvedStoreType, ItemId, EquipmentSlotId) == false)
	{
		return false;
	}

	const bool bAlreadySlotted = ArcMassItems::IsOnAnySlot(Manager, Entity, ResolvedStoreType, ItemId);
	const bool bSlotChangeOk = bAlreadySlotted
		? ArcMassItems::ChangeItemSlot(Manager, Entity, ResolvedStoreType, ItemId, ItemSlotId)
		: ArcMassItems::AddItemToSlot(Manager, Entity, ResolvedStoreType, ItemId, ItemSlotId);
	if (bSlotChangeOk == false)
	{
		return false;
	}

	FMassEntityView View(Manager, Entity);
	FArcMassEquipmentFragment* EquipFrag = View.GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();
	if (EquipFrag == nullptr)
	{
		return false;
	}

	// Equipping over an occupied equipment slot unequips the previous item first
	// (mirrors FArcMassUnequipCommand) instead of silently overwriting the entry.
	if (const FArcItemId* OccupyingId = EquipFrag->EquippedItems.Find(EquipmentSlotId))
	{
		if (OccupyingId->IsValid() && *OccupyingId != ItemId)
		{
			const FArcItemId PreviousId = *OccupyingId;
			ArcMassItems::RemoveItemFromSlot(Manager, Entity, ResolvedStoreType, PreviousId);
			EquipFrag->EquippedItems.Remove(EquipmentSlotId);
		}
	}

	EquipFrag->EquippedItems.Add(EquipmentSlotId, ItemId);

	if (UMassSignalSubsystem* SignalSS = ResolvedWorld->GetSubsystem<UMassSignalSubsystem>())
	{
		SignalSS->SignalEntity(UE::ArcMassItems::Signals::ItemEquipped, Entity);
	}

	return true;
}
