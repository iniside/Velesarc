// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcItemAttachmentIntegrationTests.h"

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "NativeGameplayTags.h"

#include "Items/ArcItemSpec.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "Equipment/ArcAttachmentHandler.h"
#include "Equipment/ArcSocketArray.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AttachTest_Slot01, "SlotId.ArcAttachTest.01");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AttachTest_Slot02, "SlotId.ArcAttachTest.02");

// ============================================================================
// Test Actor
// ============================================================================

AArcItemAttachmentTestActor::AArcItemAttachmentTestActor()
{
	AttachRoot = CreateDefaultSubobject<USceneComponent>(TEXT("AttachRoot"));
	AttachRoot->ComponentTags.Add(TEXT("TestMesh"));
	SetRootComponent(AttachRoot);

	ItemsStore = CreateDefaultSubobject<UArcItemAttachmentTestStore>(TEXT("ItemsStore"));
	AttachmentComp = CreateDefaultSubobject<UArcItemAttachmentComponent>(TEXT("AttachmentComp"));
}

// ============================================================================
// Test Attachment Handler
// ============================================================================

bool FArcTestAttachmentHandler::HandleItemAddedToSlot(
	UArcItemAttachmentComponent* InAttachmentComponent,
	const FArcItemData* InItem,
	const FArcItemData* InOwnerItem) const
{
	AActor* Character = InAttachmentComponent->FindCharacter();
	if (Character == nullptr)
	{
		return false;
	}

	FArcItemAttachment Attached = MakeItemAttachment(InAttachmentComponent, InItem, InOwnerItem);
	InAttachmentComponent->AddAttachedItem(Attached);
	return true;
}

void FArcTestAttachmentHandler::HandleItemAttach(
	UArcItemAttachmentComponent* InAttachmentComponent,
	const FArcItemId InItemId,
	const FArcItemId InOwnerItem) const
{
	const FArcItemAttachment* ItemAttachment = InAttachmentComponent->GetAttachment(InItemId);
	if (!ItemAttachment)
	{
		return;
	}

	UObject* AlreadyAttached = InAttachmentComponent->FindFirstAttachedObject(ItemAttachment->ItemDefinition);
	if (AlreadyAttached)
	{
		return;
	}

	AActor* OwnerCharacter = InAttachmentComponent->FindCharacter();
	USceneComponent* SpawnedComponent = SpawnComponent<USceneComponent>(
		OwnerCharacter, InAttachmentComponent, ItemAttachment);

	if (SpawnedComponent)
	{
		InAttachmentComponent->AddAttachmentForItem(ItemAttachment->ItemDefinition, SpawnedComponent);
		InAttachmentComponent->CallbackItemAttached(InItemId);
	}
}

void FArcTestAttachmentHandler::HandleItemDetach(
	UArcItemAttachmentComponent* InAttachmentComponent,
	const FArcItemId InItemId,
	const FArcItemId InOwnerItem) const
{
	// Cleanup of ObjectsAttachedFromItem is handled by the component
	// via RemoveAttachedForItem in HandleItemRemovedFromReplication.
}

FInstancedStruct FArcTestAttachmentHandler::Make(FName InComponentTag)
{
	FInstancedStruct IS;
	IS.InitializeAs<FArcTestAttachmentHandler>();
	FArcTestAttachmentHandler* Handler = IS.GetMutablePtr<FArcTestAttachmentHandler>();
	Handler->ComponentTag = InComponentTag;
	return IS;
}

// ============================================================================
// Test Helpers
// ============================================================================

namespace AttachmentTestHelpers
{
	UArcItemDefinition* CreateTransientItemDef(const FName& Name)
	{
		UArcItemDefinition* Def = NewObject<UArcItemDefinition>(
			GetTransientPackage(), Name, RF_Transient);
		Def->RegenerateItemId();
		return Def;
	}

	FArcItemSpec MakeItemSpec(
		const UArcItemDefinition* ItemDef,
		int32 Amount = 1,
		uint8 Level = 1)
	{
		FArcItemSpec Spec;
		Spec.SetItemDefinition(ItemDef->GetPrimaryAssetId())
			.SetItemDefinitionAsset(ItemDef)
			.SetAmount(Amount)
			.SetItemLevel(Level);
		return Spec;
	}

