// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/EntityHandle.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Fragments/ArcMassItemEventFragment.h"
#include "Operations/ArcMassItemOperations.h"
#include "StackMethods/ArcMassItemStackMethods.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemTypes.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Items/Fragments/ArcItemFragment_Stacks.h"
#include "Items/ArcItemInstance.h"
#include "Core/ArcCoreAssetManager.h"
#include "ArcMassItemTestsSettings.h"
#include "Signals/ArcMassItemSignals.h"
#include "MassSignalSubsystem.h"
#include "ArcItemTestRecordFiles.h"
#include "ArcMassItemTestHelpers.h"
#include "StructUtils/InstancedStruct.h"
#include "Mass/EntityFragments.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Slot_Primary, "Test.Item.Slot.Primary");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Slot_Secondary, "Test.Item.Slot.Secondary");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Socket_Barrel, "Test.Item.Socket.Barrel");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_State_Equipped, "Test.Item.State.Equipped");

TEST_CLASS(ArcMassItemOps_AddRemove, "ArcMassItems.Operations")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);
	}

	TEST_METHOD(AddItem_ReturnsValidId)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ItemId.IsValid()));
	}

	TEST_METHOD(AddItem_ItemExistsInStore)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsNotNull(Store));
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId) != nullptr));
	}

	TEST_METHOD(AddItem_WritesEventFragment_AndFiresItemAddedSignal)
	{
		FArcMassItemSignalListener Listener(*SignalSubsystem,
			ArcMassItems::TestHelpers::StoreQualifiedSignal(UE::ArcMassItems::Signals::ItemAdded));

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, Entity)
			.GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
		ASSERT_THAT(IsNotNull(Events));
		ASSERT_THAT(AreEqual(1, Events->Events.Num()));
		ASSERT_THAT(AreEqual(EArcMassItemEventType::Added, Events->Events[0].Type));

		ASSERT_THAT(AreEqual(1, Listener.NumReceivedFor(Entity)));
	}

	TEST_METHOD(RemoveItem_RemovesFromStore)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		bool bRemoved = ArcMassItems::RemoveItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId);
		ASSERT_THAT(IsTrue(bRemoved));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId) == nullptr));
	}

	TEST_METHOD(RemoveItem_InvalidId_ReturnsFalse)
	{
		FArcMassItemSignalListener Listener(*SignalSubsystem,
			ArcMassItems::TestHelpers::StoreQualifiedSignal(UE::ArcMassItems::Signals::ItemRemoved));

		bool bRemoved = ArcMassItems::RemoveItem(*EntityManager, Entity,
			FArcMassItemStoreFragment::StaticStruct(), FArcItemId());
		ASSERT_THAT(IsFalse(bRemoved));
		ASSERT_THAT(IsFalse(Listener.ReceivedAny()));
	}

	TEST_METHOD(RemoveItem_ReducesStacks)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 5);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		bool bRemoved = ArcMassItems::RemoveItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, 3);
		ASSERT_THAT(IsTrue(bRemoved));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(AreEqual(static_cast<uint16>(2), FoundItem->ToItem()->GetStacks()));
	}

	TEST_METHOD(RemoveItem_ReduceStacksToZero_RemovesItem)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 3);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		bool bRemoved = ArcMassItems::RemoveItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, 3);
		ASSERT_THAT(IsTrue(bRemoved));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId) == nullptr));
	}

	TEST_METHOD(RemoveItem_WritesEventFragment_AndFiresItemRemovedSignal)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		EntityManager->RemoveSparseElementFromEntity<FArcMassItemEventFragment>(Entity);

		FArcMassItemSignalListener Listener(*SignalSubsystem,
			ArcMassItems::TestHelpers::StoreQualifiedSignal(UE::ArcMassItems::Signals::ItemRemoved));

		ArcMassItems::RemoveItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId);

		const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, Entity)
			.GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
		ASSERT_THAT(IsNotNull(Events));
		ASSERT_THAT(AreEqual(1, Events->Events.Num()));
		ASSERT_THAT(AreEqual(EArcMassItemEventType::Removed, Events->Events[0].Type));

		ASSERT_THAT(AreEqual(1, Listener.NumReceivedFor(Entity)));
	}

	TEST_METHOD(TwoAddsInOneFrame_AppendsEventsAndFiresSignalOnce)
	{
		FArcMassItemSignalListener Listener(*SignalSubsystem,
			ArcMassItems::TestHelpers::StoreQualifiedSignal(UE::ArcMassItems::Signals::ItemAdded));

		const uint64 PinnedFrame = GFrameCounter;

		FArcItemSpec Spec1 = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId Id1 = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec1);
		GFrameCounter = PinnedFrame;

		FArcItemSpec Spec2 = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId Id2 = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec2);

		const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, Entity)
			.GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
		ASSERT_THAT(IsNotNull(Events));
		ASSERT_THAT(AreEqual(2, Events->Events.Num()));
		ASSERT_THAT(AreEqual(Id1, Events->Events[0].ItemId));
		ASSERT_THAT(AreEqual(Id2, Events->Events[1].ItemId));

		ASSERT_THAT(AreEqual(2, Listener.NumReceivedFor(Entity)));
	}
};

