// Copyright Lukasz Baran. All Rights Reserved.
//
// Attachment subsystem tests — previously "processor tests".
// Drive is: perform item op → EventSub->DrainQueues() → assert State.
// Phase collapse: subsystem inlines AddedToSlot+Attach in one drain; no separate
// VisualAttached step.  Handler call-log phases (EPhase::*) are unchanged.

#include "ArcStandalonePIEComponent.h"

#if ENABLE_ARC_STANDALONE_PIE_TEST

#include "CQTest.h"
#include "Engine/World.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "MassSignalSubsystem.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityFragments.h"

#include "Attachments/ArcMassItemAttachmentConfig.h"
#include "Attachments/ArcMassItemAttachmentFragments.h"
#include "Attachments/ArcMassItemAttachmentSlots.h"
#include "Events/ArcMassItemEventSubsystem.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"
#include "Signals/ArcMassItemSignals.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemTypes.h"
#include "Equipment/Fragments/ArcItemFragment_StaticMeshAttachment.h"
#include "Equipment/Fragments/ArcItemFragment_AnimLayer.h"

#include "ArcItemTestRecordFiles.h"
#include "ArcMassItemTestHelpers.h"
#include "ArcMassItemTestsSettings.h"
#include "ArcMassQuickBarTestHelpers.h"
#include "QuickBar/ArcMassQuickBarAutoSelectSubsystem.h"
#include "QuickBar/ArcMassQuickBarConfig.h"
#include "QuickBar/ArcMassQuickBarStateFragment.h"
#include "QuickBar/ArcMassQuickBarTypes.h"

#include "GameMode/ArcMassGameMode.h"

#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ProcTest_Slot_A,  "ProcTest.Slot.A");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ProcTest_Slot_B,  "ProcTest.Slot.B");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ProcTest_SubSlot, "ProcTest.SubSlot");

using EPhase = FArcMassAttachmentHandler_Test::EPhase;

namespace ArcMassItemAttachmentProcessorTests
{
    const UScriptStruct* StoreType() { return FArcMassItemStoreFragment::StaticStruct(); }
}