	/**
	 * Configures the attachment component's private fields via property reflection.
	 * Sets ItemSlotClass and StaticAttachmentSlots, then initializes handlers.
	 */
	void ConfigureAttachmentComponent(
		UArcItemAttachmentComponent* Comp,
		TSubclassOf<UArcItemsStoreComponent> StoreClass,
		const TArray<FArcItemAttachmentSlot>& Slots)
	{
		UClass* CompClass = UArcItemAttachmentComponent::StaticClass();

		// Set ItemSlotClass via property reflection (private UPROPERTY)
		{
			FProperty* Prop = CompClass->FindPropertyByName(TEXT("ItemSlotClass"));
			check(Prop);
			TSubclassOf<UArcItemsStoreComponent> Value = StoreClass;
			Prop->CopySingleValue(Prop->ContainerPtrToValuePtr<void>(Comp), &Value);
		}

		// Set StaticAttachmentSlots via property reflection (private UPROPERTY)
		FArcItemAttachmentSlotContainer* SlotContainer = nullptr;
		{
			FProperty* Prop = CompClass->FindPropertyByName(TEXT("StaticAttachmentSlots"));
			check(Prop);
			SlotContainer = Prop->ContainerPtrToValuePtr<FArcItemAttachmentSlotContainer>(Comp);
			SlotContainer->Slots = Slots;
		}

		// Initialize handlers (they missed InitializeComponent since slots were empty at spawn time)
		for (FArcItemAttachmentSlot& Slot : SlotContainer->Slots)
		{
			for (FInstancedStruct& Handler : Slot.Handlers)
			{
				if (FArcAttachmentHandler* H = Handler.GetMutablePtr<FArcAttachmentHandler>())
				{
					H->Initialize(Comp);
				}
			}
		}
	}

	/**
	 * Creates an FArcItemAttachmentSlot configured with the test handler.
	 */
	FArcItemAttachmentSlot MakeSlot(const FGameplayTag& SlotId, FName ComponentTag = TEXT("TestMesh"))
	{
		FArcItemAttachmentSlot Slot;
		Slot.SlotId = SlotId;
		Slot.Handlers.Add(FArcTestAttachmentHandler::Make(ComponentTag));
		return Slot;
	}
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CLASS(ArcItemAttachment_Integration, "ArcCore.Integration.Attachment")
{
	FActorTestSpawner Spawner;

	TObjectPtr<UArcItemDefinition> ItemDefA;
	TObjectPtr<UArcItemDefinition> ItemDefB;
	AArcItemAttachmentTestActor* TestActor = nullptr;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		// Create transient item definitions
		ItemDefA = AttachmentTestHelpers::CreateTransientItemDef(TEXT("TestDef_ItemA"));
		ItemDefB = AttachmentTestHelpers::CreateTransientItemDef(TEXT("TestDef_ItemB"));

		// Spawn test actor
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = *FGuid::NewGuid().ToString();
		TestActor = &Spawner.SpawnActor<AArcItemAttachmentTestActor>(SpawnParams);

		// Configure attachment component: set store class and add slot handlers
		TArray<FArcItemAttachmentSlot> Slots;
		Slots.Add(AttachmentTestHelpers::MakeSlot(TAG_AttachTest_Slot01));
		Slots.Add(AttachmentTestHelpers::MakeSlot(TAG_AttachTest_Slot02));

		AttachmentTestHelpers::ConfigureAttachmentComponent(
			TestActor->AttachmentComp,
			UArcItemAttachmentTestStore::StaticClass(),
			Slots);
	}

	// ----------------------------------------------------------------
	// Item slotted → replicated attachment entry is created
	// ----------------------------------------------------------------
	TEST_METHOD(AddItemToSlot_CreatesReplicatedAttachment)
	{
		FArcItemSpec Spec = AttachmentTestHelpers::MakeItemSpec(ItemDefA);
		FArcItemId ItemId = TestActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		TestActor->ItemsStore->AddItemToSlot(ItemId, TAG_AttachTest_Slot01);

		ASSERT_THAT(IsTrue(
			TestActor->AttachmentComp->ContainsAttachedItem(ItemId),
			TEXT("Attachment entry should exist after slotting item")));

		const FArcItemAttachment* Attachment = TestActor->AttachmentComp->GetAttachment(ItemId);
		ASSERT_THAT(IsNotNull(Attachment, TEXT("Should be able to retrieve attachment by ItemId")));
		ASSERT_THAT(IsTrue(
			Attachment->SlotId == TAG_AttachTest_Slot01,
			TEXT("Attachment SlotId should match the slot the item was added to")));
		ASSERT_THAT(IsTrue(
			Attachment->ItemDefinition == ItemDefA,
			TEXT("Attachment should reference the correct item definition")));
	}