TEST_CLASS(ArcMassItemOps_Slots, "ArcMassItems.Operations.Slots")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);
	}

	TEST_METHOD(AddItemToSlot_SetsSlotOnItem)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		FGameplayTag SlotTag = TAG_Test_Slot_Primary;
		bool bSlotted = ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, SlotTag);

		ASSERT_THAT(IsTrue(bSlotted));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(AreEqual(SlotTag, FoundItem->ToItem()->GetSlotId()));
	}

	TEST_METHOD(AddItemToSlot_OccupiedSlot_ReturnsFalse)
	{
		FArcItemSpec Spec1 = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemSpec Spec2 = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId1 = ArcMassItems::AddItem(*EntityManager, Entity,
			FArcMassItemStoreFragment::StaticStruct(), Spec1);
		FArcItemId ItemId2 = ArcMassItems::AddItem(*EntityManager, Entity,
			FArcMassItemStoreFragment::StaticStruct(), Spec2);

		FGameplayTag SlotTag = TAG_Test_Slot_Primary;
		ArcMassItems::AddItemToSlot(*EntityManager, Entity,
			FArcMassItemStoreFragment::StaticStruct(), ItemId1, SlotTag);

		// Occupied slot → no SlotAssigned signal should fire for the rejected item.
		FArcMassItemSignalListener Listener(*SignalSubsystem,
			ArcMassItems::TestHelpers::StoreQualifiedSignal(UE::ArcMassItems::Signals::ItemSlotAssigned));

		bool bSlotted = ArcMassItems::AddItemToSlot(*EntityManager, Entity,
			FArcMassItemStoreFragment::StaticStruct(), ItemId2, SlotTag);
		ASSERT_THAT(IsFalse(bSlotted));
		ASSERT_THAT(IsFalse(Listener.ReceivedAny()));
	}

	TEST_METHOD(AddItemToSlot_WritesEventAndFiresItemSlotAssignedSignal)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		EntityManager->RemoveSparseElementFromEntity<FArcMassItemEventFragment>(Entity);

		// AddItemToSlot on an unslotted item emits SlotAssigned (not SlotChanged).
		FArcMassItemSignalListener Listener(*SignalSubsystem,
			ArcMassItems::TestHelpers::StoreQualifiedSignal(UE::ArcMassItems::Signals::ItemSlotAssigned));

		FGameplayTag SlotTag = TAG_Test_Slot_Primary;
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, SlotTag);

		const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, Entity)
			.GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
		ASSERT_THAT(IsNotNull(Events));
		ASSERT_THAT(AreEqual(1, Events->Events.Num()));
		ASSERT_THAT(AreEqual(EArcMassItemEventType::SlotAssigned, Events->Events[0].Type));
		ASSERT_THAT(AreEqual(SlotTag, Events->Events[0].SlotTag));

		ASSERT_THAT(AreEqual(1, Listener.NumReceivedFor(Entity)));
	}

	TEST_METHOD(RemoveItemFromSlot_ClearsSlot)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		FGameplayTag SlotTag = TAG_Test_Slot_Primary;
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, SlotTag);

		bool bRemoved = ArcMassItems::RemoveItemFromSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId);
		ASSERT_THAT(IsTrue(bRemoved));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(IsFalse(FoundItem->ToItem()->GetSlotId().IsValid()));
	}

	TEST_METHOD(RemoveItemFromSlot_UnslottedItem_ReturnsFalse)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		bool bRemoved = ArcMassItems::RemoveItemFromSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId);
		ASSERT_THAT(IsFalse(bRemoved));
	}
};

TEST_CLASS(ArcMassItemOps_Stacks, "ArcMassItems.Operations.Stacks")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);
	}

	TEST_METHOD(ModifyStacks_Positive_IncreasesStacks)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 5);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		bool bModified = ArcMassItems::ModifyStacks(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, 3);
		ASSERT_THAT(IsTrue(bModified));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(AreEqual(static_cast<uint16>(8), FoundItem->ToItem()->GetStacks()));
	}

	TEST_METHOD(ModifyStacks_Negative_DecreasesStacks)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 5);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		bool bModified = ArcMassItems::ModifyStacks(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, -2);
		ASSERT_THAT(IsTrue(bModified));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(AreEqual(static_cast<uint16>(3), FoundItem->ToItem()->GetStacks()));
	}

	TEST_METHOD(ModifyStacks_BelowZero_ReturnsFalse)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 2);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		bool bModified = ArcMassItems::ModifyStacks(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, -5);
		ASSERT_THAT(IsFalse(bModified));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(AreEqual(static_cast<uint16>(2), FoundItem->ToItem()->GetStacks()));
	}

	TEST_METHOD(ModifyStacks_WritesEventAndFiresItemStacksChangedSignal)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 5);
		FArcItemId ItemId = ArcMassItems::AddItem(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		EntityManager->RemoveSparseElementFromEntity<FArcMassItemEventFragment>(Entity);

		FArcMassItemSignalListener Listener(*SignalSubsystem,
			ArcMassItems::TestHelpers::StoreQualifiedSignal(UE::ArcMassItems::Signals::ItemStacksChanged));

		ArcMassItems::ModifyStacks(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, 3);

		const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, Entity)
			.GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
		ASSERT_THAT(IsNotNull(Events));
		ASSERT_THAT(AreEqual(1, Events->Events.Num()));
		ASSERT_THAT(AreEqual(EArcMassItemEventType::StacksChanged, Events->Events[0].Type));
		ASSERT_THAT(AreEqual(3, Events->Events[0].StackDelta));

		ASSERT_THAT(AreEqual(1, Listener.NumReceivedFor(Entity)));
	}

	TEST_METHOD(ModifyStacks_InvalidItem_ReturnsFalse)
	{
		FArcMassItemSignalListener Listener(*SignalSubsystem,
			ArcMassItems::TestHelpers::StoreQualifiedSignal(UE::ArcMassItems::Signals::ItemStacksChanged));
		bool bModified = ArcMassItems::ModifyStacks(*EntityManager, Entity,
			FArcMassItemStoreFragment::StaticStruct(), FArcItemId(), 1);
		ASSERT_THAT(IsFalse(bModified));
		ASSERT_THAT(IsFalse(Listener.ReceivedAny()));
	}
};

TEST_CLASS(ArcMassItemOps_Attachment, "ArcMassItems.Operations.Attachment")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	TEST_METHOD(AttachItem_SetsOwnerAndSlot)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerSpec);

		FArcItemSpec AttachSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId AttachId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), AttachSpec);

		FGameplayTag AttachSlot = TAG_Test_Socket_Barrel;
		bool bAttached = ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, AttachSlot);
		ASSERT_THAT(IsTrue(bAttached));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(AttachId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(AreEqual(OwnerId, FoundItem->ToItem()->GetOwnerId()));
		ASSERT_THAT(AreEqual(AttachSlot, FoundItem->ToItem()->GetAttachSlot()));
	}

	TEST_METHOD(AttachItem_AddsToOwnerAttachedItems)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerSpec);

		FArcItemSpec AttachSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId AttachId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), AttachSpec);

		FGameplayTag AttachSlot = TAG_Test_Socket_Barrel;
		ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, AttachSlot);

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* OwnerItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(OwnerId);
		ASSERT_THAT(IsNotNull(OwnerItem));
		ASSERT_THAT(IsTrue(OwnerItem->ToItem()->GetAttachedItems().Contains(AttachId)));
	}

	TEST_METHOD(DetachItem_ClearsOwnerAndSlot)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerSpec);

		FArcItemSpec AttachSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId AttachId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), AttachSpec);

		FGameplayTag AttachSlot = TAG_Test_Socket_Barrel;
		ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, AttachSlot);

		bool bDetached = ArcMassItems::DetachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), AttachId);
		ASSERT_THAT(IsTrue(bDetached));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(AttachId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(IsFalse(FoundItem->ToItem()->GetOwnerId().IsValid()));
		ASSERT_THAT(IsFalse(FoundItem->ToItem()->GetAttachSlot().IsValid()));
	}

	TEST_METHOD(RemoveOwner_CascadesAttachmentRemoval)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerSpec);

		FArcItemSpec AttachSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId AttachId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), AttachSpec);

		FGameplayTag AttachSlot = TAG_Test_Socket_Barrel;
		ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, AttachSlot);

		ArcMassItems::RemoveItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId);

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(OwnerId) == nullptr));
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(AttachId) == nullptr));
	}
};

