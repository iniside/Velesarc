// Copyright Lukasz Baran. All Rights Reserved.

#include "Attachments/ArcMassItemAttachmentProcessor.h"

#include "Attachments/ArcMassItemAttachmentFragments.h"
#include "Attachments/ArcMassItemAttachmentConfig.h"
#include "Attachments/ArcMassItemAttachmentSlots.h"
#include "Attachments/ArcMassAttachmentHandler.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Fragments/ArcMassItemEventFragment.h"
#include "Operations/ArcMassItemTransaction.h"
#include "Signals/ArcMassItemSignals.h"
#include "Operations/ArcMassItemOperations.h"

#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"

UArcMassItemAttachmentProcessor::UArcMassItemAttachmentProcessor()
{
    bRequiresGameThreadExecution = true;
}

void UArcMassItemAttachmentProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManagerRef)
{
    Super::InitializeInternal(Owner, EntityManagerRef);

    const UScriptStruct* StoreType = FArcMassItemStoreFragment::StaticStruct();

    using namespace UE::ArcMassItems::Signals;
    CachedSlotChangedName       = GetSignalName(ItemSlotChanged,       StoreType);
    CachedRemovedName           = GetSignalName(ItemRemoved,           StoreType);
    CachedVisualAttachedName    = GetSignalName(ItemVisualAttached,    StoreType);
    CachedVisualDetachedName    = GetSignalName(ItemVisualDetached,    StoreType);
    CachedAttachmentChangedName = GetSignalName(ItemAttachmentChanged, StoreType);

    if (UWorld* World = GetWorld())
    {
        if (UMassSignalSubsystem* SignalSS = World->GetSubsystem<UMassSignalSubsystem>())
        {
            SubscribeToSignal(*SignalSS, CachedSlotChangedName);
            SubscribeToSignal(*SignalSS, CachedRemovedName);
            SubscribeToSignal(*SignalSS, CachedVisualAttachedName);
            SubscribeToSignal(*SignalSS, CachedVisualDetachedName);
            SubscribeToSignal(*SignalSS, CachedAttachmentChangedName);
        }
    }
}

void UArcMassItemAttachmentProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManagerRef)
{
    EntityQuery.AddConstSharedRequirement<FArcMassItemAttachmentConfigFragment>(EMassFragmentPresence::All);
    EntityQuery.AddRequirement<FArcMassItemAttachmentStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
    // Do NOT call EntityQuery.RegisterWithProcessor(*this) — already bound by {*this} initializer.
}

void UArcMassItemAttachmentProcessor::SignalEntities(
    FMassEntityManager& EntityManager,
    FMassExecutionContext& Context,
    FMassSignalNameLookup& EntitySignals)
{
    EntityQuery.ForEachEntityChunk(Context, [this, &EntityManager, &EntitySignals](FMassExecutionContext& Ctx)
    {
        const FArcMassItemAttachmentConfigFragment& ConfigFragment =
            Ctx.GetConstSharedFragment<FArcMassItemAttachmentConfigFragment>();
        if (!ConfigFragment.Config) return;

        TArrayView<FArcMassItemAttachmentStateFragment> States =
            Ctx.GetMutableFragmentView<FArcMassItemAttachmentStateFragment>();
        TArrayView<FMassActorFragment> ActorFragments =
            Ctx.GetMutableFragmentView<FMassActorFragment>();
        const bool bHasActors = !ActorFragments.IsEmpty();

        // GetSignalsForEntity requires TArray<FName> with default allocator.
        TArray<FName> SignalsForEntity;
        SignalsForEntity.Reserve(8);

        for (FMassExecutionContext::FEntityIterator It = Ctx.CreateEntityIterator(); It; ++It)
        {
            const FMassEntityHandle Entity = Ctx.GetEntity(It);
            FArcMassItemAttachmentStateFragment& State = States[It];
            AActor* Actor = bHasActors ? ActorFragments[It].GetMutable() : nullptr;

            SignalsForEntity.Reset();
            EntitySignals.GetSignalsForEntity(Entity, SignalsForEntity);

            // Phase 1
            if (SignalsForEntity.Contains(CachedSlotChangedName))
            {
                ProcessSlotChangedForEntity(EntityManager, Entity, ConfigFragment, State, Actor);
            }
            if (SignalsForEntity.Contains(CachedRemovedName))
            {
                ProcessRemovedForEntity(EntityManager, Entity, ConfigFragment, State);
            }

            // Phase 2
            if (SignalsForEntity.Contains(CachedVisualAttachedName))
            {
                ProcessVisualAttachedForEntity(EntityManager, Entity, ConfigFragment, State, Actor);
            }
            if (SignalsForEntity.Contains(CachedVisualDetachedName))
            {
                ProcessVisualDetachedForEntity(EntityManager, Entity, ConfigFragment, State);
            }

            // Phase 3
            if (SignalsForEntity.Contains(CachedAttachmentChangedName))
            {
                ProcessAttachmentChangedForEntity(EntityManager, Entity, ConfigFragment, State, Actor);
            }
        }
    });
}

