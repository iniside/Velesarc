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
#include "Items/ArcItemData.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemTypes.h"
#include "Signals/ArcMassItemSignals.h"
#include "StructUtils/InstancedStruct.h"
#include "Mass/EntityFragments.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Slot_Primary, "Test.Item.Slot.Primary");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Socket_Barrel, "Test.Item.Socket.Barrel");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_State_Equipped, "Test.Item.State.Equipped");

TEST_CLASS(ArcMassItemOps_AddRemove, "ArcMassItems.Operations")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;

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
	}

	TEST_METHOD(AddItem_ReturnsValidId)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);
		ASSERT_THAT(IsTrue(ItemId.IsValid()));
	}

	TEST_METHOD(AddItem_ItemExistsInStore)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsNotNull(Store));
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId) != nullptr));
	}

	TEST_METHOD(AddItem_WritesEventFragment)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		ArcMassItems::AddItem(*EntityManager, Entity, Spec);

		const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
		ASSERT_THAT(IsNotNull(Events));
		ASSERT_THAT(AreEqual(1, Events->Events.Num()));
		ASSERT_THAT(AreEqual(EArcMassItemEventType::Added, Events->Events[0].Type));
	}

	TEST_METHOD(RemoveItem_RemovesFromStore)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);

		bool bRemoved = ArcMassItems::RemoveItem(*EntityManager, Entity, ItemId);
		ASSERT_THAT(IsTrue(bRemoved));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId) == nullptr));
	}

	TEST_METHOD(RemoveItem_InvalidId_ReturnsFalse)
	{
		bool bRemoved = ArcMassItems::RemoveItem(*EntityManager, Entity, FArcItemId());
		ASSERT_THAT(IsFalse(bRemoved));
	}

	TEST_METHOD(RemoveItem_ReducesStacks)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 5);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);

		bool bRemoved = ArcMassItems::RemoveItem(*EntityManager, Entity, ItemId, 3);
		ASSERT_THAT(IsTrue(bRemoved));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(AreEqual(static_cast<uint16>(2), FoundItem->ToItem()->GetStacks()));
	}

	TEST_METHOD(RemoveItem_ReduceStacksToZero_RemovesItem)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 3);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);

		bool bRemoved = ArcMassItems::RemoveItem(*EntityManager, Entity, ItemId, 3);
		ASSERT_THAT(IsTrue(bRemoved));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId) == nullptr));
	}

	TEST_METHOD(RemoveItem_WritesEventFragment)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);
		EntityManager->RemoveSparseElementFromEntity<FArcMassItemEventFragment>(Entity);

		ArcMassItems::RemoveItem(*EntityManager, Entity, ItemId);

		const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
		ASSERT_THAT(IsNotNull(Events));
		ASSERT_THAT(AreEqual(1, Events->Events.Num()));
		ASSERT_THAT(AreEqual(EArcMassItemEventType::Removed, Events->Events[0].Type));
	}

	TEST_METHOD(EventOverride_LastWriterWins)
	{
		FArcItemSpec Spec1 = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId ItemId1 = ArcMassItems::AddItem(*EntityManager, Entity, Spec1);

		FArcItemSpec Spec2 = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId ItemId2 = ArcMassItems::AddItem(*EntityManager, Entity, Spec2);

		const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
		ASSERT_THAT(IsNotNull(Events));
		ASSERT_THAT(AreEqual(1, Events->Events.Num()));
		ASSERT_THAT(AreEqual(ItemId2, Events->Events[0].ItemId));
	}
};

