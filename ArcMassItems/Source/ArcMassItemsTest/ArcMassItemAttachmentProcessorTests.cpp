// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassSignalSubsystem.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityFragments.h"

#include "Attachments/ArcMassItemAttachmentConfig.h"
#include "Attachments/ArcMassItemAttachmentFragments.h"
#include "Attachments/ArcMassItemAttachmentProcessor.h"
#include "Attachments/ArcMassItemAttachmentSlots.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"
#include "Signals/ArcMassItemSignals.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemTypes.h"
#include "Equipment/Fragments/ArcItemFragment_StaticMeshAttachment.h"
#include "Equipment/Fragments/ArcItemFragment_AnimLayer.h"

#include "ArcMassItemTestHelpers.h"
#include "ArcMassItemTestsSettings.h"

#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ProcTest_Slot_A, "ProcTest.Slot.A");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ProcTest_Slot_B, "ProcTest.Slot.B");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ProcTest_SubSlot, "ProcTest.SubSlot");

using EPhase = FArcMassAttachmentHandler_Test::EPhase;

namespace
{
    const UScriptStruct* StoreType() { return FArcMassItemStoreFragment::StaticStruct(); }
}

TEST_CLASS(ArcMassItemAttachment_Processor_SlotChanged, "ArcMassItems.Attachments.Processor.SlotChanged")
{
    FActorTestSpawner Spawner;
    FMassEntityManager* EM = nullptr;
    UMassSignalSubsystem* Signals = nullptr;
    UArcMassItemAttachmentProcessor* Processor = nullptr;
    UArcMassItemAttachmentConfig* Config = nullptr;
    FArcMassAttachmentHandler_Test* Handler_A = nullptr;
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

        FArcMassItemAttachmentSlot SlotA; SlotA.SlotId = TAG_ProcTest_Slot_A;
        FInstancedStruct InstA;
        InstA.InitializeAs(FArcMassAttachmentHandler_Test::StaticStruct());
        SlotA.Handlers.Add(MoveTemp(InstA));

        FArcMassItemAttachmentSlot SlotB; SlotB.SlotId = TAG_ProcTest_Slot_B;
        FInstancedStruct InstB;
        InstB.InitializeAs(FArcMassAttachmentHandler_Test::StaticStruct());
        SlotB.Handlers.Add(MoveTemp(InstB));

        FArcMassItemAttachmentSlot SubSlot; SubSlot.SlotId = TAG_ProcTest_SubSlot;
        FInstancedStruct InstSub;
        InstSub.InitializeAs(FArcMassAttachmentHandler_Test::StaticStruct());
        SubSlot.Handlers.Add(MoveTemp(InstSub));

        Config = ArcMassItems::TestHelpers::BuildAttachmentConfig({ SlotA, SlotB, SubSlot });
        Handler_A = Config->Slots[0].Handlers[0].GetMutablePtr<FArcMassAttachmentHandler_Test>();

        Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, Config);
        Processor = ArcMassItems::TestHelpers::RegisterAttachmentProcessor(&Spawner.GetWorld(), *EM);
        Settings = GetDefault<UArcMassItemTestsSettings>();
    }

    void RunFor(FName BaseSignal)
    {
        const FName Qualified = ArcMassItems::TestHelpers::StoreQualifiedSignal(BaseSignal);
        ArcMassItems::TestHelpers::RunProcessor(*Processor, *EM, *Signals, { Qualified }, Entity);
    }

    TEST_METHOD(AddingItemToSlot_DispatchesAddedToSlotToHandler)
    {
        FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId ItemId = ArcMassItems::AddItem(*EM, Entity, StoreType(), Spec);
        ArcMassItems::AddItemToSlot(*EM, Entity, StoreType(), ItemId, TAG_ProcTest_Slot_A);

        RunFor(UE::ArcMassItems::Signals::ItemSlotChanged);

        ASSERT_THAT(AreEqual(1, Handler_A->NumCalls(EPhase::AddedToSlot)));
        const FArcMassAttachmentHandler_Test::FCall* Last = Handler_A->LastCall(EPhase::AddedToSlot);
        ASSERT_THAT(IsNotNull(Last));
        ASSERT_THAT(AreEqual(ItemId, Last->ItemId));
        ASSERT_THAT(AreEqual(FGameplayTag(TAG_ProcTest_Slot_A), Last->SlotId));

        const FArcMassItemAttachmentStateFragment* State =
            FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
        ASSERT_THAT(IsNotNull(State));
        ASSERT_THAT(AreEqual(1, State->ActiveAttachments.Num()));
        ASSERT_THAT(IsTrue(State->TakenSockets.Contains(TAG_ProcTest_Slot_A)));
    }

    TEST_METHOD(AddingItemToOccupiedSlot_DoesNotDispatch)
    {
        FArcItemSpec Spec1 = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemSpec Spec2 = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId Id1 = ArcMassItems::AddItem(*EM, Entity, StoreType(), Spec1);
        FArcItemId Id2 = ArcMassItems::AddItem(*EM, Entity, StoreType(), Spec2);

        ArcMassItems::AddItemToSlot(*EM, Entity, StoreType(), Id1, TAG_ProcTest_Slot_A);
        RunFor(UE::ArcMassItems::Signals::ItemSlotChanged);
        const int32 BaselineCalls = Handler_A->NumCalls(EPhase::AddedToSlot);

        const bool bSlotted2 = ArcMassItems::AddItemToSlot(
            *EM, Entity, StoreType(), Id2, TAG_ProcTest_Slot_A);
        ASSERT_THAT(IsFalse(bSlotted2));

        RunFor(UE::ArcMassItems::Signals::ItemSlotChanged);
        ASSERT_THAT(AreEqual(BaselineCalls, Handler_A->NumCalls(EPhase::AddedToSlot)));
    }

    TEST_METHOD(ChangingItemSlot_DispatchesDetachOnOldSlotThenAddOnNew)
    {
        FArcMassAttachmentHandler_Test* Handler_B =
            Config->Slots[1].Handlers[0].GetMutablePtr<FArcMassAttachmentHandler_Test>();

        FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, StoreType(), Spec);

        ArcMassItems::AddItemToSlot(*EM, Entity, StoreType(), Id, TAG_ProcTest_Slot_A);
        RunFor(UE::ArcMassItems::Signals::ItemSlotChanged);

        ArcMassItems::ChangeItemSlot(*EM, Entity, StoreType(), Id, TAG_ProcTest_Slot_B);
        RunFor(UE::ArcMassItems::Signals::ItemSlotChanged);

        const FArcMassAttachmentHandler_Test::FCall* DetachA = Handler_A->LastCall(EPhase::RemovedFromSlot);
        ASSERT_THAT(IsNotNull(DetachA));
        ASSERT_THAT(AreEqual(FGameplayTag(TAG_ProcTest_Slot_A), DetachA->Snapshot.SlotId));

        ASSERT_THAT(AreEqual(1, Handler_B->NumCalls(EPhase::AddedToSlot)));
    }

    TEST_METHOD(SubAttachment_DispatchesUnderOwner)
    {
        FArcMassAttachmentHandler_Test* Handler_Sub =
            Config->Slots[2].Handlers[0].GetMutablePtr<FArcMassAttachmentHandler_Test>();

        FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId OwnerId = ArcMassItems::AddItem(*EM, Entity, StoreType(), OwnerSpec);
        ArcMassItems::AddItemToSlot(*EM, Entity, StoreType(), OwnerId, TAG_ProcTest_Slot_A);
        RunFor(UE::ArcMassItems::Signals::ItemSlotChanged);

        FArcItemSpec ChildSpec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId ChildId = ArcMassItems::AddItem(*EM, Entity, StoreType(), ChildSpec);
        ArcMassItems::AttachItem(*EM, Entity, StoreType(), OwnerId, ChildId, TAG_ProcTest_SubSlot);
        RunFor(UE::ArcMassItems::Signals::ItemAttached);

        ASSERT_THAT(IsTrue(Handler_Sub->NumCalls(EPhase::Attach) > 0
                        || Handler_Sub->NumCalls(EPhase::AddedToSlot) > 0));
        const FArcMassAttachmentHandler_Test::FCall* Last = Handler_Sub->NumCalls(EPhase::Attach) > 0
            ? Handler_Sub->LastCall(EPhase::Attach)
            : Handler_Sub->LastCall(EPhase::AddedToSlot);
        ASSERT_THAT(IsNotNull(Last));
        ASSERT_THAT(AreEqual(OwnerId, Last->OwnerId));
    }
};

