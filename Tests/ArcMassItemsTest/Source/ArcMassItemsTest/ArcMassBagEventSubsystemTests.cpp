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
#include "Bags/ArcBagEventSubsystem.h"
#include "Events/ArcMassItemEventSubsystem.h"

#include "ArcMassItemTestsSettings.h"
#include "ArcMassItemTestHelpers.h"

// ---------------------------------------------------------------------------
// ArcMassBagEventSubsystem test class
// Tests the bag-change event subsystem: subscribe -> perform op -> DrainQueues
// -> assert delegate fired. All drain calls are manual (no post-actor-tick).
// ---------------------------------------------------------------------------

TEST_CLASS(ArcMassBagEventSubsystem, "ArcMassItems.BagEventSubsystem")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UArcMassBagEventSubsystem* BagEventSub = nullptr;
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
		BagEventSub = Spawner.GetWorld().GetSubsystem<UArcMassBagEventSubsystem>();
		check(BagEventSub);
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
	// Test 1: PlaceItemInSlot -> OnBagSlotsChanged fires with (Entity, BagId)
	// -----------------------------------------------------------------------

	TEST_METHOD(PlaceItemInSlot_FiresSlotsChanged)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(3);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		// Drain the AddBag list-changed event before subscribing to slot events.
		BagEventSub->DrainQueues();

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		bool bFired = false;
		FMassEntityHandle FiredEntity;
		FArcBagId FiredBagId;

		FDelegateHandle Handle = BagEventSub->AddBagSlotsChangedForEntity(
			Entity,
			UArcMassBagEventSubsystem::FOnBagSlotsChanged::FDelegate::CreateLambda(
				[&bFired, &FiredEntity, &FiredBagId](FMassEntityHandle InEntity, FArcBagId InBagId)
				{
					bFired = true;
					FiredEntity = InEntity;
					FiredBagId = InBagId;
				}));

		const bool bPlaced = ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ItemId);
		ASSERT_THAT(IsTrue(bPlaced));

		BagEventSub->DrainQueues();

		ASSERT_THAT(IsTrue(bFired));
		ASSERT_THAT(IsTrue(FiredEntity == Entity));
		ASSERT_THAT(IsTrue(FiredBagId == BagId));

		BagEventSub->RemoveForEntity(Entity, Handle);
	}

	// -----------------------------------------------------------------------
	// Test 2: ClearSlot -> OnBagSlotsChanged fires with (Entity, BagId)
	// -----------------------------------------------------------------------

	TEST_METHOD(ClearSlot_FiresSlotsChanged)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(3);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ItemId);

		// Drain setup events before subscribing.
		BagEventSub->DrainQueues();

		bool bFired = false;
		FMassEntityHandle FiredEntity;
		FArcBagId FiredBagId;

		FDelegateHandle Handle = BagEventSub->AddBagSlotsChangedForEntity(
			Entity,
			UArcMassBagEventSubsystem::FOnBagSlotsChanged::FDelegate::CreateLambda(
				[&bFired, &FiredEntity, &FiredBagId](FMassEntityHandle InEntity, FArcBagId InBagId)
				{
					bFired = true;
					FiredEntity = InEntity;
					FiredBagId = InBagId;
				}));

		const bool bCleared = ArcMassItems::Bags::ClearSlot(*EntityManager, Entity, BagId, 0);
		ASSERT_THAT(IsTrue(bCleared));

		BagEventSub->DrainQueues();

		ASSERT_THAT(IsTrue(bFired));
		ASSERT_THAT(IsTrue(FiredEntity == Entity));
		ASSERT_THAT(IsTrue(FiredBagId == BagId));

		BagEventSub->RemoveForEntity(Entity, Handle);
	}

	// -----------------------------------------------------------------------
	// Test 3a: MoveItemBetweenSlots across TWO DIFFERENT bags fires for BOTH
	// -----------------------------------------------------------------------

	TEST_METHOD(MoveItemBetweenSlots_DifferentBags_FiresBoth)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* DefA = MakeOpenBagDef(2);
		UArcBagDefinition* DefB = MakeOpenBagDef(2);
		FArcBagId BagA = ArcMassItems::Bags::AddBag(*EntityManager, Entity, DefA);
		FArcBagId BagB = ArcMassItems::Bags::AddBag(*EntityManager, Entity, DefB);
		ASSERT_THAT(IsTrue(BagA.IsValid()));
		ASSERT_THAT(IsTrue(BagB.IsValid()));

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagA, 0, ItemId);

		// Drain setup events before subscribing.
		BagEventSub->DrainQueues();

		int32 FireCount = 0;
		TArray<FArcBagId> FiredBagIds;

		FDelegateHandle Handle = BagEventSub->AddBagSlotsChangedForEntity(
			Entity,
			UArcMassBagEventSubsystem::FOnBagSlotsChanged::FDelegate::CreateLambda(
				[&FireCount, &FiredBagIds](FMassEntityHandle /*InEntity*/, FArcBagId InBagId)
				{
					++FireCount;
					FiredBagIds.Add(InBagId);
				}));

		const bool bMoved = ArcMassItems::Bags::MoveItemBetweenSlots(*EntityManager, Entity, BagA, 0, BagB, 0);
		ASSERT_THAT(IsTrue(bMoved));

		BagEventSub->DrainQueues();

		// Cross-bag move: FromBag AND ToBag each get a SlotsChanged event.
		ASSERT_THAT(AreEqual(2, FireCount));
		ASSERT_THAT(IsTrue(FiredBagIds.Contains(BagA)));
		ASSERT_THAT(IsTrue(FiredBagIds.Contains(BagB)));

		BagEventSub->RemoveForEntity(Entity, Handle);
	}

	// -----------------------------------------------------------------------
	// Test 3b: MoveItemBetweenSlots within the SAME bag fires exactly once
	// -----------------------------------------------------------------------

	TEST_METHOD(MoveItemBetweenSlots_SameBag_FiresOnce)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(4);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ItemId);

		// Drain setup events before subscribing.
		BagEventSub->DrainQueues();

		int32 FireCount = 0;
		FArcBagId FiredBagId;

		FDelegateHandle Handle = BagEventSub->AddBagSlotsChangedForEntity(
			Entity,
			UArcMassBagEventSubsystem::FOnBagSlotsChanged::FDelegate::CreateLambda(
				[&FireCount, &FiredBagId](FMassEntityHandle /*InEntity*/, FArcBagId InBagId)
				{
					++FireCount;
					FiredBagId = InBagId;
				}));

		const bool bMoved = ArcMassItems::Bags::MoveItemBetweenSlots(*EntityManager, Entity, BagId, 0, BagId, 3);
		ASSERT_THAT(IsTrue(bMoved));

		BagEventSub->DrainQueues();

		// Same-bag move: ToBag == FromBag → only one event enqueued.
		ASSERT_THAT(AreEqual(1, FireCount));
		ASSERT_THAT(IsTrue(FiredBagId == BagId));

		BagEventSub->RemoveForEntity(Entity, Handle);
	}

	// -----------------------------------------------------------------------
	// Test 4: AddBag -> OnBagListChanged fires with Entity
	// -----------------------------------------------------------------------

	TEST_METHOD(AddBag_FiresBagListChanged)
	{
		FMassEntityHandle Entity = CreateBagEntity();

		bool bFired = false;
		FMassEntityHandle FiredEntity;

		FDelegateHandle Handle = BagEventSub->AddBagListChangedForEntity(
			Entity,
			UArcMassBagEventSubsystem::FOnBagListChanged::FDelegate::CreateLambda(
				[&bFired, &FiredEntity](FMassEntityHandle InEntity)
				{
					bFired = true;
					FiredEntity = InEntity;
				}));

		UArcBagDefinition* Def = MakeOpenBagDef(2);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);
		ASSERT_THAT(IsTrue(BagId.IsValid()));

		BagEventSub->DrainQueues();

		ASSERT_THAT(IsTrue(bFired));
		ASSERT_THAT(IsTrue(FiredEntity == Entity));

		BagEventSub->RemoveForEntity(Entity, Handle);
	}

	// -----------------------------------------------------------------------
	// Test 5: RemoveBag -> OnBagListChanged fires with Entity
	// -----------------------------------------------------------------------

	TEST_METHOD(RemoveBag_FiresBagListChanged)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(2);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		// Drain the AddBag event before subscribing.
		BagEventSub->DrainQueues();

		bool bFired = false;
		FMassEntityHandle FiredEntity;

		FDelegateHandle Handle = BagEventSub->AddBagListChangedForEntity(
			Entity,
			UArcMassBagEventSubsystem::FOnBagListChanged::FDelegate::CreateLambda(
				[&bFired, &FiredEntity](FMassEntityHandle InEntity)
				{
					bFired = true;
					FiredEntity = InEntity;
				}));

		const bool bRemoved = ArcMassItems::Bags::RemoveBag(*EntityManager, Entity, BagId);
		ASSERT_THAT(IsTrue(bRemoved));

		BagEventSub->DrainQueues();

		ASSERT_THAT(IsTrue(bFired));
		ASSERT_THAT(IsTrue(FiredEntity == Entity));

		BagEventSub->RemoveForEntity(Entity, Handle);
	}

	// -----------------------------------------------------------------------
	// Test 6: ReorderBag (index changes) -> OnBagListChanged fires with Entity
	// Uses 2 bags so index 0→1 is a real move (not a no-op clamp).
	// -----------------------------------------------------------------------

	TEST_METHOD(ReorderBag_FiresBagListChanged)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* DefA = MakeOpenBagDef(2);
		UArcBagDefinition* DefB = MakeOpenBagDef(2);
		FArcBagId BagA = ArcMassItems::Bags::AddBag(*EntityManager, Entity, DefA);
		FArcBagId BagB = ArcMassItems::Bags::AddBag(*EntityManager, Entity, DefB);
		ASSERT_THAT(IsTrue(BagA.IsValid()));
		ASSERT_THAT(IsTrue(BagB.IsValid()));

		// Drain AddBag list-changed events before subscribing.
		BagEventSub->DrainQueues();

		bool bFired = false;
		FMassEntityHandle FiredEntity;

		FDelegateHandle Handle = BagEventSub->AddBagListChangedForEntity(
			Entity,
			UArcMassBagEventSubsystem::FOnBagListChanged::FDelegate::CreateLambda(
				[&bFired, &FiredEntity](FMassEntityHandle InEntity)
				{
					bFired = true;
					FiredEntity = InEntity;
				}));

		// BagA is at index 0; move it to index 1 (actual reorder).
		const bool bReordered = ArcMassItems::Bags::ReorderBag(*EntityManager, Entity, BagA, 1);
		ASSERT_THAT(IsTrue(bReordered));

		BagEventSub->DrainQueues();

		ASSERT_THAT(IsTrue(bFired));
		ASSERT_THAT(IsTrue(FiredEntity == Entity));

		BagEventSub->RemoveForEntity(Entity, Handle);
	}

	// -----------------------------------------------------------------------
	// Test 7: Scrub path — destroy a slotted item, drain item events FIRST
	//         (so UArcMassBagSyncSubsystem::OnItemRemoved queues the bag event),
	//         then drain bag events → OnBagSlotsChanged fires and slot is None.
	// -----------------------------------------------------------------------

	TEST_METHOD(ScrubPath_DestroyItem_FiresSlotsChanged_AndSlotIsNone)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(3);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ItemId);

		// Drain setup events before subscribing.
		BagEventSub->DrainQueues();

		bool bFired = false;
		FArcBagId FiredBagId;

		FDelegateHandle Handle = BagEventSub->AddBagSlotsChangedForEntity(
			Entity,
			UArcMassBagEventSubsystem::FOnBagSlotsChanged::FDelegate::CreateLambda(
				[&bFired, &FiredBagId](FMassEntityHandle /*InEntity*/, FArcBagId InBagId)
				{
					bFired = true;
					FiredBagId = InBagId;
				}));

		// Destroy item → queues OnItemRemoved in the item event subsystem.
		ArcMassItems::DestroyItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId);

		// Drain item events FIRST: UArcMassBagSyncSubsystem::OnItemRemoved runs and
		// queues BagSlotsChanged in the bag event subsystem.
		ArcMassItems::TestHelpers::DrainAttachmentEvents(&Spawner.GetWorld());

		// Now drain bag events → our delegate should fire.
		BagEventSub->DrainQueues();

		ASSERT_THAT(IsTrue(bFired));
		ASSERT_THAT(IsTrue(FiredBagId == BagId));

		// The slot must now be None (bag sync scrub cleared it).
		FArcItemId SlotItem = ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 0);
		ASSERT_THAT(IsFalse(SlotItem.IsValid()));

		BagEventSub->RemoveForEntity(Entity, Handle);
	}

	// -----------------------------------------------------------------------
	// Test 8: RemoveForEntity — after unbind, subsequent op+drain does NOT
	//         invoke the removed delegate.
	// -----------------------------------------------------------------------

	TEST_METHOD(RemoveForEntity_UnbindsDelegate)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(2);
		ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);
		BagEventSub->DrainQueues();

		bool bFired = false;

		FDelegateHandle Handle = BagEventSub->AddBagListChangedForEntity(
			Entity,
			UArcMassBagEventSubsystem::FOnBagListChanged::FDelegate::CreateLambda(
				[&bFired](FMassEntityHandle /*InEntity*/)
				{
					bFired = true;
				}));

		// Unbind before the next operation.
		BagEventSub->RemoveForEntity(Entity, Handle);

		// Add another bag — would have fired if the delegate were still bound.
		UArcBagDefinition* Def2 = MakeOpenBagDef(1);
		ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def2);
		BagEventSub->DrainQueues();

		ASSERT_THAT(IsFalse(bFired));
	}

	// -----------------------------------------------------------------------
	// Test 9: EvictEntity — clears all per-entity subscriptions; post-evict
	//         ops + drain do NOT invoke any previously registered delegates.
	// -----------------------------------------------------------------------

	TEST_METHOD(EvictEntity_ClearsSubscriptions)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(2);
		FArcBagId ExistingBag = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);
		ASSERT_THAT(IsTrue(ExistingBag.IsValid()));
		BagEventSub->DrainQueues();

		bool bListFired = false;
		bool bSlotsFired = false;

		BagEventSub->AddBagListChangedForEntity(
			Entity,
			UArcMassBagEventSubsystem::FOnBagListChanged::FDelegate::CreateLambda(
				[&bListFired](FMassEntityHandle /*InEntity*/)
				{
					bListFired = true;
				}));

		BagEventSub->AddBagSlotsChangedForEntity(
			Entity,
			UArcMassBagEventSubsystem::FOnBagSlotsChanged::FDelegate::CreateLambda(
				[&bSlotsFired](FMassEntityHandle /*InEntity*/, FArcBagId /*InBagId*/)
				{
					bSlotsFired = true;
				}));

		// Evict: clears all per-entity subscriptions.
		BagEventSub->EvictEntity(Entity);

		// Post-evict: enqueue a list event and a slots event — no delegate is registered.
		UArcBagDefinition* Def2 = MakeOpenBagDef(1);
		FArcBagId NewBag = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def2);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, NewBag, 0, ItemId);

		BagEventSub->DrainQueues();

		ASSERT_THAT(IsFalse(bListFired));
		ASSERT_THAT(IsFalse(bSlotsFired));
	}

	// -----------------------------------------------------------------------
	// Test 10: Stack-change path — an item already in a bag slot changes its
	//          Stacks (same id stays slotted). OnItemRemoved never fires, so the
	//          refresh must come from UArcMassBagSyncSubsystem::OnItemStacksChanged.
	//          Drain item events FIRST (sync subsystem queues the bag event), then
	//          drain bag events → OnBagSlotsChanged fires; slot stays occupied.
	// -----------------------------------------------------------------------

	TEST_METHOD(StackChange_SlottedItem_FiresSlotsChanged_AndSlotStillHeld)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(3);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::Bags::PlaceItemInSlot(*EntityManager, Entity, BagId, 0, ItemId);

		// Drain setup events before subscribing.
		BagEventSub->DrainQueues();
		ArcMassItems::TestHelpers::DrainAttachmentEvents(&Spawner.GetWorld());

		bool bFired = false;
		FArcBagId FiredBagId;

		FDelegateHandle Handle = BagEventSub->AddBagSlotsChangedForEntity(
			Entity,
			UArcMassBagEventSubsystem::FOnBagSlotsChanged::FDelegate::CreateLambda(
				[&bFired, &FiredBagId](FMassEntityHandle /*InEntity*/, FArcBagId InBagId)
				{
					bFired = true;
					FiredBagId = InBagId;
				}));

		// In-place stack change on the slotted item → queues OnItemStacksChanged.
		UArcMassItemEventSubsystem* ItemEvents = Spawner.GetWorld().GetSubsystem<UArcMassItemEventSubsystem>();
		check(ItemEvents);
		ItemEvents->QueueItemStacksChanged(Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, +1);

		// Drain item events FIRST: sync subsystem's OnItemStacksChanged queues the bag event.
		ArcMassItems::TestHelpers::DrainAttachmentEvents(&Spawner.GetWorld());

		// Now drain bag events → our delegate should fire for the bag holding the id.
		BagEventSub->DrainQueues();

		ASSERT_THAT(IsTrue(bFired));
		ASSERT_THAT(IsTrue(FiredBagId == BagId));

		// The slot must remain occupied — a stack change never clears the slot.
		FArcItemId SlotItem = ArcMassItems::Bags::GetSlotItem(*EntityManager, Entity, BagId, 0);
		ASSERT_THAT(IsTrue(SlotItem == ItemId));

		BagEventSub->RemoveForEntity(Entity, Handle);
	}

	// -----------------------------------------------------------------------
	// Test 11: Negative — stack change for an item NOT in any bag slot must
	//          NOT queue any bag event.
	// -----------------------------------------------------------------------

	TEST_METHOD(StackChange_UnslottedItem_DoesNotFireSlotsChanged)
	{
		FMassEntityHandle Entity = CreateBagEntity();
		UArcBagDefinition* Def = MakeOpenBagDef(3);
		FArcBagId BagId = ArcMassItems::Bags::AddBag(*EntityManager, Entity, Def);

		// Item exists in the store but is never placed in a bag slot.
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		// Drain setup events before subscribing.
		BagEventSub->DrainQueues();
		ArcMassItems::TestHelpers::DrainAttachmentEvents(&Spawner.GetWorld());

		bool bFired = false;

		FDelegateHandle Handle = BagEventSub->AddBagSlotsChangedForEntity(
			Entity,
			UArcMassBagEventSubsystem::FOnBagSlotsChanged::FDelegate::CreateLambda(
				[&bFired](FMassEntityHandle /*InEntity*/, FArcBagId /*InBagId*/)
				{
					bFired = true;
				}));

		UArcMassItemEventSubsystem* ItemEvents = Spawner.GetWorld().GetSubsystem<UArcMassItemEventSubsystem>();
		check(ItemEvents);
		ItemEvents->QueueItemStacksChanged(Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, +1);

		ArcMassItems::TestHelpers::DrainAttachmentEvents(&Spawner.GetWorld());
		BagEventSub->DrainQueues();

		ASSERT_THAT(IsFalse(bFired));

		BagEventSub->RemoveForEntity(Entity, Handle);
	}
};
