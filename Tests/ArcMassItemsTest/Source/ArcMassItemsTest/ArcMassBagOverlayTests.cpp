// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassBagOverlayTests.h"
#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityFragments.h"
#include "StructUtils/InstancedStruct.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Items/ArcItemStackMethod.h"
#include "Bags/ArcBagDefinition.h"
#include "Bags/ArcBagStoreFragment.h"
#include "Bags/ArcBagOperations.h"
#include "Core/ArcCoreAssetManager.h"

#include "ArcMassItemTestsSettings.h"
#include "ArcMassItemTestHelpers.h"

// ---------------------------------------------------------------------------
// ArcMassBagOverlay test class
// ---------------------------------------------------------------------------

TEST_CLASS(ArcMassBagOverlay, "ArcMassItems.BagOverlay")
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
	// Test 1: AddBag returns a valid FArcBagId; the fragment holds one bag
	//         with Slots.Num() == SlotCount.
	// -----------------------------------------------------------------------

	TEST_METHOD(AddBag_ReturnsValidBagId_AndSlotCountMatches)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(4);

		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		FMassEntityView View(*EntityManager, Entity);
		FArcMassBagStoreFragment* BagFrag = View.GetSparseFragmentDataPtr<FArcMassBagStoreFragment>();
		ASSERT_THAT(IsNotNull(BagFrag));
		ASSERT_THAT(AreEqual(1, BagFrag->Bags.Items.Num()));
		ASSERT_THAT(AreEqual(4, BagFrag->Bags.Items[0].Slots.Num()));
	}

	// -----------------------------------------------------------------------
	// Test 2: RemoveBag on empty → true; on occupied slot → false;
	//         after ClearSlot → true.
	// -----------------------------------------------------------------------

	TEST_METHOD(RemoveBag_EmptyBag_Succeeds)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(2);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		ASSERT_THAT(IsTrue(ArcMassItems::Bags::RemoveBag(*EntityManager, Entity, BagId)));
	}

	TEST_METHOD(RemoveBag_OccupiedSlot_Fails_ThenClearSlot_Succeeds)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(2);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ItemId)));

		// Occupied bag cannot be removed.
		ASSERT_THAT(IsFalse(ArcMassItems::Bags::RemoveBag(*EntityManager, Entity, BagId)));

		// Clear the slot first, then removal succeeds.
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::ClearSlot(*EntityManager, Entity, BagId, 0)));
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::RemoveBag(*EntityManager, Entity, BagId)));
	}

	// -----------------------------------------------------------------------
	// Test 3: PlaceItemInSlot edge cases.
	// -----------------------------------------------------------------------

	TEST_METHOD(PlaceItemInSlot_EmptyInRangeSlot_Succeeds_AndSlotHoldsId)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(3);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 1, ItemId)));
		FArcItemId SlotItem = ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 1);
		ASSERT_THAT(IsTrue(SlotItem == ItemId));
	}

	TEST_METHOD(PlaceItemInSlot_OccupiedSlot_Fails)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(3);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		FArcItemSpec SpecA = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemA = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecA);
		FArcItemSpec SpecB = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemB = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecB);

		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ItemA)));
		ASSERT_THAT(IsFalse(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ItemB)));
	}

	TEST_METHOD(PlaceItemInSlot_OutOfRangeIndex_Fails)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(2);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		ASSERT_THAT(IsFalse(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 5, ItemId)));
		ASSERT_THAT(IsFalse(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, -1, ItemId)));
	}

	TEST_METHOD(PlaceItemInSlot_NoneBagId_Fails)
	{
		FMassEntityHandle Entity = CreateBagEntity();

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		ASSERT_THAT(IsFalse(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, FArcBagId(), 0, ItemId)));
	}

	// -----------------------------------------------------------------------
	// Test 4: Acceptance via RequiredTags — matching item accepted, plain
	//         item with no tags rejected.
	// -----------------------------------------------------------------------

	TEST_METHOD(TagAcceptance_RequiredTags_MatchingItemAccepted_PlainItemRejected)
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

		// Item whose definition has the required tags → ItemAggregatedTags filled at init → accepted.
		FArcItemSpec TaggedSpec = FArcItemSpec::NewItem(MassSettings->ItemWithDefinitionTags, 1, 1);
		FArcItemId TaggedItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), TaggedSpec);
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, TaggedItemId)));

		// Plain item with no tags → ItemAggregatedTags empty → rejected.
		FArcItemSpec PlainSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId PlainItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), PlainSpec);
		ASSERT_THAT(IsFalse(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 1, PlainItemId)));
	}

	// -----------------------------------------------------------------------
	// Test 5: Acceptance via CustomCondition — a FArcBagTestRejectCondition
	//         always returns false; an open bag (no condition) always accepts.
	// -----------------------------------------------------------------------

	TEST_METHOD(CustomCondition_AlwaysReject_BlocksPlacement)
	{
		FMassEntityHandle Entity = CreateBagEntity();

		UArcBagDefinition* BagDef = NewObject<UArcBagDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		BagDef->SlotCount = 2;
		BagDef->CustomCondition.InitializeAs(FArcBagTestRejectCondition::StaticStruct());

		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, BagDef);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		ASSERT_THAT(IsFalse(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ItemId)));
	}

	TEST_METHOD(CustomCondition_NoCondition_AcceptsItem)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* BagDef = MakeOpenBagDef(2); // no CustomCondition

		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, BagDef);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ItemId)));
	}

	// -----------------------------------------------------------------------
	// Test 6a: MoveItemBetweenSlots — empty target: straight move.
	// -----------------------------------------------------------------------

	TEST_METHOD(MoveItemBetweenSlots_ToEmptySlot_Moves)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(4);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ItemId)));

		ASSERT_THAT(IsTrue(ArcMassItems::Bags::MoveItemBetweenSlots(*EntityManager, Entity, BagId, 0, BagId, 3)));

		FArcItemId SlotAfterSrc = ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 0);
		FArcItemId SlotAfterDst = ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 3);
		ASSERT_THAT(IsFalse(SlotAfterSrc.IsValid()));     // source cleared
		ASSERT_THAT(IsTrue(SlotAfterDst == ItemId));      // destination holds item
	}

	// -----------------------------------------------------------------------
	// Test 6b: MoveItemBetweenSlots — occupied target, different definitions:
	//          items swap positions.
	// -----------------------------------------------------------------------

	TEST_METHOD(MoveItemBetweenSlots_DifferentDefinitions_Swaps)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(4);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		FArcItemSpec SpecA = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemA = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecA);

		FArcItemSpec SpecB = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);
		FArcItemId ItemB = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecB);

		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ItemA)));
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 1, ItemB)));

		ASSERT_THAT(IsTrue(ArcMassItems::Bags::MoveItemBetweenSlots(*EntityManager, Entity, BagId, 0, BagId, 1)));

		FArcItemId AfterSlot0 = ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 0);
		FArcItemId AfterSlot1 = ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 1);
		ASSERT_THAT(IsTrue(AfterSlot0 == ItemB)); // B moved to 0
		ASSERT_THAT(IsTrue(AfterSlot1 == ItemA)); // A moved to 1
	}

	// -----------------------------------------------------------------------
	// Test 6c: MoveItemBetweenSlots — same definition, occupied target.
	//
	//   * Stackable fixture (MaxStacks > 1): stacks merge, source fully consumed
	//     → its slot becomes None.
	//   * Non-stackable fixture (MaxStacks <= 1): the merge branch is gated off
	//     (bStackable == false), so same-def items SWAP and NEITHER is destroyed.
	//
	// NOTE: the live `ItemWithStacks` fixture (DefaultArcTests.ini → ArcItem:52EAA861…
	// / ID_MassItem_Stacking) ships with MaxStacks.Value == 0, despite its settings
	// doc comment claiming "10+". So on the CURRENT test data this test exercises the
	// non-stackable swap-gate branch. The positive merge branch needs a fixture with
	// MaxStacks > 1 (a .uasset edit → Lore) before it can be unit-tested end-to-end;
	// the merge POLICY itself is asserted here the moment such a fixture exists.
	// -----------------------------------------------------------------------

	// Reads the live MaxStacks of an item by id (resolves its definition's stack method).
	// Returns 1 (non-stacking default) if anything fails to resolve.
	int32 ReadMaxStacks(FMassEntityHandle Entity, FArcItemId ItemId)
	{
		FMassEntityView View(*EntityManager, Entity);
		const FArcMassItemStoreFragment* Store = View.GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		if (!Store)
		{
			return 1;
		}
		const FArcMassReplicatedItem* Rep = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		if (!Rep || !Rep->ToItem())
		{
			return 1;
		}
		const FArcItemData* Data = Rep->ToItem();
		const FArcItemDefinitionData* StackDef = Data->GetItemDefinition();
		if (!StackDef)
		{
			return 1;
		}
		const FArcItemStackMethod* Method = StackDef->GetStackMethod<FArcItemStackMethod>();
		if (!Method)
		{
			return 1;
		}
		return Method->GetMaxStacksMass(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), *Data);
	}

	uint16 ReadStacks(FMassEntityHandle Entity, FArcItemId ItemId)
	{
		FMassEntityView View(*EntityManager, Entity);
		const FArcMassItemStoreFragment* Store = View.GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		if (!Store)
		{
			return 0;
		}
		const FArcMassReplicatedItem* Rep = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		if (!Rep || !Rep->ToItem())
		{
			return 0;
		}
		return Rep->ToItem()->GetStacks();
	}

	TEST_METHOD(MoveItemBetweenSlots_SameDef_MergesWhenStackable_ElseSwaps)
	{
		const UScriptStruct* StoreType = FArcMassItemStoreFragment::StaticStruct();
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(4);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		// item1: first add of the def (no existing → not merged), 1 stack. Learn MaxStacks.
		FArcItemSpec Spec1 = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);
		FArcItemId Item1 = ArcMassItems::AddItem(*EntityManager, Entity, StoreType, Spec1);
		ASSERT_THAT(IsTrue(Item1.IsValid()));

		const int32 Max = ReadMaxStacks(Entity, Item1);

		if (Max > 1)
		{
			// --- Stackable fixture: exercise the real MERGE path. ---
			// item1 is the target. Fill it to MAX so the next add overflows into a NEW item
			// (AddItem auto-merges same-def stacks; only overflow past MAX yields a distinct id).
			ASSERT_THAT(IsTrue(ArcMassItems::ModifyStacks(*EntityManager, Entity, StoreType, Item1, Max - 1)));
			ASSERT_THAT(AreEqual(static_cast<uint16>(Max), ReadStacks(Entity, Item1)));

			// item2 (source): 2-stack add overflows past item1's MAX → a distinct item.
			FArcItemSpec Spec2 = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 2);
			FArcItemId Item2 = ArcMassItems::AddItem(*EntityManager, Entity, StoreType, Spec2);
			ASSERT_THAT(IsTrue(Item2.IsValid()));
			ASSERT_THAT(IsTrue(Item2 != Item1));
			ASSERT_THAT(AreEqual(static_cast<uint16>(2), ReadStacks(Entity, Item2)));

			// Bring the target below max with exactly room for the 2-stack source.
			ASSERT_THAT(IsTrue(ArcMassItems::ModifyStacks(*EntityManager, Entity, StoreType, Item1, -2)));
			ASSERT_THAT(AreEqual(static_cast<uint16>(Max - 2), ReadStacks(Entity, Item1)));

			// Source(item2) in slot 0, target(item1) in slot 1.
			ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, Item2)));
			ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 1, Item1)));

			// Move source → target: Transfer = min(2, Max-(Max-2)) = 2 → source consumed.
			ASSERT_THAT(IsTrue(ArcMassItems::Bags::MoveItemBetweenSlots(*EntityManager, Entity, BagId, 0, BagId, 1)));

			// Source destroyed → slot 0 None; target rose Max-2 → Max.
			ASSERT_THAT(IsFalse(ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 0).IsValid()));
			ASSERT_THAT(AreEqual(static_cast<uint16>(Max), ReadStacks(Entity, Item1)));
		}
		else
		{
			// --- Non-stackable fixture (current ItemWithStacks, MaxStacks == 0): the bag
			//     must NOT merge — same-def items SWAP and neither is destroyed. ---
			// A second same-def add overflows into a distinct item (StackCheckMass with
			// Max == 0 spills everything past the existing item).
			FArcItemSpec Spec2 = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);
			FArcItemId Item2 = ArcMassItems::AddItem(*EntityManager, Entity, StoreType, Spec2);
			ASSERT_THAT(IsTrue(Item2.IsValid()));
			ASSERT_THAT(IsTrue(Item2 != Item1)); // distinct same-def items

			// item1 in slot 0, item2 in slot 1.
			ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, Item1)));
			ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 1, Item2)));

			// Move 0 → 1: non-stackable def gate → swap, no merge, no destruction.
			ASSERT_THAT(IsTrue(ArcMassItems::Bags::MoveItemBetweenSlots(*EntityManager, Entity, BagId, 0, BagId, 1)));

			// Slots swapped …
			ASSERT_THAT(IsTrue(ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 0) == Item2));
			ASSERT_THAT(IsTrue(ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 1) == Item1));
			// … and BOTH items are still resident (nothing was merged away/destroyed).
			FMassEntityView View(*EntityManager, Entity);
			const FArcMassItemStoreFragment* Store = View.GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
			ASSERT_THAT(IsNotNull(Store));
			FArcMassReplicatedItemArray& Items = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems);
			ASSERT_THAT(IsNotNull(Items.FindById(Item1)));
			ASSERT_THAT(IsNotNull(Items.FindById(Item2)));
		}
	}

	// -----------------------------------------------------------------------
	// Test 6d: MoveItemBetweenSlots — same stackable definition, target
	//          already "at or above" max (over-filled to force Transfer == 0):
	//          operation falls through to the swap path.
	// -----------------------------------------------------------------------

	TEST_METHOD(MoveItemBetweenSlots_SameStackableDef_TargetAtMax_Swaps)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(4);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		// Source: 1 stack.
		// Target: created with Amount=1000 (far exceeds any realistic MaxStacks) so that
		//   GetItemMaxStacks() − GetStacks() ≤ 0 → Transfer = 0 → swap path.
		FArcItemSpec SrcSpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);
		FArcItemId SrcId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SrcSpec);

		FArcItemSpec DstSpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1000);
		FArcItemId DstId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), DstSpec);

		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, SrcId)));
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 1, DstId)));

		// Move triggers a swap (Transfer = min(1, MaxStacks - 1000) ≤ 0).
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::MoveItemBetweenSlots(*EntityManager, Entity, BagId, 0, BagId, 1)));

		FArcItemId AfterSlot0 = ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 0);
		FArcItemId AfterSlot1 = ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 1);
		ASSERT_THAT(IsTrue(AfterSlot0 == DstId)); // over-filled item swapped to slot 0
		ASSERT_THAT(IsTrue(AfterSlot1 == SrcId)); // 1-stack item swapped to slot 1
	}

	// -----------------------------------------------------------------------
	// Test 7: ReconcileBagSlots shrink — shrinking the definition SlotCount
	//         below an occupied slot must NOT drop that slot.
	// -----------------------------------------------------------------------

	TEST_METHOD(ReconcileBagSlots_ShrinkBelowOccupiedSlot_DoesNotDropOccupied)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(4);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		// Place an item in slot 3 (the high slot).
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 3, ItemId)));

		// Shrink the definition to 2 slots.
		Def->SlotCount = 2;

		// Call ReconcileBagSlots directly on the bag instance.
		FMassEntityView View(*EntityManager, Entity);
		FArcMassBagStoreFragment* BagFrag = View.GetSparseFragmentDataPtr<FArcMassBagStoreFragment>();
		ASSERT_THAT(IsNotNull(BagFrag));
		FArcBagInstance* BagInst = ArcMassItems::Bags::FindBag(*BagFrag, BagId);
		ASSERT_THAT(IsNotNull(BagInst));

		ArcMassItems::Bags::ReconcileBagSlots(*BagInst, *Def);

		// The bag should still have 4 slots because slot 3 is occupied (tail is NOT empty).
		ASSERT_THAT(AreEqual(4, BagInst->Slots.Num()));
		// The occupied slot is intact.
		ASSERT_THAT(IsTrue(BagInst->Slots[3] == ItemId));
	}

	// -----------------------------------------------------------------------
	// Test 8: Sync scrub — place item in a bag slot, then destroy the item
	//         through ArcMassItems::DestroyItem and drain the event queue.
	//         UArcMassBagSyncSubsystem::OnItemRemoved must clear the stale slot.
	// -----------------------------------------------------------------------

	TEST_METHOD(SyncScrub_DestroyedItem_SlotClearedAfterDrain)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(3);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ItemId)));

		// Sanity: slot is occupied before destroy.
		FArcItemId PreDestroySlot = ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 0);
		ASSERT_THAT(IsTrue(PreDestroySlot == ItemId));

		// Destroy the item through the ops layer (queues ItemRemoved event).
		ArcMassItems::DestroyItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId);

		// Drain events — fires UArcMassBagSyncSubsystem::OnItemRemoved which clears the slot.
		ArcMassItems::TestHelpers::DrainAttachmentEvents(&Spawner.GetWorld());

		// Slot must now be None.
		FArcItemId PostDestroySlot = ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 0);
		ASSERT_THAT(IsFalse(PostDestroySlot.IsValid()));
	}
};
