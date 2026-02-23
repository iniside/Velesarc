// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcItemsTests.h"

#include "CQTest.h"

#include "Items/ArcItemSpec.h"
#include "ArcItemTestsSettings.h"
#include "NativeGameplayTags.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Items/ArcItemsArray.h"
#include "Items/ArcItemData.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Items/Fragments/ArcItemFragment_AbilityEffectsToApply.h"
#include "Items/Fragments/ArcItemFragment_GrantedGameplayEffects.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"
#include "Items/Fragments/ArcItemFragment_Stacks.h"
#include "Commands/ArcMoveItemBetweenStoresCommand.h"
#include "Components/ActorTestSpawner.h"
#include "Items/ArcItemsStoreSubsystem.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_IntegrationSlot_01, "QuickSlotId.ArcCoreTest.01");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_IntegrationSlot_02, "QuickSlotId.ArcCoreTest.02");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_IntegrationSlot_03, "QuickSlotId.ArcCoreTest.03");

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_IntegrationEffectSpec_01, "GameplayEffect.Test.EffectToApply.01");

// ============================================================================
// Item Add / Remove Tests
// ============================================================================

TEST_CLASS(ArcItemsIntegration_AddRemove, "ArcCore.Integration")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(AddItem_Basic)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		ASSERT_THAT(IsTrue(Id.IsValid(), TEXT("Returned ItemId should be valid")));

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		ASSERT_THAT(IsNotNull(ItemPtr, TEXT("Item should exist in store after AddItem")));
		ASSERT_THAT(AreEqual(Actor.ItemsStoreNotReplicated->GetItemNum(), 1));
	}

	TEST_METHOD(AddItem_MultipleItems)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec1;
		Spec1.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId Id1 = Actor.ItemsStoreNotReplicated->AddItem(Spec1, FArcItemId::InvalidId);

		FArcItemSpec Spec2;
		Spec2.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(2);
		FArcItemId Id2 = Actor.ItemsStoreNotReplicated->AddItem(Spec2, FArcItemId::InvalidId);

		ASSERT_THAT(IsTrue(Id1 != Id2, TEXT("Two items should have different IDs")));
		ASSERT_THAT(AreEqual(Actor.ItemsStoreNotReplicated->GetItemNum(), 2));

		const FArcItemData* Item1 = Actor.ItemsStoreNotReplicated->GetItemPtr(Id1);
		const FArcItemData* Item2 = Actor.ItemsStoreNotReplicated->GetItemPtr(Id2);
		ASSERT_THAT(IsNotNull(Item1));
		ASSERT_THAT(IsNotNull(Item2));
	}

	TEST_METHOD(AddItem_WithLevel)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(5);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		ASSERT_THAT(IsNotNull(ItemPtr));
		ASSERT_THAT(AreEqual((int32)ItemPtr->GetLevel(), 5));
	}

	TEST_METHOD(DestroyItem_Basic)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		ASSERT_THAT(AreEqual(Actor.ItemsStoreNotReplicated->GetItemNum(), 1));

		Actor.ItemsStoreNotReplicated->DestroyItem(Id);

		ASSERT_THAT(AreEqual(Actor.ItemsStoreNotReplicated->GetItemNum(), 0));
		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		ASSERT_THAT(IsNull(ItemPtr, TEXT("Item should be gone after DestroyItem")));
	}

	TEST_METHOD(DestroyItem_WithAttachments_RemovesAll)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStoreNotReplicated->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->AddItemToSlot(OwnerItemId, TAG_IntegrationSlot_01);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStoreNotReplicated->AddItem(AttachedSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_IntegrationSlot_02);

		ASSERT_THAT(AreEqual(Actor.ItemsStoreNotReplicated->GetItemNum(), 2));

		Actor.ItemsStoreNotReplicated->DestroyItem(OwnerItemId);

		ASSERT_THAT(AreEqual(Actor.ItemsStoreNotReplicated->GetItemNum(), 0));
	}
};

// ============================================================================
// Slot Management Tests
// ============================================================================

TEST_CLASS(ArcItemsIntegration_Slots, "ArcCore.Integration")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(AddItemToSlot_Basic)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->AddItemToSlot(Id, TAG_IntegrationSlot_01);

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemFromSlot(TAG_IntegrationSlot_01);
		ASSERT_THAT(IsNotNull(ItemPtr, TEXT("Item should be retrievable from slot")));
		ASSERT_THAT(AreEqual(ItemPtr->GetSlotId(), TAG_IntegrationSlot_01));
	}

	TEST_METHOD(RemoveItemFromSlot_Basic)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->AddItemToSlot(Id, TAG_IntegrationSlot_01);

		const FArcItemData* Before = Actor.ItemsStoreNotReplicated->GetItemFromSlot(TAG_IntegrationSlot_01);
		ASSERT_THAT(IsNotNull(Before));

		Actor.ItemsStoreNotReplicated->RemoveItemFromSlot(Id);

		const FArcItemData* After = Actor.ItemsStoreNotReplicated->GetItemFromSlot(TAG_IntegrationSlot_01);
		ASSERT_THAT(IsNull(After, TEXT("Slot should be empty after RemoveItemFromSlot")));

		const FArcItemData* StillExists = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		ASSERT_THAT(IsNotNull(StillExists, TEXT("Item should still exist in store, just removed from slot")));
		ASSERT_THAT(IsTrue(!StillExists->GetSlotId().IsValid(), TEXT("Item slot should be invalid")));
	}

	TEST_METHOD(ChangeItemSlot)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->AddItemToSlot(Id, TAG_IntegrationSlot_01);

		Actor.ItemsStoreNotReplicated->ChangeItemSlot(Id, TAG_IntegrationSlot_02);

		const FArcItemData* OldSlot = Actor.ItemsStoreNotReplicated->GetItemFromSlot(TAG_IntegrationSlot_01);
		ASSERT_THAT(IsNull(OldSlot, TEXT("Old slot should be empty")));

		const FArcItemData* NewSlot = Actor.ItemsStoreNotReplicated->GetItemFromSlot(TAG_IntegrationSlot_02);
		ASSERT_THAT(IsNotNull(NewSlot, TEXT("Item should be in new slot")));
		ASSERT_THAT(AreEqual(NewSlot->GetSlotId(), TAG_IntegrationSlot_02));
	}
};

