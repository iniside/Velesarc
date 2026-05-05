// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassItemAttachmentIntegrationTests.h"

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassSignalSubsystem.h"
#include "Mass/EntityHandle.h"

#include "Attachments/ArcMassItemAttachmentConfig.h"
#include "Attachments/ArcMassItemAttachmentFragments.h"
#include "Attachments/ArcMassItemAttachmentProcessor.h"
#include "Attachments/ArcMassItemAttachmentSlots.h"
#include "Attachments/ArcMassAttachmentHandler_StaticMesh.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"
#include "Signals/ArcMassItemSignals.h"
#include "Items/ArcItemSpec.h"

#include "ArcMassItemTestHelpers.h"
#include "ArcMassItemTestsSettings.h"

#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RealHandler_Slot, "ProcTest.RealHandler.Slot");

AArcMassAttachmentTestActor::AArcMassAttachmentTestActor()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->ComponentTags.Add(TEXT("TestMesh"));
	SetRootComponent(Root);
}

TEST_CLASS(ArcMassItemAttachment_RealHandler, "ArcMassItems.Attachments.Integration.RealHandler")
{
	FActorTestSpawner Spawner;
	AArcMassAttachmentTestActor* TestActor = nullptr;
	FMassEntityManager* EM = nullptr;
	UMassSignalSubsystem* Signals = nullptr;
	UArcMassItemAttachmentProcessor* Processor = nullptr;
	UArcMassItemAttachmentConfig* Config = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* Settings = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EM = &MES->GetMutableEntityManager();
		Signals = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(Signals);

		FActorSpawnParameters Params;
		Params.Name = *FGuid::NewGuid().ToString();
		TestActor = &Spawner.SpawnActor<AArcMassAttachmentTestActor>(Params);

		FArcMassItemAttachmentSlot Slot;
		Slot.SlotId = TAG_RealHandler_Slot;

		FInstancedStruct HandlerInst;
		HandlerInst.InitializeAs(FArcMassAttachmentHandler_StaticMesh::StaticStruct());
		FArcMassAttachmentHandler_StaticMesh* H =
			HandlerInst.GetMutablePtr<FArcMassAttachmentHandler_StaticMesh>();
		H->ComponentTag = TEXT("TestMesh");
		FArcSocketArray Sockets;
		Sockets.SocketName = TEXT("hand_r");
		H->AttachmentData.Add(Sockets);
		Slot.Handlers.Add(MoveTemp(HandlerInst));

		Config = ArcMassItems::TestHelpers::BuildAttachmentConfig({ Slot });
		Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, Config);
		ArcMassItems::TestHelpers::LinkActorToEntity(*EM, Entity, TestActor);
		Processor = ArcMassItems::TestHelpers::RegisterAttachmentProcessor(&Spawner.GetWorld(), *EM);

		Settings = GetDefault<UArcMassItemTestsSettings>();
	}

	bool MaybeSkip()
	{
		if (!Settings->StaticMeshAttachmentItem.IsValid())
		{
			AddWarning(TEXT(
				"StaticMeshAttachmentItem unset — author "
				"DA_TestItem_StaticMeshAttachment + DefaultArcTests.ini entry to enable"));
			return true;
		}
		return false;
	}

	void Run(FName Base)
	{
		ArcMassItems::TestHelpers::RunProcessor(*Processor, *EM, *Signals,
			{ ArcMassItems::TestHelpers::StoreQualifiedSignal(Base) }, Entity);
	}

	TEST_METHOD(SlottingItem_LoadsMeshAndSpawnsStaticMeshComponent)
	{
		if (MaybeSkip()) { return; }

		FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->StaticMeshAttachmentItem, 1, 1);
		FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::AddItemToSlot(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Id, TAG_RealHandler_Slot);
		Run(UE::ArcMassItems::Signals::ItemSlotChanged);

		const FArcMassItemAttachmentStateFragment* State =
			FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
		ASSERT_THAT(IsNotNull(State));
		const FArcMassItemAttachmentObjectArray* Arr = State->ObjectsAttachedFromItem.Find(Id);
		ASSERT_THAT(IsNotNull(Arr));
		ASSERT_THAT(IsTrue(Arr->Objects.Num() >= 1));
		ASSERT_THAT(IsNotNull(Cast<UStaticMeshComponent>(Arr->Objects[0].Get())));
	}

	TEST_METHOD(SpawnedComponent_RegisteredAndAttachedToActor)
	{
		if (MaybeSkip()) { return; }

		FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->StaticMeshAttachmentItem, 1, 1);
		FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::AddItemToSlot(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Id, TAG_RealHandler_Slot);
		Run(UE::ArcMassItems::Signals::ItemSlotChanged);

		const FArcMassItemAttachmentStateFragment* State =
			FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
		const FArcMassItemAttachmentObjectArray* Arr = State->ObjectsAttachedFromItem.Find(Id);
		ASSERT_THAT(IsNotNull(Arr));
		UStaticMeshComponent* Comp = Cast<UStaticMeshComponent>(Arr->Objects[0].Get());
		ASSERT_THAT(IsNotNull(Comp));
		ASSERT_THAT(IsTrue(Comp->IsRegistered()));
		ASSERT_THAT(AreEqual(TestActor->Root, ToRawPtr(Comp->GetAttachParent())));
		ASSERT_THAT(AreEqual(FName(TEXT("hand_r")), Comp->GetAttachSocketName()));
	}

	TEST_METHOD(RemovingItem_DestroysSpawnedComponent)
	{
		if (MaybeSkip()) { return; }

		FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->StaticMeshAttachmentItem, 1, 1);
		FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::AddItemToSlot(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Id, TAG_RealHandler_Slot);
		Run(UE::ArcMassItems::Signals::ItemSlotChanged);

		TWeakObjectPtr<UObject> SpawnedRef;
		{
			const FArcMassItemAttachmentStateFragment* State =
				FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
			const FArcMassItemAttachmentObjectArray* Arr = State->ObjectsAttachedFromItem.Find(Id);
			SpawnedRef = Arr ? Arr->Objects[0] : TWeakObjectPtr<UObject>();
		}
		ASSERT_THAT(IsTrue(SpawnedRef.IsValid()));

		ArcMassItems::RemoveItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Id);
		Run(UE::ArcMassItems::Signals::ItemRemoved);

		const FArcMassItemAttachmentStateFragment* State =
			FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
		ASSERT_THAT(IsFalse(State->ObjectsAttachedFromItem.Contains(Id)));
		ASSERT_THAT(IsFalse(SpawnedRef.IsValid()));
	}

	TEST_METHOD(ReplacingVisualItem_DestroysOldComponentSpawnsNew)
	{
		if (MaybeSkip()) { return; }

		FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->StaticMeshAttachmentItem, 1, 1);
		FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::AddItemToSlot(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Id, TAG_RealHandler_Slot);
		Run(UE::ArcMassItems::Signals::ItemSlotChanged);

		TWeakObjectPtr<UObject> OldComp;
		{
			const FArcMassItemAttachmentStateFragment* State =
				FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
			const FArcMassItemAttachmentObjectArray* Arr = State->ObjectsAttachedFromItem.Find(Id);
			OldComp = Arr ? Arr->Objects[0] : TWeakObjectPtr<UObject>();
		}

		UArcItemDefinition* Def = const_cast<UArcItemDefinition*>(Spec.GetItemDefinition());
		ArcMassItems::SetVisualItemAttachment(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Id, Def);
		Run(UE::ArcMassItems::Signals::ItemAttachmentChanged);

		const FArcMassItemAttachmentStateFragment* State =
			FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
		const FArcMassItemAttachmentObjectArray* Arr = State->ObjectsAttachedFromItem.Find(Id);
		ASSERT_THAT(IsNotNull(Arr));
		ASSERT_THAT(IsTrue(Arr->Objects.Num() >= 1));
	}

	TEST_METHOD(ItemDefinitionWithoutStaticMeshFragment_NoSpawn)
	{
		FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
		FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::AddItemToSlot(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Id, TAG_RealHandler_Slot);
		Run(UE::ArcMassItems::Signals::ItemSlotChanged);

		const FArcMassItemAttachmentStateFragment* State =
			FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
		ASSERT_THAT(IsNotNull(State));
		ASSERT_THAT(IsFalse(State->ObjectsAttachedFromItem.Contains(Id)));
	}
};
