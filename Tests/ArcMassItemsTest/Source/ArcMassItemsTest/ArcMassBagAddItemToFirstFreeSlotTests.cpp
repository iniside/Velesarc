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
#include "Items/ArcItemData.h"
#include "Bags/ArcBagDefinition.h"
#include "Bags/ArcBagStoreFragment.h"
#include "Bags/ArcBagOperations.h"

#include "ArcMassItemTestsSettings.h"
#include "ArcMassItemTestHelpers.h"

// ---------------------------------------------------------------------------
// ArcMassBagAddItemToFirstFreeSlot test class
// Tests AddItemToFirstFreeSlot: empty-bag success path, all-slots-full rollback
// contract, stackable-already-bagged merge (double-place guard), and store-level
// reject via CanNotStackUnique.
// ---------------------------------------------------------------------------

TEST_CLASS(ArcMassBagAddItemToFirstFreeSlot, "ArcMassItems.BagAddItemToFirstFreeSlot")
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
	// Entity factory — item store + bag overlay (mirrors FindFirstFreeSlot tests)
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
	// Test 1: Empty bag — item placed, returns valid id, item is in a bag slot,
	//         and the store contains the item.
	// -----------------------------------------------------------------------

	TEST_METHOD(EmptyBag_AddItem_ReturnsValidId_ItemIsInBag)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(3);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		const FArcItemId ResultId = ArcMassItems::Bags::AddItemToFirstFreeSlot(
			*EntityManager, Entity, Spec);

		ASSERT_THAT(IsTrue(ResultId.IsValid()));
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::IsItemInAnyBag(*EntityManager, Entity, ResultId)));
		ASSERT_THAT(AreEqual(
			1,
			ArcMassItems::CountItemsByDefinition(
				*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
				MassSettings->SimpleBaseItem)));
	}

	// -----------------------------------------------------------------------
	// Test 2: All slots full — returns InvalidId, store item count unchanged
	//         (exact-rollback contract: no orphan item left in the store).
	// -----------------------------------------------------------------------

	TEST_METHOD(AllSlotsFull_AddItem_ReturnsInvalidId_RollbackVerified)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(2);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		// Fill both slots.
		FArcItemSpec SpecA = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemA = ArcMassItems::AddItem(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecA);
		FArcItemSpec SpecB = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemB = ArcMassItems::AddItem(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecB);
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ItemA)));
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 1, ItemB)));

		const int32 CountBefore = ArcMassItems::CountItemsByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->SimpleBaseItem);

		// Try to add another — no slot is free; the call must roll back and return InvalidId.
		FArcItemSpec SpecC = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		const FArcItemId ResultId = ArcMassItems::Bags::AddItemToFirstFreeSlot(
			*EntityManager, Entity, SpecC);

		ASSERT_THAT(IsFalse(ResultId.IsValid()));

		const int32 CountAfter = ArcMassItems::CountItemsByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->SimpleBaseItem);
		ASSERT_THAT(AreEqual(CountBefore, CountAfter));
	}

	// -----------------------------------------------------------------------
	// Test 3: Stackable item already in a bag slot — call with same definition
	//         (amount within MaxStacks) returns the EXISTING id (double-place
	//         guard), the same single slot is used, and the stack count increases.
	// -----------------------------------------------------------------------

	TEST_METHOD(StackableItemAlreadyBagged_MergesIntoExistingSlot_NoNewSlotUsed)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(3);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		// Add one stack of the stackable item and place it in slot 0.
		FArcItemSpec InitialSpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);
		FArcItemId ExistingItemId = ArcMassItems::AddItem(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), InitialSpec);
		ASSERT_THAT(IsTrue(ExistingItemId.IsValid()));
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ExistingItemId)));

		const FArcItemData* PreData = ArcMassItems::FindItemByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->ItemWithStacks);
		ASSERT_THAT(IsTrue(PreData != nullptr));
		const int32 StacksBefore = static_cast<int32>(PreData->GetStacks());

		// Add another stack of the same definition — AddItem merges into the existing item
		// (NewId == ExistingId), IsItemInAnyBag returns true, double-place guard fires.
		FArcItemSpec MergeSpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);
		const FArcItemId MergedId = ArcMassItems::Bags::AddItemToFirstFreeSlot(
			*EntityManager, Entity, MergeSpec);

		// Returns the same id that was already bagged.
		ASSERT_THAT(IsTrue(MergedId == ExistingItemId));
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::IsItemInAnyBag(*EntityManager, Entity, MergedId)));

		// Slot 0 still holds the original item; slot 1 is untouched (no second slot used).
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 0) == ExistingItemId));
		ASSERT_THAT(IsFalse(ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 1).IsValid()));

		// Stack count increased by 1 (the merge went through).
		const FArcItemData* PostData = ArcMassItems::FindItemByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->ItemWithStacks);
		ASSERT_THAT(IsTrue(PostData != nullptr));
		ASSERT_THAT(AreEqual(StacksBefore + 1, static_cast<int32>(PostData->GetStacks())));
	}

	// -----------------------------------------------------------------------
	// Test 4: CanNotStackUnique — AddItem itself rejects the second add
	//         (store-level reject at step 3), returns InvalidId, and the
	//         original item remains in the store unchanged.
	// -----------------------------------------------------------------------

	TEST_METHOD(CanNotStackUnique_StoreReject_ReturnsInvalidId_OriginalItemPreserved)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(3);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		// Add the unique item directly — first instance must succeed.
		FArcItemSpec UniqueSpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacksCannotStack, 1, 1);
		FArcItemId UniqueItemId = ArcMassItems::AddItem(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), UniqueSpec);
		ASSERT_THAT(IsTrue(UniqueItemId.IsValid()));

		// Attempt to add a second instance via AddItemToFirstFreeSlot.
		// AddItem returns InvalidId (CanNotStackUnique) → function returns immediately.
		FArcItemSpec DuplicateSpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacksCannotStack, 1, 1);
		const FArcItemId ResultId = ArcMassItems::Bags::AddItemToFirstFreeSlot(
			*EntityManager, Entity, DuplicateSpec);

		ASSERT_THAT(IsFalse(ResultId.IsValid()));

		// The original item remains — count must still be 1.
		ASSERT_THAT(AreEqual(
			1,
			ArcMassItems::CountItemsByDefinition(
				*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
				MassSettings->ItemWithStacksCannotStack)));
	}
};
