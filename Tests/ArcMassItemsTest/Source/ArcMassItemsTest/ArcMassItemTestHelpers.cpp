// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassItemTestHelpers.h"

#include "Components/ActorTestSpawner.h"
#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "MassSignalSubsystem.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityHandle.h"
#include "MassActorSubsystem.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"
#include "Engine/World.h"

#include "Attachments/ArcMassItemAttachmentConfig.h"
#include "Attachments/ArcMassItemAttachmentSlots.h"
#include "Attachments/ArcMassItemAttachmentFragments.h"
#include "Events/ArcMassItemEventSubsystem.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Fragments/ArcMassItemEventFragment.h"
#include "Traits/ArcMassItemStoreTrait.h"
#include "Attachments/ArcMassItemAttachmentTrait.h"
#include "Items/ArcItemData.h"
#include "ArcItemTestRecordFiles.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Core/ArcCoreAssetManager.h"
#include "Signals/ArcMassItemSignals.h"

void FArcItemFragment_TestLifecycleRecorder::OnItemAttachedTo(const FArcItemData* InItem, const FArcItemData* InOwnerItem) const
{
    FArcTestLifecycleEvent Event;
    Event.Event = FName(TEXT("AttachedTo"));
    Event.ItemId = InItem ? InItem->GetItemId() : FArcItemId();
    Event.OwnerId = InOwnerItem ? InOwnerItem->GetItemId() : FArcItemId();
    ArcMassItems::TestHelpers::LifecycleLog().Add(Event);
}

void FArcItemFragment_TestLifecycleRecorder::OnItemDetachedFrom(const FArcItemData* InItem, const FArcItemData* InOwnerItem) const
{
    FArcTestLifecycleEvent Event;
    Event.Event = FName(TEXT("DetachedFrom"));
    Event.ItemId = InItem ? InItem->GetItemId() : FArcItemId();
    Event.OwnerId = InOwnerItem ? InOwnerItem->GetItemId() : FArcItemId();
    ArcMassItems::TestHelpers::LifecycleLog().Add(Event);
}

void FArcItemFragment_TestLifecycleRecorder::OnPreRemoveItem(const FArcItemData* InItem) const
{
    FArcTestLifecycleEvent Event;
    Event.Event = FName(TEXT("PreRemove"));
    Event.ItemId = InItem ? InItem->GetItemId() : FArcItemId();
    ArcMassItems::TestHelpers::LifecycleLog().Add(Event);
}

bool FArcMassAttachmentHandler_Test::HandleItemAddedToSlot(
    FMassEntityManager& /*EntityManager*/,
    FMassEntityHandle /*Entity*/,
    FArcMassItemAttachmentStateFragment& State,
    const FArcItemData* Item,
    const FArcItemData* OwnerItem) const
{
    const FArcItemId ItemId = Item ? Item->GetItemId() : FArcItemId();
    const FArcItemId OwnerId = OwnerItem ? OwnerItem->GetItemId() : FArcItemId();
    // Sub-attachments keep their slot key in AttachedToSlot, not Slot. Mirror that here so
    // the stored Attachment.SlotId is what FindSlot() expects later (e.g. on detach).
    const FGameplayTag TopSlot = Item ? Item->GetSlotId() : FGameplayTag();
    const FGameplayTag SlotId = (Item && !TopSlot.IsValid()) ? Item->GetAttachSlot() : TopSlot;

    FCall Call;
    Call.Phase = EPhase::AddedToSlot;
    Call.ItemId = ItemId;
    Call.OwnerId = OwnerId;
    Call.SlotId = SlotId;
    const_cast<FArcMassAttachmentHandler_Test*>(this)->CallLog.Add(MoveTemp(Call));

    // Mirror production handlers: maintain State so the processor's dedupe,
    // reconciliation, and Removed/Visual lookups behave realistically. Use the
    // ItemId-derived FName as a stand-in socket name (no socket contention is
    // modeled in tests, but TakenSockets must be populated for assertions on it).
    FArcMassItemAttachment Attachment;
    Attachment.ItemId = ItemId;
    Attachment.OwnerId = OwnerId;
    Attachment.SlotId = SlotId;
    Attachment.SocketName = FName(*ItemId.ToString());
    Attachment.ItemDefinition = Item ? Item->GetItemDefinitionId() : FPrimaryAssetId();
    State.ActiveAttachments.Add(MoveTemp(Attachment));

    if (SlotId.IsValid())
    {
        FArcMassItemAttachmentNameArray& Names = State.TakenSockets.FindOrAdd(SlotId);
        Names.Names.AddUnique(FName(*ItemId.ToString()));
    }
    return true;
}