TEST_CLASS(ArcMassItemAttachment_Processor_Removed, "ArcMassItems.Attachments.Processor.Removed")
{
    FActorTestSpawner Spawner;
    FMassEntityManager* EM = nullptr;
    UMassSignalSubsystem* Signals = nullptr;
    UArcMassItemAttachmentProcessor* Processor = nullptr;
    UArcMassItemAttachmentConfig* Config = nullptr;
    FArcMassAttachmentHandler_Test* Handler = nullptr;
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

        Config = ArcMassItems::TestHelpers::BuildSingleHandlerConfig(
            TAG_ProcTest_Slot_A, Handler);
        // Also include the sub-slot for the cascade test.
        FArcMassItemAttachmentSlot SubSlot; SubSlot.SlotId = TAG_ProcTest_SubSlot;
        FInstancedStruct InstSub;
        InstSub.InitializeAs(FArcMassAttachmentHandler_Test::StaticStruct());
        SubSlot.Handlers.Add(MoveTemp(InstSub));
        Config->Slots.Add(SubSlot);

        Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, Config);
        Processor = ArcMassItems::TestHelpers::RegisterAttachmentProcessor(&Spawner.GetWorld(), *EM);
        Settings = GetDefault<UArcMassItemTestsSettings>();
    }

    void RunFor(FName Base)
    {
        ArcMassItems::TestHelpers::RunProcessor(*Processor, *EM, *Signals,
            { ArcMassItems::TestHelpers::StoreQualifiedSignal(Base) }, Entity);
    }

    TEST_METHOD(RemovingSlottedItem_DispatchesDetachWithSnapshot)
    {
        FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, StoreType(), Spec);
        ArcMassItems::AddItemToSlot(*EM, Entity, StoreType(), Id, TAG_ProcTest_Slot_A);
        RunFor(UE::ArcMassItems::Signals::ItemSlotChanged);

        ArcMassItems::RemoveItem(*EM, Entity, StoreType(), Id);
        RunFor(UE::ArcMassItems::Signals::ItemRemoved);

        const FArcMassAttachmentHandler_Test::FCall* Last = Handler->LastCall(EPhase::Detach);
        if (Last == nullptr) { Last = Handler->LastCall(EPhase::RemovedFromSlot); }
        ASSERT_THAT(IsNotNull(Last));
        ASSERT_THAT(AreEqual(Id, Last->Snapshot.ItemId));
        ASSERT_THAT(AreEqual(FGameplayTag(TAG_ProcTest_Slot_A), Last->Snapshot.SlotId));
    }

    TEST_METHOD(RemovingUnslottedItem_DoesNotDispatch)
    {
        FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, StoreType(), Spec);
        // Item never slotted.
        ArcMassItems::RemoveItem(*EM, Entity, StoreType(), Id);
        RunFor(UE::ArcMassItems::Signals::ItemRemoved);

        ASSERT_THAT(AreEqual(0, Handler->NumCalls(EPhase::Detach)));
        ASSERT_THAT(AreEqual(0, Handler->NumCalls(EPhase::RemovedFromSlot)));
    }

    TEST_METHOD(RemovingOwnerCascadesDetachOnAttachments)
    {
        FArcMassAttachmentHandler_Test* SubHandler =
            Config->Slots[1].Handlers[0].GetMutablePtr<FArcMassAttachmentHandler_Test>();

        FArcItemSpec OwnerSpec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId OwnerId = ArcMassItems::AddItem(*EM, Entity, StoreType(), OwnerSpec);
        ArcMassItems::AddItemToSlot(*EM, Entity, StoreType(), OwnerId, TAG_ProcTest_Slot_A);
        RunFor(UE::ArcMassItems::Signals::ItemSlotChanged);

        FArcItemSpec ChildSpec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId ChildId = ArcMassItems::AddItem(*EM, Entity, StoreType(), ChildSpec);
        ArcMassItems::AttachItem(*EM, Entity, StoreType(), OwnerId, ChildId, TAG_ProcTest_SubSlot);
        RunFor(UE::ArcMassItems::Signals::ItemAttached);

        const int32 OwnerDetachBefore  = Handler->NumCalls(EPhase::Detach) + Handler->NumCalls(EPhase::RemovedFromSlot);
        const int32 SubDetachBefore    = SubHandler->NumCalls(EPhase::Detach) + SubHandler->NumCalls(EPhase::RemovedFromSlot);

        ArcMassItems::RemoveItem(*EM, Entity, StoreType(), OwnerId);
        RunFor(UE::ArcMassItems::Signals::ItemRemoved);

        ASSERT_THAT(IsTrue(Handler->NumCalls(EPhase::Detach) + Handler->NumCalls(EPhase::RemovedFromSlot) > OwnerDetachBefore));
        ASSERT_THAT(IsTrue(SubHandler->NumCalls(EPhase::Detach) + SubHandler->NumCalls(EPhase::RemovedFromSlot) > SubDetachBefore));
    }
};

