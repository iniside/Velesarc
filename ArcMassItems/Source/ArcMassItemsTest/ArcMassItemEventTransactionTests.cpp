// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityFragments.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Fragments/ArcMassItemEventFragment.h"
#include "Operations/ArcMassItemTransaction.h"
#include "Operations/ArcMassItemOperations.h"
#include "ArcMassItemTestsSettings.h"
#include "Items/ArcItemSpec.h"
#include "StructUtils/InstancedStruct.h"

TEST_CLASS(ArcMassItemEventTransaction, "ArcMassItems.Events")
{
    FActorTestSpawner Spawner;
    FMassEntityManager* EntityManager = nullptr;
    FMassEntityHandle EntityA;
    FMassEntityHandle EntityB;
    const UArcMassItemTestsSettings* Settings = nullptr;

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
        Settings = GetDefault<UArcMassItemTestsSettings>();
    }

    TEST_METHOD(AppendNotOverwrite_TwoEmitsBothVisible)
    {
        const UScriptStruct* StoreType = FArcMassItemStoreFragment::StaticStruct();
        {
            ArcMassItems::FArcMassItemTransaction Tx(*EntityManager, EntityA, StoreType);
            FArcMassItemEvent E1; E1.Type = EArcMassItemEventType::Added;        Tx.Emit(E1);
            FArcMassItemEvent E2; E2.Type = EArcMassItemEventType::SlotChanged;  Tx.Emit(E2);
        } // commit on scope exit

        const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, EntityA).GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
        ASSERT_THAT(IsNotNull(Events));
        ASSERT_THAT(AreEqual(2, Events->Events.Num()));
        ASSERT_THAT(AreEqual(EArcMassItemEventType::Added,       Events->Events[0].Type));
        ASSERT_THAT(AreEqual(EArcMassItemEventType::SlotChanged, Events->Events[1].Type));
    }

    TEST_METHOD(PrunePriorFrame_OldEventsRemovedOnNextCommit)
    {
        const UScriptStruct* StoreType = FArcMassItemStoreFragment::StaticStruct();

        // Frame N: emit one event
        const uint64 FrameAtFirstEmit = GFrameCounter;
        {
            ArcMassItems::FArcMassItemTransaction Tx(*EntityManager, EntityA, StoreType);
            FArcMassItemEvent E; E.Type = EArcMassItemEventType::Added; Tx.Emit(E);
        }

        // Force frame advance for the prune to take effect.
        GFrameCounter = FrameAtFirstEmit + 1;

        // Frame N+1: emit another event; commit must prune the first.
        {
            ArcMassItems::FArcMassItemTransaction Tx(*EntityManager, EntityA, StoreType);
            FArcMassItemEvent E; E.Type = EArcMassItemEventType::Removed; Tx.Emit(E);
        }

        const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, EntityA).GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
        ASSERT_THAT(IsNotNull(Events));
        ASSERT_THAT(AreEqual(1, Events->Events.Num()));
        ASSERT_THAT(AreEqual(EArcMassItemEventType::Removed, Events->Events[0].Type));
    }

    TEST_METHOD(NestedTransactionJoin_InnerEventsLandInOuterCommit)
    {
        const UScriptStruct* StoreType = FArcMassItemStoreFragment::StaticStruct();

        {
            ArcMassItems::FArcMassItemTransaction Outer(*EntityManager, EntityA, StoreType);
            FArcMassItemEvent E1; E1.Type = EArcMassItemEventType::Added; Outer.Emit(E1);

            // Same (Entity, StoreType) — inner joins outer.
            {
                ArcMassItems::FArcMassItemTransaction Inner(*EntityManager, EntityA, StoreType);
                FArcMassItemEvent E2; E2.Type = EArcMassItemEventType::SlotChanged; Inner.Emit(E2);
                // Inner destructor here is a no-op.
            }

            // Fragment should still be empty before outer commits.
            const FArcMassItemEventFragment* MidFlight = FMassEntityView(*EntityManager, EntityA).GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
            ASSERT_THAT(IsTrue(MidFlight == nullptr || MidFlight->Events.Num() == 0));
        }

        // Outer commit lands both events.
        const FArcMassItemEventFragment* Events = FMassEntityView(*EntityManager, EntityA).GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
        ASSERT_THAT(IsNotNull(Events));
        ASSERT_THAT(AreEqual(2, Events->Events.Num()));
    }

    TEST_METHOD(CrossEntityIndependent_EachEntityCommitsSeparately)
    {
        const UScriptStruct* StoreType = FArcMassItemStoreFragment::StaticStruct();
        {
            ArcMassItems::FArcMassItemTransaction TxA(*EntityManager, EntityA, StoreType);
            ArcMassItems::FArcMassItemTransaction TxB(*EntityManager, EntityB, StoreType);

            FArcMassItemEvent EA; EA.Type = EArcMassItemEventType::Added;   TxA.Emit(EA);
            FArcMassItemEvent EB; EB.Type = EArcMassItemEventType::Removed; TxB.Emit(EB);
        }

        const FArcMassItemEventFragment* EventsA = FMassEntityView(*EntityManager, EntityA).GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
        const FArcMassItemEventFragment* EventsB = FMassEntityView(*EntityManager, EntityB).GetSparseFragmentDataPtr<FArcMassItemEventFragment>();

        ASSERT_THAT(IsNotNull(EventsA));
        ASSERT_THAT(IsNotNull(EventsB));
        ASSERT_THAT(AreEqual(1, EventsA->Events.Num()));
        ASSERT_THAT(AreEqual(1, EventsB->Events.Num()));
        ASSERT_THAT(AreEqual(EArcMassItemEventType::Added,   EventsA->Events[0].Type));
        ASSERT_THAT(AreEqual(EArcMassItemEventType::Removed, EventsB->Events[0].Type));
    }
};
