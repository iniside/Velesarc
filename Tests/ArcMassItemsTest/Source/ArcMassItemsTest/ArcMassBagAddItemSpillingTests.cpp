// Copyright Lukasz Baran. All Rights Reserved.

/**
 * Tests for ArcMassItems::Bags::AddItemSpilling (ArcMassItemsRuntime).
 *
 * COVERED:
 *   1. Empty bag, Amount < MaxStacks — placed == Amount; one slot occupied.
 *   2. Amount spanning multiple slots — placed == Amount; ceil(Amount/MaxStacks) slots filled,
 *      last slot holds the remainder.
 *   3. Existing partial bagged stack — partial topped up to MaxStacks FIRST, then free slots
 *      filled; returned placed == total Amount.
 *   4. Capacity-limited bag — placed == total bag capacity (< Amount); excess left in source.
 *   5. Entity with NO bag overlay — returns 0.
 *
 * All tests use the real asset-backed defs from UArcMassItemTestsSettings (transient defs crash
 * InitializeMass). Fixtures mirror ArcMassBagAddItemToFirstFreeSlotTests.cpp.
 */

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Bags/ArcBagDefinition.h"
#include "Bags/ArcBagStoreFragment.h"
#include "Bags/ArcBagOperations.h"

#include "ArcMassItemTestsSettings.h"
#include "ArcMassItemTestHelpers.h"

#include "Engine/AssetManager.h"
#include "Mass/EntityView.h"

// ---------------------------------------------------------------------------
// File-scoped helpers
// ---------------------------------------------------------------------------

namespace ArcBagSpillingTestHelpers
{
	/** Create the entity manager + init subsystems. */
	void InitSpawner(FActorTestSpawner& Spawner, FMassEntityManager*& OutEM,
		const UArcMassItemTestsSettings*& OutSettings)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		OutEM = &MES->GetMutableEntityManager();
		OutSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	/** Entity with sparse FArcMassItemStoreFragment + sparse FArcMassBagStoreFragment. */
	FMassEntityHandle CreateBagEntity(FMassEntityManager& EM)
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EM.CreateEntity(Fragments);
		EM.AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
		FArcMassBagStoreFragment& BagFrag = EM.AddSparseElementToEntity<FArcMassBagStoreFragment>(NewEntity);
		BagFrag.TargetStoreType = FArcMassItemStoreFragment::StaticStruct();
		return NewEntity;
	}

	/** Entity with sparse FArcMassItemStoreFragment only — no bag overlay. */
	FMassEntityHandle CreateNoBagEntity(FMassEntityManager& EM)
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EM.CreateEntity(Fragments);
		EM.AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
		return NewEntity;
	}

	/** Transient bag definition — no tag filters, open to all items. */
	UArcBagDefinition* MakeOpenBagDef(int32 SlotCount)
	{
		UArcBagDefinition* Def = NewObject<UArcBagDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Def->SlotCount = SlotCount;
		return Def;
	}

	const FArcItemDefinitionData* ResolveStacksDef(const UArcMassItemTestsSettings* Settings)
	{
		const FPrimaryAssetId DefId = Settings->ItemWithStacks;
		return FArcItemDefinitionData::Find(DefId);
	}

	/** Counts item ENTRIES (not stacks) of a given definition in the entity's default item store. */
	int32 CountEntriesByDef(FMassEntityManager& EM, FMassEntityHandle Entity, const FPrimaryAssetId& DefId)
	{
		int32 Count = 0;
		const FArcMassItemStoreFragment* Store =
			FMassEntityView(EM, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		if (Store)
		{
			for (const FArcMassReplicatedItem& Item : Store->ReplicatedItems.Items)
			{
				const FArcItemData* Data = Item.ToItem();
				if (Data && Data->GetItemDefinitionId() == DefId)
				{
					++Count;
				}
			}
		}
		return Count;
	}
} // namespace ArcBagSpillingTestHelpers

// ---------------------------------------------------------------------------
// TEST CLASS
// ---------------------------------------------------------------------------