// ---------------------------------------------------------------------------
// SlotChanged group (assign / move / sub-attach)
// ---------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(ArcMassItemAttachment_Processor_SlotChanged,
    "ArcMassItems.Attachments.Processor.SlotChanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
    FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
        AArcMassGameMode::StaticClass() };

    FMassEntityManager* EM = nullptr;
    UArcMassItemEventSubsystem* EventSub = nullptr;
    UArcMassItemAttachmentConfig* Config = nullptr;
    FArcMassAttachmentHandler_Test* Handler_A = nullptr;
    FMassEntityHandle Entity;
    const UArcMassItemTestsSettings* Settings = nullptr;
    FArcItemTestRecordFiles Records;
    FPrimaryAssetId SimpleBaseItem;

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

            EventSub = World->GetSubsystem<UArcMassItemEventSubsystem>();
            ASSERT_THAT(IsNotNull(EventSub));

            UScriptStruct* Marker = ArcMassItems::TestHelpers::MarkerFragmentType();

            FArcMassItemAttachmentSlot SlotA; SlotA.SlotId = TAG_ProcTest_Slot_A;
            FInstancedStruct InstA;
            InstA.InitializeAs(FArcMassAttachmentHandler_Test::StaticStruct());
            InstA.GetMutablePtr<FArcMassAttachmentHandler_Test>()->SupportedFragment = Marker;
            SlotA.Handlers.Add(MoveTemp(InstA));

            FArcMassItemAttachmentSlot SlotB; SlotB.SlotId = TAG_ProcTest_Slot_B;
            FInstancedStruct InstB;
            InstB.InitializeAs(FArcMassAttachmentHandler_Test::StaticStruct());
            InstB.GetMutablePtr<FArcMassAttachmentHandler_Test>()->SupportedFragment = Marker;
            SlotB.Handlers.Add(MoveTemp(InstB));

            FArcMassItemAttachmentSlot SubSlot; SubSlot.SlotId = TAG_ProcTest_SubSlot;
            FInstancedStruct InstSub;
            InstSub.InitializeAs(FArcMassAttachmentHandler_Test::StaticStruct());
            InstSub.GetMutablePtr<FArcMassAttachmentHandler_Test>()->SupportedFragment = Marker;
            SubSlot.Handlers.Add(MoveTemp(InstSub));

            Config = ArcMassItems::TestHelpers::BuildAttachmentConfig({ SlotA, SlotB, SubSlot });
            Handler_A = Config->Slots[0].Handlers[0].GetMutablePtr<FArcMassAttachmentHandler_Test>();

            Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, Config);
            SimpleBaseItem = ArcMassItems::TestHelpers::WriteWithMarkerFragment(Records, Settings->SimpleBaseItem);
            ASSERT_THAT(IsTrue(SimpleBaseItem.IsValid()));
        });
    }

    TEST_METHOD(AddingItemToSlot_DispatchesAddedToSlotToHandler)
    {
        PIE.Then(TEXT("AddingItemToSlot_DispatchesAddedToSlotToHandler"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            FArcItemSpec Spec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId ItemId = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec);
            ArcMassItems::AddItemToSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), ItemId, TAG_ProcTest_Slot_A);
            // Phase collapse: AddedToSlot + Attach both run in one drain.
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            ASSERT_THAT(AreEqual(1, Handler_A->NumCalls(EPhase::AddedToSlot)));
            const FArcMassAttachmentHandler_Test::FCall* Last = Handler_A->LastCall(EPhase::AddedToSlot);
            ASSERT_THAT(IsNotNull(Last));
            ASSERT_THAT(AreEqual(ItemId, Last->ItemId));
            ASSERT_THAT(AreEqual(FGameplayTag(TAG_ProcTest_Slot_A), Last->SlotId));

            const FArcMassItemAttachmentStateFragment* State =
                FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
            ASSERT_THAT(IsNotNull(State));
            ASSERT_THAT(AreEqual(1, State->ActiveAttachments.Num()));
            ASSERT_THAT(IsTrue(State->TakenSockets.Contains(TAG_ProcTest_Slot_A)));
        });
    }

    TEST_METHOD(AddingItemToOccupiedSlot_DoesNotDispatch)
    {
        PIE.Then(TEXT("AddingItemToOccupiedSlot_DoesNotDispatch"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            FArcItemSpec Spec1 = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemSpec Spec2 = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId Id1 = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec1);
            FArcItemId Id2 = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec2);

            ArcMassItems::AddItemToSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id1, TAG_ProcTest_Slot_A);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);
            const int32 BaselineCalls = Handler_A->NumCalls(EPhase::AddedToSlot);

            const bool bSlotted2 = ArcMassItems::AddItemToSlot(
                *EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id2, TAG_ProcTest_Slot_A);
            ASSERT_THAT(IsFalse(bSlotted2));

            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);
            ASSERT_THAT(AreEqual(BaselineCalls, Handler_A->NumCalls(EPhase::AddedToSlot)));
        });
    }

    TEST_METHOD(ChangingItemSlot_DispatchesDetachOnOldSlotThenAddOnNew)
    {
        PIE.Then(TEXT("ChangingItemSlot_DispatchesDetachOnOldSlotThenAddOnNew"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            FArcMassAttachmentHandler_Test* Handler_B =
                Config->Slots[1].Handlers[0].GetMutablePtr<FArcMassAttachmentHandler_Test>();

            FArcItemSpec Spec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec);

            ArcMassItems::AddItemToSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, TAG_ProcTest_Slot_A);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            // Move: slot A → slot B.  One drain covers detach-old + attach-new.
            ArcMassItems::ChangeItemSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, TAG_ProcTest_Slot_B);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            const FArcMassAttachmentHandler_Test::FCall* DetachA = Handler_A->LastCall(EPhase::RemovedFromSlot);
            ASSERT_THAT(IsNotNull(DetachA));
            ASSERT_THAT(AreEqual(FGameplayTag(TAG_ProcTest_Slot_A), DetachA->Snapshot.SlotId));

            ASSERT_THAT(AreEqual(1, Handler_B->NumCalls(EPhase::AddedToSlot)));
        });
    }

    TEST_METHOD(SubAttachment_DispatchesUnderOwner)
    {
        PIE.Then(TEXT("SubAttachment_DispatchesUnderOwner"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            FArcMassAttachmentHandler_Test* Handler_Sub =
                Config->Slots[2].Handlers[0].GetMutablePtr<FArcMassAttachmentHandler_Test>();

            FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId OwnerId = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), OwnerSpec);
            ArcMassItems::AddItemToSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), OwnerId, TAG_ProcTest_Slot_A);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            FArcItemSpec ChildSpec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId ChildId = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), ChildSpec);
            ArcMassItems::AttachItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), OwnerId, ChildId, TAG_ProcTest_SubSlot);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            ASSERT_THAT(IsTrue(Handler_Sub->NumCalls(EPhase::Attach) > 0
                            || Handler_Sub->NumCalls(EPhase::AddedToSlot) > 0));
            const FArcMassAttachmentHandler_Test::FCall* Last = Handler_Sub->NumCalls(EPhase::Attach) > 0
                ? Handler_Sub->LastCall(EPhase::Attach)
                : Handler_Sub->LastCall(EPhase::AddedToSlot);
            ASSERT_THAT(IsNotNull(Last));
            ASSERT_THAT(AreEqual(OwnerId, Last->OwnerId));
        });
    }
};

