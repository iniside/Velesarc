// Copyright Lukasz Baran. All Rights Reserved.

#include "Equipment/ArcMassEquipmentTrait.h"
#include "Equipment/ArcMassEquipmentFragment.h"
#include "Equipment/ArcMassEquipmentSlotPreset.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"
#include "Items/ArcItemSpec.h"
#include "MassEntityTemplateRegistry.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "ArcMass/Spawner/ArcMassSpawnerSubsystem.h"

void UArcMassEquipmentTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FArcMassEquipmentSharedFragment SharedFragment;
	SharedFragment.Preset = SlotPreset;
	SharedFragment.Defaults = DefaultItems;
	BuildContext.AddConstSharedFragment(FConstSharedStruct::Make(SharedFragment));
}

void UArcMassEquipmentTrait::BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const
{
	Out.AddSparseFragment<FArcMassEquipmentFragment>();
}

void UArcMassEquipmentTrait::InitializeEntity(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UWorld& World) const
{
	if (DefaultItems == nullptr || DefaultItems->EquipmentSlots.Num() == 0)
	{
		return;
	}

	const UScriptStruct* StoreType = FArcMassItemStoreFragment::StaticStruct();

	FMassEntityView EntityView(Mgr, Entity);
	FArcMassEquipmentFragment* EquipmentFragment = EntityView.GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();
	if (EquipmentFragment == nullptr)
	{
		return;
	}

	for (const FArcMassEquipmentSlotDefaultItem& DefaultItem : DefaultItems->EquipmentSlots)
	{
		const FPrimaryAssetId& AssetId = DefaultItem.ItemId.AssetId;
		if (!AssetId.IsValid())
		{
			continue;
		}

		const FArcItemSpec Spec = FArcItemSpec::NewItem(AssetId, /*Level*/ 1, /*Amount*/ 1);
		const FArcItemId NewItemId = ArcMassItems::AddItem(Mgr, Entity, StoreType, Spec);
		if (!NewItemId.IsValid())
		{
			continue;
		}

		if (!ArcMassItems::AddItemToSlot(Mgr, Entity, StoreType, NewItemId, DefaultItem.SlotId))
		{
			continue;
		}

		EquipmentFragment->EquippedItems.Add(DefaultItem.SlotId, NewItemId);
	}

}