void UArcMassItemAttachmentProcessor::ProcessSlotChangedForEntity(
    FMassEntityManager& EntityManager, FMassEntityHandle Entity,
    const FArcMassItemAttachmentConfigFragment& ConfigFragment,
    FArcMassItemAttachmentStateFragment& State, AActor* Actor)
{
    const UScriptStruct* StoreType = FArcMassItemStoreFragment::StaticStruct();

    FMassEntityView View(EntityManager, Entity);
    const FArcMassItemStoreFragment* Store = View.GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
    if (!Store) return;

    // Add path: items in valid slots that aren't yet in ActiveAttachments.
    for (const FArcMassReplicatedItem& RepItem : Store->ReplicatedItems.Items)
    {
        const FArcItemData* ItemData = RepItem.ToItem();
        if (!ItemData) continue;

        const FGameplayTag SlotId = ItemData->GetSlotId();
        if (!SlotId.IsValid()) continue;
        if (State.ActiveAttachments.ContainsByPredicate(
                [&ItemData](const FArcMassItemAttachment& A) { return A.ItemId == ItemData->GetItemId(); }))
        {
            continue;
        }

        const FArcMassItemAttachmentSlot* Slot = ConfigFragment.Config->FindSlot(SlotId);
        if (!Slot) continue;

        const FArcItemData* OwnerData = nullptr;
        if (ItemData->GetOwnerId().IsValid())
        {
            const FArcMassReplicatedItem* OwnerRep = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemData->GetOwnerId());
            OwnerData = OwnerRep ? OwnerRep->ToItem() : nullptr;
        }

        for (const FInstancedStruct& HandlerInst : Slot->Handlers)
        {
            if (const FArcMassAttachmentHandler* Handler = HandlerInst.GetPtr<FArcMassAttachmentHandler>())
            {
                if (Handler->HandleItemAddedToSlot(EntityManager, Entity, State, ItemData, OwnerData))
                {
                    EmitVisualAttached(EntityManager, Entity, StoreType, ItemData->GetItemId());
                }
            }
        }
    }

    // Reconciliation sweep — detach orphans (entries whose item is no longer in the store).
    for (int32 i = State.ActiveAttachments.Num() - 1; i >= 0; --i)
    {
        const FArcItemId ItemId = State.ActiveAttachments[i].ItemId;
        const bool bStillInStore =
            const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId) != nullptr;

        if (!bStillInStore)
        {
            FArcMassItemAttachment Snapshot = State.ActiveAttachments[i];
            State.ActiveAttachments.RemoveAt(i);
            State.PendingVisualDetaches.Add(Snapshot.ItemId, Snapshot);
            DispatchRemovedHandlers(ConfigFragment, EntityManager, Entity, State, Snapshot);
            EmitVisualDetached(EntityManager, Entity, StoreType, Snapshot.ItemId);
        }
    }
}