TEST_CLASS(ArcMassItemOps_DynamicTags, "ArcMassItems.Operations.DynamicTags")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	TEST_METHOD(AddDynamicTag_TagIsPresent)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		FGameplayTag Tag = TAG_Test_State_Equipped;
		bool bAdded = ArcMassItems::AddDynamicTag(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, Tag);
		ASSERT_THAT(IsTrue(bAdded));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(IsTrue(FoundItem->ToItem()->DynamicTags.HasTag(Tag)));
	}

	TEST_METHOD(RemoveDynamicTag_TagIsRemoved)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		FGameplayTag Tag = TAG_Test_State_Equipped;
		ArcMassItems::AddDynamicTag(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, Tag);
		bool bRemoved = ArcMassItems::RemoveDynamicTag(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, Tag);
		ASSERT_THAT(IsTrue(bRemoved));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(IsFalse(FoundItem->ToItem()->DynamicTags.HasTag(Tag)));
	}
};

TEST_CLASS(ArcMassItemOps_Level, "ArcMassItems.Operations.Level")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	TEST_METHOD(SetLevel_UpdatesLevel)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		bool bSet = ArcMassItems::SetLevel(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, 5);
		ASSERT_THAT(IsTrue(bSet));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(AreEqual(static_cast<uint8>(5), FoundItem->ToItem()->GetLevel()));
	}
};

TEST_CLASS(ArcMassItemOps_Move, "ArcMassItems.Operations.Move")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle EntityA;
	FMassEntityHandle EntityB;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		EntityA = CreateItemEntity();
		EntityB = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	TEST_METHOD(MoveItem_TransfersToTarget)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, EntityA, FArcMassItemStoreFragment::StaticStruct(), Spec);

		bool bMoved = ArcMassItems::MoveItem(*EntityManager, EntityA, EntityB, ItemId,
			FArcMassItemStoreFragment::StaticStruct(), FArcMassItemStoreFragment::StaticStruct());
		ASSERT_THAT(IsTrue(bMoved));

		const FArcMassItemStoreFragment* StoreA = FMassEntityView(*EntityManager, EntityA).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(StoreA->ReplicatedItems).FindById(ItemId) == nullptr));

		const FArcMassItemStoreFragment* StoreB = FMassEntityView(*EntityManager, EntityB).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(StoreB->ReplicatedItems).FindById(ItemId) != nullptr));
	}

	TEST_METHOD(MoveItem_InvalidItem_ReturnsFalse)
	{
		bool bMoved = ArcMassItems::MoveItem(*EntityManager, EntityA, EntityB, FArcItemId(),
			FArcMassItemStoreFragment::StaticStruct(), FArcMassItemStoreFragment::StaticStruct());
		ASSERT_THAT(IsFalse(bMoved));
	}
};

TEST_CLASS(ArcMassItemOps_Stacking, "ArcMassItems.Operations.Stacking")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	TEST_METHOD(StackByType_CanStackMass_ReturnsTrueWhenExisting)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 3);
		ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		// CanStackMass is space-aware: true only while an existing stack sits below MaxStacks,
		// so the probe method needs an authored max (a default FScalableFloat evaluates to 0).
		FArcMassItemStackMethod_StackByType StackMethod;
		StackMethod.MaxStacks.Value = 10.f;
		FArcItemSpec QuerySpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);
		ASSERT_THAT(IsTrue(StackMethod.CanStackMass(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), QuerySpec)));
	}

	TEST_METHOD(StackByType_CanStackMass_ReturnsFalseWhenExistingIsFull)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 3);
		ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		// The only existing stack is at/over the probe's max — no merge target with space.
		FArcMassItemStackMethod_StackByType StackMethod;
		StackMethod.MaxStacks.Value = 3.f;
		FArcItemSpec QuerySpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);
		ASSERT_THAT(IsFalse(StackMethod.CanStackMass(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), QuerySpec)));
	}

	TEST_METHOD(StackByType_CanStackMass_ReturnsFalseWhenNoExisting)
	{
		FArcMassItemStackMethod_StackByType StackMethod;
		FArcItemSpec QuerySpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);
		ASSERT_THAT(IsFalse(StackMethod.CanStackMass(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), QuerySpec)));
	}

	TEST_METHOD(CanNotStackUnique_CanAddMass_ReturnsFalseWhenExisting)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->ItemWithStacksCannotStack, 1, 1);
		ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		FArcMassItemStackMethod_CanNotStackUnique StackMethod;
		FArcItemSpec DuplicateSpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacksCannotStack, 1, 1);
		ASSERT_THAT(IsFalse(StackMethod.CanAddMass(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), DuplicateSpec)));
	}

	TEST_METHOD(CanNotStackUnique_CanAddMass_ReturnsTrueWhenNoExisting)
	{
		FArcMassItemStackMethod_CanNotStackUnique StackMethod;
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->ItemWithStacksCannotStack, 1, 1);
		ASSERT_THAT(IsTrue(StackMethod.CanAddMass(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec)));
	}
};

TEST_CLASS(ArcMassItemOps_Initialize, "ArcMassItems.Operations.Initialize")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	TEST_METHOD(InitializeItemData_DoesNotCrashOnInvalidItem)
	{
		ArcMassItems::InitializeItemData(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), FArcItemId());
		// Expect no crash
	}

	TEST_METHOD(InitializeItemData_DoesNotCrashOnValidItemWithoutDefinition)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ItemId.IsValid()));

		ArcMassItems::InitializeItemData(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId);
		// Expect no crash (definition is null, so InitializeMass returns early)
	}

	TEST_METHOD(AddItem_SetsWorldOnItemData)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(IsNotNull(FoundItem->ToItem()->World.Get()));
	}
};