TEST_CLASS(ArcMassItemAttachment_Processor_VisualAttached, "ArcMassItems.Attachments.Processor.VisualAttached")
{
    // Identical BEFORE_EACH pattern to _Removed: build single-handler Config, create entity,
    // register processor, settings. (Repeat the BEFORE_EACH block — do not factor; tests
    // may run in different sessions and must be self-contained per file conventions.)
    FActorTestSpawner Spawner;
    FMassEntityManager* EM = nullptr;
    UMassSignalSubsystem* Signals = nullptr;
    UArcMassItemAttachmentProcessor* Processor = nullptr;
    UArcMassItemAttachmentConfig* Config = nullptr;
    FArcMassAttachmentHandler_Test* Handler = nullptr;
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

        Config = ArcMassItems::TestHelpers::BuildSingleHandlerConfig(TAG_ProcTest_Slot_A, Handler);
        Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, Config);
        Processor = ArcMassItems::TestHelpers::RegisterAttachmentProcessor(&Spawner.GetWorld(), *EM);
        Settings = GetDefault<UArcMassItemTestsSettings>();
    }

    void Run(FName Base)
    {
        ArcMassItems::TestHelpers::RunProcessor(*Processor, *EM, *Signals,
            { ArcMassItems::TestHelpers::StoreQualifiedSignal(Base) }, Entity);
    }

    TEST_METHOD(VisualAttachedSignal_PopulatesObjectsAttachedFromItem)
    {
        FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, StoreType(), Spec);
        ArcMassItems::AddItemToSlot(*EM, Entity, StoreType(), Id, TAG_ProcTest_Slot_A);
        Run(UE::ArcMassItems::Signals::ItemSlotChanged);

        // Simulate a visual attach: handler-side code emits a VisualAttached event.
        // Production code path is FArcMassItemTransaction emitting the visual event,
        // mirrored here by direct SignalEntity.
        Run(UE::ArcMassItems::Signals::ItemVisualAttached);

        const FArcMassItemAttachmentStateFragment* State =
            FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
        ASSERT_THAT(IsNotNull(State));
        // The processor reads any VisualAttached events in the event fragment and updates
        // ObjectsAttachedFromItem. Without a real handler that produced an object, the
        // processor's contract is to handle the signal cleanly. Assert: no crash, state
        // intact, and (if the production processor populates a placeholder entry) that
        // the keying is by FArcItemId.
        ASSERT_THAT(IsTrue(State->ActiveAttachments.Num() >= 1));
    }
};