// ============================================================================
// Attachment Tests
// ============================================================================

TEST_CLASS(ArcItemsIntegration_Attachments, "ArcCore.Integration")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(AttachItem_Basic)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStoreNotReplicated->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->AddItemToSlot(OwnerItemId, TAG_IntegrationSlot_01);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStoreNotReplicated->AddItem(AttachedSpec, FArcItemId::InvalidId);

		Actor.ItemsStoreNotReplicated->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_IntegrationSlot_02);

		TArray<const FArcItemData*> AttachedItems = Actor.ItemsStoreNotReplicated->GetAttachedItems(OwnerItemId);
		ASSERT_THAT(AreEqual(AttachedItems.Num(), 1));
		ASSERT_THAT(AreEqual(AttachedItems[0]->GetOwnerId(), OwnerItemId));

		const FArcItemData* AttachedItemPtr = Actor.ItemsStoreNotReplicated->GetItemAttachedTo(OwnerItemId, TAG_IntegrationSlot_02);
		ASSERT_THAT(IsNotNull(AttachedItemPtr));
		ASSERT_THAT(AreEqual(AttachedItemPtr->GetItemId(), AttachedItemId));
	}

	TEST_METHOD(DetachItem_Basic)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStoreNotReplicated->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->AddItemToSlot(OwnerItemId, TAG_IntegrationSlot_01);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStoreNotReplicated->AddItem(AttachedSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_IntegrationSlot_02);

		TArray<const FArcItemData*> Before = Actor.ItemsStoreNotReplicated->GetAttachedItems(OwnerItemId);
		ASSERT_THAT(AreEqual(Before.Num(), 1));

		Actor.ItemsStoreNotReplicated->DetachItemFrom(AttachedItemId);

		TArray<const FArcItemData*> After = Actor.ItemsStoreNotReplicated->GetAttachedItems(OwnerItemId);
		ASSERT_THAT(AreEqual(After.Num(), 0));
	}

	TEST_METHOD(AttachItem_InvalidSlot_NoAttachment)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStoreNotReplicated->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->AddItemToSlot(OwnerItemId, TAG_IntegrationSlot_01);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStoreNotReplicated->AddItem(AttachedSpec, FArcItemId::InvalidId);

		Actor.ItemsStoreNotReplicated->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_IntegrationSlot_03);

		TArray<const FArcItemData*> AttachedItems = Actor.ItemsStoreNotReplicated->GetAttachedItems(OwnerItemId);
		ASSERT_THAT(AreEqual(AttachedItems.Num(), 0));
	}

	TEST_METHOD(RemoveOwnerFromSlot_AttachmentStillExists)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStoreNotReplicated->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->AddItemToSlot(OwnerItemId, TAG_IntegrationSlot_01);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStoreNotReplicated->AddItem(AttachedSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_IntegrationSlot_02);

		Actor.ItemsStoreNotReplicated->RemoveItemFromSlot(OwnerItemId);

		TArray<const FArcItemData*> AttachedItems = Actor.ItemsStoreNotReplicated->GetAttachedItems(OwnerItemId);
		ASSERT_THAT(AreEqual(AttachedItems.Num(), 1));
		ASSERT_THAT(AreEqual(AttachedItems[0]->GetItemId(), AttachedItemId));
	}
};

// ============================================================================
// Move Between Stores Tests
// ============================================================================