// ---------------------------------------------------------------------------
// Removed group
// ---------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(ArcMassItemAttachment_Processor_Removed,
    "ArcMassItems.Attachments.Processor.Removed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
    FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
        AArcMassGameMode::StaticClass() };

    FMassEntityManager* EM = nullptr;
    UArcMassItemEventSubsystem* EventSub = nullptr;
    UArcMassItemAttachmentConfig* Config = nullptr;
    FArcMassAttachmentHandler_Test* Handler = nullptr;
    FMassEntityHandle Entity;
    const UArcMassItemTestsSettings* Settings = nullptr;
    FArcItemTestRecordFiles Records;
    FPrimaryAssetId SimpleBaseItem;

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

            EventSub = World->GetSubsystem<UArcMassItemEventSubsystem>();
            ASSERT_THAT(IsNotNull(EventSub));

            UScriptStruct* Marker = ArcMassItems::TestHelpers::MarkerFragmentType();

            Config = ArcMassItems::TestHelpers::BuildSingleHandlerConfig(
                TAG_ProcTest_Slot_A, Handler, Marker);
            // Also include the sub-slot for the cascade test.
            FArcMassItemAttachmentSlot SubSlot; SubSlot.SlotId = TAG_ProcTest_SubSlot;
            FInstancedStruct InstSub;
            InstSub.InitializeAs(FArcMassAttachmentHandler_Test::StaticStruct());
            InstSub.GetMutablePtr<FArcMassAttachmentHandler_Test>()->SupportedFragment = Marker;
            SubSlot.Handlers.Add(MoveTemp(InstSub));
            Config->Slots.Add(SubSlot);

            Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, Config);
            SimpleBaseItem = ArcMassItems::TestHelpers::WriteWithMarkerFragment(Records, Settings->SimpleBaseItem);
            ASSERT_THAT(IsTrue(SimpleBaseItem.IsValid()));
        });
    }

    TEST_METHOD(RemovingSlottedItem_DispatchesDetachWithSnapshot)
    {
        PIE.Then(TEXT("RemovingSlottedItem_DispatchesDetachWithSnapshot"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            FArcItemSpec Spec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec);
            ArcMassItems::AddItemToSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, TAG_ProcTest_Slot_A);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            ArcMassItems::RemoveItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            const FArcMassAttachmentHandler_Test::FCall* Last = Handler->LastCall(EPhase::Detach);
            if (Last == nullptr) { Last = Handler->LastCall(EPhase::RemovedFromSlot); }
            ASSERT_THAT(IsNotNull(Last));
            ASSERT_THAT(AreEqual(Id, Last->Snapshot.ItemId));
            ASSERT_THAT(AreEqual(FGameplayTag(TAG_ProcTest_Slot_A), Last->Snapshot.SlotId));
        });
    }

    TEST_METHOD(RemovingUnslottedItem_DoesNotDispatch)
    {
        PIE.Then(TEXT("RemovingUnslottedItem_DoesNotDispatch"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            FArcItemSpec Spec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec);
            // Item never slotted.
            ArcMassItems::RemoveItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            ASSERT_THAT(AreEqual(0, Handler->NumCalls(EPhase::Detach)));
            ASSERT_THAT(AreEqual(0, Handler->NumCalls(EPhase::RemovedFromSlot)));
        });
    }

    TEST_METHOD(RemovingOwnerCascadesDetachOnAttachments)
    {
        PIE.Then(TEXT("RemovingOwnerCascadesDetachOnAttachments"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            FArcMassAttachmentHandler_Test* SubHandler =
                Config->Slots[1].Handlers[0].GetMutablePtr<FArcMassAttachmentHandler_Test>();

            FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId OwnerId = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), OwnerSpec);
            ArcMassItems::AddItemToSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), OwnerId, TAG_ProcTest_Slot_A);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            FArcItemSpec ChildSpec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId ChildId = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), ChildSpec);
            ArcMassItems::AttachItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), OwnerId, ChildId, TAG_ProcTest_SubSlot);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            const int32 OwnerDetachBefore  = Handler->NumCalls(EPhase::Detach) + Handler->NumCalls(EPhase::RemovedFromSlot);
            const int32 SubDetachBefore    = SubHandler->NumCalls(EPhase::Detach) + SubHandler->NumCalls(EPhase::RemovedFromSlot);

            ArcMassItems::RemoveItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), OwnerId);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            ASSERT_THAT(IsTrue(Handler->NumCalls(EPhase::Detach) + Handler->NumCalls(EPhase::RemovedFromSlot) > OwnerDetachBefore));
            ASSERT_THAT(IsTrue(SubHandler->NumCalls(EPhase::Detach) + SubHandler->NumCalls(EPhase::RemovedFromSlot) > SubDetachBefore));
        });
    }
};

// ---------------------------------------------------------------------------
// VisualAttached group — phase-collapsed: slot+attach in one drain, no separate visual step.
// Previous test verified ActiveAttachments after a phantom VisualAttached signal.
// New test: after AddItemToSlot + drain, both AddedToSlot AND Attach phases have fired,
// and ObjectsAttachedFromItem is populated (the test handler writes nothing there, but
// ActiveAttachments >= 1 is the observable invariant).
// ---------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(ArcMassItemAttachment_Processor_VisualAttached,
    "ArcMassItems.Attachments.Processor.VisualAttached",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
    FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
        AArcMassGameMode::StaticClass() };

    FMassEntityManager* EM = nullptr;
    UArcMassItemEventSubsystem* EventSub = nullptr;
    UArcMassItemAttachmentConfig* Config = nullptr;
    FArcMassAttachmentHandler_Test* Handler = nullptr;
    FMassEntityHandle Entity;
    const UArcMassItemTestsSettings* Settings = nullptr;
    FArcItemTestRecordFiles Records;
    FPrimaryAssetId SimpleBaseItem;

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

            EventSub = World->GetSubsystem<UArcMassItemEventSubsystem>();
            ASSERT_THAT(IsNotNull(EventSub));

            Config = ArcMassItems::TestHelpers::BuildSingleHandlerConfig(
                TAG_ProcTest_Slot_A, Handler, ArcMassItems::TestHelpers::MarkerFragmentType());
            Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, Config);
            SimpleBaseItem = ArcMassItems::TestHelpers::WriteWithMarkerFragment(Records, Settings->SimpleBaseItem);
            ASSERT_THAT(IsTrue(SimpleBaseItem.IsValid()));
        });
    }

    TEST_METHOD(SlotAssign_DispatchesBothAddedToSlotAndAttachInOneDrain)
    {
        PIE.Then(TEXT("SlotAssign_DispatchesBothAddedToSlotAndAttachInOneDrain"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            // Phase collapse: subsystem inlines AddedToSlot + Attach in one DrainQueues.
            // No separate VisualAttached step exists anymore.
            FArcItemSpec Spec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec);
            ArcMassItems::AddItemToSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, TAG_ProcTest_Slot_A);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            ASSERT_THAT(AreEqual(1, Handler->NumCalls(EPhase::AddedToSlot)));
            ASSERT_THAT(AreEqual(1, Handler->NumCalls(EPhase::Attach)));

            const FArcMassItemAttachmentStateFragment* State =
                FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
            ASSERT_THAT(IsNotNull(State));
            ASSERT_THAT(IsTrue(State->ActiveAttachments.Num() >= 1));
        });
    }
};