void UArcMassItemAttachmentProcessor::DispatchRemovedHandlers(
    const FArcMassItemAttachmentConfigFragment& ConfigFragment,
    FMassEntityManager& EntityManager, FMassEntityHandle Entity,
    FArcMassItemAttachmentStateFragment& State,
    const FArcMassItemAttachment& Snapshot)
{
    const FArcMassItemAttachmentSlot* Slot = ConfigFragment.Config->FindSlot(Snapshot.SlotId);
    if (!Slot) return;

    // Remove socket from TakenSockets.
    if (FArcMassItemAttachmentNameArray* Sockets = State.TakenSockets.Find(Snapshot.SlotId))
    {
        Sockets->Names.Remove(Snapshot.SocketName);
    }

    for (const FInstancedStruct& HandlerInst : Slot->Handlers)
    {
        if (const FArcMassAttachmentHandler* Handler = HandlerInst.GetPtr<FArcMassAttachmentHandler>())
        {
            Handler->HandleItemRemovedFromSlot(EntityManager, Entity, State, Snapshot);
        }
    }
}

void UArcMassItemAttachmentProcessor::EmitVisualAttached(
    FMassEntityManager& EM, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId)
{
    ArcMassItems::FArcMassItemTransaction Tx(EM, Entity, StoreType);
    FArcMassItemEvent Event;
    Event.ItemId = ItemId;
    Event.Type = EArcMassItemEventType::VisualAttached;
    Tx.Emit(MoveTemp(Event));
}

void UArcMassItemAttachmentProcessor::EmitVisualDetached(
    FMassEntityManager& EM, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId)
{
    ArcMassItems::FArcMassItemTransaction Tx(EM, Entity, StoreType);
    FArcMassItemEvent Event;
    Event.ItemId = ItemId;
    Event.Type = EArcMassItemEventType::VisualDetached;
    Tx.Emit(MoveTemp(Event));
}

void UArcMassItemAttachmentProcessor::ProcessRemovedForEntity(
    FMassEntityManager& EntityManager, FMassEntityHandle Entity,
    const FArcMassItemAttachmentConfigFragment& ConfigFragment,
    FArcMassItemAttachmentStateFragment& State)
{
    const UScriptStruct* StoreType = FArcMassItemStoreFragment::StaticStruct();

    FMassEntityView View(EntityManager, Entity);
    const FArcMassItemEventFragment* Events = View.GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
    if (!Events) return;

    for (const FArcMassItemEvent& Event : Events->Events)
    {
        if (Event.Type != EArcMassItemEventType::Removed) continue;
        if (Event.StoreType != StoreType) continue;

        const int32 Index = State.ActiveAttachments.IndexOfByPredicate(
            [&Event](const FArcMassItemAttachment& A) { return A.ItemId == Event.ItemId; });
        if (Index == INDEX_NONE) continue;

        FArcMassItemAttachment Snapshot = State.ActiveAttachments[Index];
        State.ActiveAttachments.RemoveAt(Index);
        State.PendingVisualDetaches.Add(Snapshot.ItemId, Snapshot);
        DispatchRemovedHandlers(ConfigFragment, EntityManager, Entity, State, Snapshot);
        EmitVisualDetached(EntityManager, Entity, StoreType, Snapshot.ItemId);
    }
}