void FArcMassAttachmentHandler_Test::HandleItemAttach(
    FMassEntityManager& /*EntityManager*/,
    FMassEntityHandle /*Entity*/,
    FArcMassItemAttachmentStateFragment& /*State*/,
    const FArcItemId ItemId,
    const FArcItemId OwnerItem) const
{
    FCall Call;
    Call.Phase = EPhase::Attach;
    Call.ItemId = ItemId;
    Call.OwnerId = OwnerItem;
    const_cast<FArcMassAttachmentHandler_Test*>(this)->CallLog.Add(MoveTemp(Call));
}

void FArcMassAttachmentHandler_Test::HandleItemRemovedFromSlot(
    FMassEntityManager& /*EntityManager*/,
    FMassEntityHandle /*Entity*/,
    FArcMassItemAttachmentStateFragment& /*State*/,
    const FArcMassItemAttachment& Snapshot) const
{
    FCall Call;
    Call.Phase = EPhase::RemovedFromSlot;
    Call.ItemId = Snapshot.ItemId;
    Call.OwnerId = Snapshot.OwnerId;
    Call.SlotId = Snapshot.SlotId;
    Call.Snapshot = Snapshot;
    const_cast<FArcMassAttachmentHandler_Test*>(this)->CallLog.Add(MoveTemp(Call));
}

void FArcMassAttachmentHandler_Test::HandleItemDetach(
    FMassEntityManager& /*EntityManager*/,
    FMassEntityHandle /*Entity*/,
    FArcMassItemAttachmentStateFragment& State,
    const FArcMassItemAttachment& Snapshot) const
{
    FCall Call;
    Call.Phase = EPhase::Detach;
    Call.ItemId = Snapshot.ItemId;
    Call.OwnerId = Snapshot.OwnerId;
    Call.SlotId = Snapshot.SlotId;
    Call.Snapshot = Snapshot;
    const_cast<FArcMassAttachmentHandler_Test*>(this)->CallLog.Add(MoveTemp(Call));

    // Mirror FArcMassAttachmentHandler_StaticMesh::HandleItemDetach: production
    // contract is that detach removes the spawned objects map entry for ItemId.
    State.ObjectsAttachedFromItem.Remove(Snapshot.ItemId);
}

void FArcMassAttachmentHandler_Test::HandleItemAttachmentChanged(
    FMassEntityManager& /*EntityManager*/,
    FMassEntityHandle /*Entity*/,
    FArcMassItemAttachmentStateFragment& /*State*/,
    const FArcMassItemAttachment& ItemAttachment) const
{
    FCall Call;
    Call.Phase = EPhase::AttachmentChanged;
    Call.ItemId = ItemAttachment.ItemId;
    Call.OwnerId = ItemAttachment.OwnerId;
    Call.SlotId = ItemAttachment.SlotId;
    Call.Snapshot = ItemAttachment;
    const_cast<FArcMassAttachmentHandler_Test*>(this)->CallLog.Add(MoveTemp(Call));
}

int32 FArcMassAttachmentHandler_Test::NumCalls(EPhase Phase) const
{
    int32 N = 0;
    for (const FCall& C : CallLog) { if (C.Phase == Phase) { ++N; } }
    return N;
}

const FArcMassAttachmentHandler_Test::FCall*
FArcMassAttachmentHandler_Test::LastCall(EPhase Phase) const
{
    for (int32 i = CallLog.Num() - 1; i >= 0; --i)
    {
        if (CallLog[i].Phase == Phase) { return &CallLog[i]; }
    }
    return nullptr;
}