TEST_CLASS(ArcMassItemOps_Query, "ArcMassItems.Operations.Query")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	TEST_METHOD(FindItemByDefinition_ResolvedDefinitionId_ReturnsItem)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		const FArcItemDefinitionData* Def = Spec.GetItemDefinition();
		ASSERT_THAT(IsNotNull(Def));
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		const FArcItemData* Found = ArcMassItems::FindItemByDefinition(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Def->GetPrimaryAssetId());
		ASSERT_THAT(IsNotNull(Found));
		ASSERT_THAT(AreEqual(ItemId, Found->GetItemId()));
	}

	TEST_METHOD(FindItemByDefinition_InvalidId_ReturnsNull)
	{
		const FArcItemData* Found = ArcMassItems::FindItemByDefinition(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), FPrimaryAssetId());
		ASSERT_THAT(IsNull(Found));
	}

	TEST_METHOD(CountItemsByDefinition_SumsStacks)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 3);
		ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		FArcItemSpec Spec2 = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 2);
		ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec2);

		int32 Count = ArcMassItems::CountItemsByDefinition(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec.GetItemDefinitionId());
		ASSERT_THAT(AreEqual(5, Count));
	}

	TEST_METHOD(CountItemsByDefinition_NoMatch_ReturnsZero)
	{
		int32 Count = ArcMassItems::CountItemsByDefinition(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), FPrimaryAssetId());
		ASSERT_THAT(AreEqual(0, Count));
	}

	TEST_METHOD(Contains_ReturnsTrueWhenFound)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		const FPrimaryAssetId DefId = Spec.GetItemDefinitionId();
		ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		ASSERT_THAT(IsTrue(ArcMassItems::Contains(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), DefId)));
	}

	TEST_METHOD(Contains_ReturnsFalseWhenNotFound)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		const FPrimaryAssetId DefId = Spec.GetItemDefinitionId();

		ASSERT_THAT(IsFalse(ArcMassItems::Contains(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), DefId)));
	}
};

TEST_CLASS(ArcMassItemOps_DefinitionTags, "ArcMassItems.Operations.DefinitionTags")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	TEST_METHOD(FindItemByDefinitionTags_HasMatchingTags_ReturnsItem)
	{
		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(MassSettings->ItemWithDefinitionTags.AssetId);
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->ItemWithDefinitionTags, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		const FArcItemFragment_Tags* TagFrag = Def->FindFragment<FArcItemFragment_Tags>();
		const FArcItemData* Found = ArcMassItems::FindItemByDefinitionTags(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), TagFrag ? TagFrag->ItemTags : FGameplayTagContainer());
		ASSERT_THAT(IsNotNull(Found));
		ASSERT_THAT(AreEqual(ItemId, Found->GetItemId()));
	}

	TEST_METHOD(FindItemByDefinitionTags_NoMatchingTags_ReturnsNull)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		FGameplayTagContainer Tags;
		Tags.AddTag(TAG_Test_Slot_Primary);
		const FArcItemData* Found = ArcMassItems::FindItemByDefinitionTags(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Tags);
		ASSERT_THAT(IsNull(Found));
	}

	TEST_METHOD(FindItemsByDefinitionTags_ReturnsMatches)
	{
		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(MassSettings->ItemWithDefinitionTags.AssetId);
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->ItemWithDefinitionTags, 1, 1);
		ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		const FArcItemFragment_Tags* TagFrag = Def->FindFragment<FArcItemFragment_Tags>();
		TArray<const FArcItemData*> Found = ArcMassItems::FindItemsByDefinitionTags(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), TagFrag ? TagFrag->ItemTags : FGameplayTagContainer());
		ASSERT_THAT(AreEqual(1, Found.Num()));
	}

	TEST_METHOD(FindItemsByDefinitionTags_NoMatches_ReturnsEmpty)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		FGameplayTagContainer Tags;
		Tags.AddTag(TAG_Test_Slot_Primary);
		TArray<const FArcItemData*> Found = ArcMassItems::FindItemsByDefinitionTags(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Tags);
		ASSERT_THAT(AreEqual(0, Found.Num()));
	}
};

TEST_CLASS(ArcMassItemOps_SlotChange, "ArcMassItems.Operations.SlotChange")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);
	}

	TEST_METHOD(IsOnAnySlot_ReturnsFalseWhenNotSlotted)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		ASSERT_THAT(IsFalse(ArcMassItems::IsOnAnySlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId)));
	}

	TEST_METHOD(IsOnAnySlot_ReturnsTrueWhenSlotted)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, TAG_Test_Slot_Primary);

		ASSERT_THAT(IsTrue(ArcMassItems::IsOnAnySlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId)));
	}

	TEST_METHOD(IsOnAnySlot_InvalidItem_ReturnsFalse)
	{
		ASSERT_THAT(IsFalse(ArcMassItems::IsOnAnySlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), FArcItemId())));
	}

	TEST_METHOD(ChangeItemSlot_MovesToNewSlot)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, TAG_Test_Slot_Primary);

		FGameplayTag SecondarySlot = TAG_Test_Slot_Secondary;
		bool bChanged = ArcMassItems::ChangeItemSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, SecondarySlot);
		ASSERT_THAT(IsTrue(bChanged));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(AreEqual(SecondarySlot, FoundItem->ToItem()->GetSlotId()));
	}

	TEST_METHOD(ChangeItemSlot_OccupiedSlot_ReturnsFalse)
	{
		FArcItemSpec Spec1 = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemSpec Spec2 = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId1 = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec1);
		FArcItemId ItemId2 = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec2);

		ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId1, TAG_Test_Slot_Primary);
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId2, TAG_Test_Slot_Secondary);

		bool bChanged = ArcMassItems::ChangeItemSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId1, TAG_Test_Slot_Secondary);
		ASSERT_THAT(IsFalse(bChanged));
	}

	TEST_METHOD(ChangeItemSlot_SameSlot_Succeeds)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, TAG_Test_Slot_Primary);

		bool bChanged = ArcMassItems::ChangeItemSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, TAG_Test_Slot_Primary);
		ASSERT_THAT(IsTrue(bChanged));
	}

	TEST_METHOD(ChangeItemSlot_WritesEventAndFiresItemSlotAssignedSignal)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		EntityManager->RemoveSparseElementFromEntity<FArcMassItemEventFragment>(Entity);

		// Item has no prior slot → ChangeItemSlot emits SlotAssigned (invalid→slot).
		FArcMassItemSignalListener Listener(*SignalSubsystem,
			ArcMassItems::TestHelpers::StoreQualifiedSignal(UE::ArcMassItems::Signals::ItemSlotAssigned));

		bool bChanged = ArcMassItems::ChangeItemSlot(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, TAG_Test_Slot_Primary);
		ASSERT_THAT(IsTrue(bChanged));

		const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, Entity)
			.GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
		ASSERT_THAT(IsNotNull(Events));
		ASSERT_THAT(AreEqual(1, Events->Events.Num()));
		ASSERT_THAT(AreEqual(EArcMassItemEventType::SlotAssigned, Events->Events[0].Type));
		FGameplayTag PrimarySlot = TAG_Test_Slot_Primary;
		ASSERT_THAT(AreEqual(PrimarySlot, Events->Events[0].SlotTag));

		ASSERT_THAT(AreEqual(1, Listener.NumReceivedFor(Entity)));
	}
};