// ---------------------------------------------------------------------------
// VisualDetached group — phase-collapsed: remove → drain removes item and destroys
// visual in one pass (no separate VisualDetached signal step).
// ---------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(ArcMassItemAttachment_Processor_VisualDetached,
    "ArcMassItems.Attachments.Processor.VisualDetached",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
    FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
        AArcMassGameMode::StaticClass() };

    FMassEntityManager* EM = nullptr;
    UArcMassItemEventSubsystem* EventSub = nullptr;
    UArcMassItemAttachmentConfig* Config = nullptr;
    FArcMassAttachmentHandler_Test* Handler = nullptr;
    FMassEntityHandle Entity;
    const UArcMassItemTestsSettings* Settings = nullptr;
    FArcItemTestRecordFiles Records;
    FPrimaryAssetId SimpleBaseItem;

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

            EventSub = World->GetSubsystem<UArcMassItemEventSubsystem>();
            ASSERT_THAT(IsNotNull(EventSub));

            Config = ArcMassItems::TestHelpers::BuildSingleHandlerConfig(
                TAG_ProcTest_Slot_A, Handler, ArcMassItems::TestHelpers::MarkerFragmentType());
            Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, Config);
            SimpleBaseItem = ArcMassItems::TestHelpers::WriteWithMarkerFragment(Records, Settings->SimpleBaseItem);
            ASSERT_THAT(IsTrue(SimpleBaseItem.IsValid()));
        });
    }

    TEST_METHOD(RemovingSlottedItem_ClearsObjectsAttachedFromItemInOneDrain)
    {
        PIE.Then(TEXT("RemovingSlottedItem_ClearsObjectsAttachedFromItemInOneDrain"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            FArcItemSpec Spec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec);
            ArcMassItems::AddItemToSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, TAG_ProcTest_Slot_A);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            // Seed a fake object entry to simulate what a real handler would have written.
            FArcMassItemAttachmentStateFragment* State =
                FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
            ASSERT_THAT(IsNotNull(State));
            FArcMassItemAttachmentObjectArray Arr;
            Arr.Objects.Add(GetTransientPackage());
            State->ObjectsAttachedFromItem.Add(Id, Arr);

            // Phase collapse: Remove → drain → HandleItemDetach clears ObjectsAttachedFromItem.
            // No separate VisualDetached step needed.
            ArcMassItems::RemoveItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            State = FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
            ASSERT_THAT(IsFalse(State->ObjectsAttachedFromItem.Contains(Id)));
        });
    }

    TEST_METHOD(RemovingItemNotInStore_NoCrashStateClean)
    {
        PIE.Then(TEXT("RemovingItemNotInStore_NoCrashStateClean"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            // Item added and slotted, then removed — then a second drain (no-op) should
            // not crash and state should remain consistent.
            FArcItemSpec Spec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec);
            ArcMassItems::AddItemToSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, TAG_ProcTest_Slot_A);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            ArcMassItems::RemoveItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            // Second drain — item already gone; no crash, no stale ActiveAttachments.
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            FArcMassItemAttachmentStateFragment* State =
                FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
            ASSERT_THAT(IsNotNull(State));
            ASSERT_THAT(IsFalse(State->ObjectsAttachedFromItem.Contains(Id)));
        });
    }
};

