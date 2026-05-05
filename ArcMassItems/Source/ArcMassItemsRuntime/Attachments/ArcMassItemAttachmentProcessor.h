// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "ArcMassItemAttachmentProcessor.generated.h"

struct FArcMassItemAttachmentConfigFragment;
struct FArcMassItemAttachmentStateFragment;
struct FArcMassItemAttachment;
struct FMassActorFragment;
struct FArcItemId;

UCLASS()
class ARCMASSITEMSRUNTIME_API UArcMassItemAttachmentProcessor : public UMassSignalProcessorBase
{
    GENERATED_BODY()

public:
    UArcMassItemAttachmentProcessor();

protected:
    virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;

private:
    void ProcessSlotChangedForEntity(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        const FArcMassItemAttachmentConfigFragment& ConfigFragment,
        FArcMassItemAttachmentStateFragment& State,
        AActor* Actor);

    void ProcessRemovedForEntity(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        const FArcMassItemAttachmentConfigFragment& ConfigFragment,
        FArcMassItemAttachmentStateFragment& State);

    void ProcessVisualAttachedForEntity(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        const FArcMassItemAttachmentConfigFragment& ConfigFragment,
        FArcMassItemAttachmentStateFragment& State,
        AActor* Actor);

    void ProcessVisualDetachedForEntity(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        const FArcMassItemAttachmentConfigFragment& ConfigFragment,
        FArcMassItemAttachmentStateFragment& State);

    void ProcessAttachmentChangedForEntity(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        const FArcMassItemAttachmentConfigFragment& ConfigFragment,
        FArcMassItemAttachmentStateFragment& State,
        AActor* Actor);

    // Helpers
    void DispatchRemovedHandlers(
        const FArcMassItemAttachmentConfigFragment& ConfigFragment,
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        FArcMassItemAttachmentStateFragment& State,
        const FArcMassItemAttachment& Snapshot);

    void EmitVisualAttached(FMassEntityManager& EM, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);
    void EmitVisualDetached(FMassEntityManager& EM, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);

    // Cached qualified signal names (computed in InitializeInternal).
    FName CachedSlotChangedName;
    FName CachedRemovedName;
    FName CachedVisualAttachedName;
    FName CachedVisualDetachedName;
    FName CachedAttachmentChangedName;

    FMassEntityQuery EntityQuery{*this};
};