TEST_CLASS(ArcItemsIntegration_MoveBetweenStores, "ArcCore.Integration")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(MoveItem_Basic_BetweenStores)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		ASSERT_THAT(AreEqual(Actor.ItemsStoreNotReplicated->GetItemNum(), 1));
		ASSERT_THAT(AreEqual(Actor.ItemsStore2->GetItemNum(), 0));

		FArcMoveItemBetweenStoresCommand Command(Actor.ItemsStoreNotReplicated, Actor.ItemsStore2, Id, 1);
		Command.Execute();

		ASSERT_THAT(AreEqual(Actor.ItemsStoreNotReplicated->GetItemNum(), 0));
		ASSERT_THAT(AreEqual(Actor.ItemsStore2->GetItemNum(), 1));
	}

	TEST_METHOD(MoveItem_WithAttachments_CopyHelper)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStoreNotReplicated->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->AddItemToSlot(OwnerItemId, TAG_IntegrationSlot_01);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStoreNotReplicated->AddItem(AttachedSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_IntegrationSlot_02);

		ASSERT_THAT(AreEqual(Actor.ItemsStoreNotReplicated->GetItemNum(), 2));

		FArcItemCopyContainerHelper Copy = Actor.ItemsStoreNotReplicated->GetItemCopyHelper(OwnerItemId);
		TArray<FArcItemId> NewIds = Actor.ItemsStore2->AddItemDataInternal(Copy);

		ASSERT_THAT(IsTrue(NewIds.Num() > 0, TEXT("Should have added items")));
		ASSERT_THAT(AreEqual(Actor.ItemsStore2->GetItemNum(), 2));

		const FArcItemData* CopiedOwner = Actor.ItemsStore2->GetItemPtr(NewIds[0]);
		ASSERT_THAT(IsNotNull(CopiedOwner, TEXT("Copied owner should exist")));

		const FArcItemData* CopiedAttachment = Actor.ItemsStore2->GetItemAttachedTo(NewIds[0], TAG_IntegrationSlot_02);
		ASSERT_THAT(IsNotNull(CopiedAttachment, TEXT("Attachment should be copied with owner")));
	}

	TEST_METHOD(MoveItem_WithAttachments_DestroyOriginal)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStoreNotReplicated->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->AddItemToSlot(OwnerItemId, TAG_IntegrationSlot_01);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStoreNotReplicated->AddItem(AttachedSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_IntegrationSlot_02);

		const FArcItemDataInternal* InternalItem = Actor.ItemsStoreNotReplicated->GetInternalItem(OwnerItemId);
		FArcItemCopyContainerHelper Copy = FArcItemCopyContainerHelper::New(Actor.ItemsStoreNotReplicated, *InternalItem);
		TArray<FArcItemId> NewIds = Actor.ItemsStore2->AddItemDataInternal(Copy);

		Actor.ItemsStoreNotReplicated->DestroyItem(OwnerItemId);

		ASSERT_THAT(AreEqual(Actor.ItemsStoreNotReplicated->GetItemNum(), 0));
		ASSERT_THAT(AreEqual(Actor.ItemsStore2->GetItemNum(), 2));

		const FArcItemData* CopiedAttachment = Actor.ItemsStore2->GetItemAttachedTo(NewIds[0], TAG_IntegrationSlot_02);
		ASSERT_THAT(IsNotNull(CopiedAttachment, TEXT("Attachment in target store should survive source destruction")));
	}

	TEST_METHOD(MoveItem_WithoutAttachments_BetweenStores)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(3);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		FArcItemCopyContainerHelper Copy = Actor.ItemsStoreNotReplicated->GetItemCopyHelper(Id);

		ASSERT_THAT(AreEqual(Copy.ItemAttachments.Num(), 0));

		TArray<FArcItemId> NewIds = Actor.ItemsStore2->AddItemDataInternal(Copy);
		ASSERT_THAT(AreEqual(NewIds.Num(), 1));
		ASSERT_THAT(AreEqual(Actor.ItemsStore2->GetItemNum(), 1));

		const FArcItemData* CopiedItem = Actor.ItemsStore2->GetItemPtr(NewIds[0]);
		ASSERT_THAT(IsNotNull(CopiedItem));
		ASSERT_THAT(AreEqual((int32)CopiedItem->GetLevel(), 3));
	}
};

// ============================================================================
// FArcItemCopyContainerHelper Tests
// ============================================================================

