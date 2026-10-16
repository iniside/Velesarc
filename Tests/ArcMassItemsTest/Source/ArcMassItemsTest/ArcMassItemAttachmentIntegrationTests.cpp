// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassItemAttachmentIntegrationTests.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

AArcMassAttachmentTestActor::AArcMassAttachmentTestActor()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->ComponentTags.Add(TEXT("TestMesh"));
	SetRootComponent(Root);
}

// ---------------------------------------------------------------------------
// PIE-backed integration tests — require ENABLE_ARC_STANDALONE_PIE_TEST
// ---------------------------------------------------------------------------

#include "ArcStandalonePIEComponent.h"

#if ENABLE_ARC_STANDALONE_PIE_TEST

#include "CQTest.h"
#include "Engine/World.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "MassSignalSubsystem.h"
#include "Mass/EntityHandle.h"

#include "Attachments/ArcMassItemAttachmentConfig.h"
#include "Attachments/ArcMassItemAttachmentFragments.h"
#include "Attachments/ArcMassItemAttachmentSlots.h"
#include "Attachments/ArcMassAttachmentHandler_StaticMesh.h"
#include "Events/ArcMassItemEventSubsystem.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"
#include "Items/ArcItemSpec.h"

#include "ArcMassItemTestHelpers.h"
#include "ArcMassItemTestsSettings.h"

#include "GameMode/ArcMassGameMode.h"

#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RealHandler_Slot, "ProcTest.RealHandler.Slot");

