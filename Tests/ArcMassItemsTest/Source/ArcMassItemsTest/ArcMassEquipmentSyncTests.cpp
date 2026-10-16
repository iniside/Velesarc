// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityFragments.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"
#include "NativeGameplayTags.h"
#include "GameplayTagContainer.h"
#include "Operations/ArcMassItemOperations.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Equipment/ArcMassEquipmentFragment.h"
#include "Equipment/ArcMassEquipmentSlotPreset.h"
#include "Equipment/ArcMassEquipmentTypes.h"
#include "Equipment/ArcMassEquipmentSyncSubsystem.h"
#include "Events/ArcMassItemEventSubsystem.h"
#include "Commands/ArcMassEquipFromStoreCommand.h"
#include "Items/ArcItemSpec.h"
#include "ArcMassItemTestsSettings.h"
#include "ArcMassItemTestHelpers.h"

// ---------------------------------------------------------------------------
// Native test tags
// ---------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EquipSync_EquipSlot, "Test.Equipment.Sync.Slot");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EquipSync_ItemSlotA, "Test.Equipment.Sync.ItemSlotA");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EquipSync_ItemSlotB, "Test.Equipment.Sync.ItemSlotB");

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

TEST_CLASS(ArcMassEquipmentSync, "ArcMassItems.Equipment.Sync")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* Settings = nullptr;

	/** Creates a minimal entity with item store, equipment fragment, and equipment shared fragment. */
	FMassEntityHandle CreateSyncEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);

		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
		EntityManager->AddSparseElementToEntity<FArcMassEquipmentFragment>(NewEntity);

		UArcMassEquipmentSlotPreset* Preset = NewObject<UArcMassEquipmentSlotPreset>();
		FArcMassEquipmentSlot Slot;
		Slot.SlotId = TAG_EquipSync_EquipSlot.GetTag();
		Preset->EquipmentSlots.Add(Slot);

		FArcMassEquipmentSharedFragment SharedFrag;
		SharedFrag.Preset = Preset;
		const FConstSharedStruct& SharedStruct = EntityManager->GetOrCreateConstSharedFragment(SharedFrag);
		EntityManager->AddConstSharedFragmentToEntity(NewEntity, SharedStruct);

		return NewEntity;
	}

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
		Settings = GetDefault<UArcMassItemTestsSettings>();
		Entity = CreateSyncEntity();
	}

	// -----------------------------------------------------------------------
	// Test 1 — RemoveItem evicts the item from EquippedItems via the sync subsystem
	// -----------------------------------------------------------------------
	TEST_METHOD(ItemRemoved_EvictsEquippedItemsEntry)
	{
		ASSERT_THAT(IsNotNull(Spawner.GetWorld().GetSubsystem<UArcMassEquipmentSyncSubsystem>()));
		ASSERT_THAT(IsNotNull(Spawner.GetWorld().GetSubsystem<UArcMassItemEventSubsystem>()));

		FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
		FArcItemId IdA = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(IdA.IsValid()));

		ASSERT_THAT(IsTrue(ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), IdA, TAG_EquipSync_ItemSlotA.GetTag())));

		// Write equip state directly — bypasses the equip command to test the sync subsystem in isolation.
		FArcMassEquipmentFragment* EquipFrag = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();
		ASSERT_THAT(IsNotNull(EquipFrag));
		EquipFrag->EquippedItems.Add(TAG_EquipSync_EquipSlot.GetTag(), IdA);

		ASSERT_THAT(IsTrue(ArcMassItems::RemoveItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), IdA)));

		ArcMassItems::TestHelpers::DrainAttachmentEvents(&Spawner.GetWorld());

		// Re-fetch sparse pointer after drain — subsystem may have mutated the fragment.
		EquipFrag = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();
		ASSERT_THAT(IsNotNull(EquipFrag));
		ASSERT_THAT(AreEqual(0, EquipFrag->EquippedItems.Num()));
	}

	// -----------------------------------------------------------------------
	// Test 2 — equipping to an already-occupied slot unequips the previous item
	// -----------------------------------------------------------------------
	TEST_METHOD(EquipFromStore_OccupiedSlot_UnequipsPrevious)
	{
		// Add first item and equip it to the slot.
		FArcItemSpec SpecA = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
		FArcItemId IdA = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecA);
		ASSERT_THAT(IsTrue(IdA.IsValid()));

		FArcMassEquipFromStoreCommand CmdA(Entity, IdA, TAG_EquipSync_ItemSlotA.GetTag(), TAG_EquipSync_EquipSlot.GetTag());
		ASSERT_THAT(IsTrue(CmdA.Execute(&Spawner.GetWorld())));

		FArcMassEquipmentFragment* EquipFrag = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();
		ASSERT_THAT(IsNotNull(EquipFrag));

		const FArcItemId* EquippedA = EquipFrag->EquippedItems.Find(TAG_EquipSync_EquipSlot.GetTag());
		ASSERT_THAT(IsNotNull(EquippedA));
		ASSERT_THAT(IsTrue(*EquippedA == IdA));
		ASSERT_THAT(IsTrue(ArcMassItems::IsOnAnySlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), IdA)));

		// Add second item and equip it to the same equipment slot (different item-store slot).
		FArcItemSpec SpecB = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
		FArcItemId IdB = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecB);
		ASSERT_THAT(IsTrue(IdB.IsValid()));

		FArcMassEquipFromStoreCommand CmdB(Entity, IdB, TAG_EquipSync_ItemSlotB.GetTag(), TAG_EquipSync_EquipSlot.GetTag());
		ASSERT_THAT(IsTrue(CmdB.Execute(&Spawner.GetWorld())));

		// Re-fetch fragment ptr after the second command mutated the store.
		EquipFrag = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();
		ASSERT_THAT(IsNotNull(EquipFrag));

		// Slot must now hold IdB; IdA must have been evicted.
		ASSERT_THAT(AreEqual(1, EquipFrag->EquippedItems.Num()));

		const FArcItemId* EquippedB = EquipFrag->EquippedItems.Find(TAG_EquipSync_EquipSlot.GetTag());
		ASSERT_THAT(IsNotNull(EquippedB));
		ASSERT_THAT(IsTrue(*EquippedB == IdB));

		ASSERT_THAT(IsFalse(ArcMassItems::IsOnAnySlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), IdA)));
		ASSERT_THAT(IsTrue(ArcMassItems::IsOnAnySlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), IdB)));
	}
};