TEST_CLASS(ArcItemsIntegration_CopyContainerHelper, "ArcCore.Integration")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(CopyHelper_New_FromItemsStore_WithoutAttachments)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		FArcItemCopyContainerHelper Copy = Actor.ItemsStoreNotReplicated->GetItemCopyHelper(Id);

		ASSERT_THAT(IsTrue(Copy.Item.GetItemDefinitionId() == Settings->SimpleBaseItem.AssetId,
			TEXT("Copy should preserve item definition")));
		ASSERT_THAT(AreEqual(Copy.ItemAttachments.Num(), 0));
	}

	TEST_METHOD(CopyHelper_New_FromItemsStore_WithAttachments)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStoreNotReplicated->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->AddItemToSlot(OwnerItemId, TAG_IntegrationSlot_01);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStoreNotReplicated->AddItem(AttachedSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_IntegrationSlot_02);

		FArcItemCopyContainerHelper Copy = Actor.ItemsStoreNotReplicated->GetItemCopyHelper(OwnerItemId);

		ASSERT_THAT(AreEqual(Copy.ItemAttachments.Num(), 1));
		ASSERT_THAT(AreEqual(Copy.SlotId, TAG_IntegrationSlot_01));
		ASSERT_THAT(AreEqual(Copy.ItemAttachments[0].SlotId, TAG_IntegrationSlot_02));
	}

	TEST_METHOD(CopyHelper_New_FromItemData)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		FArcItemCopyContainerHelper Copy = FArcItemCopyContainerHelper::New(ItemPtr);

		ASSERT_THAT(IsTrue(Copy.Item.GetItemDefinitionId() == Settings->SimpleBaseItem.AssetId));
	}

	TEST_METHOD(CopyHelper_FromSpec)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(5);
		Spec.ItemId = FArcItemId::Generate();

		FArcItemCopyContainerHelper Copy = FArcItemCopyContainerHelper::FromSpec(Spec);

		ASSERT_THAT(IsTrue(Copy.Item.GetItemDefinitionId() == Settings->SimpleBaseItem.AssetId));
		ASSERT_THAT(AreEqual((int32)Copy.Item.Level, 5));
		ASSERT_THAT(AreEqual(Copy.ItemAttachments.Num(), 0));
	}

	TEST_METHOD(CopyHelper_FromSpec_AddToStore)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(3);
		Spec.ItemId = FArcItemId::Generate();

		FArcItemCopyContainerHelper Copy = FArcItemCopyContainerHelper::FromSpec(Spec);
		TArray<FArcItemId> NewIds = Actor.ItemsStoreNotReplicated->AddItemDataInternal(Copy);

		ASSERT_THAT(AreEqual(NewIds.Num(), 1));
		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(NewIds[0]);
		ASSERT_THAT(IsNotNull(ItemPtr));
		ASSERT_THAT(AreEqual((int32)ItemPtr->GetLevel(), 3));
	}

	TEST_METHOD(CopyHelper_ToSpec_PreservesDefinition)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(4);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		FArcItemSpec ResultSpec = FArcItemCopyContainerHelper::ToSpec(ItemPtr);

		ASSERT_THAT(IsTrue(ResultSpec.GetItemDefinitionId() == Settings->SimpleBaseItem.AssetId,
			TEXT("ToSpec should preserve item definition")));
		ASSERT_THAT(AreEqual((int32)ResultSpec.Level, 4));
		ASSERT_THAT(AreEqual(ResultSpec.ItemId, Id));
	}

	TEST_METHOD(CopyHelper_ToSpec_PreservesPersistentInstances)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemFragment_ItemStats* ItemStats = new FArcItemFragment_ItemStats();
		ItemStats->DefaultStats.Add({UArcCoreTestAttributeSet::GetTestAttributeAAttribute(), 25.f});

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1)
			.AddInstanceData(ItemStats);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);

		const FArcItemInstance_ItemStats* StatsInstance = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(ItemPtr);
		ASSERT_THAT(IsNotNull(StatsInstance, TEXT("Stats instance should exist")));
		ASSERT_THAT(IsTrue(StatsInstance->ShouldPersist(), TEXT("ItemStats should persist")));

		FArcItemSpec ResultSpec = FArcItemCopyContainerHelper::ToSpec(ItemPtr);

		ASSERT_THAT(IsTrue(ResultSpec.InitialInstanceData.Num() > 0,
			TEXT("ToSpec should copy persistent instance data")));
	}

	TEST_METHOD(CopyHelper_ToSpec_RoundTrip)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemFragment_ItemStats* ItemStats = new FArcItemFragment_ItemStats();
		ItemStats->DefaultStats.Add({UArcCoreTestAttributeSet::GetTestAttributeAAttribute(), 42.f});

		FArcItemSpec OriginalSpec;
		OriginalSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(7)
			.AddInstanceData(ItemStats);
		FArcItemId OrigId = Actor.ItemsStoreNotReplicated->AddItem(OriginalSpec, FArcItemId::InvalidId);

		const FArcItemData* OrigPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(OrigId);
		FArcItemSpec ConvertedSpec = FArcItemCopyContainerHelper::ToSpec(OrigPtr);

		FArcItemCopyContainerHelper Copy = FArcItemCopyContainerHelper::FromSpec(ConvertedSpec);
		TArray<FArcItemId> NewIds = Actor.ItemsStore2->AddItemDataInternal(Copy);

		ASSERT_THAT(AreEqual(NewIds.Num(), 1));
		const FArcItemData* NewPtr = Actor.ItemsStore2->GetItemPtr(NewIds[0]);
		ASSERT_THAT(IsNotNull(NewPtr));
		ASSERT_THAT(AreEqual((int32)NewPtr->GetLevel(), 7));

		const FArcItemInstance_ItemStats* NewStats = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(NewPtr);
		ASSERT_THAT(IsNotNull(NewStats, TEXT("Stats instance should survive round-trip")));
		float StatValue = NewStats->GetStatValue(UArcCoreTestAttributeSet::GetTestAttributeAAttribute());
		ASSERT_THAT(IsNear(StatValue, 42.f, 0.1f));
	}
};

// ============================================================================
// Granted Abilities Fragment Tests
// ============================================================================