FArcMassItemSignalListener::FArcMassItemSignalListener(
    UMassSignalSubsystem& InSubsystem, FName InQualifiedSignalName)
    : Subsystem(&InSubsystem)
    , SignalName(InQualifiedSignalName)
{
    UE::MassSignal::FSignalDelegate& Delegate = Subsystem->GetSignalDelegateByName(SignalName);
    Handle = Delegate.AddLambda(
        [this](FName /*Name*/, TConstArrayView<FMassEntityHandle> Entities)
        {
            for (FMassEntityHandle E : Entities)
            {
                Received.Add({ E });
            }
        });
}

FArcMassItemSignalListener::~FArcMassItemSignalListener()
{
    if (Subsystem && Handle.IsValid())
    {
        Subsystem->GetSignalDelegateByName(SignalName).Remove(Handle);
    }
}

int32 FArcMassItemSignalListener::NumReceivedFor(FMassEntityHandle Entity) const
{
    int32 N = 0;
    for (const FReceived& R : Received) { if (R.Entity == Entity) { ++N; } }
    return N;
}

namespace ArcMassItems::TestHelpers
{

UArcMassItemAttachmentConfig* BuildAttachmentConfig(
    TArrayView<const FArcMassItemAttachmentSlot> Slots)
{
    UArcMassItemAttachmentConfig* Config = NewObject<UArcMassItemAttachmentConfig>(
        GetTransientPackage(), NAME_None, RF_Transient);
    Config->Slots = TArray<FArcMassItemAttachmentSlot>(Slots.GetData(), Slots.Num());
    return Config;
}

UScriptStruct* MarkerFragmentType()
{
    return FArcItemFragment_Tags::StaticStruct();
}

FPrimaryAssetId WriteWithMarkerFragment(FArcItemTestRecordFiles& Records, const FPrimaryAssetId& BaseId)
{
    return Records.WriteVariant(BaseId, [](FArcItemDefinitionData& Def)
    {
        if (Def.FindFragment<FArcItemFragment_Tags>() == nullptr)
        {
            ArcItemTestRecords::AddFragment(Def, FArcItemFragment_Tags{});
        }
    });
}

TArray<FArcTestLifecycleEvent>& LifecycleLog()
{
    static TArray<FArcTestLifecycleEvent> Log;
    return Log;
}

FPrimaryAssetId WriteWithLifecycleRecorder(FArcItemTestRecordFiles& Records, const FPrimaryAssetId& BaseId)
{
    return Records.WriteVariant(BaseId, [](FArcItemDefinitionData& Def)
    {
        if (Def.FindFragment<FArcItemFragment_TestLifecycleRecorder>() == nullptr)
        {
            ArcItemTestRecords::AddFragment(Def, FArcItemFragment_TestLifecycleRecorder{});
        }
    });
}

FPrimaryAssetId WriteWithScalableFragment(FArcItemTestRecordFiles& Records, const FPrimaryAssetId& BaseId)
{
    return Records.WriteVariant(BaseId, [](FArcItemDefinitionData& Def)
    {
        if (Def.GetScalableFloatFragment<FArcScalableFloatItemFragment_Test>() == nullptr)
        {
            ArcItemTestRecords::AddFragmentByType(Def, FArcScalableFloatItemFragment_Test::StaticStruct());
        }
    });
}

UArcMassItemAttachmentConfig* BuildSingleHandlerConfig(
    FGameplayTag SlotId,
    FArcMassAttachmentHandler_Test*& OutHandlerPtr,
    UScriptStruct* SupportedFragment)
{
    FArcMassItemAttachmentSlot Slot;
    Slot.SlotId = SlotId;

    FInstancedStruct HandlerInst;
    HandlerInst.InitializeAs(FArcMassAttachmentHandler_Test::StaticStruct());
    FArcMassAttachmentHandler_Test* HandlerPtr = HandlerInst.GetMutablePtr<FArcMassAttachmentHandler_Test>();
    HandlerPtr->SupportedFragment = SupportedFragment;
    Slot.Handlers.Add(MoveTemp(HandlerInst));

    UArcMassItemAttachmentConfig* Config = BuildAttachmentConfig({ Slot });
    OutHandlerPtr = Config->Slots[0].Handlers[0].GetMutablePtr<FArcMassAttachmentHandler_Test>();
    return Config;
}

FMassEntityHandle CreateAttachmentEntity(
    FMassEntityManager& EntityManager,
    UArcMassItemAttachmentConfig* Config)
{
    // Mirror the production spawn pipeline (UArcMassSpawnerSubsystem::BuildOne): a raw
    // CreateEntity(Archetype, ...) silently drops sparse fragments, so build via
    // UE::Mass::FEntityBuilder, which routes each Add<T> by concept — regular fragment,
    // const-shared (FMassConstSharedFragment), or sparse (FMassSparseFragment). Commit()
    // makes sparse fragments readable immediately; no FlushCommands needed.
    //
    // The seeded set reproduces what the traits' BuildSparseFragments + BuildTemplate
    // would emit for an attachment-bearing store entity:
    //   - FTransformFragment                     (regular)
    //   - FArcMassItemAttachmentTag              (tag, from UArcMassItemAttachmentTrait::BuildTemplate)
    //   - FArcMassItemAttachmentConfigFragment   (const-shared config)
    //   - FArcMassItemStoreFragment              (sparse, UArcMassItemStoreTrait — store for AddItem)
    //   - FArcMassItemEventFragment              (sparse, UArcMassItemStoreTrait — transaction event queue)
    //   - FArcMassItemAttachmentStateFragment    (sparse, UArcMassItemAttachmentTrait — handler state)
    FArcMassItemAttachmentConfigFragment ConfigFragment;
    ConfigFragment.Config = Config;
    ConfigFragment.InterestedStoreTypes.Add(FArcMassItemStoreFragment::StaticStruct());

    UE::Mass::FEntityBuilder Builder(EntityManager);
    const FMassEntityHandle Entity = Builder
        .Add<FTransformFragment>()
        .Add<FArcMassItemAttachmentTag>()
        .Add<FArcMassItemAttachmentConfigFragment>(ConfigFragment)
        .Add<FArcMassItemStoreFragment>()
        .Add<FArcMassItemEventFragment>()
        .Add<FArcMassItemAttachmentStateFragment>()
        .Commit();

    return Entity;
}

FMassEntityHandle CreateDualStoreAttachmentEntity(
    FMassEntityManager& EntityManager,
    UArcMassItemAttachmentConfig* Config)
{
    FArcMassItemAttachmentConfigFragment ConfigFragment;
    ConfigFragment.Config = Config;
    ConfigFragment.InterestedStoreTypes.Add(FArcMassItemStoreFragment::StaticStruct());
    ConfigFragment.InterestedStoreTypes.Add(FArcTestSecondaryItemStoreFragment::StaticStruct());

    UE::Mass::FEntityBuilder Builder(EntityManager);
    const FMassEntityHandle Entity = Builder
        .Add<FTransformFragment>()
        .Add<FArcMassItemAttachmentTag>()
        .Add<FArcMassItemAttachmentConfigFragment>(ConfigFragment)
        .Add<FArcMassItemStoreFragment>()
        .Add<FArcTestSecondaryItemStoreFragment>()
        .Add<FArcMassItemEventFragment>()
        .Add<FArcMassItemAttachmentStateFragment>()
        .Commit();

    return Entity;
}

void DrainAttachmentEvents(UWorld* World)
{
    if (!World) return;
    UArcMassItemEventSubsystem* EventSub = World->GetSubsystem<UArcMassItemEventSubsystem>();
    if (EventSub)
    {
        EventSub->DrainQueues();
    }
}

void LinkActorToEntity(
    FMassEntityManager& EntityManager,
    FMassEntityHandle Entity,
    AActor* Actor)
{
    FMassActorFragment ActorFragment;
    ActorFragment.SetNoHandleMapUpdate(Entity, Actor, /*bIsOwnedByMass*/ false);
    EntityManager.AddFragmentInstanceListToEntity(
        Entity, { FInstancedStruct::Make(ActorFragment) });
}

FName StoreQualifiedSignal(FName BaseSignalName)
{
    // Signals are now generic (un-qualified). Helper kept so existing tests compile;
    // it simply returns the base name now.
    return BaseSignalName;
}

} // namespace ArcMassItems::TestHelpers
