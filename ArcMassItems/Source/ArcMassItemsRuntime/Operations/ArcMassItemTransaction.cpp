// Copyright Lukasz Baran. All Rights Reserved.

#include "Operations/ArcMassItemTransaction.h"
#include "Operations/ArcMassItemOperations.h"
#include "Fragments/ArcMassItemEventFragment.h"

#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "MassSignalSubsystem.h"
#include "Engine/World.h"
#include "UObject/Class.h"
#include "Events/ArcMassItemEventSubsystem.h"

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
                const FName Name = ArcMassItems::GetSignalForEventType(Kind);
                SignalSS->SignalEntity(Name, Entity);
            }
        }

        // 6. Broadcast each committed event to UArcMassItemEventSubsystem.
        UArcMassItemEventSubsystem* EventSubsystem = nullptr;
        if (UWorld* World = EntityManager.GetWorld())
        {
            EventSubsystem = World->GetSubsystem<UArcMassItemEventSubsystem>();
        }
        if (EventSubsystem)
        {
            const int32 PendingCount = Pending.Num();
            const int32 StartIdx = Frag->Events.Num() - PendingCount;
            for (int32 Idx = StartIdx; Idx < Frag->Events.Num(); ++Idx)
            {
                const FArcMassItemEvent& Event = Frag->Events[Idx];
                switch (Event.Type)
                {
                    case EArcMassItemEventType::Added:
                        EventSubsystem->QueueItemAdded(Entity, Event.StoreType, Event.ItemId);
                        break;
                    case EArcMassItemEventType::Removed:
                        EventSubsystem->QueueItemRemoved(Entity, Event.StoreType, Event.ItemId);
                        break;
                    case EArcMassItemEventType::SlotAssigned:
                        EventSubsystem->QueueItemSlotAssigned(Entity, Event.StoreType, Event.ItemId, Event.SlotTag);
                        break;
                    case EArcMassItemEventType::SlotCleared:
                        EventSubsystem->QueueItemSlotCleared(Entity, Event.StoreType, Event.ItemId, Event.OldSlotTag);
                        break;
                    case EArcMassItemEventType::SlotChanged:
                        EventSubsystem->QueueItemSlotChanged(Entity, Event.StoreType, Event.ItemId, Event.OldSlotTag, Event.SlotTag);
                        break;
                    case EArcMassItemEventType::StacksChanged:
                        EventSubsystem->QueueItemStacksChanged(Entity, Event.StoreType, Event.ItemId, Event.StackDelta);
                        break;
                    case EArcMassItemEventType::Attached:
                        EventSubsystem->QueueItemAttached(Entity, Event.StoreType, Event.ItemId, Event.SlotTag);
                        break;
                    case EArcMassItemEventType::Detached:
                        EventSubsystem->QueueItemDetached(Entity, Event.StoreType, Event.ItemId);
                        break;
                    case EArcMassItemEventType::AttachmentChanged:
                        EventSubsystem->QueueItemAttachmentChanged(Entity, Event.StoreType, Event.ItemId);
                        break;
                    default:
                        break;
                }
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