TEST_CLASS(ArcMassItemOps_SocketDefaults, "ArcMassItems.Operations.SocketDefaults")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	TEST_METHOD(AddItem_WithSocketDefaults_CreatesChildren)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->ItemWithSocketDefaults, 1, 1);
		FArcItemId ParentId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ParentId.IsValid()));

		TArray<const FArcItemData*> Children = ArcMassItems::GetAttachedItems(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ParentId);
		ASSERT_THAT(IsTrue(Children.Num() > 0));
	}

	TEST_METHOD(AddItem_WithoutSocketDefaults_NoChildren)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ItemId.IsValid()));

		TArray<const FArcItemData*> Children = ArcMassItems::GetAttachedItems(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId);
		ASSERT_THAT(AreEqual(0, Children.Num()));
	}
};

TEST_CLASS(ArcMassItemOps_Destroy, "ArcMassItems.Operations.Destroy")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	TEST_METHOD(DestroyItem_RemovesFromStore)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		bool bDestroyed = ArcMassItems::DestroyItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId);
		ASSERT_THAT(IsTrue(bDestroyed));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId) == nullptr));
	}

	TEST_METHOD(DestroyItem_WritesRemovedEvent)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		EntityManager->RemoveSparseElementFromEntity<FArcMassItemEventFragment>(Entity);

		ArcMassItems::DestroyItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId);

		const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
		ASSERT_THAT(IsNotNull(Events));
		ASSERT_THAT(AreEqual(1, Events->Events.Num()));
		ASSERT_THAT(AreEqual(EArcMassItemEventType::Removed, Events->Events[0].Type));
	}

	TEST_METHOD(DestroyItem_CascadesToAttachments)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerSpec);

		FArcItemSpec AttachSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId AttachId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), AttachSpec);

		ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, TAG_Test_Socket_Barrel);

		ArcMassItems::DestroyItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId);

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(OwnerId) == nullptr));
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(AttachId) == nullptr));
	}

	TEST_METHOD(DestroyItem_ClearsOwnerLink)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerSpec);

		FArcItemSpec AttachSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId AttachId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), AttachSpec);

		ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, TAG_Test_Socket_Barrel);

		ArcMassItems::DestroyItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), AttachId);

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* OwnerItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(OwnerId);
		ASSERT_THAT(IsNotNull(OwnerItem));
		ASSERT_THAT(IsFalse(OwnerItem->ToItem()->GetAttachedItems().Contains(AttachId)));

		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(AttachId) == nullptr));
	}

	TEST_METHOD(DestroyItem_InvalidId_ReturnsFalse)
	{
		bool bDestroyed = ArcMassItems::DestroyItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), FArcItemId());
		ASSERT_THAT(IsFalse(bDestroyed));
	}
};

TEST_CLASS(ArcMassItemOps_AttachmentQuery, "ArcMassItems.Operations.AttachmentQuery")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	TEST_METHOD(GetAttachedItems_ReturnsAttachedItems)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerSpec);

		FArcItemSpec AttachSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId AttachId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), AttachSpec);

		ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, TAG_Test_Socket_Barrel);

		TArray<const FArcItemData*> Attachments = ArcMassItems::GetAttachedItems(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId);
		ASSERT_THAT(AreEqual(1, Attachments.Num()));
		ASSERT_THAT(AreEqual(AttachId, Attachments[0]->GetItemId()));
	}

	TEST_METHOD(GetAttachedItems_NoAttachments_ReturnsEmpty)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		TArray<const FArcItemData*> Attachments = ArcMassItems::GetAttachedItems(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId);
		ASSERT_THAT(AreEqual(0, Attachments.Num()));
	}

	TEST_METHOD(FindAttachedItemOnSlot_FindsCorrectSlot)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerSpec);

		FArcItemSpec AttachSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId AttachId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), AttachSpec);

		ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, TAG_Test_Socket_Barrel);

		const FArcItemData* Found = ArcMassItems::FindAttachedItemOnSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, TAG_Test_Socket_Barrel);
		ASSERT_THAT(IsNotNull(Found));
		ASSERT_THAT(AreEqual(AttachId, Found->GetItemId()));
	}

	TEST_METHOD(FindAttachedItemOnSlot_WrongSlot_ReturnsNull)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerSpec);

		FArcItemSpec AttachSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId AttachId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), AttachSpec);

		ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, TAG_Test_Socket_Barrel);

		const FArcItemData* Found = ArcMassItems::FindAttachedItemOnSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, TAG_Test_Slot_Primary);
		ASSERT_THAT(IsNull(Found));
	}

	TEST_METHOD(GetItemAttachedTo_ReturnsSameAsFindAttachedItemOnSlot)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerSpec);

		FArcItemSpec AttachSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId AttachId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), AttachSpec);

		ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, TAG_Test_Socket_Barrel);

		const FArcItemData* Found = ArcMassItems::GetItemAttachedTo(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, TAG_Test_Socket_Barrel);
		ASSERT_THAT(IsNotNull(Found));
		ASSERT_THAT(AreEqual(AttachId, Found->GetItemId()));
	}
};

TEST_CLASS(ArcMassItemOps_SetItemInstances, "ArcMassItems.Operations.SetItemInstances")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	// Verify that after AddItem with a stacking definition the instance's OwningItem
	// points back at the FArcItemData that owns it (not null, not a stale ptr).
	TEST_METHOD(SetItemInstances_OwningItemIsThis)
	{
		// ItemWithStacks has FArcItemFragment_Stacks which creates FArcItemInstance_Stacks.
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ItemId.IsValid()));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsNotNull(Store));
		const FArcMassReplicatedItem* ReplicatedItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(ReplicatedItem));
		const FArcItemData* ItemData = ReplicatedItem->ToItem();
		ASSERT_THAT(IsNotNull(ItemData));

		const FArcItemInstanceArray* Instances = ArcMassItems::GetItemInstances(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId);
		ASSERT_THAT(IsNotNull(Instances));
		ASSERT_THAT(IsTrue(Instances->Data.Num() > 0));

		const FArcItemInstance* Instance = Instances->Data[0].GetPtr<FArcItemInstance>();
		ASSERT_THAT(IsNotNull(Instance));
		ASSERT_THAT(AreEqual(ItemData, static_cast<const FArcItemData*>(Instance->OwningItem)));
	}

	// Verify that SetItemInstances with an external array replaces the item's instances
	// rather than silently ignoring the input (regression for the old no-op bug).
	TEST_METHOD(SetItemInstances_ExternalArray_ReplacesInstances)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ItemId.IsValid()));

		const FArcItemInstanceArray* Original = ArcMassItems::GetItemInstances(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId);
		ASSERT_THAT(IsNotNull(Original));
		const int32 OriginalNum = Original->Data.Num();
		ASSERT_THAT(IsTrue(OriginalNum > 0));

		// Build a distinct array with an additional instance appended.
		FArcItemInstanceArray NewArray = *Original;
		FInstancedStruct ExtraEntry = FInstancedStruct::Make(FArcItemInstance_Stacks());
		NewArray.Data.Add(MoveTemp(ExtraEntry));
		ASSERT_THAT(AreEqual(OriginalNum + 1, NewArray.Data.Num()));

		bool bSet = ArcMassItems::SetItemInstances(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, NewArray);
		ASSERT_THAT(IsTrue(bSet));

		const FArcItemInstanceArray* After = ArcMassItems::GetItemInstances(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId);
		ASSERT_THAT(IsNotNull(After));
		ASSERT_THAT(AreEqual(OriginalNum + 1, After->Data.Num()));
	}
};