	// ----------------------------------------------------------------
	// Item slotted → visual component is spawned and tracked
	// ----------------------------------------------------------------
	TEST_METHOD(AddItemToSlot_SpawnsVisualComponent)
	{
		FArcItemSpec Spec = AttachmentTestHelpers::MakeItemSpec(ItemDefA);
		FArcItemId ItemId = TestActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		TestActor->ItemsStore->AddItemToSlot(ItemId, TAG_AttachTest_Slot01);

		TArray<UObject*> AttachedObjects = TestActor->AttachmentComp->FindAttachedObjects(ItemDefA);
		ASSERT_THAT(AreEqual(AttachedObjects.Num(), 1,
			TEXT("One visual component should be spawned for the slotted item")));

		USceneComponent* SpawnedComp = Cast<USceneComponent>(AttachedObjects[0]);
		ASSERT_THAT(IsNotNull(SpawnedComp,
			TEXT("Spawned visual object should be a USceneComponent")));
	}

	// ----------------------------------------------------------------
	// Item removed from slot → attachment and visual are cleaned up
	// ----------------------------------------------------------------
	TEST_METHOD(RemoveItemFromSlot_CleansUpAttachmentAndVisual)
	{
		FArcItemSpec Spec = AttachmentTestHelpers::MakeItemSpec(ItemDefA);
		FArcItemId ItemId = TestActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		TestActor->ItemsStore->AddItemToSlot(ItemId, TAG_AttachTest_Slot01);

		// Verify attachment exists
		ASSERT_THAT(IsTrue(TestActor->AttachmentComp->ContainsAttachedItem(ItemId)));

		// Remove from slot
		TestActor->ItemsStore->RemoveItemFromSlot(ItemId);

		ASSERT_THAT(IsFalse(
			TestActor->AttachmentComp->ContainsAttachedItem(ItemId),
			TEXT("Attachment entry should be removed after unslotting")));

		TArray<UObject*> AttachedObjects = TestActor->AttachmentComp->FindAttachedObjects(ItemDefA);
		ASSERT_THAT(AreEqual(AttachedObjects.Num(), 0,
			TEXT("Visual objects should be cleaned up after unslotting")));
	}

	// ----------------------------------------------------------------
	// Multiple items on different slots → independent attachments
	// ----------------------------------------------------------------
	TEST_METHOD(MultipleItems_DifferentSlots_IndependentAttachments)
	{
		FArcItemSpec SpecA = AttachmentTestHelpers::MakeItemSpec(ItemDefA);
		FArcItemId ItemIdA = TestActor->ItemsStore->AddItem(SpecA, FArcItemId::InvalidId);
		TestActor->ItemsStore->AddItemToSlot(ItemIdA, TAG_AttachTest_Slot01);

		FArcItemSpec SpecB = AttachmentTestHelpers::MakeItemSpec(ItemDefB);
		FArcItemId ItemIdB = TestActor->ItemsStore->AddItem(SpecB, FArcItemId::InvalidId);
		TestActor->ItemsStore->AddItemToSlot(ItemIdB, TAG_AttachTest_Slot02);

		// Both attachments should exist
		ASSERT_THAT(IsTrue(TestActor->AttachmentComp->ContainsAttachedItem(ItemIdA)));
		ASSERT_THAT(IsTrue(TestActor->AttachmentComp->ContainsAttachedItem(ItemIdB)));

		// Each slot should have an attachment
		ASSERT_THAT(IsTrue(
			TestActor->AttachmentComp->DoesSlotHaveAttachment(TAG_AttachTest_Slot01),
			TEXT("Slot01 should have an attachment")));
		ASSERT_THAT(IsTrue(
			TestActor->AttachmentComp->DoesSlotHaveAttachment(TAG_AttachTest_Slot02),
			TEXT("Slot02 should have an attachment")));

		// Each item should have visual objects
		ASSERT_THAT(AreEqual(
			TestActor->AttachmentComp->FindAttachedObjects(ItemDefA).Num(), 1));
		ASSERT_THAT(AreEqual(
			TestActor->AttachmentComp->FindAttachedObjects(ItemDefB).Num(), 1));

		// Remove first item — second should remain
		TestActor->ItemsStore->RemoveItemFromSlot(ItemIdA);

		ASSERT_THAT(IsFalse(TestActor->AttachmentComp->ContainsAttachedItem(ItemIdA)));
		ASSERT_THAT(IsTrue(TestActor->AttachmentComp->ContainsAttachedItem(ItemIdB),
			TEXT("Second attachment should survive removal of first")));
	}

	// ----------------------------------------------------------------
	// Item added to slot with no configured handler → no attachment
	// ----------------------------------------------------------------
	TEST_METHOD(AddItemToSlot_NoConfiguredHandler_NoAttachment)
	{
		// Use a slot tag that has no handler configured on the attachment component
		FGameplayTag UnknownSlot = TAG_Invalid_Item_Slot;

		FArcItemSpec Spec = AttachmentTestHelpers::MakeItemSpec(ItemDefA);
		FArcItemId ItemId = TestActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		TestActor->ItemsStore->AddItemToSlot(ItemId, UnknownSlot);

		ASSERT_THAT(IsFalse(
			TestActor->AttachmentComp->ContainsAttachedItem(ItemId),
			TEXT("No attachment should be created for a slot without handlers")));
	}