TEST_CLASS(ArcItemsIntegration_GrantedAbilities, "ArcCore.Integration")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(AddItemToSlot_GrantsAbility)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->AddItemToSlot(Id, TAG_IntegrationSlot_01);

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemFromSlot(TAG_IntegrationSlot_01);
		ASSERT_THAT(IsNotNull(ItemPtr));

		const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(ItemPtr);
		ASSERT_THAT(IsNotNull(Instance, TEXT("GrantedAbilities instance should exist")));
		ASSERT_THAT(AreEqual(Instance->GetGrantedAbilities().Num(), 1));
	}

	TEST_METHOD(RemoveItemFromSlot_RevokesAbility)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->AddItemToSlot(Id, TAG_IntegrationSlot_01);

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemFromSlot(TAG_IntegrationSlot_01);
		const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(ItemPtr);
		ASSERT_THAT(AreEqual(Instance->GetGrantedAbilities().Num(), 1));

		Actor.ItemsStoreNotReplicated->RemoveItemFromSlot(Id);

		ASSERT_THAT(AreEqual(Instance->GetGrantedAbilities().Num(), 0));
		ASSERT_THAT(IsTrue(!ItemPtr->GetSlotId().IsValid()));
	}

	TEST_METHOD(AddItemNotOnSlot_NoAbilityGranted)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(ItemPtr);
		ASSERT_THAT(IsNotNull(Instance));
		ASSERT_THAT(AreEqual(Instance->GetGrantedAbilities().Num(), 0));
	}

	TEST_METHOD(AttachedItem_GrantsAbility_WhenOwnerOnSlot)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStoreNotReplicated->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->AddItemToSlot(OwnerItemId, TAG_IntegrationSlot_01);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1).SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStoreNotReplicated->AddItem(AttachedSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_IntegrationSlot_02);

		FArcItemData* AttachItemData = Actor.ItemsStoreNotReplicated->GetItemPtr(AttachedItemId);
		const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(AttachItemData);
		ASSERT_THAT(IsNotNull(Instance));
		ASSERT_THAT(AreEqual(Instance->GetGrantedAbilities().Num(), 1));
	}

	TEST_METHOD(AttachedItem_NoAbility_WhenOwnerNotOnSlot)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStoreNotReplicated->AddItem(OwnerSpec, FArcItemId::InvalidId);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1).SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStoreNotReplicated->AddItem(AttachedSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_IntegrationSlot_02);

		FArcItemData* AttachItemData = Actor.ItemsStoreNotReplicated->GetItemPtr(AttachedItemId);
		const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(AttachItemData);
		ASSERT_THAT(IsNotNull(Instance));
		ASSERT_THAT(AreEqual(Instance->GetGrantedAbilities().Num(), 0));
	}

	TEST_METHOD(AttachedItem_AbilityGranted_ThenOwnerRemovedFromSlot_AbilityRevoked)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStoreNotReplicated->AddItem(OwnerSpec, FArcItemId::InvalidId);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1).SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStoreNotReplicated->AddItem(AttachedSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_IntegrationSlot_02);

		FArcItemData* AttachItemData = Actor.ItemsStoreNotReplicated->GetItemPtr(AttachedItemId);
		const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(AttachItemData);
		ASSERT_THAT(AreEqual(Instance->GetGrantedAbilities().Num(), 0));

		Actor.ItemsStoreNotReplicated->AddItemToSlot(OwnerItemId, TAG_IntegrationSlot_01);
		ASSERT_THAT(AreEqual(Instance->GetGrantedAbilities().Num(), 1));

		Actor.ItemsStoreNotReplicated->RemoveItemFromSlot(OwnerItemId);
		ASSERT_THAT(AreEqual(Instance->GetGrantedAbilities().Num(), 0));
	}

	TEST_METHOD(AttachedItem_WithAbility_MovedToOtherStore_AbilityCleanedUp)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStoreNotReplicated->AddItem(OwnerSpec, FArcItemId::InvalidId);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->ItemWithGrantedAbility).SetAmount(1).SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStoreNotReplicated->AddItem(AttachedSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_IntegrationSlot_02);

		Actor.ItemsStoreNotReplicated->AddItemToSlot(OwnerItemId, TAG_IntegrationSlot_01);

		FArcItemData* AttachItemData = Actor.ItemsStoreNotReplicated->GetItemPtr(AttachedItemId);
		const FArcItemInstance_GrantedAbilities* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_GrantedAbilities>(AttachItemData);
		ASSERT_THAT(AreEqual(Instance->GetGrantedAbilities().Num(), 1));

		FGameplayAbilitySpecHandle AbilitySpecHandle = Instance->GetGrantedAbilities()[0];

		const FArcItemDataInternal* InternalItem = Actor.ItemsStoreNotReplicated->GetInternalItem(OwnerItemId);
		FArcItemCopyContainerHelper Copy = FArcItemCopyContainerHelper::New(Actor.ItemsStoreNotReplicated, *InternalItem);
		TArray<FArcItemId> NewIds = Actor.ItemsStore2->AddItemDataInternal(Copy);

		Actor.ItemsStoreNotReplicated->DestroyItem(OwnerItemId);

		FGameplayAbilitySpec* Spec = Actor.AbilitySystemComponent3->FindAbilitySpecFromHandle(AbilitySpecHandle);
		ASSERT_THAT(IsNull(Spec, TEXT("Ability spec should be cleaned up after destroy")));
	}
};

// ============================================================================
// Effects To Apply Fragment Tests
// ============================================================================

TEST_CLASS(ArcItemsIntegration_EffectsToApply, "ArcCore.Integration")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(AddItem_WithEffectToApply)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithEffectToApply).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		const FArcItemInstance_EffectToApply* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_EffectToApply>(ItemPtr);
		ASSERT_THAT(IsNotNull(Instance, TEXT("EffectToApply instance should exist")));

		TArray<const FArcEffectSpecItem*> Specs = Instance->GetEffectSpecHandles(TAG_IntegrationEffectSpec_01);
		ASSERT_THAT(AreEqual(Specs.Num(), 1));
	}

	TEST_METHOD(AttachItem_WithEffectToApply_EffectOnOwner)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStoreNotReplicated->AddItem(OwnerSpec, FArcItemId::InvalidId);

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->ItemWithEffectToApply).SetAmount(1).SetItemLevel(1);
		FArcItemId AttachedItemId = Actor.ItemsStoreNotReplicated->AddItem(AttachedSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->InternalAttachToItem(OwnerItemId, AttachedItemId, FGameplayTag::EmptyTag);

		const FArcItemData* OwnerPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(OwnerItemId);
		const FArcItemInstance_EffectToApply* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_EffectToApply>(OwnerPtr);
		ASSERT_THAT(IsNotNull(Instance));

		TArray<const FArcEffectSpecItem*> Specs = Instance->GetEffectSpecHandles(TAG_IntegrationEffectSpec_01);
		ASSERT_THAT(AreEqual(Specs.Num(), 1));
	}

	TEST_METHOD(CopyItem_WithEffectToApply_PreservesEffect)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithEffectToApply).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		const FArcItemDataInternal* InternalItem = Actor.ItemsStoreNotReplicated->GetInternalItem(Id);
		FArcItemCopyContainerHelper Copy = FArcItemCopyContainerHelper::New(Actor.ItemsStoreNotReplicated, *InternalItem);
		TArray<FArcItemId> CopyIds = Actor.ItemsStore2->AddItemDataInternal(Copy);

		ASSERT_THAT(AreEqual(CopyIds.Num(), 1));

		const FArcItemData* CopyItemPtr = Actor.ItemsStore2->GetItemPtr(CopyIds[0]);
		const FArcItemInstance_EffectToApply* CopyInstance = ArcItemsHelper::FindInstance<FArcItemInstance_EffectToApply>(CopyItemPtr);
		ASSERT_THAT(IsNotNull(CopyInstance, TEXT("Effect instance should survive copy")));
	}
};