TEST_CLASS(ArcMassItemOps_MoveAttachments, "ArcMassItems.Operations.MoveAttachments")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle EntityA;
	FMassEntityHandle EntityB;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		EntityA = CreateItemEntity();
		EntityB = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	TEST_METHOD(MoveItem_MovesAttachmentsToo)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, EntityA, FArcMassItemStoreFragment::StaticStruct(), OwnerSpec);

		FArcItemSpec AttachSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId AttachId = ArcMassItems::AddItem(*EntityManager, EntityA, FArcMassItemStoreFragment::StaticStruct(), AttachSpec);

		ArcMassItems::AttachItem(*EntityManager, EntityA, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, TAG_Test_Socket_Barrel);

		bool bMoved = ArcMassItems::MoveItem(*EntityManager, EntityA, EntityB, OwnerId,
			FArcMassItemStoreFragment::StaticStruct(), FArcMassItemStoreFragment::StaticStruct());
		ASSERT_THAT(IsTrue(bMoved));

		const FArcMassItemStoreFragment* StoreA = FMassEntityView(*EntityManager, EntityA).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(StoreA->ReplicatedItems).FindById(OwnerId) == nullptr));
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(StoreA->ReplicatedItems).FindById(AttachId) == nullptr));

		const FArcMassItemStoreFragment* StoreB = FMassEntityView(*EntityManager, EntityB).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(StoreB->ReplicatedItems).FindById(OwnerId) != nullptr));
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(StoreB->ReplicatedItems).FindById(AttachId) != nullptr));
	}
};

TEST_CLASS(ArcMassItemOps_MoveEntityHandle, "ArcMassItems.Operations")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle EntityA;
	FMassEntityHandle EntityB;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		EntityA = CreateItemEntity();
		EntityB = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	TEST_METHOD(MoveItem_UpdatesMassEntityHandle)
	{
		// Add a root item plus one child attachment so the descendants path is also exercised.
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, EntityA, FArcMassItemStoreFragment::StaticStruct(), OwnerSpec);

		FArcItemSpec ChildSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ChildId = ArcMassItems::AddItem(*EntityManager, EntityA, FArcMassItemStoreFragment::StaticStruct(), ChildSpec);

		ArcMassItems::AttachItem(*EntityManager, EntityA, FArcMassItemStoreFragment::StaticStruct(), OwnerId, ChildId, TAG_Test_Socket_Barrel);

		bool bMoved = ArcMassItems::MoveItem(*EntityManager, EntityA, EntityB, OwnerId,
			FArcMassItemStoreFragment::StaticStruct(), FArcMassItemStoreFragment::StaticStruct());
		ASSERT_THAT(IsTrue(bMoved));

		const FArcMassItemStoreFragment* StoreB = FMassEntityView(*EntityManager, EntityB).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsNotNull(StoreB));

		const FArcMassReplicatedItem* MovedOwner = const_cast<FArcMassReplicatedItemArray&>(StoreB->ReplicatedItems).FindById(OwnerId);
		ASSERT_THAT(IsNotNull(MovedOwner));
		ASSERT_THAT(IsTrue(MovedOwner->ToItem()->MassEntityHandle == EntityB));

		const FArcMassReplicatedItem* MovedChild = const_cast<FArcMassReplicatedItemArray&>(StoreB->ReplicatedItems).FindById(ChildId);
		ASSERT_THAT(IsNotNull(MovedChild));
		ASSERT_THAT(IsTrue(MovedChild->ToItem()->MassEntityHandle == EntityB));
	}

	TEST_METHOD(MoveItem_AttachedRoot_DetachedFromOwnerFirst)
	{
		// On EntityA: owner O, child C attached to O. Move C to EntityB.
		// After the move: O's AttachedItems/ItemsInSockets must be empty; C is in EntityB's store with no owner.
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, EntityA, FArcMassItemStoreFragment::StaticStruct(), OwnerSpec);

		FArcItemSpec ChildSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ChildId = ArcMassItems::AddItem(*EntityManager, EntityA, FArcMassItemStoreFragment::StaticStruct(), ChildSpec);

		ArcMassItems::AttachItem(*EntityManager, EntityA, FArcMassItemStoreFragment::StaticStruct(), OwnerId, ChildId, TAG_Test_Socket_Barrel);

		bool bMoved = ArcMassItems::MoveItem(*EntityManager, EntityA, EntityB, ChildId,
			FArcMassItemStoreFragment::StaticStruct(), FArcMassItemStoreFragment::StaticStruct());
		ASSERT_THAT(IsTrue(bMoved));

		// O must no longer list C as an attachment.
		const FArcMassItemStoreFragment* StoreA = FMassEntityView(*EntityManager, EntityA).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsNotNull(StoreA));
		const FArcMassReplicatedItem* OwnerItem = const_cast<FArcMassReplicatedItemArray&>(StoreA->ReplicatedItems).FindById(OwnerId);
		ASSERT_THAT(IsNotNull(OwnerItem));
		ASSERT_THAT(AreEqual(0, OwnerItem->ToItem()->GetAttachedItems().Num()));
		ASSERT_THAT(AreEqual(0, OwnerItem->ToItem()->GetItemsInSockets().Num()));

		// C is in EntityB's store and must have no owner.
		const FArcMassItemStoreFragment* StoreB = FMassEntityView(*EntityManager, EntityB).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsNotNull(StoreB));
		const FArcMassReplicatedItem* MovedChild = const_cast<FArcMassReplicatedItemArray&>(StoreB->ReplicatedItems).FindById(ChildId);
		ASSERT_THAT(IsNotNull(MovedChild));
		ASSERT_THAT(IsFalse(MovedChild->ToItem()->GetOwnerId().IsValid()));
	}
};

