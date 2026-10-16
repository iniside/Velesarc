// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcMassUnequipCommand.h"

#include "Engine/World.h"
#include "Equipment/ArcMassEquipmentFragment.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemId.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "MassSignalSubsystem.h"
#include "Operations/ArcMassItemOperations.h"
#include "Signals/ArcMassItemSignals.h"

bool FArcMassUnequipCommand::CanSendCommand() const
{
	if (Entity.IsSet() == false)
	{
		return false;
	}
	if (EquipmentSlotId.IsValid() == false)
	{
		return false;
	}
	return true;
}

bool FArcMassUnequipCommand::Execute(UObject* WorldContext)
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

	FMassEntityView View(Manager, Entity);
	FArcMassEquipmentFragment* EquipFrag = View.GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();
	if (EquipFrag == nullptr)
	{
		return false;
	}

	const FArcItemId* BoundItemId = EquipFrag->EquippedItems.Find(EquipmentSlotId);
	if (BoundItemId == nullptr || BoundItemId->IsValid() == false)
	{
		return false;
	}

	const FArcItemId ItemIdCopy = *BoundItemId;
	ArcMassItems::RemoveItemFromSlot(Manager, Entity, ResolvedStoreType, ItemIdCopy);
	EquipFrag->EquippedItems.Remove(EquipmentSlotId);

	if (UMassSignalSubsystem* SignalSS = ResolvedWorld->GetSubsystem<UMassSignalSubsystem>())
	{
		SignalSS->SignalEntity(UE::ArcMassItems::Signals::ItemUnequipped, Entity);
	}

	return true;
}