// ============================================================================
// Item Stats Fragment Tests
// ============================================================================

TEST_CLASS(ArcItemsIntegration_ItemStats, "ArcCore.Integration")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(AddItem_WithOneStat)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithStatsA).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		const FArcItemInstance_ItemStats* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(ItemPtr);
		ASSERT_THAT(IsNotNull(Instance, TEXT("ItemStats instance should exist")));

		float StatValue = Instance->GetStatValue(UArcCoreTestAttributeSet::GetTestAttributeAAttribute());
		ASSERT_THAT(IsNear(StatValue, 10.f, 0.1f));
	}

	TEST_METHOD(AddItem_WithSpecFragment_Stats)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemFragment_ItemStats* ItemStats = new FArcItemFragment_ItemStats();
		ItemStats->DefaultStats.Add({UArcCoreTestAttributeSet::GetTestAttributeAAttribute(), 10.f});
		ItemStats->DefaultStats.Add({UArcCoreTestAttributeSet::GetTestAttributeBAttribute(), 15.f});

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1)
			.AddInstanceData(ItemStats);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		const FArcItemInstance_ItemStats* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(ItemPtr);
		ASSERT_THAT(IsNotNull(Instance));

		float TestStatA = Instance->GetStatValue(UArcCoreTestAttributeSet::GetTestAttributeAAttribute());
		ASSERT_THAT(IsNear(TestStatA, 10.f, 0.1f));

		float TestStatB = Instance->GetStatValue(UArcCoreTestAttributeSet::GetTestAttributeBAttribute());
		ASSERT_THAT(IsNear(TestStatB, 15.f, 0.1f));
	}

	TEST_METHOD(ItemStats_ShouldPersist)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithStatsA).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		const FArcItemInstance_ItemStats* Instance = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(ItemPtr);
		ASSERT_THAT(IsNotNull(Instance));
		ASSERT_THAT(IsTrue(Instance->ShouldPersist(), TEXT("ItemStats should persist")));
	}

	TEST_METHOD(ItemStats_PreservedWhenMovingBetweenStores)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemFragment_ItemStats* ItemStats = new FArcItemFragment_ItemStats();
		ItemStats->DefaultStats.Add({UArcCoreTestAttributeSet::GetTestAttributeAAttribute(), 33.f});

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1)
			.AddInstanceData(ItemStats);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		FArcItemCopyContainerHelper Copy = Actor.ItemsStoreNotReplicated->GetItemCopyHelper(Id);
		TArray<FArcItemId> NewIds = Actor.ItemsStore2->AddItemDataInternal(Copy);

		Actor.ItemsStoreNotReplicated->DestroyItem(Id);

		ASSERT_THAT(AreEqual(NewIds.Num(), 1));
		const FArcItemData* NewItemPtr = Actor.ItemsStore2->GetItemPtr(NewIds[0]);
		ASSERT_THAT(IsNotNull(NewItemPtr));

		const FArcItemInstance_ItemStats* NewInstance = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(NewItemPtr);
		ASSERT_THAT(IsNotNull(NewInstance, TEXT("Stats should be preserved after move")));

		float Stat = NewInstance->GetStatValue(UArcCoreTestAttributeSet::GetTestAttributeAAttribute());
		ASSERT_THAT(IsNear(Stat, 33.f, 0.1f));
	}

	TEST_METHOD(ItemStats_PreservedOnToSpecRoundTrip)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemFragment_ItemStats* ItemStats = new FArcItemFragment_ItemStats();
		ItemStats->DefaultStats.Add({UArcCoreTestAttributeSet::GetTestAttributeAAttribute(), 77.f});
		ItemStats->DefaultStats.Add({UArcCoreTestAttributeSet::GetTestAttributeBAttribute(), 88.f});

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1)
			.AddInstanceData(ItemStats);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		FArcItemSpec ConvertedSpec = FArcItemCopyContainerHelper::ToSpec(ItemPtr);

		FArcItemId NewId = Actor.ItemsStore2->AddItem(ConvertedSpec, FArcItemId::InvalidId);
		const FArcItemData* NewPtr = Actor.ItemsStore2->GetItemPtr(NewId);
		ASSERT_THAT(IsNotNull(NewPtr));

		const FArcItemInstance_ItemStats* NewInstance = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(NewPtr);
		ASSERT_THAT(IsNotNull(NewInstance));

		float StatA = NewInstance->GetStatValue(UArcCoreTestAttributeSet::GetTestAttributeAAttribute());
		ASSERT_THAT(IsNear(StatA, 77.f, 0.1f));
		float StatB = NewInstance->GetStatValue(UArcCoreTestAttributeSet::GetTestAttributeBAttribute());
		ASSERT_THAT(IsNear(StatB, 88.f, 0.1f));
	}
};

// ============================================================================
// Stacks Fragment Tests
// ============================================================================

