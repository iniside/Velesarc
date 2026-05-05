// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Attachments/ArcMassAttachmentHandler.h"
#include "Attachments/ArcMassItemAttachmentFragments.h"
#include "GameplayTagContainer.h"
#include "ArcMassItemTestHelpers.generated.h"

class AActor;
class UArcMassItemAttachmentConfig;
class UArcMassItemAttachmentProcessor;
class UMassSignalSubsystem;
class UWorld;
struct FArcItemAttachmentSlot;
struct FArcItemData;
struct FArcItemId;
struct FArcMassItemAttachmentSlot;
struct FMassEntityManager;

USTRUCT()
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

    UArcMassItemAttachmentProcessor* RegisterAttachmentProcessor(
        UWorld* World,
        FMassEntityManager& EntityManager);

    void RunProcessor(
        UArcMassItemAttachmentProcessor& Processor,
        FMassEntityManager& EntityManager,
        UMassSignalSubsystem& SignalSubsystem,
        TArrayView<const FName> QualifiedSignalNames,
        FMassEntityHandle Entity,
        float DeltaTime = 0.f);

    void LinkActorToEntity(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        AActor* Actor);

    FName StoreQualifiedSignal(FName BaseSignalName);
}
