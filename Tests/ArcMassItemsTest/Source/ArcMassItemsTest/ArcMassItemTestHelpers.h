// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "Engine/Engine.h"
#include "Mass/EntityHandle.h"
#include "Attachments/ArcMassAttachmentHandler.h"
#include "Attachments/ArcMassItemAttachmentFragments.h"
#include "Items/ArcItemId.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Commands/ArcReplicatedCommand.h"
#include "Commands/ArcReplicatedCommandHost.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "GameplayTagContainer.h"
#include "ArcMassItemTestHelpers.generated.h"

class AActor;
class UArcMassItemAttachmentConfig;
class UArcMassItemEventSubsystem;
class UMassSignalSubsystem;
class UWorld;
struct FArcItemAttachmentSlot;
struct FArcItemData;
class FArcItemTestRecordFiles;
struct FArcItemId;
struct FArcMassItemAttachmentSlot;
struct FMassEntityManager;

USTRUCT(meta = (Hidden))
struct ARCMASSITEMSTEST_API FArcMassAttachmentHandler_Test : public FArcMassAttachmentHandler
{
    GENERATED_BODY()

    enum class EPhase : uint8
    {
        AddedToSlot,
        Attach,
        RemovedFromSlot,
        Detach,
        AttachmentChanged,
    };

    struct FCall
    {
        EPhase Phase;
        FArcItemId ItemId;
        FArcItemId OwnerId;
        FGameplayTag SlotId;
        FArcMassItemAttachment Snapshot;   // populated for RemovedFromSlot / Detach
    };

    // Recorded calls — mutated through const virtuals via const_cast.
    mutable TArray<FCall> CallLog;

    // Optional — restrict dispatch to items carrying this fragment type.
    UPROPERTY()
    TObjectPtr<UScriptStruct> SupportedFragment = nullptr;

    virtual UScriptStruct* SupportedItemFragment() const override { return SupportedFragment; }

    virtual bool HandleItemAddedToSlot(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        FArcMassItemAttachmentStateFragment& State,
        const FArcItemData* Item,
        const FArcItemData* OwnerItem) const override;

    virtual void HandleItemAttach(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        FArcMassItemAttachmentStateFragment& State,
        const FArcItemId ItemId,
        const FArcItemId OwnerItem) const override;

    virtual void HandleItemRemovedFromSlot(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        FArcMassItemAttachmentStateFragment& State,
        const FArcMassItemAttachment& Snapshot) const override;

    virtual void HandleItemDetach(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        FArcMassItemAttachmentStateFragment& State,
        const FArcMassItemAttachment& Snapshot) const override;

    virtual void HandleItemAttachmentChanged(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        FArcMassItemAttachmentStateFragment& State,
        const FArcMassItemAttachment& ItemAttachment) const override;

    int32 NumCalls(EPhase Phase) const;
    const FCall* LastCall(EPhase Phase) const;
};

/** One recorded item-fragment lifecycle callback. */
struct FArcTestLifecycleEvent
{
    FName Event;
    FArcItemId ItemId;
    FArcItemId OwnerId;
};

/**
 * Item fragment that records lifecycle callbacks (OnItemAttachedTo / OnItemDetachedFrom /
 * OnPreRemoveItem) into a process-global log accessible via
 * ArcMassItems::TestHelpers::LifecycleLog(). Add to a definition via
 * WriteWithLifecycleRecorder; clear the log in BEFORE_EACH.
 */
USTRUCT(meta = (Hidden))
struct ARCMASSITEMSTEST_API FArcItemFragment_TestLifecycleRecorder : public FArcItemFragment_ItemInstanceBase
{
    GENERATED_BODY()

    virtual void OnItemAttachedTo(const FArcItemData* InItem, const FArcItemData* InOwnerItem) const override;
    virtual void OnItemDetachedFrom(const FArcItemData* InItem, const FArcItemData* InOwnerItem) const override;
    virtual void OnPreRemoveItem(const FArcItemData* InItem) const override;
};

/** Scalable-float fragment used to observe attach-time merge into the owner's map. */
USTRUCT(meta = (Hidden))
struct ARCMASSITEMSTEST_API FArcScalableFloatItemFragment_Test : public FArcScalableFloatItemFragment
{
    GENERATED_BODY()
};

/** Second scalable-float fragment type, so a record rewrite can change which types the record holds. */
USTRUCT(meta = (Hidden))
struct ARCMASSITEMSTEST_API FArcScalableFloatItemFragment_TestB : public FArcScalableFloatItemFragment
{
    GENERATED_BODY()
};

/** Derived item store for multi-store tests (second store on one entity). */
USTRUCT(meta = (Hidden))
struct ARCMASSITEMSTEST_API FArcTestSecondaryItemStoreFragment : public FArcMassItemStoreFragment
{
    GENERATED_BODY()
};