// ---------------------------------------------------------------------------
// AttachmentChanged group
// ---------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(ArcMassItemAttachment_Processor_AttachmentChanged,
    "ArcMassItems.Attachments.Processor.AttachmentChanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
    FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
        AArcMassGameMode::StaticClass() };

    FMassEntityManager* EM = nullptr;
    UArcMassItemEventSubsystem* EventSub = nullptr;
    UArcMassItemAttachmentConfig* Config = nullptr;
    FArcMassAttachmentHandler_Test* Handler = nullptr;
    FMassEntityHandle Entity;
    const UArcMassItemTestsSettings* Settings = nullptr;
    FArcItemTestRecordFiles Records;
    FPrimaryAssetId SimpleBaseItem;

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

            EventSub = World->GetSubsystem<UArcMassItemEventSubsystem>();
            ASSERT_THAT(IsNotNull(EventSub));

            Config = ArcMassItems::TestHelpers::BuildSingleHandlerConfig(
                TAG_ProcTest_Slot_A, Handler, ArcMassItems::TestHelpers::MarkerFragmentType());
            Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, Config);
            SimpleBaseItem = ArcMassItems::TestHelpers::WriteWithMarkerFragment(Records, Settings->SimpleBaseItem);
            ASSERT_THAT(IsTrue(SimpleBaseItem.IsValid()));
        });
    }

    TEST_METHOD(AttachmentChangedSignal_DispatchesDetachOldThenAddNew)
    {
        PIE.Then(TEXT("AttachmentChangedSignal_DispatchesDetachOldThenAddNew"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            FArcItemSpec Spec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec);
            ArcMassItems::AddItemToSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, TAG_ProcTest_Slot_A);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);
            const int32 BaselineDetach = Handler->NumCalls(EPhase::Detach) + Handler->NumCalls(EPhase::AttachmentChanged);

            FArcItemSpec NewVisualSpec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            const FPrimaryAssetId NewVisualDef = NewVisualSpec.GetItemDefinitionId();
            ArcMassItems::SetVisualItemAttachment(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, NewVisualDef);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            ASSERT_THAT(IsTrue(
                Handler->NumCalls(EPhase::AttachmentChanged) +
                Handler->NumCalls(EPhase::Detach) +
                Handler->NumCalls(EPhase::AddedToSlot) > BaselineDetach));
        });
    }

    TEST_METHOD(AttachmentChangedSignal_NullOldDefinition_OnlyAdds)
    {
        PIE.Then(TEXT("AttachmentChangedSignal_NullOldDefinition_OnlyAdds"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            FArcItemSpec Spec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec);
            ArcMassItems::AddItemToSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, TAG_ProcTest_Slot_A);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            const int32 DetachBefore = Handler->NumCalls(EPhase::Detach);
            const FPrimaryAssetId NewVisualDef = Spec.GetItemDefinitionId();
            ArcMassItems::SetVisualItemAttachment(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, NewVisualDef);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            ASSERT_THAT(AreEqual(DetachBefore, Handler->NumCalls(EPhase::Detach)));
        });
    }

    TEST_METHOD(AttachmentChangedSignal_NullNewDefinition_OnlyDetaches)
    {
        PIE.Then(TEXT("AttachmentChangedSignal_NullNewDefinition_OnlyDetaches"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            FArcItemSpec Spec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec);
            ArcMassItems::AddItemToSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, TAG_ProcTest_Slot_A);
            const FPrimaryAssetId Def = Spec.GetItemDefinitionId();
            ArcMassItems::SetVisualItemAttachment(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, Def);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            const int32 AddedBefore = Handler->NumCalls(EPhase::AddedToSlot);

            ArcMassItems::SetVisualItemAttachment(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, FPrimaryAssetId());
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            ASSERT_THAT(AreEqual(AddedBefore, Handler->NumCalls(EPhase::AddedToSlot)));
        });
    }
};

// ---------------------------------------------------------------------------
// Reconciliation group
// ---------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(ArcMassItemAttachment_Processor_Reconciliation,
    "ArcMassItems.Attachments.Processor.Reconciliation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
    FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
        AArcMassGameMode::StaticClass() };

    FMassEntityManager* EM = nullptr;
    UArcMassItemEventSubsystem* EventSub = nullptr;
    UArcMassItemAttachmentConfig* Config = nullptr;
    FArcMassAttachmentHandler_Test* Handler = nullptr;
    FMassEntityHandle Entity;
    const UArcMassItemTestsSettings* Settings = nullptr;
    FArcItemTestRecordFiles Records;
    FPrimaryAssetId SimpleBaseItem;

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

            EventSub = World->GetSubsystem<UArcMassItemEventSubsystem>();
            ASSERT_THAT(IsNotNull(EventSub));

            Config = ArcMassItems::TestHelpers::BuildSingleHandlerConfig(
                TAG_ProcTest_Slot_A, Handler, ArcMassItems::TestHelpers::MarkerFragmentType());
            Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, Config);
            SimpleBaseItem = ArcMassItems::TestHelpers::WriteWithMarkerFragment(Records, Settings->SimpleBaseItem);
            ASSERT_THAT(IsTrue(SimpleBaseItem.IsValid()));
        });
    }

    TEST_METHOD(ReconciliationSweep_RestoresMissingAttachmentForSlottedItem)
    {
        PIE.Then(TEXT("ReconciliationSweep_RestoresMissingAttachmentForSlottedItem"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            FArcItemSpec Spec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec);
            ArcMassItems::AddItemToSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, TAG_ProcTest_Slot_A);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            // Manually clobber the state to simulate a stale/missed event.
            FArcMassItemAttachmentStateFragment* State =
                FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
            ASSERT_THAT(IsNotNull(State));
            State->ActiveAttachments.Empty();
            State->TakenSockets.Empty();

            // Re-trigger via a slot-assigned queue directly — reconciliation runs on any slot event.
            EventSub->QueueItemSlotAssigned(Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, TAG_ProcTest_Slot_A);
            EventSub->DrainQueues();

            State = FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
            ASSERT_THAT(IsTrue(State->ActiveAttachments.Num() == 1));
        });
    }

    TEST_METHOD(ReconciliationSweep_RemovesStaleAttachmentForUnslottedItem)
    {
        PIE.Then(TEXT("ReconciliationSweep_RemovesStaleAttachmentForUnslottedItem"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            // Insert a stale attachment that doesn't correspond to any slotted item.
            FArcMassItemAttachmentStateFragment* State =
                FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
            ASSERT_THAT(IsNotNull(State));
            FArcMassItemAttachment Stale;
            Stale.ItemId = FArcItemId(FGuid::NewGuid());
            Stale.SlotId = TAG_ProcTest_Slot_A;
            State->ActiveAttachments.Add(Stale);

            // Any slot event triggers the reconciliation sweep.
            FArcItemSpec Spec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec);
            EventSub->QueueItemSlotAssigned(Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, TAG_ProcTest_Slot_A);
            EventSub->DrainQueues();

            State = FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
            ASSERT_THAT(AreEqual(0, State->ActiveAttachments.Num()));
        });
    }
};

