// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityFragments.h"

#include "Attachments/ArcMassItemAttachmentFragments.h"
#include "Attachments/ArcMassItemAttachmentConfig.h"
#include "Attachments/ArcMassAttachmentHandler.h"
#include "Attachments/ArcMassAttachmentHandler_StaticMesh.h"
#include "Attachments/ArcMassAttachmentHandler_AnimLayer.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"
#include "Signals/ArcMassItemSignals.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemTypes.h"
#include "Core/ArcCoreAssetManager.h"

#include "ArcMassItemTestsSettings.h"

#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Attach_Weapon, "Test.Attach.Slot.Weapon");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Attach_Armor, "Test.Attach.Slot.Armor");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Attach_Sub, "Test.Attach.Slot.Sub");

TEST_CLASS(ArcMassItemAttachment_State, "ArcMassItems.Attachments.State")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	FMassEntityHandle CreateEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
		EntityManager->AddFragmentToEntity(NewEntity, FArcMassItemAttachmentStateFragment::StaticStruct());
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
		Entity = CreateEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	TEST_METHOD(StateFragment_StartsEmpty)
	{
		FMassEntityView View(*EntityManager, Entity);
		const FArcMassItemAttachmentStateFragment* State = View.GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
		ASSERT_THAT(IsNotNull(State));
		ASSERT_THAT(AreEqual(0, State->ActiveAttachments.Num()));
		ASSERT_THAT(AreEqual(0, State->ObjectsAttachedFromItem.Num()));
		ASSERT_THAT(AreEqual(0, State->PendingVisualDetaches.Num()));
		ASSERT_THAT(AreEqual(0, State->TakenSockets.Num()));
	}

	TEST_METHOD(StateFragment_CanAddAttachment)
	{
		FMassEntityView View(*EntityManager, Entity);
		FArcMassItemAttachmentStateFragment* State = View.GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
		ASSERT_THAT(IsNotNull(State));

		FArcMassItemAttachment Attachment;
		Attachment.ItemId = FGuid::NewGuid();
		Attachment.SlotId = TAG_Test_Attach_Weapon;
		Attachment.SocketName = FName(TEXT("hand_r"));
		State->ActiveAttachments.Add(Attachment);

		ASSERT_THAT(AreEqual(1, State->ActiveAttachments.Num()));
		ASSERT_THAT(AreEqual(Attachment.ItemId, State->ActiveAttachments[0].ItemId));
	}
};

TEST_CLASS(ArcMassItemAttachment_Config, "ArcMassItems.Attachments.Config")
{
	TEST_METHOD(Config_CreatesWithSlots)
	{
		UArcMassItemAttachmentConfig* Config = NewObject<UArcMassItemAttachmentConfig>();
		ASSERT_THAT(IsNotNull(Config));
		ASSERT_THAT(AreEqual(0, Config->Slots.Num()));

		FArcMassItemAttachmentSlot Slot;
		Slot.SlotId = TAG_Test_Attach_Weapon;
		Config->Slots.Add(Slot);

		ASSERT_THAT(AreEqual(1, Config->Slots.Num()));
		ASSERT_THAT(AreEqual(FGameplayTag(TAG_Test_Attach_Weapon), Config->Slots[0].SlotId));
	}

	TEST_METHOD(Config_SlotWithHandler)
	{
		UArcMassItemAttachmentConfig* Config = NewObject<UArcMassItemAttachmentConfig>();

		FArcMassItemAttachmentSlot Slot;
		Slot.SlotId = TAG_Test_Attach_Weapon;

		FInstancedStruct HandlerInst;
		HandlerInst.InitializeAs(FArcMassAttachmentHandler_StaticMesh::StaticStruct());
		Slot.Handlers.Add(HandlerInst);

		Config->Slots.Add(Slot);

		ASSERT_THAT(AreEqual(1, Config->Slots.Num()));
		ASSERT_THAT(AreEqual(1, Config->Slots[0].Handlers.Num()));
		ASSERT_THAT(IsTrue(Config->Slots[0].Handlers[0].GetPtr<FArcMassAttachmentHandler_StaticMesh>() != nullptr));
	}
};

TEST_CLASS(ArcMassItemAttachment_HandlerSelection, "ArcMassItems.Attachments.HandlerSelection")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	FMassEntityHandle CreateEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(NewEntity);
		EntityManager->AddFragmentToEntity(NewEntity, FArcMassItemAttachmentStateFragment::StaticStruct());
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
		Entity = CreateEntity();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	TEST_METHOD(Handler_StaticMesh_ReturnsCorrectSupportedFragment)
	{
		FArcMassAttachmentHandler_StaticMesh Handler;
		ASSERT_THAT(AreEqual(FArcItemFragment_StaticMeshAttachment::StaticStruct(), Handler.SupportedItemFragment()));
	}

	TEST_METHOD(Handler_AnimLayer_ReturnsCorrectSupportedFragment)
	{
		FArcMassAttachmentHandler_AnimLayer Handler;
		ASSERT_THAT(AreEqual(FArcItemFragment_AnimLayer::StaticStruct(), Handler.SupportedItemFragment()));
	}

	TEST_METHOD(Handler_Base_ReturnsNullSupportedFragment)
	{
		FArcMassAttachmentHandler BaseHandler;
		ASSERT_THAT(IsNull(BaseHandler.SupportedItemFragment()));
	}

	TEST_METHOD(Handler_MakeItemAttachment_PopulatesFields)
	{
		FArcMassAttachmentHandler_StaticMesh Handler;
		Handler.AttachmentData.Add(FArcSocketArray());
		Handler.AttachmentData[0].SocketName = FName(TEXT("hand_r"));

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, TAG_Test_Attach_Weapon);

		FMassEntityView View(*EntityManager, Entity);
		const FArcMassItemStoreFragment* Store = View.GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		const FArcMassReplicatedItem* Found = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(Found));

		const FArcMassItemAttachmentStateFragment* StateForMake =
			FMassEntityView(*EntityManager, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
		ASSERT_THAT(IsNotNull(StateForMake));
		FArcMassItemAttachment Attachment = Handler.MakeItemAttachment(Found->ToItem(), nullptr, *StateForMake);
		ASSERT_THAT(AreEqual(ItemId, Attachment.ItemId));
		ASSERT_THAT(AreEqual(FGameplayTag(TAG_Test_Attach_Weapon), Attachment.SlotId));
		ASSERT_THAT(AreEqual(FName(TEXT("hand_r")), Attachment.SocketName));
		ASSERT_THAT(IsNotNull(Attachment.ItemDefinition.Get()));
	}
};

