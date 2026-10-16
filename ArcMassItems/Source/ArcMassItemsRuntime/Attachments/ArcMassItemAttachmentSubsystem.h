// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemId.h"
#include "ArcMassItemAttachmentSubsystem.generated.h"

struct FArcMassItemAttachmentConfigFragment;
struct FArcMassItemAttachmentStateFragment;
struct FArcMassItemAttachment;
struct FMassEntityManager;
class UArcMassItemEventSubsystem;

/**
 * UArcMassItemAttachmentSubsystem
 *
 * World subsystem that drives FArcMassAttachmentHandler dispatch by consuming
 * the global delegates exposed on UArcMassItemEventSubsystem (post-actor-tick
 * drain).  Replaces the signal-driven UArcMassItemAttachmentProcessor — all
 * Mass API calls are context-free and safe in the post-actor-tick window
 * (IsProcessing()==false).
 */
UCLASS()
class ARCMASSITEMSRUNTIME_API UArcMassItemAttachmentSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Tear down all active attachments for an entity — called by the tag-remove
	 * observer when FArcMassItemAttachmentTag is removed (entity destruction).
	 * Destroys all UObjects in ObjectsAttachedFromItem and clears the state maps.
	 */
	void TeardownEntity(FMassEntityHandle Entity);

private:
	// ----- Lazy EntityManager cache ------------------------------------------

	/** Returns true if EntityManager is valid (fetches it on first call). */
	bool EnsureCached();

	FMassEntityManager* EntityManager = nullptr;
	UArcMassItemEventSubsystem* EventSubsystem = nullptr;

	// ----- Delegate handles (stored for Deinitialize unbind) -----------------

	FDelegateHandle HandleSlotChanged;
	FDelegateHandle HandleItemAttached;
	FDelegateHandle HandleItemDetached;
	FDelegateHandle HandleAttachmentChanged;
	FDelegateHandle HandleItemRemoved;

	// ----- Delegate callbacks ------------------------------------------------

	/** Bound to the generic OnItemSlotChanged rail, which fires for assign, clear,
	 *  and move alike — so one reconcile covers every slot transition. */
	void OnItemSlotChanged(FMassEntityHandle Entity, FArcItemId ItemId, FGameplayTag NewSlot, const UScriptStruct* StoreType);
	void OnItemAttached(FMassEntityHandle Entity, FArcItemId AttachmentId, FGameplayTag AttachSlot, const UScriptStruct* StoreType);
	void OnItemDetached(FMassEntityHandle Entity, FArcItemId AttachmentId, const UScriptStruct* StoreType);
	void OnItemAttachmentChanged(FMassEntityHandle Entity, FArcItemId ItemId, const UScriptStruct* StoreType);
	void OnItemRemoved(FMassEntityHandle Entity, FArcItemId ItemId, const UScriptStruct* StoreType);

	// ----- Core dispatch helpers ---------------------------------------------

	/**
	 * Full reconciliation sweep — ports ProcessSlotChangedForEntity.
	 * Re-reads all InterestedStoreTypes and reconciles ActiveAttachments:
	 *   - stale entries (item gone or slot mismatch) → detach immediately
	 *   - items in valid slots not yet in ActiveAttachments → add+attach immediately
	 */
	void ReconcileEntity(
		FMassEntityHandle Entity,
		const FArcMassItemAttachmentConfigFragment& Config,
		FArcMassItemAttachmentStateFragment& State);

	/**
	 * Evict a single item from ActiveAttachments by ItemId.
	 * Snapshots the entry, removes it, calls DispatchRemovedHandlers +
	 * HandleItemDetach inline.  Returns false if the item was not active.
	 */
	bool EvictActiveItem(
		FMassEntityHandle Entity,
		FArcItemId ItemId,
		const FArcMassItemAttachmentConfigFragment& Config,
		FArcMassItemAttachmentStateFragment& State);

	void DispatchRemovedHandlers(
		const FArcMassItemAttachmentConfigFragment& Config,
		FMassEntityHandle Entity,
		FArcMassItemAttachmentStateFragment& State,
		const FArcMassItemAttachment& Snapshot);
};