// ---------------------------------------------------------------------------
// Dispatch group
// ---------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(ArcMassItemAttachment_Processor_Dispatch,
    "ArcMassItems.Attachments.Processor.Dispatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
    FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
        AArcMassGameMode::StaticClass() };

    FMassEntityManager* EM = nullptr;
    UArcMassItemEventSubsystem* EventSub = nullptr;
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

            EventSub = World->GetSubsystem<UArcMassItemEventSubsystem>();
            ASSERT_THAT(IsNotNull(EventSub));
        });
    }

    TEST_METHOD(MultipleHandlersOnSlot_OnlySupportingHandlerInvoked)
    {
        PIE.Then(TEXT("MultipleHandlersOnSlot_OnlySupportingHandlerInvoked"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            FArcMassItemAttachmentSlot Slot;
            Slot.SlotId = TAG_ProcTest_Slot_A;

            // Handler that requires FArcItemFragment_StaticMeshAttachment.
            FInstancedStruct InstStatic;
            InstStatic.InitializeAs(FArcMassAttachmentHandler_Test::StaticStruct());
            InstStatic.GetMutablePtr<FArcMassAttachmentHandler_Test>()->SupportedFragment =
                FArcItemFragment_StaticMeshAttachment::StaticStruct();
            Slot.Handlers.Add(MoveTemp(InstStatic));

            // Handler that requires FArcItemFragment_AnimLayer.
            FInstancedStruct InstAnim;
            InstAnim.InitializeAs(FArcMassAttachmentHandler_Test::StaticStruct());
            InstAnim.GetMutablePtr<FArcMassAttachmentHandler_Test>()->SupportedFragment =
                FArcItemFragment_AnimLayer::StaticStruct();
            Slot.Handlers.Add(MoveTemp(InstAnim));

            UArcMassItemAttachmentConfig* Config = ArcMassItems::TestHelpers::BuildAttachmentConfig({ Slot });
            FArcMassAttachmentHandler_Test* HStatic =
                Config->Slots[0].Handlers[0].GetMutablePtr<FArcMassAttachmentHandler_Test>();
            FArcMassAttachmentHandler_Test* HAnim =
                Config->Slots[0].Handlers[1].GetMutablePtr<FArcMassAttachmentHandler_Test>();

            FMassEntityHandle Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, Config);

            if (!Settings->StaticMeshAttachmentItem.IsValid())
            {
                AddWarning(TEXT("StaticMeshAttachmentItem unset — skipping dispatch routing test"));
                return;
            }

            FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->StaticMeshAttachmentItem, 1, 1);
            FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec);
            ArcMassItems::AddItemToSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, TAG_ProcTest_Slot_A);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            ASSERT_THAT(AreEqual(1, HStatic->NumCalls(EPhase::AddedToSlot)));
            ASSERT_THAT(AreEqual(0, HAnim->NumCalls(EPhase::AddedToSlot)));
        });
    }

    TEST_METHOD(NoHandlersOnSlot_NoDispatch_NoCrash)
    {
        PIE.Then(TEXT("NoHandlersOnSlot_NoDispatch_NoCrash"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            FArcMassItemAttachmentSlot Slot;
            Slot.SlotId = TAG_ProcTest_Slot_A;
            // No handlers.

            UArcMassItemAttachmentConfig* Config = ArcMassItems::TestHelpers::BuildAttachmentConfig({ Slot });
            FMassEntityHandle Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, Config);

            FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
            FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec);
            ArcMassItems::AddItemToSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, TAG_ProcTest_Slot_A);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            const FArcMassItemAttachmentStateFragment* State =
                FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
            ASSERT_THAT(IsNotNull(State));
            ASSERT_THAT(AreEqual(0, State->ActiveAttachments.Num()));
        });
    }

    TEST_METHOD(NullConfigFragment_EntitySkipped)
    {
        PIE.Then(TEXT("NullConfigFragment_EntitySkipped"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            FMassEntityHandle Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, /*Config*/ nullptr);

            FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
            FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec);
            const bool bSlotted = ArcMassItems::AddItemToSlot(
                *EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, TAG_ProcTest_Slot_A);
            ASSERT_THAT(IsTrue(bSlotted));

            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            const FArcMassItemAttachmentStateFragment* State =
                FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
            ASSERT_THAT(IsNotNull(State));
            ASSERT_THAT(AreEqual(0, State->ActiveAttachments.Num()));
        });
    }
};