TEST_CLASS(ArcMassItemOps_Slots, "ArcMassItems.Operations.Slots")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;

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
	}

	TEST_METHOD(AddItemToSlot_SetsSlotOnItem)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);

		FGameplayTag SlotTag = TAG_Test_Slot_Primary;
		bool bSlotted = ArcMassItems::AddItemToSlot(*EntityManager, Entity, ItemId, SlotTag);

		ASSERT_THAT(IsTrue(bSlotted));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(AreEqual(SlotTag, FoundItem->ToItem()->GetSlotId()));
	}

	TEST_METHOD(AddItemToSlot_UpdatesSlottedItemsMap)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);

		FGameplayTag SlotTag = TAG_Test_Slot_Primary;
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, ItemId, SlotTag);

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsTrue(Store->SlottedItems.Contains(SlotTag)));
		ASSERT_THAT(AreEqual(ItemId, Store->SlottedItems[SlotTag]));
	}

	TEST_METHOD(AddItemToSlot_OccupiedSlot_ReturnsFalse)
	{
		FArcItemSpec Spec1 = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemSpec Spec2 = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId ItemId1 = ArcMassItems::AddItem(*EntityManager, Entity, Spec1);
		FArcItemId ItemId2 = ArcMassItems::AddItem(*EntityManager, Entity, Spec2);

		FGameplayTag SlotTag = TAG_Test_Slot_Primary;
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, ItemId1, SlotTag);

		bool bSlotted = ArcMassItems::AddItemToSlot(*EntityManager, Entity, ItemId2, SlotTag);
		ASSERT_THAT(IsFalse(bSlotted));
	}

	TEST_METHOD(AddItemToSlot_WritesSlotChangedEvent)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);
		EntityManager->RemoveSparseElementFromEntity<FArcMassItemEventFragment>(Entity);

		FGameplayTag SlotTag = TAG_Test_Slot_Primary;
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, ItemId, SlotTag);

		const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
		ASSERT_THAT(IsNotNull(Events));
		ASSERT_THAT(AreEqual(1, Events->Events.Num()));
		ASSERT_THAT(AreEqual(EArcMassItemEventType::SlotChanged, Events->Events[0].Type));
		ASSERT_THAT(AreEqual(SlotTag, Events->Events[0].SlotTag));
	}

	TEST_METHOD(RemoveItemFromSlot_ClearsSlot)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);

		FGameplayTag SlotTag = TAG_Test_Slot_Primary;
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, ItemId, SlotTag);

		bool bRemoved = ArcMassItems::RemoveItemFromSlot(*EntityManager, Entity, ItemId);
		ASSERT_THAT(IsTrue(bRemoved));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(IsFalse(FoundItem->ToItem()->GetSlotId().IsValid()));
		ASSERT_THAT(IsFalse(Store->SlottedItems.Contains(SlotTag)));
	}

	TEST_METHOD(RemoveItemFromSlot_UnslottedItem_ReturnsFalse)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);

		bool bRemoved = ArcMassItems::RemoveItemFromSlot(*EntityManager, Entity, ItemId);
		ASSERT_THAT(IsFalse(bRemoved));
	}
};

TEST_CLASS(ArcMassItemOps_Stacks, "ArcMassItems.Operations.Stacks")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;

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
	}

	TEST_METHOD(ModifyStacks_Positive_IncreasesStacks)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 5);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);

		bool bModified = ArcMassItems::ModifyStacks(*EntityManager, Entity, ItemId, 3);
		ASSERT_THAT(IsTrue(bModified));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(AreEqual(static_cast<uint16>(8), FoundItem->ToItem()->GetStacks()));
	}

	TEST_METHOD(ModifyStacks_Negative_DecreasesStacks)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 5);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);

		bool bModified = ArcMassItems::ModifyStacks(*EntityManager, Entity, ItemId, -2);
		ASSERT_THAT(IsTrue(bModified));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(AreEqual(static_cast<uint16>(3), FoundItem->ToItem()->GetStacks()));
	}

	TEST_METHOD(ModifyStacks_BelowZero_ReturnsFalse)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 2);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);

		bool bModified = ArcMassItems::ModifyStacks(*EntityManager, Entity, ItemId, -5);
		ASSERT_THAT(IsFalse(bModified));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(AreEqual(static_cast<uint16>(2), FoundItem->ToItem()->GetStacks()));
	}

	TEST_METHOD(ModifyStacks_WritesEvent)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 5);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);
		EntityManager->RemoveSparseElementFromEntity<FArcMassItemEventFragment>(Entity);

		ArcMassItems::ModifyStacks(*EntityManager, Entity, ItemId, 3);

		const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
		ASSERT_THAT(IsNotNull(Events));
		ASSERT_THAT(AreEqual(1, Events->Events.Num()));
		ASSERT_THAT(AreEqual(EArcMassItemEventType::StacksChanged, Events->Events[0].Type));
		ASSERT_THAT(AreEqual(3, Events->Events[0].StackDelta));
	}

	TEST_METHOD(ModifyStacks_InvalidItem_ReturnsFalse)
	{
		bool bModified = ArcMassItems::ModifyStacks(*EntityManager, Entity, FArcItemId(), 1);
		ASSERT_THAT(IsFalse(bModified));
	}
};