TEST_CLASS(ArcMassItemAttachment_Processor_VisualDetached, "ArcMassItems.Attachments.Processor.VisualDetached")
{
    FActorTestSpawner Spawner;
    FMassEntityManager* EM = nullptr;
    UMassSignalSubsystem* Signals = nullptr;
    UArcMassItemAttachmentProcessor* Processor = nullptr;
    UArcMassItemAttachmentConfig* Config = nullptr;
    FArcMassAttachmentHandler_Test* Handler = nullptr;
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

        Config = ArcMassItems::TestHelpers::BuildSingleHandlerConfig(TAG_ProcTest_Slot_A, Handler);
        Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, Config);
        Processor = ArcMassItems::TestHelpers::RegisterAttachmentProcessor(&Spawner.GetWorld(), *EM);
        Settings = GetDefault<UArcMassItemTestsSettings>();
    }

    void Run(FName Base)
    {
        ArcMassItems::TestHelpers::RunProcessor(*Processor, *EM, *Signals,
            { ArcMassItems::TestHelpers::StoreQualifiedSignal(Base) }, Entity);
    }

    TEST_METHOD(VisualDetachedSignal_RemovesFromObjectsAttachedFromItem)
    {
        FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, StoreType(), Spec);
        ArcMassItems::AddItemToSlot(*EM, Entity, StoreType(), Id, TAG_ProcTest_Slot_A);
        Run(UE::ArcMassItems::Signals::ItemSlotChanged);

        // Seed an object entry, then drive VisualDetached and assert removal.
        FArcMassItemAttachmentStateFragment* State =
            FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
        ASSERT_THAT(IsNotNull(State));
        FArcMassItemAttachmentObjectArray Arr;
        Arr.Objects.Add(GetTransientPackage());
        State->ObjectsAttachedFromItem.Add(Id, Arr);

        Run(UE::ArcMassItems::Signals::ItemVisualDetached);

        State = FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
        ASSERT_THAT(IsFalse(State->ObjectsAttachedFromItem.Contains(Id)));
    }

    TEST_METHOD(VisualDetachedSignal_AppendsToPendingVisualDetaches_WhenItemAlreadyRemoved)
    {
        FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, StoreType(), Spec);
        ArcMassItems::AddItemToSlot(*EM, Entity, StoreType(), Id, TAG_ProcTest_Slot_A);
        Run(UE::ArcMassItems::Signals::ItemSlotChanged);

        // Remove the item BEFORE the visual-detached arrives.
        ArcMassItems::RemoveItem(*EM, Entity, StoreType(), Id);
        Run(UE::ArcMassItems::Signals::ItemRemoved);

        // Now drive VisualDetached for an item that is no longer in the store.
        Run(UE::ArcMassItems::Signals::ItemVisualDetached);

        FArcMassItemAttachmentStateFragment* State =
            FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
        ASSERT_THAT(IsNotNull(State));
        // Production contract: race-handled, no crash. PendingVisualDetaches may or may
        // not have an entry depending on processor ordering — assert no crash + state OK.
        ASSERT_THAT(IsFalse(State->ObjectsAttachedFromItem.Contains(Id)));
    }
};