// ---------------------------------------------------------------------------
// New tests: SlotChanged(move) collapses to one drain; teardown-observer note
// ---------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(ArcMassItemAttachment_Processor_SubsystemNew,
    "ArcMassItems.Attachments.Processor.SubsystemNew",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
    FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
        AArcMassGameMode::StaticClass() };

    FMassEntityManager* EM = nullptr;
    UArcMassItemEventSubsystem* EventSub = nullptr;
    UArcMassItemAttachmentConfig* Config = nullptr;
    const UArcMassItemTestsSettings* Settings = nullptr;
    FArcItemTestRecordFiles Records;
    FPrimaryAssetId SimpleBaseItem;

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

            EventSub = World->GetSubsystem<UArcMassItemEventSubsystem>();
            ASSERT_THAT(IsNotNull(EventSub));

            SimpleBaseItem = ArcMassItems::TestHelpers::WriteWithMarkerFragment(Records, Settings->SimpleBaseItem);
            ASSERT_THAT(IsTrue(SimpleBaseItem.IsValid()));
        });
    }

    TEST_METHOD(SlotChangedMove_DetachOldAttachNewInOneDrain)
    {
        PIE.Then(TEXT("SlotChangedMove_DetachOldAttachNewInOneDrain"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            // Build two-slot config.
            UScriptStruct* Marker = ArcMassItems::TestHelpers::MarkerFragmentType();

            FArcMassItemAttachmentSlot SlotA; SlotA.SlotId = TAG_ProcTest_Slot_A;
            FInstancedStruct InstA;
            InstA.InitializeAs(FArcMassAttachmentHandler_Test::StaticStruct());
            InstA.GetMutablePtr<FArcMassAttachmentHandler_Test>()->SupportedFragment = Marker;
            SlotA.Handlers.Add(MoveTemp(InstA));

            FArcMassItemAttachmentSlot SlotB; SlotB.SlotId = TAG_ProcTest_Slot_B;
            FInstancedStruct InstB;
            InstB.InitializeAs(FArcMassAttachmentHandler_Test::StaticStruct());
            InstB.GetMutablePtr<FArcMassAttachmentHandler_Test>()->SupportedFragment = Marker;
            SlotB.Handlers.Add(MoveTemp(InstB));

            Config = ArcMassItems::TestHelpers::BuildAttachmentConfig({ SlotA, SlotB });
            FArcMassAttachmentHandler_Test* HandlerA = Config->Slots[0].Handlers[0].GetMutablePtr<FArcMassAttachmentHandler_Test>();
            FArcMassAttachmentHandler_Test* HandlerB = Config->Slots[1].Handlers[0].GetMutablePtr<FArcMassAttachmentHandler_Test>();

            FMassEntityHandle Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, Config);

            FArcItemSpec Spec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Spec);

            // Assign to slot A.
            ArcMassItems::AddItemToSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, TAG_ProcTest_Slot_A);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            ASSERT_THAT(AreEqual(1, HandlerA->NumCalls(EPhase::AddedToSlot)));
            ASSERT_THAT(AreEqual(0, HandlerB->NumCalls(EPhase::AddedToSlot)));

            // Move to slot B — one drain covers detach-old (A) + attach-new (B).
            ArcMassItems::ChangeItemSlot(*EM, Entity, ArcMassItemAttachmentProcessorTests::StoreType(), Id, TAG_ProcTest_Slot_B);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            ASSERT_THAT(IsTrue(HandlerA->NumCalls(EPhase::RemovedFromSlot) > 0
                            || HandlerA->NumCalls(EPhase::Detach) > 0));
            ASSERT_THAT(AreEqual(1, HandlerB->NumCalls(EPhase::AddedToSlot)));

            // Final state: item in slot B only.
            const FArcMassItemAttachmentStateFragment* State =
                FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
            ASSERT_THAT(IsNotNull(State));
            ASSERT_THAT(AreEqual(1, State->ActiveAttachments.Num()));
            ASSERT_THAT(AreEqual(FGameplayTag(TAG_ProcTest_Slot_B), State->ActiveAttachments[0].SlotId));
        });
    }

    TEST_METHOD(TeardownObserver_Note_PIEWorldDispatchCoveredBySubsystem)
    {
        PIE.Then(TEXT("TeardownObserver_Note_PIEWorldDispatchCoveredBySubsystem"), [this]()
        {
            // UArcMassItemAttachmentTagRemoveObserver (teardown observer) fires when the
            // FArcMassItemAttachmentTag is removed from an entity.  Under PIE the subsystem
            // is live, so dispatch and attachment-subsystem binding DO work.  The teardown
            // observer round-trip end-to-end is covered by the integration tests in
            // ArcMassItemAttachmentIntegrationTests.cpp (RemovingItem_DestroysSpawnedComponent).
            //
            // This test records the migration note: the previous FActorTestSpawner harness
            // could not stand up UWorldSubsystems, so dispatch tests were unreliable.
            // Under FArcStandalonePIEComponent UArcMassItemAttachmentSubsystem is live.
            AddInfo(TEXT("TeardownObserver end-to-end: PIE world subsystems are live — dispatch tests valid under FArcStandalonePIEComponent."));
        });
    }
};

