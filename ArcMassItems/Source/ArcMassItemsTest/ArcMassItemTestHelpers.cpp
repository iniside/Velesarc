// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassItemTestHelpers.h"

#include "Components/ActorTestSpawner.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassExecutor.h"
#include "MassProcessingContext.h"
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
#include "Attachments/ArcMassItemAttachmentProcessor.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Traits/ArcMassItemStoreTrait.h"
#include "Items/ArcItemData.h"
#include "Signals/ArcMassItemSignals.h"

bool FArcMassAttachmentHandler_Test::HandleItemAddedToSlot(
    FMassEntityManager& /*EntityManager*/,
    FMassEntityHandle /*Entity*/,
    FArcMassItemAttachmentStateFragment& /*State*/,
    const FArcItemData* Item,
    const FArcItemData* OwnerItem) const
{
    FCall Call;
    Call.Phase = EPhase::AddedToSlot;
    Call.ItemId = Item ? Item->GetItemId() : FArcItemId();
    Call.OwnerId = OwnerItem ? OwnerItem->GetItemId() : FArcItemId();
    Call.SlotId = Item ? Item->GetSlotId() : FGameplayTag();
    const_cast<FArcMassAttachmentHandler_Test*>(this)->CallLog.Add(MoveTemp(Call));
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
    FArcMassItemAttachmentStateFragment& /*State*/,
    const FArcMassItemAttachment& Snapshot) const
{
    FCall Call;
    Call.Phase = EPhase::Detach;
    Call.ItemId = Snapshot.ItemId;
    Call.OwnerId = Snapshot.OwnerId;
    Call.SlotId = Snapshot.SlotId;
    Call.Snapshot = Snapshot;
    const_cast<FArcMassAttachmentHandler_Test*>(this)->CallLog.Add(MoveTemp(Call));
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
    // Build archetype with all regular fragments + tag up front. Sparse
    // FArcMassItemStoreFragment is added by UArcMassItemStoreInitObserver
    // reacting to FArcMassItemStoreTag; const-shared config fragment is
    // supplied via FMassArchetypeSharedFragmentValues. No post-creation
    // composition mutations.
    TArray<const UScriptStruct*> FragmentsAndTags;
    FragmentsAndTags.Add(FTransformFragment::StaticStruct());
    FragmentsAndTags.Add(FArcMassItemAttachmentStateFragment::StaticStruct());
    FragmentsAndTags.Add(FArcMassItemStoreTag::StaticStruct());
    const FMassArchetypeHandle Archetype = EntityManager.CreateArchetype(FragmentsAndTags);

    FArcMassItemAttachmentConfigFragment ConfigFragment;
    ConfigFragment.Config = Config;
    FMassArchetypeSharedFragmentValues SharedValues;
    SharedValues.Add(EntityManager.GetOrCreateConstSharedFragment(ConfigFragment));

    return EntityManager.CreateEntity(Archetype, SharedValues);
}

UArcMassItemAttachmentProcessor* RegisterAttachmentProcessor(
    UWorld* World, FMassEntityManager& EntityManager)
{
    UArcMassItemAttachmentProcessor* Processor =
        NewObject<UArcMassItemAttachmentProcessor>(World);
    Processor->CallInitialize(EntityManager.GetOwner(), EntityManager.AsShared());
    return Processor;
}

void RunProcessor(
    UArcMassItemAttachmentProcessor& Processor,
    FMassEntityManager& EntityManager,
    UMassSignalSubsystem& SignalSubsystem,
    TArrayView<const FName> QualifiedSignalNames,
    FMassEntityHandle Entity,
    float DeltaTime)
{
    for (const FName& Name : QualifiedSignalNames)
    {
        SignalSubsystem.SignalEntity(Name, Entity);
    }
    UE::Mass::FProcessingContext Context(EntityManager, DeltaTime);
    UE::Mass::Executor::Run(Processor, Context);
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
    return UE::ArcMassItems::Signals::GetSignalName(
        BaseSignalName, FArcMassItemStoreFragment::StaticStruct());
}

} // namespace ArcMassItems::TestHelpers
