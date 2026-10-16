// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"
#include "Containers/Queue.h"
#include "Engine/World.h"
#include "Items/ArcItemId.h"
#include "Events/ArcItemEventKey.h"
#include "ArcMassItemEventSubsystem.generated.h"

// ---------------------------------------------------------------------------
// Plain-struct queue payloads (not USTRUCTs — worker-thread PODs only)
// ---------------------------------------------------------------------------

struct FArcQueuedItemAdded
{
	FMassEntityHandle    Entity;
	const UScriptStruct* StoreType = nullptr;
	FArcItemId           ItemId;
};

struct FArcQueuedItemRemoved
{
	FMassEntityHandle    Entity;
	const UScriptStruct* StoreType = nullptr;
	FArcItemId           ItemId;
};

struct FArcQueuedItemSlotChanged
{
	FMassEntityHandle    Entity;
	const UScriptStruct* StoreType = nullptr;
	FArcItemId           ItemId;
	FGameplayTag         OldSlot; // invalid tag = was unslotted
	FGameplayTag         NewSlot; // invalid tag = now unslotted
};

struct FArcQueuedItemStacksChanged
{
	FMassEntityHandle    Entity;
	const UScriptStruct* StoreType = nullptr;
	FArcItemId           ItemId;
	int32                StackDelta;
};

struct FArcQueuedItemAttached
{
	FMassEntityHandle    Entity;
	const UScriptStruct* StoreType = nullptr;
	FArcItemId           AttachmentId;
	FGameplayTag         AttachSlot;
};

struct FArcQueuedItemDetached
{
	FMassEntityHandle    Entity;
	const UScriptStruct* StoreType = nullptr;
	FArcItemId           AttachmentId;
};

struct FArcQueuedItemSlotAssigned
{
	FMassEntityHandle    Entity;
	const UScriptStruct* StoreType = nullptr;
	FArcItemId           ItemId;
	FGameplayTag         NewSlot; // always valid
};

struct FArcQueuedItemSlotCleared
{
	FMassEntityHandle    Entity;
	const UScriptStruct* StoreType = nullptr;
	FArcItemId           ItemId;
	FGameplayTag         OldSlot; // always valid
};

struct FArcQueuedItemAttachmentChanged
{
	FMassEntityHandle    Entity;
	const UScriptStruct* StoreType = nullptr;
	FArcItemId           ItemId;
};

// ---------------------------------------------------------------------------
// Subsystem
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASSITEMSRUNTIME_API UArcMassItemEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// -- Typed delegate signatures ------------------------------------------
	// Every global multicast carries the emitting store fragment type as its
	// last parameter so subsystem listeners can filter by store. The per-entity
	// Add*ForEntity API keys on StoreType instead and is unaffected.

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnItemAdded,
		FMassEntityHandle, FArcItemId, const UScriptStruct* /*StoreType*/);

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnItemRemoved,
		FMassEntityHandle, FArcItemId, const UScriptStruct* /*StoreType*/);

	DECLARE_MULTICAST_DELEGATE_FourParams(FOnItemSlotChanged,
		FMassEntityHandle, FArcItemId, FGameplayTag /*NewSlot, invalid = unslotted*/, const UScriptStruct* /*StoreType*/);

	DECLARE_MULTICAST_DELEGATE_FourParams(FOnItemSlotAssigned,
		FMassEntityHandle, FArcItemId, FGameplayTag /*NewSlot — always valid*/, const UScriptStruct* /*StoreType*/);

	DECLARE_MULTICAST_DELEGATE_FourParams(FOnItemSlotCleared,
		FMassEntityHandle, FArcItemId, FGameplayTag /*OldSlot — always valid*/, const UScriptStruct* /*StoreType*/);

	DECLARE_MULTICAST_DELEGATE_FourParams(FOnItemStacksChanged,
		FMassEntityHandle, FArcItemId, int32 /*StackDelta*/, const UScriptStruct* /*StoreType*/);

	DECLARE_MULTICAST_DELEGATE_FourParams(FOnItemAttached,
		FMassEntityHandle, FArcItemId /*AttachmentId*/, FGameplayTag /*AttachSlot*/, const UScriptStruct* /*StoreType*/);

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnItemDetached,
		FMassEntityHandle, FArcItemId /*AttachmentId*/, const UScriptStruct* /*StoreType*/);

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnItemAttachmentChanged,
		FMassEntityHandle, FArcItemId, const UScriptStruct* /*StoreType*/);

	// -- Global multicasts (fired for every drained event) ------------------

	FOnItemAdded               OnItemAdded;
	FOnItemRemoved             OnItemRemoved;
	FOnItemSlotChanged         OnItemSlotChanged;
	FOnItemSlotAssigned        OnItemSlotAssigned;
	FOnItemSlotCleared         OnItemSlotCleared;
	FOnItemStacksChanged       OnItemStacksChanged;
	FOnItemAttached            OnItemAttached;
	FOnItemDetached            OnItemDetached;
	FOnItemAttachmentChanged   OnItemAttachmentChanged;

	// -- Per-entity bind / unbind API ---------------------------------------
	//    Returns a FDelegateHandle the caller must store to unbind.
	//    Call RemoveForEntity to unbind — it searches all per-entity maps.

	FDelegateHandle AddItemAddedForEntity        (FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemAdded::FDelegate Delegate);
	FDelegateHandle AddItemRemovedForEntity      (FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemRemoved::FDelegate Delegate);
	FDelegateHandle AddItemSlotChangedForEntity  (FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemSlotChanged::FDelegate Delegate);
	FDelegateHandle AddItemSlotAssignedForEntity (FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemSlotAssigned::FDelegate Delegate);
	FDelegateHandle AddItemSlotClearedForEntity  (FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemSlotCleared::FDelegate Delegate);
	FDelegateHandle AddItemStacksChangedForEntity(FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemStacksChanged::FDelegate Delegate);
	FDelegateHandle AddItemAttachedForEntity            (FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemAttached::FDelegate Delegate);
	FDelegateHandle AddItemDetachedForEntity            (FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemDetached::FDelegate Delegate);
	FDelegateHandle AddItemAttachmentChangedForEntity   (FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemAttachmentChanged::FDelegate Delegate);

	// -- Per-(entity + slot) bind API ---------------------------------------
	//    Fires when an item enters OR leaves the given SlotId (the slot tag carried
	//    on the SlotChanged event). The callback's NewSlot is the item's destination
	//    slot — invalid when the item left this slot and is now unslotted.
	//    Unbind via RemoveForEntity (it searches the per-slot map too).

	FDelegateHandle AddItemSlotChangedForEntityAndSlot(FMassEntityHandle Entity, const UScriptStruct* StoreType, FGameplayTag SlotId, FOnItemSlotChanged::FDelegate Delegate);

	/** Removes the binding identified by Handle from whichever per-entity or per-slot multicast owns it.
	 *  Walks every map for keys whose Entity matches; prunes empty map entries. */
	void RemoveForEntity(FMassEntityHandle Entity, FDelegateHandle Handle);

	// -- Worker-thread producer API (no delegate touched here) --------------

	void QueueItemAdded        (FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);
	void QueueItemRemoved      (FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);
	void QueueItemSlotChanged  (FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag OldSlot, FGameplayTag NewSlot);
	void QueueItemStacksChanged(FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, int32 StackDelta);
	void QueueItemAttached     (FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId AttachmentId, FGameplayTag AttachSlot);
	void QueueItemDetached     (FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId AttachmentId);

	/** Explicit assign rail — item entered a slot from unslotted. NewSlot must be valid. */
	void QueueItemSlotAssigned     (FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag NewSlot);
	/** Explicit clear rail — item left a slot and is now unslotted. OldSlot must be valid. */
	void QueueItemSlotCleared      (FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag OldSlot);
	/** Attachment-changed rail — fires when socket attachments change without a slot transition. */
	void QueueItemAttachmentChanged(FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);

	// -- USubsystem ---------------------------------------------------------

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Drain all queued events immediately and fire delegates.
	 *  Intended for test code only — production code relies on the automatic
	 *  post-actor-tick drain registered in Initialize(). */
	void DrainQueues();

	/** Called each post-actor-tick to drain queued events onto game-thread delegates.
	 *  Public so ArcEventSubsystemCore::BindPostActorTickDrain can form a member pointer
	 *  from outside this translation unit. */
	void DrainOnPostActorTick(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds);

	/** Remove all per-entity delegate entries for Entity.
	 *  Public so the teardown observer (Step 4) can call it directly. */
	void EvictEntity(FMassEntityHandle Entity);