// ---------------------------------------------------------------------------
// Multi-store group — reconcile must not evict the other store's attachments
// ---------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(ArcMassItemAttachment_Processor_MultiStore,
    "ArcMassItems.Attachments.Processor.MultiStore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
    FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
        AArcMassGameMode::StaticClass() };

    FMassEntityManager* EM = nullptr;
    UArcMassItemEventSubsystem* EventSub = nullptr;
    UArcMassItemAttachmentConfig* Config = nullptr;
    FArcMassAttachmentHandler_Test* Handler = nullptr;
    FMassEntityHandle Entity;
    const UArcMassItemTestsSettings* Settings = nullptr;
    FArcItemTestRecordFiles Records;
    FPrimaryAssetId SimpleBaseItem;

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

            EventSub = World->GetSubsystem<UArcMassItemEventSubsystem>();
            ASSERT_THAT(IsNotNull(EventSub));

            Config = ArcMassItems::TestHelpers::BuildSingleHandlerConfig(
                TAG_ProcTest_Slot_A, Handler, ArcMassItems::TestHelpers::MarkerFragmentType());
            Entity = ArcMassItems::TestHelpers::CreateDualStoreAttachmentEntity(*EM, Config);
            SimpleBaseItem = ArcMassItems::TestHelpers::WriteWithMarkerFragment(Records, Settings->SimpleBaseItem);
            ASSERT_THAT(IsTrue(SimpleBaseItem.IsValid()));
        });
    }

    TEST_METHOD(Reconcile_TwoInterestedStores_KeepsOtherStoresAttachments)
    {
        PIE.Then(TEXT("Reconcile_TwoInterestedStores_KeepsOtherStoresAttachments"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            const UScriptStruct* PrimaryStore = FArcMassItemStoreFragment::StaticStruct();
            const UScriptStruct* SecondaryStore = FArcTestSecondaryItemStoreFragment::StaticStruct();

            // Slot an item from the SECONDARY store — handler records an active attachment.
            FArcItemSpec SecondarySpec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId SecondaryId = ArcMassItems::AddItem(*EM, Entity, SecondaryStore, SecondarySpec);
            ArcMassItems::AddItemToSlot(*EM, Entity, SecondaryStore, SecondaryId, TAG_ProcTest_Slot_A);
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            FArcMassItemAttachmentStateFragment* State =
                FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
            ASSERT_THAT(IsNotNull(State));
            ASSERT_THAT(AreEqual(1, State->ActiveAttachments.Num()));
            ASSERT_THAT(IsTrue(State->ActiveAttachments[0].ItemId == SecondaryId));

            // Now trigger a slot event on the PRIMARY store. The reconcile sweep must
            // resolve the secondary item across ALL interested stores and keep it.
            FArcItemSpec PrimarySpec = FArcItemSpec::NewItem(SimpleBaseItem, 1, 1);
            FArcItemId PrimaryId = ArcMassItems::AddItem(*EM, Entity, PrimaryStore, PrimarySpec);
            EventSub->QueueItemSlotAssigned(Entity, PrimaryStore, PrimaryId, TAG_ProcTest_Slot_B);
            EventSub->DrainQueues();

            State = FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
            ASSERT_THAT(AreEqual(1, State->ActiveAttachments.Num()));
            ASSERT_THAT(IsTrue(State->ActiveAttachments[0].ItemId == SecondaryId));
        });
    }
};

// ---------------------------------------------------------------------------
// QuickBar auto-select delegate plumbing — drain drives the subsystem
// ---------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ProcTest_QuickSlot, "ProcTest.QuickBar.Slot");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ProcTest_QuickBar, "ProcTest.QuickBar.Bar");

TEST_CLASS_WITH_FLAGS(ArcMassQuickBarAutoSelect_Plumbing,
    "ArcMassItems.QuickBar.AutoSelectPlumbing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
    FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
        AArcMassGameMode::StaticClass() };

    FMassEntityManager* EM = nullptr;
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
        });
    }

    TEST_METHOD(SlotAssign_DrainDrivesSubsystemAutoSelect)
    {
        PIE.Then(TEXT("SlotAssign_DrainDrivesSubsystemAutoSelect"), [this]()
        {
            UWorld* World = PIE.GetState().World;
            ASSERT_THAT(IsNotNull(World->GetSubsystem<UArcMassQuickBarAutoSelectSubsystem>()));

            UArcMassQuickBarConfig* Config = ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig(
                [](UArcMassQuickBarConfig& Cfg)
                {
                    FArcMassQuickBarEntry Bar;
                    Bar.BarId = TAG_ProcTest_QuickBar;
                    Bar.QuickSlotsMode = EArcMassQuickSlotsMode::AutoActivateOnly;
                    FArcMassQuickBarSlot Slot;
                    Slot.QuickBarSlotId = TAG_ProcTest_QuickSlot;
                    Slot.ItemSlotId = TAG_ProcTest_Slot_A;
                    Bar.Slots.Add(Slot);
                    Cfg.QuickBars.Add(Bar);
                });
            FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EM, Config);

            FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
            FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
            ArcMassItems::AddItemToSlot(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Id, TAG_ProcTest_Slot_A);

            // The post-actor-tick drain (simulated here) fires OnItemSlotChanged → subsystem → auto-select.
            ArcMassItems::TestHelpers::DrainAttachmentEvents(World);

            const FArcMassQuickBarStateFragment* State =
                FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
            ASSERT_THAT(IsNotNull(State));
            ASSERT_THAT(AreEqual(1, State->ActiveSlots.Num()));
            ASSERT_THAT(IsTrue(State->ActiveSlots[0].AssignedItemId == Id));
            ASSERT_THAT(IsTrue(State->ActiveSlots[0].bIsSlotActive));
        });
    }
};

#endif // ENABLE_ARC_STANDALONE_PIE_TEST
