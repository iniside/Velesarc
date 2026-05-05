// Copyright Lukasz Baran. All Rights Reserved.

#include "Operations/ArcMassItemTransaction.h"
#include "Operations/ArcMassItemOperations.h"
#include "Fragments/ArcMassItemEventFragment.h"

#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "MassSignalSubsystem.h"
#include "Engine/World.h"
#include "UObject/Class.h"

namespace ArcMassItems::Private
{
    struct FActiveTxKey
    {
        FMassEntityHandle Entity;
        const UScriptStruct* StoreType = nullptr;

        bool operator==(const FActiveTxKey& Other) const
        {
            return Entity == Other.Entity && StoreType == Other.StoreType;
        }
    };

    FORCEINLINE uint32 GetTypeHash(const FActiveTxKey& Key)
    {
        const uint64 StoreTypePtr = reinterpret_cast<uint64>(Key.StoreType);
        return HashCombine(GetTypeHash(Key.Entity), ::GetTypeHash(StoreTypePtr));
    }

    static thread_local TMap<FActiveTxKey, FArcMassItemTransaction*> GActiveTransactions;

    static UMassSignalSubsystem* GetSignalSubsystem(FMassEntityManager& EntityManager)
    {
        UWorld* World = EntityManager.GetWorld();
        return World ? World->GetSubsystem<UMassSignalSubsystem>() : nullptr;
    }
}

namespace ArcMassItems
{
    FArcMassItemTransaction::FArcMassItemTransaction(
        FMassEntityManager& InEntityManager,
        FMassEntityHandle InEntity,
        const UScriptStruct* InStoreType)
        : EntityManager(InEntityManager)
        , Entity(InEntity)
        , StoreType(InStoreType)
    {
        check(InStoreType);
        check(IsInGameThread());

        const Private::FActiveTxKey Key{ Entity, StoreType };
        if (FArcMassItemTransaction** Existing = Private::GActiveTransactions.Find(Key))
        {
            OuterPtr = *Existing;
            bIsOutermost = false;
        }
        else
        {
            bIsOutermost = true;
            Private::GActiveTransactions.Add(Key, this);
        }
    }

    FArcMassItemTransaction::~FArcMassItemTransaction()
    {
        if (!bIsOutermost)
        {
            return;
        }

        Private::GActiveTransactions.Remove(Private::FActiveTxKey{ Entity, StoreType });

        // 1. Acquire fragment, creating if needed.
        FMassEntityView View(EntityManager, Entity);
        FArcMassItemEventFragment* Frag = View.GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
        if (!Frag)
        {
            Frag = &EntityManager.AddSparseElementToEntity<FArcMassItemEventFragment>(Entity);
        }

        // 2. Prune entries from prior frames.
        const uint64 ThisFrame = GFrameCounter;
        Frag->Events.RemoveAll([ThisFrame](const FArcMassItemEvent& E)
        {
            return E.FrameNumber != ThisFrame;
        });

        if (Pending.Num() == 0)
        {
            return;
        }

        // 3. Collect unique kinds BEFORE moving — Pending entries are moved-from
        //    below and must not be read for Type after that.
        TSet<EArcMassItemEventType> UniqueKinds;
        UniqueKinds.Reserve(Pending.Num());
        for (const FArcMassItemEvent& Event : Pending)
        {
            UniqueKinds.Add(Event.Type);
        }

        // 4. Stamp + append.
        Frag->Events.Reserve(Frag->Events.Num() + Pending.Num());
        for (FArcMassItemEvent& Event : Pending)
        {
            Event.FrameNumber = ThisFrame;
            Event.StoreType = const_cast<UScriptStruct*>(StoreType);
            Frag->Events.Add(MoveTemp(Event));
        }

        // 5. Fire one signal per unique kind in this batch.
        if (UMassSignalSubsystem* SignalSS = Private::GetSignalSubsystem(EntityManager))
        {
            for (EArcMassItemEventType Kind : UniqueKinds)
            {
                const FName Name = ArcMassItems::GetSignalForEventType(Kind, StoreType);
                SignalSS->SignalEntity(Name, Entity);
            }
        }
    }

    void FArcMassItemTransaction::Emit(FArcMassItemEvent Event)
    {
        if (bIsOutermost)
        {
            Pending.Add(MoveTemp(Event));
        }
        else
        {
            check(OuterPtr);
            OuterPtr->Emit(MoveTemp(Event));
        }
    }
}