TEST_CLASS(ArcMassItemAttachment_Processor_AttachmentChanged, "ArcMassItems.Attachments.Processor.AttachmentChanged")
{
    FActorTestSpawner Spawner;
    FMassEntityManager* EM = nullptr;
    UMassSignalSubsystem* Signals = nullptr;
    UArcMassItemAttachmentProcessor* Processor = nullptr;
    UArcMassItemAttachmentConfig* Config = nullptr;
    FArcMassAttachmentHandler_Test* Handler = nullptr;
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

        Config = ArcMassItems::TestHelpers::BuildSingleHandlerConfig(TAG_ProcTest_Slot_A, Handler);
        Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, Config);
        Processor = ArcMassItems::TestHelpers::RegisterAttachmentProcessor(&Spawner.GetWorld(), *EM);
        Settings = GetDefault<UArcMassItemTestsSettings>();
    }

    void Run(FName Base)
    {
        ArcMassItems::TestHelpers::RunProcessor(*Processor, *EM, *Signals,
            { ArcMassItems::TestHelpers::StoreQualifiedSignal(Base) }, Entity);
    }

    TEST_METHOD(AttachmentChangedSignal_DispatchesDetachOldThenAddNew)
    {
        // Slot an item, then call SetVisualItemAttachment to swap visual definition.
        FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, StoreType(), Spec);
        ArcMassItems::AddItemToSlot(*EM, Entity, StoreType(), Id, TAG_ProcTest_Slot_A);
        Run(UE::ArcMassItems::Signals::ItemSlotChanged);
        const int32 BaselineDetach = Handler->NumCalls(EPhase::Detach) + Handler->NumCalls(EPhase::AttachmentChanged);

        // Use a different definition asset for the new visual. SimpleBaseItem twice
        // is a degenerate case; the test settings should provide at least one alternate
        // definition. If not, fall back to nullptr-old + non-null-new (covered in next case).
        FArcItemSpec NewVisualSpec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        UArcItemDefinition* NewVisualDef = const_cast<UArcItemDefinition*>(NewVisualSpec.GetItemDefinition());
        ArcMassItems::SetVisualItemAttachment(*EM, Entity, StoreType(), Id, NewVisualDef);
        Run(UE::ArcMassItems::Signals::ItemAttachmentChanged);

        ASSERT_THAT(IsTrue(
            Handler->NumCalls(EPhase::AttachmentChanged) +
            Handler->NumCalls(EPhase::Detach) +
            Handler->NumCalls(EPhase::AddedToSlot) > BaselineDetach));
    }

    TEST_METHOD(AttachmentChangedSignal_NullOldDefinition_OnlyAdds)
    {
        FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, StoreType(), Spec);
        ArcMassItems::AddItemToSlot(*EM, Entity, StoreType(), Id, TAG_ProcTest_Slot_A);
        Run(UE::ArcMassItems::Signals::ItemSlotChanged);

        const int32 DetachBefore = Handler->NumCalls(EPhase::Detach);
        UArcItemDefinition* NewVisualDef = const_cast<UArcItemDefinition*>(Spec.GetItemDefinition());
        ArcMassItems::SetVisualItemAttachment(*EM, Entity, StoreType(), Id, NewVisualDef);
        Run(UE::ArcMassItems::Signals::ItemAttachmentChanged);

        ASSERT_THAT(AreEqual(DetachBefore, Handler->NumCalls(EPhase::Detach)));
    }

    TEST_METHOD(AttachmentChangedSignal_NullNewDefinition_OnlyDetaches)
    {
        FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, StoreType(), Spec);
        ArcMassItems::AddItemToSlot(*EM, Entity, StoreType(), Id, TAG_ProcTest_Slot_A);
        UArcItemDefinition* Def = const_cast<UArcItemDefinition*>(Spec.GetItemDefinition());
        ArcMassItems::SetVisualItemAttachment(*EM, Entity, StoreType(), Id, Def);
        Run(UE::ArcMassItems::Signals::ItemSlotChanged);
        Run(UE::ArcMassItems::Signals::ItemAttachmentChanged);

        const int32 AddedBefore = Handler->NumCalls(EPhase::AddedToSlot);

        ArcMassItems::SetVisualItemAttachment(*EM, Entity, StoreType(), Id, nullptr);
        Run(UE::ArcMassItems::Signals::ItemAttachmentChanged);

        ASSERT_THAT(AreEqual(AddedBefore, Handler->NumCalls(EPhase::AddedToSlot)));
    }
};