TEST_CLASS(ArcMassBagAddItemSpilling, "ArcMassItems.Bag.AddItemSpilling")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		ArcBagSpillingTestHelpers::InitSpawner(Spawner, EntityManager, MassSettings);
	}

	// =========================================================================
	// Test 1 — Empty bag, Amount < MaxStacks
	//
	// AddItemSpilling on an empty bag with Amount < MaxStacks must return Amount,
	// occupy exactly one slot, and store the correct stacks.
	// =========================================================================
	TEST_METHOD(EmptyBag_AmountLessThanMaxStacks_PlacesAll)
	{
		using namespace ArcBagSpillingTestHelpers;

		FMassEntityHandle Entity = CreateBagEntity(*EntityManager);
		UArcBagDefinition* BagDef = MakeOpenBagDef(3);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, BagDef);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		const FArcItemDefinitionData* StacksDef = ResolveStacksDef(MassSettings);
		const int32 MaxStacks = ArcMassItems::Bags::GetMaxStacksForDef(StacksDef);
		ASSERT_THAT(IsTrue(MaxStacks > 1, TEXT("ItemWithStacks must have MaxStacks > 1 for this test to be meaningful")));

		const int32 Amount = FMath::Max(1, MaxStacks / 2); // Amount < MaxStacks
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, Amount);

		const int32 Placed = ArcMassItems::Bags::AddItemSpilling(*EntityManager, Entity, Spec);

		ASSERT_THAT(AreEqual(Amount, Placed,
			TEXT("AddItemSpilling must return the full Amount when bag has enough room")));

		// One item placed in the store and it is bagged.
		ASSERT_THAT(AreEqual(1, CountEntriesByDef(
			*EntityManager, Entity, MassSettings->ItemWithStacks),
			TEXT("Exactly one item entry must exist in the store")));

		FArcItemId Slot0Item = ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 0);
		ASSERT_THAT(IsTrue(Slot0Item.IsValid(), TEXT("Slot 0 must hold the placed item")));

		// Slot 1 is still empty — only one slot was consumed.
		FArcItemId Slot1Item = ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 1);
		ASSERT_THAT(IsFalse(Slot1Item.IsValid(), TEXT("Slot 1 must remain empty")));

		// Verify stacks are exactly Amount.
		const FArcItemData* Data = ArcMassItems::FindItemByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->ItemWithStacks);
		ASSERT_THAT(IsTrue(Data != nullptr, TEXT("FindItemByDefinition must find the placed item")));
		ASSERT_THAT(AreEqual(Amount, static_cast<int32>(Data->GetStacks()),
			TEXT("Item stacks must equal Amount")));
	}

	// =========================================================================
	// Test 2 — Amount spanning multiple slots
	//
	// Amount = MaxStacks + 1 with 3 available slots: two slots must be filled
	// (slot 0 holds MaxStacks, slot 1 holds the remainder 1).
	// Total placed must equal Amount.
	// =========================================================================
	TEST_METHOD(MultiSlotSpill_PlacesAll_SlotsFilledCorrectly)
	{
		using namespace ArcBagSpillingTestHelpers;

		FMassEntityHandle Entity = CreateBagEntity(*EntityManager);
		UArcBagDefinition* BagDef = MakeOpenBagDef(3); // 3 slots, only 2 needed
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, BagDef);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		const FArcItemDefinitionData* StacksDef = ResolveStacksDef(MassSettings);
		const int32 MaxStacks = ArcMassItems::Bags::GetMaxStacksForDef(StacksDef);
		ASSERT_THAT(IsTrue(MaxStacks > 1, TEXT("ItemWithStacks must have MaxStacks > 1")));

		// Amount = MaxStacks + 1 → needs exactly 2 slots (one full + one with 1 stack).
		const int32 Amount = MaxStacks + 1;
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, Amount);

		const int32 Placed = ArcMassItems::Bags::AddItemSpilling(*EntityManager, Entity, Spec);

		ASSERT_THAT(AreEqual(Amount, Placed,
			TEXT("AddItemSpilling must return the full Amount when capacity is sufficient")));

		// Exactly 2 items in the store (one per slot).
		ASSERT_THAT(AreEqual(2, CountEntriesByDef(
			*EntityManager, Entity, MassSettings->ItemWithStacks),
			TEXT("Two item entries expected (one per slot)")));

		// Slots 0 and 1 occupied; slot 2 still empty.
		FArcItemId Slot0Item = ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 0);
		FArcItemId Slot1Item = ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 1);
		FArcItemId Slot2Item = ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 2);
		ASSERT_THAT(IsTrue(Slot0Item.IsValid(), TEXT("Slot 0 must be occupied")));
		ASSERT_THAT(IsTrue(Slot1Item.IsValid(), TEXT("Slot 1 must be occupied")));
		ASSERT_THAT(IsFalse(Slot2Item.IsValid(), TEXT("Slot 2 must remain empty — only 2 slots needed")));
	}

	// =========================================================================
	// Test 3 — Existing partial bagged stack topped up first
	//
	// Slot 0 has a partial item (1 stack). Spilling MaxStacks more stacks:
	//   Phase a (top-up): adds (MaxStacks-1) to slot 0 → slot 0 now at MaxStacks.
	//   Phase b (free slots): adds 1 to slot 1.
	// returned placed = MaxStacks; FindItemByDefinition (first item in store = slot 0) must
	// have MaxStacks stacks, verifying the partial was topped up first not bypassed.
	// =========================================================================
	TEST_METHOD(ExistingPartial_TopsUpFirst_ThenFillsFreeSlots)
	{
		using namespace ArcBagSpillingTestHelpers;

		FMassEntityHandle Entity = CreateBagEntity(*EntityManager);
		UArcBagDefinition* BagDef = MakeOpenBagDef(2);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, BagDef);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		const FArcItemDefinitionData* StacksDef = ResolveStacksDef(MassSettings);
		const int32 MaxStacks = ArcMassItems::Bags::GetMaxStacksForDef(StacksDef);
		ASSERT_THAT(IsTrue(MaxStacks > 1, TEXT("ItemWithStacks must have MaxStacks > 1")));

		// Place 1 stack of ItemWithStacks in slot 0 (a partial stack).
		FArcItemSpec InitialSpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);
		FArcItemId ExistingItemId = ArcMassItems::AddItem(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), InitialSpec);
		ASSERT_THAT(IsTrue(ExistingItemId.IsValid()));
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ExistingItemId)));

		// Verify the partial starts at 1 stack.
		{
			const FArcItemData* PreData = ArcMassItems::FindItemByDefinition(
				*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
				MassSettings->ItemWithStacks);
			ASSERT_THAT(IsTrue(PreData != nullptr));
			ASSERT_THAT(AreEqual(1, static_cast<int32>(PreData->GetStacks())));
		}

		// Spill MaxStacks more stacks.
		// Phase a: top up slot 0 from 1 → MaxStacks (adds MaxStacks-1, remaining=1).
		// Phase b: place 1 in slot 1 (remaining=0).
		// Total placed = MaxStacks.
		FArcItemSpec SpillSpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, MaxStacks);
		const int32 Placed = ArcMassItems::Bags::AddItemSpilling(*EntityManager, Entity, SpillSpec);

		ASSERT_THAT(AreEqual(MaxStacks, Placed,
			TEXT("AddItemSpilling must return MaxStacks (top-up + one new slot)")));

		// Slot 0 item must now hold MaxStacks (the partial was topped up first, not bypassed).
		// FindItemByDefinition returns the first matching item in the store — the original slot 0
		// item, which was added before the spill-phase items.
		const FArcItemData* PostData = ArcMassItems::FindItemByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->ItemWithStacks);
		ASSERT_THAT(IsTrue(PostData != nullptr));
		ASSERT_THAT(AreEqual(MaxStacks, static_cast<int32>(PostData->GetStacks()),
			TEXT("Slot 0 item (first in store) must be at MaxStacks — topped up, not bypassed")));

		// 2 items in the store: slot 0 (original, topped up) + slot 1 (new from phase b).
		ASSERT_THAT(AreEqual(2, CountEntriesByDef(
			*EntityManager, Entity, MassSettings->ItemWithStacks),
			TEXT("Two item entries: slot 0 topped up + slot 1 newly filled")));
	}

	// =========================================================================
	// Test 4 — Capacity-limited: Amount > total bag capacity
	//
	// 2 slots, Amount = 3*MaxStacks. Only 2*MaxStacks can land; the rest stays in the source.
	// Returned placed = 2*MaxStacks (total capacity).
	// =========================================================================
	TEST_METHOD(CapacityLimited_ReturnsOnlyPlacedCount)
	{
		using namespace ArcBagSpillingTestHelpers;

		FMassEntityHandle Entity = CreateBagEntity(*EntityManager);
		UArcBagDefinition* BagDef = MakeOpenBagDef(2); // capacity = 2 * MaxStacks
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, BagDef);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		const FArcItemDefinitionData* StacksDef = ResolveStacksDef(MassSettings);
		const int32 MaxStacks = ArcMassItems::Bags::GetMaxStacksForDef(StacksDef);
		ASSERT_THAT(IsTrue(MaxStacks > 1, TEXT("ItemWithStacks must have MaxStacks > 1 — a cold def returning 1 would make this test meaningless")));

		const int32 Capacity = 2 * MaxStacks;
		// Try to spill 3*MaxStacks — only 2*MaxStacks can fit.
		const int32 Requested = 3 * MaxStacks;
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, Requested);

		const int32 Placed = ArcMassItems::Bags::AddItemSpilling(*EntityManager, Entity, Spec);

		ASSERT_THAT(AreEqual(Capacity, Placed,
			TEXT("AddItemSpilling must return exactly the bag capacity when Amount exceeds it")));
		ASSERT_THAT(IsTrue(Placed < Requested,
			TEXT("Placed must be less than Requested — excess must stay in the source")));

		// Both slots occupied.
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 0).IsValid(),
			TEXT("Slot 0 must be occupied")));
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 1).IsValid(),
			TEXT("Slot 1 must be occupied")));

		// 2 distinct items in the store.
		ASSERT_THAT(AreEqual(2, CountEntriesByDef(
			*EntityManager, Entity, MassSettings->ItemWithStacks),
			TEXT("Two item entries expected (one per slot)")));
	}

	// =========================================================================
	// Test 5 — Entity with NO bag overlay returns 0
	//
	// An entity that only has FArcMassItemStoreFragment but no FArcMassBagStoreFragment:
	// AddItemSpilling must return 0 and leave the store untouched.
	// =========================================================================
	TEST_METHOD(NoBagOverlay_ReturnsZero)
	{
		using namespace ArcBagSpillingTestHelpers;

		FMassEntityHandle Entity = CreateNoBagEntity(*EntityManager);
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);

		const int32 Placed = ArcMassItems::Bags::AddItemSpilling(*EntityManager, Entity, Spec);

		ASSERT_THAT(AreEqual(0, Placed, TEXT("No bag overlay: AddItemSpilling must return 0")));

		// The store must remain empty (nothing was added).
		ASSERT_THAT(AreEqual(0, ArcMassItems::CountItemsByDefinition(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->ItemWithStacks),
			TEXT("Store must remain empty when there is no bag overlay")));
	}
};
