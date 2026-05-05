// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
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
#include "Core/ArcCoreAssetManager.h"
#include "ArcMassItemTestsSettings.h"
#include "Signals/ArcMassItemSignals.h"
#include "MassSignalSubsystem.h"
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

		FArcMassItemSignalListener Listener(*SignalSubsystem,
			ArcMassItems::TestHelpers::StoreQualifiedSignal(UE::ArcMassItems::Signals::ItemSlotChanged));

		bool bSlotted = ArcMassItems::AddItemToSlot(*EntityManager, Entity,
			FArcMassItemStoreFragment::StaticStruct(), ItemId2, SlotTag);
		ASSERT_THAT(IsFalse(bSlotted));
		ASSERT_THAT(IsFalse(Listener.ReceivedAny()));
	}

	TEST_METHOD(AddItemToSlot_WritesEventAndFiresItemSlotChangedSignal)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		EntityManager->RemoveSparseElementFromEntity<FArcMassItemEventFragment>(Entity);

		FArcMassItemSignalListener Listener(*SignalSubsystem,
			ArcMassItems::TestHelpers::StoreQualifiedSignal(UE::ArcMassItems::Signals::ItemSlotChanged));

		FGameplayTag SlotTag = TAG_Test_Slot_Primary;
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, SlotTag);

		const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, Entity)
			.GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
		ASSERT_THAT(IsNotNull(Events));
		ASSERT_THAT(AreEqual(1, Events->Events.Num()));
		ASSERT_THAT(AreEqual(EArcMassItemEventType::SlotChanged, Events->Events[0].Type));
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

		FArcMassItemStackMethod_StackByType StackMethod;
		FArcItemSpec QuerySpec = FArcItemSpec::NewItem(MassSettings->ItemWithStacks, 1, 1);
		ASSERT_THAT(IsTrue(StackMethod.CanStackMass(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), QuerySpec)));
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

	TEST_METHOD(FindItemByDefinition_DefinitionPointer_ReturnsItem)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		const UArcItemDefinition* Def = Spec.GetItemDefinition();
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		const FArcItemData* Found = ArcMassItems::FindItemByDefinition(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Def);
		ASSERT_THAT(IsNotNull(Found));
		ASSERT_THAT(AreEqual(ItemId, Found->GetItemId()));
	}

	TEST_METHOD(FindItemByDefinition_DefinitionPointer_NullDef_ReturnsNull)
	{
		const FArcItemData* Found = ArcMassItems::FindItemByDefinition(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), static_cast<const UArcItemDefinition*>(nullptr));
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
		const UArcItemDefinition* Def = Spec.GetItemDefinition();
		ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		ASSERT_THAT(IsTrue(ArcMassItems::Contains(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Def)));
	}

	TEST_METHOD(Contains_ReturnsFalseWhenNotFound)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		const UArcItemDefinition* Def = Spec.GetItemDefinition();

		ASSERT_THAT(IsFalse(ArcMassItems::Contains(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Def)));
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
		const UArcItemDefinition* Def = UArcCoreAssetManager::Get().GetAsset<UArcItemDefinition>(MassSettings->ItemWithDefinitionTags.AssetId);
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
		const UArcItemDefinition* Def = UArcCoreAssetManager::Get().GetAsset<UArcItemDefinition>(MassSettings->ItemWithDefinitionTags.AssetId);
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

	TEST_METHOD(ChangeItemSlot_WritesEventAndFiresItemSlotChangedSignal)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		EntityManager->RemoveSparseElementFromEntity<FArcMassItemEventFragment>(Entity);

		FArcMassItemSignalListener Listener(*SignalSubsystem,
			ArcMassItems::TestHelpers::StoreQualifiedSignal(UE::ArcMassItems::Signals::ItemSlotChanged));

		bool bChanged = ArcMassItems::ChangeItemSlot(
			*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, TAG_Test_Slot_Primary);
		ASSERT_THAT(IsTrue(bChanged));

		const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, Entity)
			.GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
		ASSERT_THAT(IsNotNull(Events));
		ASSERT_THAT(AreEqual(1, Events->Events.Num()));
		ASSERT_THAT(AreEqual(EArcMassItemEventType::SlotChanged, Events->Events[0].Type));
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