TEST_CLASS(ArcMassItemOps_AttachValidation, "ArcMassItems.Operations.AttachValidation")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	TEST_METHOD(AttachItem_SelfAttach_ReturnsFalse)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		bool bAttached = ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, ItemId, TAG_Test_Socket_Barrel);
		ASSERT_THAT(IsFalse(bAttached));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* Item = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(Item));
		ASSERT_THAT(IsFalse(Item->ToItem()->GetOwnerId().IsValid()));
	}

	TEST_METHOD(AttachItem_AlreadyOwned_ReturnsFalse)
	{
		FArcItemSpec SpecA = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId OwnerA = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecA);

		FArcItemSpec SpecB = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId OwnerB = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecB);

		FArcItemSpec SpecC = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId AttachC = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecC);

		// Attach C to A on slot S1 — must succeed.
		bool bFirst = ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerA, AttachC, TAG_Test_Socket_Barrel);
		ASSERT_THAT(IsTrue(bFirst));

		// Attempt to attach C to B on slot S2 — must fail; C already owned by A.
		bool bSecond = ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerB, AttachC, TAG_Test_Slot_Secondary);
		ASSERT_THAT(IsFalse(bSecond));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* ItemC = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(AttachC);
		ASSERT_THAT(IsNotNull(ItemC));
		ASSERT_THAT(AreEqual(OwnerA, ItemC->ToItem()->GetOwnerId()));
	}

	TEST_METHOD(AttachItem_OccupiedAttachSlot_ReturnsFalse)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerSpec);

		FArcItemSpec SpecC1 = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId C1 = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecC1);

		FArcItemSpec SpecC2 = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId C2 = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecC2);

		// Attach C1 to Owner on slot Barrel — must succeed.
		bool bFirst = ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, C1, TAG_Test_Socket_Barrel);
		ASSERT_THAT(IsTrue(bFirst));

		// Attempt to attach C2 to Owner on the same slot — must fail.
		bool bSecond = ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, C2, TAG_Test_Socket_Barrel);
		ASSERT_THAT(IsFalse(bSecond));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* OwnerItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(OwnerId);
		ASSERT_THAT(IsNotNull(OwnerItem));
		ASSERT_THAT(AreEqual(1, OwnerItem->ToItem()->GetAttachedItems().Num()));
		ASSERT_THAT(IsTrue(OwnerItem->ToItem()->GetAttachedItems().Contains(C1)));
	}

	TEST_METHOD(AttachItem_WouldCreateCycle_ReturnsFalse)
	{
		FArcItemSpec SpecA = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId IdA = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecA);

		FArcItemSpec SpecB = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId IdB = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), SpecB);

		// Attach B under A on slot S1 — must succeed.
		bool bFirst = ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), IdA, IdB, TAG_Test_Socket_Barrel);
		ASSERT_THAT(IsTrue(bFirst));

		// Attempt to attach A under B on slot S2 — would create A→B→A cycle, must fail.
		bool bSecond = ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), IdB, IdA, TAG_Test_Slot_Secondary);
		ASSERT_THAT(IsFalse(bSecond));
	}
};

TEST_CLASS(ArcMassItemOps_Hierarchy, "ArcMassItems.Operations.Hierarchy")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;
	FArcItemTestRecordFiles Records;
	FPrimaryAssetId TaggedItem;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
		return NewEntity;
	}

	// Owner = SimpleBaseItem, attachment = ItemWithDefinitionTags (distinct definitions so
	// fragments added to the attachment's definition are absent from the owner's).
	void AddOwnerAndAttachment(FArcItemId& OutOwnerId, FArcItemId& OutAttachId)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		OutOwnerId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerSpec);

		FArcItemSpec AttachSpec = FArcItemSpec::NewItem(TaggedItem, 1, 1);
		OutAttachId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), AttachSpec);
	}

	FArcItemData* FindItemData(FArcItemId ItemId)
	{
		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		FArcMassReplicatedItem* Item = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		return Item ? Item->ToItem() : nullptr;
	}

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
		TaggedItem = MassSettings->ItemWithDefinitionTags;
		ArcMassItems::TestHelpers::LifecycleLog().Reset();
	}

	TEST_METHOD(AttachItem_PopulatesItemsInSockets)
	{
		FArcItemId OwnerId, AttachId;
		AddOwnerAndAttachment(OwnerId, AttachId);

		ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, TAG_Test_Socket_Barrel);

		FArcItemData* OwnerData = FindItemData(OwnerId);
		ASSERT_THAT(IsNotNull(OwnerData));
		ASSERT_THAT(AreEqual(1, OwnerData->GetItemsInSockets().Num()));
		ASSERT_THAT(AreEqual(AttachId, OwnerData->GetItemsInSockets()[0]->GetItemId()));
	}

	TEST_METHOD(AttachItem_SetsOwnerCachedPtr)
	{
		FArcItemId OwnerId, AttachId;
		AddOwnerAndAttachment(OwnerId, AttachId);

		ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, TAG_Test_Socket_Barrel);

		FArcItemData* OwnerData = FindItemData(OwnerId);
		FArcItemData* AttachData = FindItemData(AttachId);
		ASSERT_THAT(IsNotNull(AttachData));
		ASSERT_THAT(IsTrue(AttachData->GetOwnerPtr() == OwnerData));
	}

	TEST_METHOD(DetachItem_ClearsHierarchyCaches)
	{
		FArcItemId OwnerId, AttachId;
		AddOwnerAndAttachment(OwnerId, AttachId);

		ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, TAG_Test_Socket_Barrel);
		ArcMassItems::DetachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), AttachId);

		FArcItemData* OwnerData = FindItemData(OwnerId);
		FArcItemData* AttachData = FindItemData(AttachId);
		ASSERT_THAT(AreEqual(0, OwnerData->GetItemsInSockets().Num()));
		ASSERT_THAT(IsTrue(AttachData->GetOwnerPtr() == nullptr));
	}

	TEST_METHOD(AttachItem_MergesScalableFloatFragments)
	{
		TaggedItem = ArcMassItems::TestHelpers::WriteWithScalableFragment(Records, MassSettings->ItemWithDefinitionTags);
		ASSERT_THAT(IsTrue(TaggedItem.IsValid()));

		FArcItemId OwnerId, AttachId;
		AddOwnerAndAttachment(OwnerId, AttachId);

		FArcItemData* OwnerData = FindItemData(OwnerId);
		ASSERT_THAT(IsTrue(OwnerData->FindScalableItemFragment<FArcScalableFloatItemFragment_Test>() == nullptr));

		ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, TAG_Test_Socket_Barrel);

		ASSERT_THAT(IsTrue(OwnerData->FindScalableItemFragment<FArcScalableFloatItemFragment_Test>() != nullptr));
	}

	TEST_METHOD(DetachItem_UnmergesScalableFloatFragments)
	{
		TaggedItem = ArcMassItems::TestHelpers::WriteWithScalableFragment(Records, MassSettings->ItemWithDefinitionTags);
		ASSERT_THAT(IsTrue(TaggedItem.IsValid()));

		FArcItemId OwnerId, AttachId;
		AddOwnerAndAttachment(OwnerId, AttachId);

		ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, TAG_Test_Socket_Barrel);
		ArcMassItems::DetachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), AttachId);

		FArcItemData* OwnerData = FindItemData(OwnerId);
		ASSERT_THAT(IsTrue(OwnerData->FindScalableItemFragment<FArcScalableFloatItemFragment_Test>() == nullptr));
	}

	TEST_METHOD(AttachItem_FiresOnItemAttachedTo)
	{
		TaggedItem = ArcMassItems::TestHelpers::WriteWithLifecycleRecorder(Records, MassSettings->ItemWithDefinitionTags);
		ASSERT_THAT(IsTrue(TaggedItem.IsValid()));

		FArcItemId OwnerId, AttachId;
		AddOwnerAndAttachment(OwnerId, AttachId);
		ArcMassItems::TestHelpers::LifecycleLog().Reset();

		ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, TAG_Test_Socket_Barrel);

		const TArray<FArcTestLifecycleEvent>& Log = ArcMassItems::TestHelpers::LifecycleLog();
		bool bFound = false;
		for (const FArcTestLifecycleEvent& Event : Log)
		{
			if (Event.Event == FName(TEXT("AttachedTo")) && Event.ItemId == AttachId && Event.OwnerId == OwnerId)
			{
				bFound = true;
			}
		}
		ASSERT_THAT(IsTrue(bFound));

		ArcMassItems::TestHelpers::LifecycleLog().Reset();
		ArcMassItems::DetachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), AttachId);

		bool bDetachFound = false;
		for (const FArcTestLifecycleEvent& Event : ArcMassItems::TestHelpers::LifecycleLog())
		{
			if (Event.Event == FName(TEXT("DetachedFrom")) && Event.ItemId == AttachId && Event.OwnerId == OwnerId)
			{
				bDetachFound = true;
			}
		}
		ASSERT_THAT(IsTrue(bDetachFound));
	}

	TEST_METHOD(FindFragment_TraversesToOwnerAndSockets)
	{
		TaggedItem = ArcMassItems::TestHelpers::WriteWithLifecycleRecorder(Records, MassSettings->ItemWithDefinitionTags);
		ASSERT_THAT(IsTrue(TaggedItem.IsValid()));

		FArcItemId OwnerId, AttachId;
		AddOwnerAndAttachment(OwnerId, AttachId);

		FArcItemData* OwnerData = FindItemData(OwnerId);
		ASSERT_THAT(IsTrue(ArcItemsHelper::FindFragment<FArcItemFragment_TestLifecycleRecorder>(OwnerData) == nullptr));

		ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, AttachId, TAG_Test_Socket_Barrel);

		// Owner finds the fragment through its socket item.
		ASSERT_THAT(IsTrue(ArcItemsHelper::FindFragment<FArcItemFragment_TestLifecycleRecorder>(OwnerData) != nullptr));

		// Attachment finds fragments through its owner traversal too (self-hit here).
		FArcItemData* AttachData = FindItemData(AttachId);
		ASSERT_THAT(IsTrue(ArcItemsHelper::FindFragment<FArcItemFragment_TestLifecycleRecorder>(AttachData) != nullptr));
	}
};

