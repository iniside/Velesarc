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
#include "Items/ArcItemDefinitionData.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Bags/ArcBagDefinition.h"
#include "Bags/ArcBagStoreFragment.h"
#include "Bags/ArcBagOperations.h"
#include "Commands/ArcMassAddNewItemToBagCommand.h"
#include "Core/ArcCoreAssetManager.h"

#include "ArcMassItemTestsSettings.h"
#include "ArcMassItemTestHelpers.h"

// ---------------------------------------------------------------------------
// ArcMassBagAddItemToSlot test class
// Tests AddItemToSlot: specific-slot success path, stackable-already-bagged
// merge (double-place guard, no placement performed), RequiredTags-mismatch
// rejection, occupied-slot rejection, and out-of-range-slot rejection — all
// with the exact-rollback contract (store state unchanged on failure).
// ---------------------------------------------------------------------------

TEST_CLASS(ArcMassBagAddItemToSlot, "ArcMassItems.BagAddItemToSlot")
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
	// Test 1: Empty accepting slot — item placed into exactly the requested
	//         slot, returns a valid id, no other slot in the bag is touched.
	// -----------------------------------------------------------------------

	TEST_METHOD(SpecificEmptyAcceptingSlot_AddItem_ReturnsValidId_OccupiesExactlyThatSlot)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(3);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		const FArcItemId ResultId = ArcMassItems::Bags::AddItemToSlot(
			*EntityManager, Entity, Spec, BagId, 1);

		ASSERT_THAT(IsTrue(ResultId.IsValid()));
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 1) == ResultId));
		ASSERT_THAT(IsFalse(ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 0).IsValid()));
		ASSERT_THAT(IsFalse(ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 2).IsValid()));
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::IsItemInAnyBag(*EntityManager, Entity, ResultId)));
	}

	// -----------------------------------------------------------------------
	// Test 2: Stackable item already in a bag slot — a spec that merges
	//         wholly into it returns the EXISTING id (double-place guard)
	//         and the requested (empty) target slot is left untouched.
	// -----------------------------------------------------------------------

	TEST_METHOD(StackableItemAlreadyBagged_MergesIntoExisting_TargetSlotNotUsed)
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

		// Merge another stack of the same definition, targeting the empty slot 1 —
		// the double-place guard should short-circuit before any placement into slot 1.
		FArcItemSpec MergeSpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);
		const FArcItemId MergedId = ArcMassItems::Bags::AddItemToSlot(
			*EntityManager, Entity, MergeSpec, BagId, 1);

		ASSERT_THAT(IsTrue(MergedId == ExistingItemId));
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::IsItemInAnyBag(*EntityManager, Entity, MergedId)));

		// Slot 0 still holds the original item; slot 1 (the requested target) is untouched.
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 0) == ExistingItemId));
		ASSERT_THAT(IsFalse(ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 1).IsValid()));

		// Stack count increased by 1 (the merge went through), no second item created.
		const FArcItemData* PostData = ArcMassItems::FindItemByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->ItemWithStacks);
		ASSERT_THAT(IsTrue(PostData != nullptr));
		ASSERT_THAT(AreEqual(StacksBefore + 1, static_cast<int32>(PostData->GetStacks())));
	}

	// -----------------------------------------------------------------------
	// Test 3: Non-accepting bag (RequiredTags mismatch) — a plain item with
	//         no matching tags is rejected; the store's stack state is
	//         identical to the pre-call state (exact rollback).
	// -----------------------------------------------------------------------

	TEST_METHOD(RequiredTagsMismatch_AddItem_Fails_StoreRolledBack)
	{
		// Load the tagged fixture to discover its ItemTags.
		const FArcItemDefinitionData* TaggedDef = FArcItemDefinitionData::Find(MassSettings->ItemWithDefinitionTags);
		if (!TaggedDef)
		{
			return; // fixture not configured — skip
		}
		const FArcItemFragment_Tags* TagsFrag = TaggedDef->FindFragment<FArcItemFragment_Tags>();
		if (!TagsFrag || TagsFrag->ItemTags.IsEmpty())
		{
			return; // no tags to test against — skip
		}

		FMassEntityHandle Entity = CreateBagEntity();

		UArcBagDefinition* BagDef = NewObject<UArcBagDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		BagDef->SlotCount = 2;
		BagDef->RequiredTags = TagsFrag->ItemTags; // accepts only items that carry ALL these tags

		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, BagDef);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		const int32 CountBefore = ArcMassItems::CountItemsByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->SimpleBaseItem);

		// Plain item has no tags → ItemFitsBagSlot rejects → PlaceItemInSlot fails → rollback.
		FArcItemSpec PlainSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		const FArcItemId ResultId = ArcMassItems::Bags::AddItemToSlot(
			*EntityManager, Entity, PlainSpec, BagId, 0);

		ASSERT_THAT(IsFalse(ResultId.IsValid()));
		ASSERT_THAT(IsFalse(ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 0).IsValid()));

		const int32 CountAfter = ArcMassItems::CountItemsByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->SimpleBaseItem);
		ASSERT_THAT(AreEqual(CountBefore, CountAfter));
	}

	// -----------------------------------------------------------------------
	// Test 4: Occupied target slot — the call fails and the store's item
	//         count for the new item's definition is unchanged (rollback).
	// -----------------------------------------------------------------------

	TEST_METHOD(OccupiedTargetSlot_AddItem_Fails_RollbackVerified)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(2);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		// Occupy slot 0 with an existing item.
		FArcItemSpec SpecA = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemA = ArcMassItems::AddItem(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecA);
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ItemA)));

		const int32 CountBefore = ArcMassItems::CountItemsByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->SimpleBaseItem);

		// Try to add a second item into the already-occupied slot 0 — must roll back.
		FArcItemSpec SpecB = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		const FArcItemId ResultId = ArcMassItems::Bags::AddItemToSlot(
			*EntityManager, Entity, SpecB, BagId, 0);

		ASSERT_THAT(IsFalse(ResultId.IsValid()));
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 0) == ItemA));

		const int32 CountAfter = ArcMassItems::CountItemsByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->SimpleBaseItem);
		ASSERT_THAT(AreEqual(CountBefore, CountAfter));
	}

	// -----------------------------------------------------------------------
	// Test 5: Out-of-range SlotIndex — the call fails and the store's item
	//         count for the new item's definition is unchanged (rollback).
	// -----------------------------------------------------------------------

	TEST_METHOD(OutOfRangeSlotIndex_AddItem_Fails_RollbackVerified)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(2);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		const int32 CountBefore = ArcMassItems::CountItemsByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->SimpleBaseItem);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		const FArcItemId ResultId = ArcMassItems::Bags::AddItemToSlot(
			*EntityManager, Entity, Spec, BagId, 5);

		ASSERT_THAT(IsFalse(ResultId.IsValid()));

		const int32 CountAfter = ArcMassItems::CountItemsByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->SimpleBaseItem);
		ASSERT_THAT(AreEqual(CountBefore, CountAfter));
	}

	// -----------------------------------------------------------------------
	// Test 6: Spec.Amount > 1 — a fresh add lands Amount stacks in the slot,
	//         and a follow-up merge adds its full Amount on top (regression:
	//         debugger "Amount" reported as adding only 1 per click).
	// -----------------------------------------------------------------------

	TEST_METHOD(AmountGreaterThanOne_FreshAddAndMerge_HonorsFullAmount)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(3);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 5);
		const FArcItemId ResultId = ArcMassItems::Bags::AddItemToSlot(
			*EntityManager, Entity, Spec, BagId, 0);
		ASSERT_THAT(IsTrue(ResultId.IsValid()));

		const FArcItemData* FreshData = ArcMassItems::FindItemByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->ItemWithStacks);
		ASSERT_THAT(IsTrue(FreshData != nullptr));
		ASSERT_THAT(AreEqual(5, static_cast<int32>(FreshData->GetStacks())));

		FArcItemSpec MergeSpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 3);
		const FArcItemId MergedId = ArcMassItems::Bags::AddItemToSlot(
			*EntityManager, Entity, MergeSpec, BagId, 1);
		ASSERT_THAT(IsTrue(MergedId == ResultId));

		const FArcItemData* MergedData = ArcMassItems::FindItemByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->ItemWithStacks);
		ASSERT_THAT(IsTrue(MergedData != nullptr));
		ASSERT_THAT(AreEqual(8, static_cast<int32>(MergedData->GetStacks())));
	}

	// -----------------------------------------------------------------------
	// Test 7: Command layer — FArcMassAddNewItemToBagCommand::Execute with
	//         Amount > 1 lands the full amount (first-free-slot mode). Covers
	//         the AssetManager resolve + spec construction inside Execute,
	//         i.e. everything between the debugger UI and the ops layer.
	// -----------------------------------------------------------------------

	TEST_METHOD(CommandExecute_AmountGreaterThanOne_HonorsFullAmount)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(3);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		FArcMassAddNewItemToBagCommand Command(Entity, MassSettings->ItemWithStacks,
			/*Level*/ 1, /*Amount*/ 5, FArcBagId(), INDEX_NONE);
		ASSERT_THAT(IsTrue(Command.CanSendCommand()));

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		const bool bExecuted = Command.Execute(MES);
		ASSERT_THAT(IsTrue(bExecuted));

		const FArcItemData* Data = ArcMassItems::FindItemByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->ItemWithStacks);
		ASSERT_THAT(IsTrue(Data != nullptr));
		ASSERT_THAT(AreEqual(5, static_cast<int32>(Data->GetStacks())));
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::IsItemInAnyBag(*EntityManager, Entity, Data->GetItemId())));
	}

	// -----------------------------------------------------------------------
	// Test 8: Regression — once the first stack is at MaxStacks, further adds
	//         must top up the partially-filled overflow stack, not spawn a new
	//         item per add (first-by-definition returned the full stack forever).
	// -----------------------------------------------------------------------

	TEST_METHOD(FirstStackAtMax_NextAdds_TopUpPartialOverflowStack)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(5);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		FArcItemSpec ProbeSpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);
		const FArcItemDefinitionData* StacksDef = ProbeSpec.GetItemDefinition();
		ASSERT_THAT(IsTrue(StacksDef != nullptr));
		const int32 MaxStacks = ArcMassItems::Bags::GetMaxStacksForDef(StacksDef);
		ASSERT_THAT(IsTrue(MaxStacks > 1, TEXT("ItemWithStacks must have MaxStacks > 1")));

		// Fill the first stack exactly to max.
		FArcItemSpec FillSpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, MaxStacks);
		const FArcItemId FullId = ArcMassItems::Bags::AddItemToFirstFreeSlot(*EntityManager, Entity, FillSpec);
		ASSERT_THAT(IsTrue(FullId.IsValid()));

		// Overflow by 1 — creates the partial second stack.
		FArcItemSpec OverflowSpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);
		const FArcItemId PartialId = ArcMassItems::Bags::AddItemToFirstFreeSlot(*EntityManager, Entity, OverflowSpec);
		ASSERT_THAT(IsTrue(PartialId.IsValid()));
		ASSERT_THAT(IsTrue(PartialId != FullId));

		// Next add must merge into the partial stack — same id, +1 stacks, still 2 items total.
		FArcItemSpec TopUpSpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);
		const FArcItemId TopUpId = ArcMassItems::Bags::AddItemToFirstFreeSlot(*EntityManager, Entity, TopUpSpec);
		ASSERT_THAT(IsTrue(TopUpId == PartialId));

		const FArcItemData* PartialData = ArcMassItems::FindItemById(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), PartialId);
		ASSERT_THAT(IsTrue(PartialData != nullptr));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(PartialData->GetStacks())));

		// Two item ENTRIES total (CountItemsByDefinition sums stacks, not entries).
		const int32 EntryCount = ArcMassItems::FindItemsByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->ItemWithStacks).Num();
		ASSERT_THAT(AreEqual(2, EntryCount));

		// The full stack is untouched.
		const FArcItemData* FullData = ArcMassItems::FindItemById(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), FullId);
		ASSERT_THAT(IsTrue(FullData != nullptr));
		ASSERT_THAT(AreEqual(MaxStacks, static_cast<int32>(FullData->GetStacks())));
	}
};
