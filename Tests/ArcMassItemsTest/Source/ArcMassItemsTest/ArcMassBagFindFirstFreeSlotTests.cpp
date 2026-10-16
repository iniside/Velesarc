// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityFragments.h"
#include "StructUtils/InstancedStruct.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"
#include "Items/ArcItemSpec.h"
#include "Bags/ArcBagDefinition.h"
#include "Bags/ArcBagStoreFragment.h"
#include "Bags/ArcBagOperations.h"

#include "ArcMassItemTestsSettings.h"
#include "ArcMassItemTestHelpers.h"
#include "ArcMassBagOverlayTests.h" // FArcBagTestRejectCondition

// ---------------------------------------------------------------------------
// ArcMassBagFindFirstFreeSlot test class
// Tests FindFirstFreeSlotForItem: resident-item query, slot ordering, full-bag
// rejection, invalid-item guard, and accepting-bag skip logic.
// ---------------------------------------------------------------------------

TEST_CLASS(ArcMassBagFindFirstFreeSlot, "ArcMassItems.BagFindFirstFreeSlot")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	// -----------------------------------------------------------------------
	// Per-test setup
	// -----------------------------------------------------------------------

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	// -----------------------------------------------------------------------
	// Entity factory — item store + bag overlay
	// -----------------------------------------------------------------------

	FMassEntityHandle CreateBagEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
		FArcMassBagStoreFragment& BagFrag = EntityManager->AddSparseElementToEntity<FArcMassBagStoreFragment>(NewEntity);
		BagFrag.TargetStoreType = FArcMassItemStoreFragment::StaticStruct();
		return NewEntity;
	}

	// -----------------------------------------------------------------------
	// Bag definition factory — no tag filters, no custom condition
	// -----------------------------------------------------------------------

	UArcBagDefinition* MakeOpenBagDef(int32 SlotCount)
	{
		UArcBagDefinition* Def = NewObject<UArcBagDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Def->SlotCount = SlotCount;
		return Def;
	}

	// -----------------------------------------------------------------------
	// Test 1: Empty bag → returns true, OutBag == the bag's id, OutSlot == 0.
	// -----------------------------------------------------------------------

	TEST_METHOD(EmptyBag_ReturnsTrueAndSlotZero)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(3);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ItemId.IsValid()));

		FArcBagId OutBag;
		int32 OutSlot = -1;
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::FindFirstFreeSlotForItem(*EntityManager, Entity, ItemId, OutBag, OutSlot)));
		ASSERT_THAT(IsTrue(OutBag == BagId));
		ASSERT_THAT(AreEqual(0, OutSlot));
	}

	// -----------------------------------------------------------------------
	// Test 2: Slot 0 occupied → returns true, OutSlot == 1.
	// -----------------------------------------------------------------------

	TEST_METHOD(SlotZeroOccupied_ReturnsSlotOne)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(3);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		// Place ItemA in slot 0.
		FArcItemSpec SpecA = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemA = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecA);
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ItemA)));

		// ItemB is resident but not yet placed — query should skip slot 0.
		FArcItemSpec SpecB = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemB = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecB);

		FArcBagId OutBag;
		int32 OutSlot = -1;
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::FindFirstFreeSlotForItem(*EntityManager, Entity, ItemB, OutBag, OutSlot)));
		ASSERT_THAT(IsTrue(OutBag == BagId));
		ASSERT_THAT(AreEqual(1, OutSlot));
	}

	// -----------------------------------------------------------------------
	// Test 3: All slots occupied in the only bag → returns false.
	// -----------------------------------------------------------------------

	TEST_METHOD(FullyOccupiedBag_ReturnsFalse)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(2);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		FArcItemSpec SpecA = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemA = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecA);
		FArcItemSpec SpecB = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemB = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecB);

		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ItemA)));
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 1, ItemB)));

		// ItemC is resident but has nowhere to go — both slots are full.
		FArcItemSpec SpecC = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemC = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecC);

		FArcBagId OutBag;
		int32 OutSlot = -1;
		ASSERT_THAT(IsFalse(ArcMassItems::Bags::FindFirstFreeSlotForItem(*EntityManager, Entity, ItemC, OutBag, OutSlot)));
	}

	// -----------------------------------------------------------------------
	// Test 4: Invalid FArcItemId → returns false (not resident guard).
	// -----------------------------------------------------------------------

	TEST_METHOD(InvalidItemId_ReturnsFalse)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(4);
		ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		FArcBagId OutBag;
		int32 OutSlot = -1;
		ASSERT_THAT(IsFalse(ArcMassItems::Bags::FindFirstFreeSlotForItem(*EntityManager, Entity, FArcItemId(), OutBag, OutSlot)));
	}

	// -----------------------------------------------------------------------
	// Test 5: First bag rejects the item (FArcBagTestRejectCondition); second
	//         open bag is returned instead (OutBag == open bag, OutSlot == 0).
	// -----------------------------------------------------------------------

	TEST_METHOD(RejectingBagSkipped_ReturnsSecondAcceptingBag)
	{
		FMassEntityHandle Entity = CreateBagEntity();

		// First bag: always-reject condition — no item will ever fit.
		UArcBagDefinition* RejectDef = NewObject<UArcBagDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		RejectDef->SlotCount = 2;
		RejectDef->CustomCondition.InitializeAs(FArcBagTestRejectCondition::StaticStruct());
		FArcBagId RejectBagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, RejectDef);
		ASSERT_THAT(IsTrue(RejectBagId.IsValid()));

		// Second bag: no filters, accepts everything.
		UArcBagDefinition* OpenDef = MakeOpenBagDef(2);
		FArcBagId OpenBagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, OpenDef);
		ASSERT_THAT(IsTrue(OpenBagId.IsValid()));

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ItemId.IsValid()));

		FArcBagId OutBag;
		int32 OutSlot = -1;
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::FindFirstFreeSlotForItem(*EntityManager, Entity, ItemId, OutBag, OutSlot)));
		ASSERT_THAT(IsTrue(OutBag == OpenBagId));
		ASSERT_THAT(AreEqual(0, OutSlot));
	}
};