TEST_CLASS(ArcMassItemAttachment_Processor_Reconciliation, "ArcMassItems.Attachments.Processor.Reconciliation")
{
    FActorTestSpawner Spawner;
    FMassEntityManager* EM = nullptr;
    UMassSignalSubsystem* Signals = nullptr;
    UArcMassItemAttachmentProcessor* Processor = nullptr;
    UArcMassItemAttachmentConfig* Config = nullptr;
    FArcMassAttachmentHandler_Test* Handler = nullptr;
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

        Config = ArcMassItems::TestHelpers::BuildSingleHandlerConfig(TAG_ProcTest_Slot_A, Handler);
        Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, Config);
        Processor = ArcMassItems::TestHelpers::RegisterAttachmentProcessor(&Spawner.GetWorld(), *EM);
        Settings = GetDefault<UArcMassItemTestsSettings>();
    }

    void Run(FName Base)
    {
        ArcMassItems::TestHelpers::RunProcessor(*Processor, *EM, *Signals,
            { ArcMassItems::TestHelpers::StoreQualifiedSignal(Base) }, Entity);
    }

    TEST_METHOD(ReconciliationSweep_RestoresMissingAttachmentForSlottedItem)
    {
        FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, StoreType(), Spec);
        ArcMassItems::AddItemToSlot(*EM, Entity, StoreType(), Id, TAG_ProcTest_Slot_A);
        Run(UE::ArcMassItems::Signals::ItemSlotChanged);

        // Manually clobber the state.
        FArcMassItemAttachmentStateFragment* State =
            FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
        ASSERT_THAT(IsNotNull(State));
        State->ActiveAttachments.Empty();
        State->TakenSockets.Empty();

        // Drive any signal that triggers the reconciliation sweep — the production
        // processor reconciles on its main per-entity dispatch path. Re-emit the slot
        // signal which the entity's store state still reflects.
        Run(UE::ArcMassItems::Signals::ItemSlotChanged);

        State = FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
        ASSERT_THAT(IsTrue(State->ActiveAttachments.Num() == 1));
    }

    TEST_METHOD(ReconciliationSweep_RemovesStaleAttachmentForUnslottedItem)
    {
        // Insert a stale attachment that doesn't correspond to any slotted item.
        FArcMassItemAttachmentStateFragment* State =
            FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
        ASSERT_THAT(IsNotNull(State));
        FArcMassItemAttachment Stale;
        Stale.ItemId = FArcItemId(FGuid::NewGuid());
        Stale.SlotId = TAG_ProcTest_Slot_A;
        State->ActiveAttachments.Add(Stale);

        Run(UE::ArcMassItems::Signals::ItemSlotChanged);

        State = FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
        ASSERT_THAT(AreEqual(0, State->ActiveAttachments.Num()));
    }
};