TEST_CLASS(ArcMassItemOps_RemoveLifecycle, "ArcMassItems.Operations.RemoveLifecycle")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;
	FArcItemTestRecordFiles Records;
	FPrimaryAssetId TaggedItem;

	FMassEntityHandle CreateItemEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
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
		Entity = CreateItemEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
		TaggedItem = MassSettings->ItemWithDefinitionTags;
		ArcMassItems::TestHelpers::LifecycleLog().Reset();
	}

	TEST_METHOD(RemoveItem_FullRemoval_FiresOnPreRemove)
	{
		TaggedItem = ArcMassItems::TestHelpers::WriteWithLifecycleRecorder(Records, MassSettings->ItemWithDefinitionTags);
		ASSERT_THAT(IsTrue(TaggedItem.IsValid()));

		FArcItemSpec Spec = FArcItemSpec::NewItem(TaggedItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::TestHelpers::LifecycleLog().Reset();

		bool bRemoved = ArcMassItems::RemoveItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId);
		ASSERT_THAT(IsTrue(bRemoved));

		bool bPreRemoveFired = false;
		for (const FArcTestLifecycleEvent& Event : ArcMassItems::TestHelpers::LifecycleLog())
		{
			if (Event.Event == FName(TEXT("PreRemove")) && Event.ItemId == ItemId)
			{
				bPreRemoveFired = true;
			}
		}
		ASSERT_THAT(IsTrue(bPreRemoveFired));
	}

	TEST_METHOD(RemoveItem_AttachedChild_DetachedFromOwner)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerSpec);

		FArcItemSpec ChildSpec = FArcItemSpec::NewItem(TaggedItem, 1, 1);
		FArcItemId ChildId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ChildSpec);

		ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, ChildId, TAG_Test_Socket_Barrel);

		bool bRemoved = ArcMassItems::RemoveItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ChildId);
		ASSERT_THAT(IsTrue(bRemoved));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* OwnerItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(OwnerId);
		ASSERT_THAT(IsNotNull(OwnerItem));
		ASSERT_THAT(AreEqual(0, OwnerItem->ToItem()->GetAttachedItems().Num()));
		ASSERT_THAT(AreEqual(0, OwnerItem->ToItem()->GetItemsInSockets().Num()));
	}

	TEST_METHOD(RemoveItem_SlottedItem_EmitsSlotCleared)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, TAG_Test_Slot_Primary);

		bool bRemoved = ArcMassItems::RemoveItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId);
		ASSERT_THAT(IsTrue(bRemoved));

		const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, Entity)
			.GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
		ASSERT_THAT(IsNotNull(Events));

		int32 NumSlotCleared = 0;
		int32 NumRemoved = 0;
		for (const FArcMassItemEvent& Event : Events->Events)
		{
			if (Event.ItemId == ItemId && Event.Type == EArcMassItemEventType::SlotCleared)
			{
				++NumSlotCleared;
			}
			if (Event.ItemId == ItemId && Event.Type == EArcMassItemEventType::Removed)
			{
				++NumRemoved;
			}
		}
		ASSERT_THAT(AreEqual(1, NumSlotCleared));
		ASSERT_THAT(AreEqual(1, NumRemoved));
	}
};