	// ----------------------------------------------------------------
	// Socket tracking: TakenSockets records the occupied socket
	// ----------------------------------------------------------------
	TEST_METHOD(TakenSockets_TrackedOnAddAndCleared)
	{
		FArcItemSpec Spec = AttachmentTestHelpers::MakeItemSpec(ItemDefA);
		FArcItemId ItemId = TestActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		TestActor->ItemsStore->AddItemToSlot(ItemId, TAG_AttachTest_Slot01);

		const FArcItemAttachment* Attachment = TestActor->AttachmentComp->GetAttachment(ItemId);
		ASSERT_THAT(IsNotNull(Attachment));

		// Verify socket is tracked as taken (if a socket name was assigned)
		const TMap<FGameplayTag, TSet<FName>>& TakenSockets = TestActor->AttachmentComp->GetTakenSockets();
		const TSet<FName>* SocketSet = TakenSockets.Find(TAG_AttachTest_Slot01);
		ASSERT_THAT(IsNotNull(SocketSet,
			TEXT("TakenSockets should have entry for the slot")));

		// Remove attachment — socket should be freed
		TestActor->AttachmentComp->RemoveAttachment(ItemId);

		const TSet<FName>* SocketSetAfter = TestActor->AttachmentComp->GetTakenSockets().Find(TAG_AttachTest_Slot01);
		if (SocketSetAfter)
		{
			ASSERT_THAT(AreEqual(SocketSetAfter->Num(), 0,
				TEXT("Socket set should be empty after removing the attachment")));
		}
	}

	// ----------------------------------------------------------------
	// Verify attachment data integrity
	// ----------------------------------------------------------------
	TEST_METHOD(AttachmentData_CorrectItemIdAndDefinition)
	{
		FArcItemSpec Spec = AttachmentTestHelpers::MakeItemSpec(ItemDefA);
		FArcItemId ItemId = TestActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		TestActor->ItemsStore->AddItemToSlot(ItemId, TAG_AttachTest_Slot01);

		const FArcItemAttachment* Attachment = TestActor->AttachmentComp->GetAttachment(ItemId);
		ASSERT_THAT(IsNotNull(Attachment));
		ASSERT_THAT(IsTrue(Attachment->ItemId == ItemId,
			TEXT("Attachment ItemId must match the source item")));
		ASSERT_THAT(IsTrue(Attachment->ItemDefinition == ItemDefA,
			TEXT("Attachment ItemDefinition must match")));
		ASSERT_THAT(IsFalse(Attachment->OwnerId.IsValid(),
			TEXT("Top-level attachment should have no OwnerId")));
		ASSERT_THAT(IsTrue(Attachment->SlotId == TAG_AttachTest_Slot01,
			TEXT("Attachment SlotId must match the slot")));
	}

	// ----------------------------------------------------------------
	// Re-slot item: remove from one slot, add to another
	// ----------------------------------------------------------------
	TEST_METHOD(ReslotItem_AttachmentMovesToNewSlot)
	{
		FArcItemSpec Spec = AttachmentTestHelpers::MakeItemSpec(ItemDefA);
		FArcItemId ItemId = TestActor->ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
		TestActor->ItemsStore->AddItemToSlot(ItemId, TAG_AttachTest_Slot01);

		ASSERT_THAT(IsTrue(
			TestActor->AttachmentComp->DoesSlotHaveAttachment(TAG_AttachTest_Slot01)));

		// Move to different slot
		TestActor->ItemsStore->RemoveItemFromSlot(ItemId);
		TestActor->ItemsStore->AddItemToSlot(ItemId, TAG_AttachTest_Slot02);

		ASSERT_THAT(IsFalse(
			TestActor->AttachmentComp->DoesSlotHaveAttachment(TAG_AttachTest_Slot01),
			TEXT("Old slot should no longer have attachment")));
		ASSERT_THAT(IsTrue(
			TestActor->AttachmentComp->DoesSlotHaveAttachment(TAG_AttachTest_Slot02),
			TEXT("New slot should have the attachment")));

		const FArcItemAttachment* Attachment = TestActor->AttachmentComp->GetAttachment(ItemId);
		ASSERT_THAT(IsNotNull(Attachment));
		ASSERT_THAT(IsTrue(Attachment->SlotId == TAG_AttachTest_Slot02,
			TEXT("Attachment should reference the new slot")));
	}
};