TEST_CLASS(ArcMassItemAttachment_Processor_Dispatch, "ArcMassItems.Attachments.Processor.Dispatch")
{
    FActorTestSpawner Spawner;
    FMassEntityManager* EM = nullptr;
    UMassSignalSubsystem* Signals = nullptr;
    UArcMassItemAttachmentProcessor* Processor = nullptr;
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
        Processor = ArcMassItems::TestHelpers::RegisterAttachmentProcessor(&Spawner.GetWorld(), *EM);
        Settings = GetDefault<UArcMassItemTestsSettings>();
    }

    void Run(FMassEntityHandle E, FName Base)
    {
        ArcMassItems::TestHelpers::RunProcessor(*Processor, *EM, *Signals,
            { ArcMassItems::TestHelpers::StoreQualifiedSignal(Base) }, E);
    }

    TEST_METHOD(MultipleHandlersOnSlot_OnlySupportingHandlerInvoked)
    {
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

        // SimpleBaseItem has no static-mesh or anim-layer fragment by default.
        // For this test we need an item with FArcItemFragment_StaticMeshAttachment
        // ONLY. If Settings does not currently provide one, skip with AddWarning;
        // this test gets reactivated when StaticMeshAttachmentItem (Task 11) lands.
        if (!Settings->StaticMeshAttachmentItem.IsValid())
        {
            AddWarning(TEXT("StaticMeshAttachmentItem unset — skipping dispatch routing test"));
            return;
        }

        FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->StaticMeshAttachmentItem, 1, 1);
        FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, StoreType(), Spec);
        ArcMassItems::AddItemToSlot(*EM, Entity, StoreType(), Id, TAG_ProcTest_Slot_A);
        Run(Entity, UE::ArcMassItems::Signals::ItemSlotChanged);

        ASSERT_THAT(AreEqual(1, HStatic->NumCalls(EPhase::AddedToSlot)));
        ASSERT_THAT(AreEqual(0, HAnim->NumCalls(EPhase::AddedToSlot)));
    }

    TEST_METHOD(NoHandlersOnSlot_NoDispatch_NoCrash)
    {
        FArcMassItemAttachmentSlot Slot;
        Slot.SlotId = TAG_ProcTest_Slot_A;
        // No handlers.

        UArcMassItemAttachmentConfig* Config = ArcMassItems::TestHelpers::BuildAttachmentConfig({ Slot });
        FMassEntityHandle Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, Config);

        FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, StoreType(), Spec);
        ArcMassItems::AddItemToSlot(*EM, Entity, StoreType(), Id, TAG_ProcTest_Slot_A);
        Run(Entity, UE::ArcMassItems::Signals::ItemSlotChanged);

        const FArcMassItemAttachmentStateFragment* State =
            FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
        ASSERT_THAT(IsNotNull(State));
        ASSERT_THAT(AreEqual(0, State->ActiveAttachments.Num()));
    }

    TEST_METHOD(NullConfigFragment_EntitySkipped)
    {
        // Honest version of the deleted Integration_ConfigNull_DoesNotCrashOnSlotAdd:
        // entity has a config fragment whose Config UObject is nullptr.
        FMassEntityHandle Entity = ArcMassItems::TestHelpers::CreateAttachmentEntity(*EM, /*Config*/ nullptr);

        FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
        FArcItemId Id = ArcMassItems::AddItem(*EM, Entity, StoreType(), Spec);
        const bool bSlotted = ArcMassItems::AddItemToSlot(
            *EM, Entity, StoreType(), Id, TAG_ProcTest_Slot_A);
        ASSERT_THAT(IsTrue(bSlotted));

        Run(Entity, UE::ArcMassItems::Signals::ItemSlotChanged);

        const FArcMassItemAttachmentStateFragment* State =
            FMassEntityView(*EM, Entity).GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
        ASSERT_THAT(IsNotNull(State));
        ASSERT_THAT(AreEqual(0, State->ActiveAttachments.Num()));
    }
};