void UArcMassItemAttachmentProcessor::ProcessVisualAttachedForEntity(
    FMassEntityManager& EntityManager, FMassEntityHandle Entity,
    const FArcMassItemAttachmentConfigFragment& ConfigFragment,
    FArcMassItemAttachmentStateFragment& State, AActor* Actor)
{
    const UScriptStruct* StoreType = FArcMassItemStoreFragment::StaticStruct();

    FMassEntityView View(EntityManager, Entity);
    const FArcMassItemEventFragment* Events = View.GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
    if (!Events) return;

    for (const FArcMassItemEvent& Event : Events->Events)
    {
        if (Event.Type != EArcMassItemEventType::VisualAttached) continue;
        if (Event.StoreType != StoreType) continue;

        const int32 Index = State.ActiveAttachments.IndexOfByPredicate(
            [&Event](const FArcMassItemAttachment& A) { return A.ItemId == Event.ItemId; });
        if (Index == INDEX_NONE) continue;

        const FArcMassItemAttachment& Attachment = State.ActiveAttachments[Index];
        const FArcMassItemAttachmentSlot* Slot = ConfigFragment.Config->FindSlot(Attachment.SlotId);
        if (!Slot) continue;

        for (const FInstancedStruct& HandlerInst : Slot->Handlers)
        {
            if (const FArcMassAttachmentHandler* Handler = HandlerInst.GetPtr<FArcMassAttachmentHandler>())
            {
                Handler->HandleItemAttach(EntityManager, Entity, State, Event.ItemId, Attachment.OwnerId);
            }
        }
    }
}

void UArcMassItemAttachmentProcessor::ProcessVisualDetachedForEntity(
    FMassEntityManager& EntityManager, FMassEntityHandle Entity,
    const FArcMassItemAttachmentConfigFragment& ConfigFragment,
    FArcMassItemAttachmentStateFragment& State)
{
    const UScriptStruct* StoreType = FArcMassItemStoreFragment::StaticStruct();

    FMassEntityView View(EntityManager, Entity);
    const FArcMassItemEventFragment* Events = View.GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
    if (!Events) return;

    for (const FArcMassItemEvent& Event : Events->Events)
    {
        if (Event.Type != EArcMassItemEventType::VisualDetached) continue;
        if (Event.StoreType != StoreType) continue;

        const FArcMassItemAttachment* Snapshot = State.PendingVisualDetaches.Find(Event.ItemId);
        if (!Snapshot) continue;

        const FArcMassItemAttachmentSlot* Slot = ConfigFragment.Config->FindSlot(Snapshot->SlotId);
        if (Slot)
        {
            for (const FInstancedStruct& HandlerInst : Slot->Handlers)
            {
                if (const FArcMassAttachmentHandler* Handler = HandlerInst.GetPtr<FArcMassAttachmentHandler>())
                {
                    Handler->HandleItemDetach(EntityManager, Entity, State, *Snapshot);
                }
            }
        }
        State.PendingVisualDetaches.Remove(Event.ItemId);
    }
}

void UArcMassItemAttachmentProcessor::ProcessAttachmentChangedForEntity(
    FMassEntityManager& EntityManager, FMassEntityHandle Entity,
    const FArcMassItemAttachmentConfigFragment& ConfigFragment,
    FArcMassItemAttachmentStateFragment& State, AActor* Actor)
{
    const UScriptStruct* StoreType = FArcMassItemStoreFragment::StaticStruct();

    FMassEntityView View(EntityManager, Entity);
    const FArcMassItemEventFragment* Events = View.GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
    if (!Events) return;

    for (const FArcMassItemEvent& Event : Events->Events)
    {
        if (Event.Type != EArcMassItemEventType::AttachmentChanged) continue;
        if (Event.StoreType != StoreType) continue;

        const int32 Index = State.ActiveAttachments.IndexOfByPredicate(
            [&Event](const FArcMassItemAttachment& A) { return A.ItemId == Event.ItemId; });
        if (Index == INDEX_NONE) continue;

        const FArcMassItemAttachment& Attachment = State.ActiveAttachments[Index];
        const FArcMassItemAttachmentSlot* Slot = ConfigFragment.Config->FindSlot(Attachment.SlotId);
        if (!Slot) continue;

        for (const FInstancedStruct& HandlerInst : Slot->Handlers)
        {
            if (const FArcMassAttachmentHandler* Handler = HandlerInst.GetPtr<FArcMassAttachmentHandler>())
            {
                Handler->HandleItemAttachmentChanged(EntityManager, Entity, State, Attachment);
            }
        }
    }
}