TEST_CLASS_WITH_FLAGS(ArcMassItemAttachment_RealHandler,
	"ArcMassItems.Attachments.Integration.RealHandler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
		AArcMassGameMode::StaticClass() };

	AArcMassAttachmentTestActor* TestActor = nullptr;
	FMassEntityManager* EM = nullptr;
	UArcMassItemAttachmentConfig* Config = nullptr;
	FMassEntityHandle Entity;
	const UArcMassItemTestsSettings* Settings = nullptr;

	BEFORE_EACH()
	{
		TestRunner->AddExpectedError(
			TEXT("FMassEntityQuery::CacheArchetypes"),
			EAutomationExpectedErrorFlags::Contains, -1);

		TestRunner->AddExpectedError(
			TEXT("Failed to identify experience"),
			EAutomationExpectedErrorFlags::Contains, -1);

		Settings = GetDefault<UArcMassItemTestsSettings>();
		if (Settings != nullptr && !Settings->TestMap.IsNull())
		{
			PIE.WithMap(Settings->TestMap.ToSoftObjectPath());
		}

		PIE.Then(TEXT("Setup"), [this]()
		{
			FAutomationTestBase::bSuppressLogErrors = true;
			FAutomationTestBase::bSuppressLogWarnings = true;

			UWorld* World = PIE.GetState().World;
			ASSERT_THAT(IsNotNull(World));

			UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
			ASSERT_THAT(IsNotNull(MES));
			EM = &MES->GetMutableEntityManager();

			FActorSpawnParameters Params;
			Params.Name = *FGuid::NewGuid().ToString();
			TestActor = World->SpawnActor<AArcMassAttachmentTestActor>(
				AArcMassAttachmentTestActor::StaticClass(),
				FTransform::Identity,
				Params);
			ASSERT_THAT(IsNotNull(TestActor));

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
		});
	}

	void Drain(UWorld* World)
	{
		ArcMassItems::TestHelpers::DrainAttachmentEvents(World);
	}

	TEST_METHOD(SlottingItem_LoadsMeshAndSpawnsStaticMeshComponent)
	{
		PIE.Then(TEXT("SlotItem"), [this]()
		{
			ASSERT_THAT(IsNotNull(Settings));
			ASSERT_THAT(IsTrue(Settings->StaticMeshAttachmentItem.IsValid()));

			UWorld* World = PIE.GetState().World;
			FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->StaticMeshAttachmentItem, 1, 1);
			FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
			ArcMassItems::AddItemToSlot(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Id, TAG_RealHandler_Slot);
			// Phase collapse: AddedToSlot + Attach both happen in one drain.
			Drain(World);

			const FArcMassItemAttachmentStateFragment* State =
				FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
			ASSERT_THAT(IsNotNull(State));
			const FArcMassItemAttachmentObjectArray* Arr = State->ObjectsAttachedFromItem.Find(Id);
			ASSERT_THAT(IsNotNull(Arr));
			ASSERT_THAT(IsTrue(Arr->Objects.Num() >= 1));
			ASSERT_THAT(IsNotNull(Cast<UStaticMeshComponent>(Arr->Objects[0].Get())));
		});
	}

	TEST_METHOD(SpawnedComponent_RegisteredAndAttachedToActor)
	{
		PIE.Then(TEXT("SlotItem"), [this]()
		{
			ASSERT_THAT(IsNotNull(Settings));
			ASSERT_THAT(IsTrue(Settings->StaticMeshAttachmentItem.IsValid()));

			UWorld* World = PIE.GetState().World;
			FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->StaticMeshAttachmentItem, 1, 1);
			FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
			ArcMassItems::AddItemToSlot(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Id, TAG_RealHandler_Slot);
			Drain(World);

			const FArcMassItemAttachmentStateFragment* State =
				FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
			const FArcMassItemAttachmentObjectArray* Arr = State->ObjectsAttachedFromItem.Find(Id);
			ASSERT_THAT(IsNotNull(Arr));
			UStaticMeshComponent* Comp = Cast<UStaticMeshComponent>(Arr->Objects[0].Get());
			ASSERT_THAT(IsNotNull(Comp));
			ASSERT_THAT(IsTrue(Comp->IsRegistered()));
			ASSERT_THAT(AreEqual(TestActor->Root, ToRawPtr(Comp->GetAttachParent())));
			ASSERT_THAT(AreEqual(FName(TEXT("hand_r")), Comp->GetAttachSocketName()));
		});
	}

	TEST_METHOD(RemovingItem_DestroysSpawnedComponent)
	{
		TWeakObjectPtr<UObject> SpawnedRef;

		PIE.Then(TEXT("SlotAndCapture"), [this, &SpawnedRef]()
		{
			ASSERT_THAT(IsNotNull(Settings));
			ASSERT_THAT(IsTrue(Settings->StaticMeshAttachmentItem.IsValid()));

			UWorld* World = PIE.GetState().World;
			UE_LOGF(LogTemp, Warning,"[CRASHDBG] entity valid=%d World=%p", EM->IsEntityValid(Entity), World);
			FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->StaticMeshAttachmentItem, 1, 1);
			FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
			UE_LOGF(LogTemp, Warning,"[CRASHDBG] after AddItem Id.IsValid=%d", Id.IsValid());
			ArcMassItems::AddItemToSlot(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Id, TAG_RealHandler_Slot);
			UE_LOGF(LogTemp, Warning,"[CRASHDBG] after AddItemToSlot, before Drain");
			// Phase collapse: AddedToSlot + Attach in one drain.
			Drain(World);
			UE_LOGF(LogTemp, Warning,"[CRASHDBG] after Drain, entity valid=%d", EM->IsEntityValid(Entity));

			{
				const FArcMassItemAttachmentStateFragment* State =
					FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
				UE_LOGF(LogTemp, Warning,"[CRASHDBG] State ptr=%p", State);
				const FArcMassItemAttachmentObjectArray* Arr = State->ObjectsAttachedFromItem.Find(Id);
				UE_LOGF(LogTemp, Warning,"[CRASHDBG] Arr ptr=%p Objects.Num=%d", Arr, Arr ? Arr->Objects.Num() : -1);
				SpawnedRef = Arr ? Arr->Objects[0] : TWeakObjectPtr<UObject>();
				UE_LOGF(LogTemp, Warning,"[CRASHDBG] SpawnedRef captured valid=%d", SpawnedRef.IsValid());
			}
			ASSERT_THAT(IsTrue(SpawnedRef.IsValid()));

			// Phase collapse: Remove + detach/destroy in one drain (no VisualDetached step).
			UE_LOGF(LogTemp, Warning,"[CRASHDBG] before RemoveItem, entity valid=%d", EM->IsEntityValid(Entity));
			ArcMassItems::RemoveItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Id);
			UE_LOGF(LogTemp, Warning,"[CRASHDBG] after RemoveItem, entity valid=%d, before Drain2", EM->IsEntityValid(Entity));
			Drain(World);
			UE_LOGF(LogTemp, Warning,"[CRASHDBG] after Drain2, entity valid=%d", EM->IsEntityValid(Entity));

			const FArcMassItemAttachmentStateFragment* State =
				FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
			UE_LOGF(LogTemp, Warning,"[CRASHDBG] State2 ptr=%p SpawnedRef.valid=%d", State, SpawnedRef.IsValid());
			ASSERT_THAT(IsFalse(State->ObjectsAttachedFromItem.Contains(Id)));
			UE_LOGF(LogTemp, Warning,"[CRASHDBG] passed Contains assert");
			ASSERT_THAT(IsFalse(SpawnedRef.IsValid()));
		});
	}

	TEST_METHOD(ReplacingVisualItem_DestroysOldComponentSpawnsNew)
	{
		PIE.Then(TEXT("SlotAndReplace"), [this]()
		{
			ASSERT_THAT(IsNotNull(Settings));
			ASSERT_THAT(IsTrue(Settings->StaticMeshAttachmentItem.IsValid()));

			UWorld* World = PIE.GetState().World;
			FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->StaticMeshAttachmentItem, 1, 1);
			FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
			ArcMassItems::AddItemToSlot(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Id, TAG_RealHandler_Slot);
			Drain(World);

			TWeakObjectPtr<UObject> OldComp;
			{
				const FArcMassItemAttachmentStateFragment* State =
					FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
				const FArcMassItemAttachmentObjectArray* Arr = State->ObjectsAttachedFromItem.Find(Id);
				OldComp = Arr ? Arr->Objects[0] : TWeakObjectPtr<UObject>();
			}

			const FPrimaryAssetId Def = Spec.GetItemDefinitionId();
			ArcMassItems::SetVisualItemAttachment(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Id, Def);
			Drain(World);

			const FArcMassItemAttachmentStateFragment* State =
				FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
			const FArcMassItemAttachmentObjectArray* Arr = State->ObjectsAttachedFromItem.Find(Id);
			ASSERT_THAT(IsNotNull(Arr));
			ASSERT_THAT(IsTrue(Arr->Objects.Num() >= 1));

			// The swap must destroy the old component and spawn a fresh one.
			ASSERT_THAT(IsFalse(OldComp.IsValid()));
			ASSERT_THAT(IsTrue(Arr->Objects[0].IsValid()));
			ASSERT_THAT(IsTrue(Arr->Objects[0].Get() != OldComp.Get()));
		});
	}

	TEST_METHOD(ItemDefinitionWithoutStaticMeshFragment_NoSpawn)
	{
		PIE.Then(TEXT("SlotBaseItem"), [this]()
		{
			ASSERT_THAT(IsNotNull(Settings));

			UWorld* World = PIE.GetState().World;
			FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
			FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
			ArcMassItems::AddItemToSlot(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Id, TAG_RealHandler_Slot);
			Drain(World);

			const FArcMassItemAttachmentStateFragment* State =
				FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
			ASSERT_THAT(IsNotNull(State));
			ASSERT_THAT(IsFalse(State->ObjectsAttachedFromItem.Contains(Id)));
		});
	}
};

#endif // ENABLE_ARC_STANDALONE_PIE_TEST