TEST_CLASS(ArcMassItemOps_Attachment, "ArcMassItems.Operations.Attachment")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;

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
	}

	TEST_METHOD(AttachItem_SetsOwnerAndSlot)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, Entity, OwnerSpec);

		FArcItemSpec AttachSpec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId AttachId = ArcMassItems::AddItem(*EntityManager, Entity, AttachSpec);

		FGameplayTag AttachSlot = TAG_Test_Socket_Barrel;
		bool bAttached = ArcMassItems::AttachItem(*EntityManager, Entity, OwnerId, AttachId, AttachSlot);
		ASSERT_THAT(IsTrue(bAttached));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(AttachId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(AreEqual(OwnerId, FoundItem->ToItem()->GetOwnerId()));
		ASSERT_THAT(AreEqual(AttachSlot, FoundItem->ToItem()->GetAttachSlot()));
	}

	TEST_METHOD(AttachItem_AddsToOwnerAttachedItems)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, Entity, OwnerSpec);

		FArcItemSpec AttachSpec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId AttachId = ArcMassItems::AddItem(*EntityManager, Entity, AttachSpec);

		FGameplayTag AttachSlot = TAG_Test_Socket_Barrel;
		ArcMassItems::AttachItem(*EntityManager, Entity, OwnerId, AttachId, AttachSlot);

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* OwnerItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(OwnerId);
		ASSERT_THAT(IsNotNull(OwnerItem));
		ASSERT_THAT(IsTrue(OwnerItem->ToItem()->GetAttachedItems().Contains(AttachId)));
	}

	TEST_METHOD(DetachItem_ClearsOwnerAndSlot)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, Entity, OwnerSpec);

		FArcItemSpec AttachSpec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId AttachId = ArcMassItems::AddItem(*EntityManager, Entity, AttachSpec);

		FGameplayTag AttachSlot = TAG_Test_Socket_Barrel;
		ArcMassItems::AttachItem(*EntityManager, Entity, OwnerId, AttachId, AttachSlot);

		bool bDetached = ArcMassItems::DetachItem(*EntityManager, Entity, AttachId);
		ASSERT_THAT(IsTrue(bDetached));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(AttachId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(IsFalse(FoundItem->ToItem()->GetOwnerId().IsValid()));
		ASSERT_THAT(IsFalse(FoundItem->ToItem()->GetAttachSlot().IsValid()));
	}

	TEST_METHOD(RemoveOwner_CascadesAttachmentRemoval)
	{
		FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId OwnerId = ArcMassItems::AddItem(*EntityManager, Entity, OwnerSpec);

		FArcItemSpec AttachSpec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId AttachId = ArcMassItems::AddItem(*EntityManager, Entity, AttachSpec);

		FGameplayTag AttachSlot = TAG_Test_Socket_Barrel;
		ArcMassItems::AttachItem(*EntityManager, Entity, OwnerId, AttachId, AttachSlot);

		ArcMassItems::RemoveItem(*EntityManager, Entity, OwnerId);

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
	}

	TEST_METHOD(AddDynamicTag_TagIsPresent)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);

		FGameplayTag Tag = TAG_Test_State_Equipped;
		bool bAdded = ArcMassItems::AddDynamicTag(*EntityManager, Entity, ItemId, Tag);
		ASSERT_THAT(IsTrue(bAdded));

		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* FoundItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(FoundItem));
		ASSERT_THAT(IsTrue(FoundItem->ToItem()->DynamicTags.HasTag(Tag)));
	}

	TEST_METHOD(RemoveDynamicTag_TagIsRemoved)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);

		FGameplayTag Tag = TAG_Test_State_Equipped;
		ArcMassItems::AddDynamicTag(*EntityManager, Entity, ItemId, Tag);
		bool bRemoved = ArcMassItems::RemoveDynamicTag(*EntityManager, Entity, ItemId, Tag);
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
	}

	TEST_METHOD(SetLevel_UpdatesLevel)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, Spec);

		bool bSet = ArcMassItems::SetLevel(*EntityManager, Entity, ItemId, 5);
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
	}

	TEST_METHOD(MoveItem_TransfersToTarget)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(FPrimaryAssetId(), 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, EntityA, Spec);

		bool bMoved = ArcMassItems::MoveItem(*EntityManager, EntityA, EntityB, ItemId);
		ASSERT_THAT(IsTrue(bMoved));

		const FArcMassItemStoreFragment* StoreA = FMassEntityView(*EntityManager, EntityA).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(StoreA->ReplicatedItems).FindById(ItemId) == nullptr));

		const FArcMassItemStoreFragment* StoreB = FMassEntityView(*EntityManager, EntityB).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsTrue(const_cast<FArcMassReplicatedItemArray&>(StoreB->ReplicatedItems).FindById(ItemId) != nullptr));
	}

	TEST_METHOD(MoveItem_InvalidItem_ReturnsFalse)
	{
		bool bMoved = ArcMassItems::MoveItem(*EntityManager, EntityA, EntityB, FArcItemId());
		ASSERT_THAT(IsFalse(bMoved));
	}
};