TEST_CLASS(ArcItemsIntegration_Stacks, "ArcCore.Integration")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(Stacks_ShouldPersist)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithStacks).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		FArcItemInstance_Stacks* Stacks = ArcItemsHelper::FindMutableInstance<FArcItemInstance_Stacks>(ItemPtr);
		ASSERT_THAT(IsNotNull(Stacks));
		ASSERT_THAT(IsTrue(Stacks->ShouldPersist(), TEXT("Stacks should persist")));
	}

	TEST_METHOD(Stacks_PreservedOnMove)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithStacks).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		FArcItemInstance_Stacks* Stacks = ArcItemsHelper::FindMutableInstance<FArcItemInstance_Stacks>(ItemPtr);
		ASSERT_THAT(IsNotNull(Stacks));

		Stacks->AddStacks(4);
		ASSERT_THAT(AreEqual(Stacks->GetStacks(), 5));

		FArcItemCopyContainerHelper Copy = Actor.ItemsStoreNotReplicated->GetItemCopyHelper(Id);
		TArray<FArcItemId> NewIds = Actor.ItemsStore2->AddItemDataInternal(Copy);

		ASSERT_THAT(AreEqual(NewIds.Num(), 1));
		const FArcItemData* NewPtr = Actor.ItemsStore2->GetItemPtr(NewIds[0]);
		ASSERT_THAT(IsNotNull(NewPtr));

		FArcItemInstance_Stacks* NewStacks = ArcItemsHelper::FindMutableInstance<FArcItemInstance_Stacks>(NewPtr);
		ASSERT_THAT(IsNotNull(NewStacks));
		ASSERT_THAT(AreEqual(NewStacks->GetStacks(), 5));
	}

	TEST_METHOD(Stacks_PreservedOnToSpecRoundTrip)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithStacks).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		FArcItemInstance_Stacks* Stacks = ArcItemsHelper::FindMutableInstance<FArcItemInstance_Stacks>(ItemPtr);
		Stacks->AddStacks(2);
		ASSERT_THAT(AreEqual(Stacks->GetStacks(), 3));

		FArcItemSpec ConvertedSpec = FArcItemCopyContainerHelper::ToSpec(ItemPtr);

		FArcItemId NewId = Actor.ItemsStore2->AddItem(ConvertedSpec, FArcItemId::InvalidId);
		const FArcItemData* NewPtr = Actor.ItemsStore2->GetItemPtr(NewId);
		ASSERT_THAT(IsNotNull(NewPtr));

		FArcItemInstance_Stacks* NewStacks = ArcItemsHelper::FindMutableInstance<FArcItemInstance_Stacks>(NewPtr);
		ASSERT_THAT(IsNotNull(NewStacks));
		ASSERT_THAT(AreEqual(NewStacks->GetStacks(), 3));
	}
};

// ============================================================================
// Persistence Round-Trip Tests
// ============================================================================