template<>
struct TMassFragmentTraits<FArcTestSecondaryItemStoreFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

struct ARCMASSITEMSTEST_API FArcMassItemSignalListener
{
    FArcMassItemSignalListener(UMassSignalSubsystem& InSubsystem, FName InQualifiedSignalName);
    ~FArcMassItemSignalListener();

    FArcMassItemSignalListener(const FArcMassItemSignalListener&) = delete;
    FArcMassItemSignalListener& operator=(const FArcMassItemSignalListener&) = delete;

    struct FReceived { FMassEntityHandle Entity; };
    TArray<FReceived> Received;

    int32 NumReceivedFor(FMassEntityHandle Entity) const;
    bool ReceivedAny() const { return Received.Num() > 0; }
    void Reset() { Received.Reset(); }

private:
    UMassSignalSubsystem* Subsystem = nullptr;
    FName SignalName;
    FDelegateHandle Handle;
};

namespace ArcMassItems::TestHelpers
{
    UArcMassItemAttachmentConfig* BuildAttachmentConfig(
        TArrayView<const FArcMassItemAttachmentSlot> Slots);

    UArcMassItemAttachmentConfig* BuildSingleHandlerConfig(
        FGameplayTag SlotId,
        FArcMassAttachmentHandler_Test*& OutHandlerPtr,
        UScriptStruct* SupportedFragment = nullptr);

    FMassEntityHandle CreateAttachmentEntity(
        FMassEntityManager& EntityManager,
        UArcMassItemAttachmentConfig* Config);

    /** Like CreateAttachmentEntity, but seeds TWO item stores (FArcMassItemStoreFragment +
     *  FArcTestSecondaryItemStoreFragment) and lists both in InterestedStoreTypes. */
    FMassEntityHandle CreateDualStoreAttachmentEntity(
        FMassEntityManager& EntityManager,
        UArcMassItemAttachmentConfig* Config);

    /** Drain the UArcMassItemEventSubsystem queue — equivalent of the post-actor-tick
     *  flush that the subsystem performs in production.  Tests call this after any
     *  item operation to let UArcMassItemAttachmentSubsystem dispatch handlers. */
    void DrainAttachmentEvents(UWorld* World);

    void LinkActorToEntity(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        AActor* Actor);

    FName StoreQualifiedSignal(FName BaseSignalName);

    // Universal marker fragment type used by test fake handlers' SupportedItemFragment.
    // Tests that use FArcMassAttachmentHandler_Test should set its SupportedFragment to
    // this and ensure the test item carries it via WriteWithMarkerFragment.
    UScriptStruct* MarkerFragmentType();

    // Writes a copy of the BaseId record that carries the marker fragment (FArcItemFragment_Tags)
    // and returns its id; the base record is not modified. Invalid id when the base is not indexed
    // or the copy could not be written.
    FPrimaryAssetId WriteWithMarkerFragment(FArcItemTestRecordFiles& Records, const FPrimaryAssetId& BaseId);

    // Process-global log written by FArcItemFragment_TestLifecycleRecorder. Clear in BEFORE_EACH.
    TArray<FArcTestLifecycleEvent>& LifecycleLog();

    // Writes a copy of the BaseId record that carries FArcItemFragment_TestLifecycleRecorder.
    FPrimaryAssetId WriteWithLifecycleRecorder(FArcItemTestRecordFiles& Records, const FPrimaryAssetId& BaseId);

    // Writes a copy of the BaseId record whose scalable set carries FArcScalableFloatItemFragment_Test.
    FPrimaryAssetId WriteWithScalableFragment(FArcItemTestRecordFiles& Records, const FPrimaryAssetId& BaseId);

    // Dispatches a pre-built Mass item replicated command via Arcx::SendServerCommandConfirm
    // and returns the server-side Execute() result via OutExecuteResult (captured through the
    // confirm delegate). Returns the dispatch bool from SendServerCommandConfirm.
    //
    // Entity must already be embedded in Cmd via its parameterised constructor.
    template <typename CommandT>
    bool SendMassItemCommand(UWorld* World, CommandT&& Cmd, bool& OutExecuteResult)
    {
        OutExecuteResult = false;

        if (World == nullptr)
        {
            return false;
        }

        bool bCaptured = false;
        FArcCommandConfirmedDelegate Confirm;
        Confirm.BindLambda([&bCaptured](bool bResult)
        {
            bCaptured = bResult;
        });

        using FCommand = std::remove_reference_t<CommandT>;
        FCommand* Raw = new FCommand(Forward<CommandT>(Cmd));

        const bool bDispatched = Arcx::SendServerCommandConfirm(World, MoveTemp(Confirm), MoveTemp(Raw));
        OutExecuteResult = bCaptured;
        return bDispatched;
    }
}
