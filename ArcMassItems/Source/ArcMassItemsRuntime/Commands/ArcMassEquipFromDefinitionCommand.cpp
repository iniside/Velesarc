// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcMassEquipFromDefinitionCommand.h"

#include "Engine/World.h"
#include "Equipment/ArcMassEquipmentFragment.h"
#include "Equipment/ArcMassEquipmentUtils.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Core/ArcCoreAssetManager.h"

bool FArcMassEquipFromDefinitionCommand::CanSendCommand() const
{
	if (Entity.IsSet() == false)
	{
		return false;
	}
	if (Spec.GetItemDefinitionId().IsValid() == false)
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

bool FArcMassEquipFromDefinitionCommand::Execute(UObject* WorldContext)
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

	if (Arc::MassEquipment::CanEquip(Manager, Entity, Spec.GetItemDefinitionId(), EquipmentSlotId) == false)
	{
		return false;
	}

	const FArcItemId NewItemId = ArcMassItems::AddItem(Manager, Entity, ResolvedStoreType, Spec);
	if (NewItemId.IsValid() == false)
	{
		return false;
	}

	if (ArcMassItems::AddItemToSlot(Manager, Entity, ResolvedStoreType, NewItemId, ItemSlotId) == false)
	{
		return false;
	}

	FMassEntityView View(Manager, Entity);
	FArcMassEquipmentFragment* EquipFrag = View.GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();
	if (EquipFrag == nullptr)
	{
		return false;
	}

	EquipFrag->EquippedItems.Add(EquipmentSlotId, NewItemId);
	return true;
}