private:
	// MPSC queues — written on worker threads, drained on game thread
	TQueue<FArcQueuedItemAdded,         EQueueMode::Mpsc> ItemAddedQueue;
	TQueue<FArcQueuedItemRemoved,       EQueueMode::Mpsc> ItemRemovedQueue;
	TQueue<FArcQueuedItemSlotChanged,   EQueueMode::Mpsc> ItemSlotChangedQueue;
	TQueue<FArcQueuedItemStacksChanged, EQueueMode::Mpsc> ItemStacksChangedQueue;
	TQueue<FArcQueuedItemAttached,      EQueueMode::Mpsc> ItemAttachedQueue;
	TQueue<FArcQueuedItemDetached,      EQueueMode::Mpsc> ItemDetachedQueue;

	// TODO(B-or-later): wire UMassObserverProcessor to evict entity entries on destruction.
	// Per-entity dispatch maps — keyed on {Entity, StoreType} (game-thread only).
	TMap<FArcItemEventKey, FOnItemAdded>         PerEntityItemAdded;
	TMap<FArcItemEventKey, FOnItemRemoved>       PerEntityItemRemoved;
	TMap<FArcItemEventKey, FOnItemSlotChanged>   PerEntityItemSlotChanged;
	TMap<FArcItemEventKey, FOnItemSlotAssigned>  PerEntityItemSlotAssigned;
	TMap<FArcItemEventKey, FOnItemSlotCleared>   PerEntityItemSlotCleared;
	TMap<FArcItemEventKey, FOnItemStacksChanged> PerEntityItemStacksChanged;
	TMap<FArcItemEventKey, FOnItemAttached>      PerEntityItemAttached;
	TMap<FArcItemEventKey, FOnItemDetached>      PerEntityItemDetached;

	TQueue<FArcQueuedItemSlotAssigned,     EQueueMode::Mpsc> ItemSlotAssignedQueue;
	TQueue<FArcQueuedItemSlotCleared,      EQueueMode::Mpsc> ItemSlotClearedQueue;
	TQueue<FArcQueuedItemAttachmentChanged,EQueueMode::Mpsc> ItemAttachmentChangedQueue;

	// Per-(entity + store + slot) dispatch map — fired for the matching slot when an
	// item enters it (NewSlot key) or leaves it (OldSlot key).
	TMap<FArcItemSlotEventKey, FOnItemSlotChanged>        PerSlotItemSlotChanged;
	TMap<FArcItemEventKey,     FOnItemAttachmentChanged>  PerEntityItemAttachmentChanged;

	/** Handle returned by FWorldDelegates::OnWorldPostActorTick.Add(...). */
	FDelegateHandle PostActorTickHandle;

};