TEST_CLASS(ArcItemsIntegration_Persistence, "ArcCore.Integration")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(PersistentData_CopiedCorrectly_ItemWithStats_MoveViaHelper)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemFragment_ItemStats* ItemStats = new FArcItemFragment_ItemStats();
		ItemStats->DefaultStats.Add({UArcCoreTestAttributeSet::GetTestAttributeAAttribute(), 100.f});
		ItemStats->DefaultStats.Add({UArcCoreTestAttributeSet::GetTestAttributeBAttribute(), 200.f});

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(5)
			.AddInstanceData(ItemStats);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		const FArcItemData* OrigPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		const FArcItemInstance_ItemStats* OrigStats = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(OrigPtr);
		ASSERT_THAT(IsNotNull(OrigStats));

		FArcItemCopyContainerHelper Copy = Actor.ItemsStoreNotReplicated->GetItemCopyHelper(Id);
		TArray<FArcItemId> NewIds = Actor.ItemsStore2->AddItemDataInternal(Copy);

		const FArcItemData* NewPtr = Actor.ItemsStore2->GetItemPtr(NewIds[0]);
		const FArcItemInstance_ItemStats* NewStats = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(NewPtr);
		ASSERT_THAT(IsNotNull(NewStats));

		float StatA = NewStats->GetStatValue(UArcCoreTestAttributeSet::GetTestAttributeAAttribute());
		float StatB = NewStats->GetStatValue(UArcCoreTestAttributeSet::GetTestAttributeBAttribute());
		ASSERT_THAT(IsNear(StatA, 100.f, 0.1f));
		ASSERT_THAT(IsNear(StatB, 200.f, 0.1f));
		ASSERT_THAT(AreEqual((int32)NewPtr->GetLevel(), 5));
	}

	TEST_METHOD(PersistentData_CopiedCorrectly_ItemWithAttachmentAndStats)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec OwnerSpec;
		OwnerSpec.SetItemDefinition(Settings->ItemWithAttachmentSlots).SetAmount(1).SetItemLevel(1);
		FArcItemId OwnerItemId = Actor.ItemsStoreNotReplicated->AddItem(OwnerSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->AddItemToSlot(OwnerItemId, TAG_IntegrationSlot_01);

		FArcItemFragment_ItemStats* AttachStats = new FArcItemFragment_ItemStats();
		AttachStats->DefaultStats.Add({UArcCoreTestAttributeSet::GetTestAttributeAAttribute(), 55.f});

		FArcItemSpec AttachedSpec;
		AttachedSpec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(3)
			.AddInstanceData(AttachStats);
		FArcItemId AttachedItemId = Actor.ItemsStoreNotReplicated->AddItem(AttachedSpec, FArcItemId::InvalidId);
		Actor.ItemsStoreNotReplicated->InternalAttachToItem(OwnerItemId, AttachedItemId, TAG_IntegrationSlot_02);

		const FArcItemData* AttachPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(AttachedItemId);
		const FArcItemInstance_ItemStats* AttachStatsInst = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(AttachPtr);
		ASSERT_THAT(IsNotNull(AttachStatsInst));
		ASSERT_THAT(IsNear(AttachStatsInst->GetStatValue(UArcCoreTestAttributeSet::GetTestAttributeAAttribute()), 55.f, 0.1f));

		FArcItemCopyContainerHelper Copy = Actor.ItemsStoreNotReplicated->GetItemCopyHelper(OwnerItemId);
		ASSERT_THAT(AreEqual(Copy.ItemAttachments.Num(), 1));

		TArray<FArcItemId> NewIds = Actor.ItemsStore2->AddItemDataInternal(Copy);
		ASSERT_THAT(AreEqual(Actor.ItemsStore2->GetItemNum(), 2));

		const FArcItemData* NewAttachPtr = Actor.ItemsStore2->GetItemAttachedTo(NewIds[0], TAG_IntegrationSlot_02);
		ASSERT_THAT(IsNotNull(NewAttachPtr, TEXT("Attachment should be in target store")));

		const FArcItemInstance_ItemStats* NewAttachStats = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(NewAttachPtr);
		ASSERT_THAT(IsNotNull(NewAttachStats, TEXT("Attachment stats should survive copy")));
		ASSERT_THAT(IsNear(NewAttachStats->GetStatValue(UArcCoreTestAttributeSet::GetTestAttributeAAttribute()), 55.f, 0.1f));
		ASSERT_THAT(AreEqual((int32)NewAttachPtr->GetLevel(), 3));
	}

	TEST_METHOD(PersistentData_Stacks_CopiedCorrectly_MoveViaHelper)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->ItemWithStacks).SetAmount(1).SetItemLevel(1);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		const FArcItemData* ItemPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(Id);
		FArcItemInstance_Stacks* Stacks = ArcItemsHelper::FindMutableInstance<FArcItemInstance_Stacks>(ItemPtr);
		ASSERT_THAT(IsNotNull(Stacks));
		Stacks->SetStacks(10);
		ASSERT_THAT(AreEqual(Stacks->GetStacks(), 10));

		FArcItemCopyContainerHelper Copy = Actor.ItemsStoreNotReplicated->GetItemCopyHelper(Id);
		TArray<FArcItemId> NewIds = Actor.ItemsStore2->AddItemDataInternal(Copy);

		const FArcItemData* NewPtr = Actor.ItemsStore2->GetItemPtr(NewIds[0]);
		FArcItemInstance_Stacks* NewStacks = ArcItemsHelper::FindMutableInstance<FArcItemInstance_Stacks>(NewPtr);
		ASSERT_THAT(IsNotNull(NewStacks));
		ASSERT_THAT(AreEqual(NewStacks->GetStacks(), 10));
	}

	TEST_METHOD(PersistentData_FullRoundTrip_CreateMoveDestroyOriginal)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(SpawnParameters);

		FArcItemFragment_ItemStats* ItemStats = new FArcItemFragment_ItemStats();
		ItemStats->DefaultStats.Add({UArcCoreTestAttributeSet::GetTestAttributeAAttribute(), 500.f});

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(10)
			.AddInstanceData(ItemStats);
		FArcItemId Id = Actor.ItemsStoreNotReplicated->AddItem(Spec, FArcItemId::InvalidId);

		FArcItemCopyContainerHelper Copy = Actor.ItemsStoreNotReplicated->GetItemCopyHelper(Id);
		TArray<FArcItemId> NewIds = Actor.ItemsStore2->AddItemDataInternal(Copy);

		Actor.ItemsStoreNotReplicated->DestroyItem(Id);
		ASSERT_THAT(AreEqual(Actor.ItemsStoreNotReplicated->GetItemNum(), 0));

		const FArcItemData* NewPtr = Actor.ItemsStore2->GetItemPtr(NewIds[0]);
		ASSERT_THAT(IsNotNull(NewPtr));
		ASSERT_THAT(AreEqual((int32)NewPtr->GetLevel(), 10));

		const FArcItemInstance_ItemStats* NewStats = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(NewPtr);
		ASSERT_THAT(IsNotNull(NewStats));
		ASSERT_THAT(IsNear(NewStats->GetStatValue(UArcCoreTestAttributeSet::GetTestAttributeAAttribute()), 500.f, 0.1f));

		FArcItemSpec SecondSpec = FArcItemCopyContainerHelper::ToSpec(NewPtr);
		FArcItemCopyContainerHelper SecondCopy = FArcItemCopyContainerHelper::FromSpec(SecondSpec);
		TArray<FArcItemId> FinalIds = Actor.ItemsStoreNotReplicated->AddItemDataInternal(SecondCopy);

		Actor.ItemsStore2->DestroyItem(NewIds[0]);
		ASSERT_THAT(AreEqual(Actor.ItemsStore2->GetItemNum(), 0));

		const FArcItemData* FinalPtr = Actor.ItemsStoreNotReplicated->GetItemPtr(FinalIds[0]);
		ASSERT_THAT(IsNotNull(FinalPtr));
		ASSERT_THAT(AreEqual((int32)FinalPtr->GetLevel(), 10));

		const FArcItemInstance_ItemStats* FinalStats = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(FinalPtr);
		ASSERT_THAT(IsNotNull(FinalStats, TEXT("Stats should survive full round-trip")));
		ASSERT_THAT(IsNear(FinalStats->GetStatValue(UArcCoreTestAttributeSet::GetTestAttributeAAttribute()), 500.f, 0.1f));
	}
};
