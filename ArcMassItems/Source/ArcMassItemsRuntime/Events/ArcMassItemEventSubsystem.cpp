// Copyright Lukasz Baran. All Rights Reserved.

#include "Events/ArcMassItemEventSubsystem.h"
#include "Engine/World.h"
#include "Events/ArcEventSubsystemCore.h"

namespace ArcMassItemEventSubsystem_Internal
{
	/** Helper: try to remove Handle from every entry in Map whose key's Entity matches.
	 *  Returns true if the handle was found and removed in any entry. */
	template <typename TKey, typename TDelegate>
	static bool TryRemoveHandleByEntity(
		TMap<TKey, TDelegate>& Map,
		FMassEntityHandle      Entity,
		FDelegateHandle        Handle)
	{
		TArray<TKey> EmptyKeys;
		bool bFound = false;

		for (typename TMap<TKey, TDelegate>::TIterator It(Map); It; ++It)
		{
			if (!(It.Key().Entity == Entity))
			{
				continue;
			}
			if (It.Value().Remove(Handle))
			{
				bFound = true;
				if (!It.Value().IsBound())
				{
					EmptyKeys.Add(It.Key());
				}
				// A handle can only live in one delegate, so stop searching this map.
				break;
			}
		}

		for (const TKey& Key : EmptyKeys)
		{
			Map.Remove(Key);
		}

		return bFound;
	}

	/** Helper: remove all entries from Map whose key's Entity matches. Two-pass. */
	template <typename TKey, typename TDelegate>
	static void EvictEntityFromMap(TMap<TKey, TDelegate>& Map, FMassEntityHandle Entity)
	{
		TArray<TKey> ToRemove;
		for (const typename TMap<TKey, TDelegate>::ElementType& Pair : Map)
		{
			if (Pair.Key.Entity == Entity)
			{
				ToRemove.Add(Pair.Key);
			}
		}
		for (const TKey& Key : ToRemove)
		{
			Map.Remove(Key);
		}
	}
} // namespace ArcMassItemEventSubsystem_Internal

// ---------------------------------------------------------------------------
// USubsystem lifecycle
// ---------------------------------------------------------------------------

void UArcMassItemEventSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PostActorTickHandle = ArcEventSubsystemCore::BindPostActorTickDrain(this);
}

void UArcMassItemEventSubsystem::Deinitialize()
{
	ArcEventSubsystemCore::UnbindPostActorTickDrain(PostActorTickHandle);

	// Drain queues to discard any pending entries
	{
		FArcQueuedItemAdded Discarded0;
		while (ItemAddedQueue.Dequeue(Discarded0)) {}
	}
	{
		FArcQueuedItemRemoved Discarded1;
		while (ItemRemovedQueue.Dequeue(Discarded1)) {}
	}
	{
		FArcQueuedItemSlotChanged Discarded2;
		while (ItemSlotChangedQueue.Dequeue(Discarded2)) {}
	}
	{
		FArcQueuedItemStacksChanged Discarded3;
		while (ItemStacksChangedQueue.Dequeue(Discarded3)) {}
	}
	{
		FArcQueuedItemAttached Discarded4;
		while (ItemAttachedQueue.Dequeue(Discarded4)) {}
	}
	{
		FArcQueuedItemDetached Discarded5;
		while (ItemDetachedQueue.Dequeue(Discarded5)) {}
	}
	{
		FArcQueuedItemSlotAssigned Discarded7;
		while (ItemSlotAssignedQueue.Dequeue(Discarded7)) {}
	}
	{
		FArcQueuedItemSlotCleared Discarded8;
		while (ItemSlotClearedQueue.Dequeue(Discarded8)) {}
	}
	{
		FArcQueuedItemAttachmentChanged Discarded9;
		while (ItemAttachmentChangedQueue.Dequeue(Discarded9)) {}
	}

	PerEntityItemAdded.Empty();
	PerEntityItemRemoved.Empty();
	PerEntityItemSlotChanged.Empty();
	PerEntityItemSlotAssigned.Empty();
	PerEntityItemSlotCleared.Empty();
	PerEntityItemStacksChanged.Empty();
	PerEntityItemAttached.Empty();
	PerEntityItemDetached.Empty();
	PerEntityItemAttachmentChanged.Empty();
	PerSlotItemSlotChanged.Empty();

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Per-entity bind / unbind
// ---------------------------------------------------------------------------

FDelegateHandle UArcMassItemEventSubsystem::AddItemAddedForEntity(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemAdded::FDelegate Delegate)
{
	check(StoreType != nullptr);
	const FArcItemEventKey Key{Entity, StoreType};
	return PerEntityItemAdded.FindOrAdd(Key).Add(MoveTemp(Delegate));
}

FDelegateHandle UArcMassItemEventSubsystem::AddItemRemovedForEntity(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemRemoved::FDelegate Delegate)
{
	check(StoreType != nullptr);
	const FArcItemEventKey Key{Entity, StoreType};
	return PerEntityItemRemoved.FindOrAdd(Key).Add(MoveTemp(Delegate));
}

FDelegateHandle UArcMassItemEventSubsystem::AddItemSlotChangedForEntity(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemSlotChanged::FDelegate Delegate)
{
	check(StoreType != nullptr);
	const FArcItemEventKey Key{Entity, StoreType};
	return PerEntityItemSlotChanged.FindOrAdd(Key).Add(MoveTemp(Delegate));
}

FDelegateHandle UArcMassItemEventSubsystem::AddItemSlotAssignedForEntity(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemSlotAssigned::FDelegate Delegate)
{
	check(StoreType != nullptr);
	const FArcItemEventKey Key{Entity, StoreType};
	return PerEntityItemSlotAssigned.FindOrAdd(Key).Add(MoveTemp(Delegate));
}

FDelegateHandle UArcMassItemEventSubsystem::AddItemSlotClearedForEntity(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemSlotCleared::FDelegate Delegate)
{
	check(StoreType != nullptr);
	const FArcItemEventKey Key{Entity, StoreType};
	return PerEntityItemSlotCleared.FindOrAdd(Key).Add(MoveTemp(Delegate));
}

FDelegateHandle UArcMassItemEventSubsystem::AddItemStacksChangedForEntity(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemStacksChanged::FDelegate Delegate)
{
	check(StoreType != nullptr);
	const FArcItemEventKey Key{Entity, StoreType};
	return PerEntityItemStacksChanged.FindOrAdd(Key).Add(MoveTemp(Delegate));
}

FDelegateHandle UArcMassItemEventSubsystem::AddItemAttachedForEntity(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemAttached::FDelegate Delegate)
{
	check(StoreType != nullptr);
	const FArcItemEventKey Key{Entity, StoreType};
	return PerEntityItemAttached.FindOrAdd(Key).Add(MoveTemp(Delegate));
}

FDelegateHandle UArcMassItemEventSubsystem::AddItemDetachedForEntity(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemDetached::FDelegate Delegate)
{
	check(StoreType != nullptr);
	const FArcItemEventKey Key{Entity, StoreType};
	return PerEntityItemDetached.FindOrAdd(Key).Add(MoveTemp(Delegate));
}

FDelegateHandle UArcMassItemEventSubsystem::AddItemAttachmentChangedForEntity(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FOnItemAttachmentChanged::FDelegate Delegate)
{
	check(StoreType != nullptr);
	const FArcItemEventKey Key{Entity, StoreType};
	return PerEntityItemAttachmentChanged.FindOrAdd(Key).Add(MoveTemp(Delegate));
}

FDelegateHandle UArcMassItemEventSubsystem::AddItemSlotChangedForEntityAndSlot(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FGameplayTag SlotId, FOnItemSlotChanged::FDelegate Delegate)
{
	check(StoreType != nullptr);
	const FArcItemSlotEventKey Key{Entity, StoreType, SlotId};
	return PerSlotItemSlotChanged.FindOrAdd(Key).Add(MoveTemp(Delegate));
}

void UArcMassItemEventSubsystem::RemoveForEntity(FMassEntityHandle Entity, FDelegateHandle Handle)
{
	using namespace ArcMassItemEventSubsystem_Internal;

	if (TryRemoveHandleByEntity(PerEntityItemAdded,          Entity, Handle)) { return; }
	if (TryRemoveHandleByEntity(PerEntityItemRemoved,        Entity, Handle)) { return; }
	if (TryRemoveHandleByEntity(PerEntityItemSlotChanged,    Entity, Handle)) { return; }
	if (TryRemoveHandleByEntity(PerEntityItemSlotAssigned,   Entity, Handle)) { return; }
	if (TryRemoveHandleByEntity(PerEntityItemSlotCleared,    Entity, Handle)) { return; }
	if (TryRemoveHandleByEntity(PerEntityItemStacksChanged,  Entity, Handle)) { return; }
	if (TryRemoveHandleByEntity(PerEntityItemAttached,             Entity, Handle)) { return; }
	if (TryRemoveHandleByEntity(PerEntityItemDetached,             Entity, Handle)) { return; }
	if (TryRemoveHandleByEntity(PerEntityItemAttachmentChanged,    Entity, Handle)) { return; }
	TryRemoveHandleByEntity(PerSlotItemSlotChanged, Entity, Handle);
}

// ---------------------------------------------------------------------------
// Worker-thread producer API
// ---------------------------------------------------------------------------

void UArcMassItemEventSubsystem::QueueItemAdded(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId)
{
	check(StoreType != nullptr);
	FArcQueuedItemAdded Item;
	Item.Entity    = Entity;
	Item.StoreType = StoreType;
	Item.ItemId    = ItemId;
	ItemAddedQueue.Enqueue(Item);
}

void UArcMassItemEventSubsystem::QueueItemRemoved(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId)
{
	check(StoreType != nullptr);
	FArcQueuedItemRemoved Item;
	Item.Entity    = Entity;
	Item.StoreType = StoreType;
	Item.ItemId    = ItemId;
	ItemRemovedQueue.Enqueue(Item);
}

void UArcMassItemEventSubsystem::QueueItemSlotChanged(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId,
	FGameplayTag OldSlot, FGameplayTag NewSlot)
{
	check(StoreType != nullptr);
	FArcQueuedItemSlotChanged Item;
	Item.Entity    = Entity;
	Item.StoreType = StoreType;
	Item.ItemId    = ItemId;
	Item.OldSlot   = OldSlot;
	Item.NewSlot   = NewSlot;
	ItemSlotChangedQueue.Enqueue(Item);
}

void UArcMassItemEventSubsystem::QueueItemStacksChanged(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, int32 StackDelta)
{
	check(StoreType != nullptr);
	FArcQueuedItemStacksChanged Item;
	Item.Entity     = Entity;
	Item.StoreType  = StoreType;
	Item.ItemId     = ItemId;
	Item.StackDelta = StackDelta;
	ItemStacksChangedQueue.Enqueue(Item);
}

void UArcMassItemEventSubsystem::QueueItemAttached(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId AttachmentId, FGameplayTag AttachSlot)
{
	check(StoreType != nullptr);
	FArcQueuedItemAttached Item;
	Item.Entity       = Entity;
	Item.StoreType    = StoreType;
	Item.AttachmentId = AttachmentId;
	Item.AttachSlot   = AttachSlot;
	ItemAttachedQueue.Enqueue(Item);
}

void UArcMassItemEventSubsystem::QueueItemDetached(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId AttachmentId)
{
	check(StoreType != nullptr);
	FArcQueuedItemDetached Item;
	Item.Entity       = Entity;
	Item.StoreType    = StoreType;
	Item.AttachmentId = AttachmentId;
	ItemDetachedQueue.Enqueue(Item);
}

void UArcMassItemEventSubsystem::QueueItemSlotAssigned(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag NewSlot)
{
	check(StoreType != nullptr);
	FArcQueuedItemSlotAssigned Item;
	Item.Entity    = Entity;
	Item.StoreType = StoreType;
	Item.ItemId    = ItemId;
	Item.NewSlot   = NewSlot;
	ItemSlotAssignedQueue.Enqueue(Item);
}

void UArcMassItemEventSubsystem::QueueItemSlotCleared(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag OldSlot)
{
	check(StoreType != nullptr);
	FArcQueuedItemSlotCleared Item;
	Item.Entity    = Entity;
	Item.StoreType = StoreType;
	Item.ItemId    = ItemId;
	Item.OldSlot   = OldSlot;
	ItemSlotClearedQueue.Enqueue(Item);
}

void UArcMassItemEventSubsystem::QueueItemAttachmentChanged(
	FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId)
{
	check(StoreType != nullptr);
	FArcQueuedItemAttachmentChanged Item;
	Item.Entity    = Entity;
	Item.StoreType = StoreType;
	Item.ItemId    = ItemId;
	ItemAttachmentChangedQueue.Enqueue(Item);
}

// ---------------------------------------------------------------------------
// Game-thread drain
// ---------------------------------------------------------------------------

void UArcMassItemEventSubsystem::DrainQueues()
{
	// -----------------------------------------------------------------------
	// 1. ItemAdded
	// -----------------------------------------------------------------------
	{
		FArcQueuedItemAdded Item;
		while (ItemAddedQueue.Dequeue(Item))
		{
			const FArcItemEventKey Key{Item.Entity, Item.StoreType};

			FOnItemAdded* PerEntity = PerEntityItemAdded.Find(Key);
			if (PerEntity != nullptr)
			{
				PerEntity->Broadcast(Item.Entity, Item.ItemId, Item.StoreType);
			}
			OnItemAdded.Broadcast(Item.Entity, Item.ItemId, Item.StoreType);
		}
	}

	// -----------------------------------------------------------------------
	// 2. ItemRemoved
	// -----------------------------------------------------------------------
	{
		FArcQueuedItemRemoved Item;
		while (ItemRemovedQueue.Dequeue(Item))
		{
			const FArcItemEventKey Key{Item.Entity, Item.StoreType};

			FOnItemRemoved* PerEntity = PerEntityItemRemoved.Find(Key);
			if (PerEntity != nullptr)
			{
				PerEntity->Broadcast(Item.Entity, Item.ItemId, Item.StoreType);
			}
			OnItemRemoved.Broadcast(Item.Entity, Item.ItemId, Item.StoreType);
		}
	}

	// -----------------------------------------------------------------------
	// 3. ItemSlotChanged (per-entity) — MOVE-only (slot A → slot B).
	//    Assigned/Cleared derivation has been removed; those come from the
	//    explicit ItemSlotAssignedQueue / ItemSlotClearedQueue blocks below.
	//    Per-slot dual-fire (NewSlot + OldSlot) is done here for the move case;
	//    the explicit Assigned/Cleared blocks each do their own single per-slot fire.
	// -----------------------------------------------------------------------
	{
		FArcQueuedItemSlotChanged Item;
		while (ItemSlotChangedQueue.Dequeue(Item))
		{
			const FArcItemEventKey Key{Item.Entity, Item.StoreType};

			// Generic SlotChanged fires for any move.
			FOnItemSlotChanged* PerEntityChanged = PerEntityItemSlotChanged.Find(Key);
			if (PerEntityChanged != nullptr)
			{
				PerEntityChanged->Broadcast(Item.Entity, Item.ItemId, Item.NewSlot, Item.StoreType);
			}
			OnItemSlotChanged.Broadcast(Item.Entity, Item.ItemId, Item.NewSlot, Item.StoreType);

			// Per-slot: fire destination slot (item arrived) and source slot (item left) —
			// dual-fire preserves swap behaviour for equipment model.
			if (Item.NewSlot.IsValid())
			{
				const FArcItemSlotEventKey NewSlotKey{Item.Entity, Item.StoreType, Item.NewSlot};
				FOnItemSlotChanged* PerSlotNew = PerSlotItemSlotChanged.Find(NewSlotKey);
				if (PerSlotNew != nullptr)
				{
					PerSlotNew->Broadcast(Item.Entity, Item.ItemId, Item.NewSlot, Item.StoreType);
				}
			}
			if (Item.OldSlot.IsValid() && Item.OldSlot != Item.NewSlot)
			{
				const FArcItemSlotEventKey OldSlotKey{Item.Entity, Item.StoreType, Item.OldSlot};
				FOnItemSlotChanged* PerSlotOld = PerSlotItemSlotChanged.Find(OldSlotKey);
				if (PerSlotOld != nullptr)
				{
					PerSlotOld->Broadcast(Item.Entity, Item.ItemId, Item.NewSlot, Item.StoreType);
				}
			}
		}
	}

	// -----------------------------------------------------------------------
	// 4. ItemStacksChanged
	// -----------------------------------------------------------------------
	{
		FArcQueuedItemStacksChanged Item;
		while (ItemStacksChangedQueue.Dequeue(Item))
		{
			const FArcItemEventKey Key{Item.Entity, Item.StoreType};

			FOnItemStacksChanged* PerEntity = PerEntityItemStacksChanged.Find(Key);
			if (PerEntity != nullptr)
			{
				PerEntity->Broadcast(Item.Entity, Item.ItemId, Item.StackDelta, Item.StoreType);
			}
			OnItemStacksChanged.Broadcast(Item.Entity, Item.ItemId, Item.StackDelta, Item.StoreType);
		}
	}

	// -----------------------------------------------------------------------
	// 5. ItemAttached
	// -----------------------------------------------------------------------
	{
		FArcQueuedItemAttached Item;
		while (ItemAttachedQueue.Dequeue(Item))
		{
			const FArcItemEventKey Key{Item.Entity, Item.StoreType};

			FOnItemAttached* PerEntity = PerEntityItemAttached.Find(Key);
			if (PerEntity != nullptr)
			{
				PerEntity->Broadcast(Item.Entity, Item.AttachmentId, Item.AttachSlot, Item.StoreType);
			}
			OnItemAttached.Broadcast(Item.Entity, Item.AttachmentId, Item.AttachSlot, Item.StoreType);
		}
	}

	// -----------------------------------------------------------------------
	// 6. ItemDetached
	// -----------------------------------------------------------------------
	{
		FArcQueuedItemDetached Item;
		while (ItemDetachedQueue.Dequeue(Item))
		{
			const FArcItemEventKey Key{Item.Entity, Item.StoreType};

			FOnItemDetached* PerEntity = PerEntityItemDetached.Find(Key);
			if (PerEntity != nullptr)
			{
				PerEntity->Broadcast(Item.Entity, Item.AttachmentId, Item.StoreType);
			}
			OnItemDetached.Broadcast(Item.Entity, Item.AttachmentId, Item.StoreType);
		}
	}

	// -----------------------------------------------------------------------
	// 7. ItemSlotAssigned — explicit assign rail (unslotted → slot).
	//    Fires: OnItemSlotAssigned + per-entity Assigned;
	//           generic OnItemSlotChanged + per-entity Changed (store model needs "any change");
	//           per-slot NewSlot key (item arrived in slot).
	// -----------------------------------------------------------------------
	{
		FArcQueuedItemSlotAssigned Item;
		while (ItemSlotAssignedQueue.Dequeue(Item))
		{
			const FArcItemEventKey Key{Item.Entity, Item.StoreType};

			FOnItemSlotAssigned* PerEntityAssigned = PerEntityItemSlotAssigned.Find(Key);
			if (PerEntityAssigned != nullptr)
			{
				PerEntityAssigned->Broadcast(Item.Entity, Item.ItemId, Item.NewSlot, Item.StoreType);
			}
			OnItemSlotAssigned.Broadcast(Item.Entity, Item.ItemId, Item.NewSlot, Item.StoreType);

			FOnItemSlotChanged* PerEntityChanged = PerEntityItemSlotChanged.Find(Key);
			if (PerEntityChanged != nullptr)
			{
				PerEntityChanged->Broadcast(Item.Entity, Item.ItemId, Item.NewSlot, Item.StoreType);
			}
			OnItemSlotChanged.Broadcast(Item.Entity, Item.ItemId, Item.NewSlot, Item.StoreType);

			const FArcItemSlotEventKey NewSlotKey{Item.Entity, Item.StoreType, Item.NewSlot};
			FOnItemSlotChanged* PerSlotNew = PerSlotItemSlotChanged.Find(NewSlotKey);
			if (PerSlotNew != nullptr)
			{
				PerSlotNew->Broadcast(Item.Entity, Item.ItemId, Item.NewSlot, Item.StoreType);
			}
		}
	}

	// -----------------------------------------------------------------------
	// 9. ItemSlotCleared — explicit clear rail (slot → unslotted).
	//    Fires: OnItemSlotCleared + per-entity Cleared (with OldSlot);
	//           generic OnItemSlotChanged + per-entity Changed (invalid NewSlot);
	//           per-slot OldSlot key (item left that slot).
	// -----------------------------------------------------------------------
	{
		FArcQueuedItemSlotCleared Item;
		while (ItemSlotClearedQueue.Dequeue(Item))
		{
			const FArcItemEventKey Key{Item.Entity, Item.StoreType};

			FOnItemSlotCleared* PerEntityCleared = PerEntityItemSlotCleared.Find(Key);
			if (PerEntityCleared != nullptr)
			{
				PerEntityCleared->Broadcast(Item.Entity, Item.ItemId, Item.OldSlot, Item.StoreType);
			}
			OnItemSlotCleared.Broadcast(Item.Entity, Item.ItemId, Item.OldSlot, Item.StoreType);

			FOnItemSlotChanged* PerEntityChanged = PerEntityItemSlotChanged.Find(Key);
			if (PerEntityChanged != nullptr)
			{
				PerEntityChanged->Broadcast(Item.Entity, Item.ItemId, FGameplayTag(), Item.StoreType);
			}
			OnItemSlotChanged.Broadcast(Item.Entity, Item.ItemId, FGameplayTag(), Item.StoreType);

			const FArcItemSlotEventKey OldSlotKey{Item.Entity, Item.StoreType, Item.OldSlot};
			FOnItemSlotChanged* PerSlotOld = PerSlotItemSlotChanged.Find(OldSlotKey);
			if (PerSlotOld != nullptr)
			{
				PerSlotOld->Broadcast(Item.Entity, Item.ItemId, FGameplayTag(), Item.StoreType);
			}
		}
	}

	// -----------------------------------------------------------------------
	// 10. ItemAttachmentChanged — socket attachment change without slot transition.
	//     Fires: OnItemAttachmentChanged + per-entity AttachmentChanged.
	// -----------------------------------------------------------------------
	{
		FArcQueuedItemAttachmentChanged Item;
		while (ItemAttachmentChangedQueue.Dequeue(Item))
		{
			const FArcItemEventKey Key{Item.Entity, Item.StoreType};

			FOnItemAttachmentChanged* PerEntity = PerEntityItemAttachmentChanged.Find(Key);
			if (PerEntity != nullptr)
			{
				PerEntity->Broadcast(Item.Entity, Item.ItemId, Item.StoreType);
			}
			OnItemAttachmentChanged.Broadcast(Item.Entity, Item.ItemId, Item.StoreType);
		}
	}
}

void UArcMassItemEventSubsystem::DrainOnPostActorTick(
	UWorld* InWorld, ELevelTick /*TickType*/, float /*DeltaSeconds*/)
{
	// Guard: post-actor-tick fires for every world in the editor; only drain ours.
	if (InWorld != GetWorld())
	{
		return;
	}

	DrainQueues();
}

// ---------------------------------------------------------------------------
// Entity eviction (called on explicit eviction; B-or-later: observer-driven)
// ---------------------------------------------------------------------------

void UArcMassItemEventSubsystem::EvictEntity(FMassEntityHandle Entity)
{
	using namespace ArcMassItemEventSubsystem_Internal;

	EvictEntityFromMap(PerEntityItemAdded,         Entity);
	EvictEntityFromMap(PerEntityItemRemoved,        Entity);
	EvictEntityFromMap(PerEntityItemSlotChanged,    Entity);
	EvictEntityFromMap(PerEntityItemSlotAssigned,   Entity);
	EvictEntityFromMap(PerEntityItemSlotCleared,    Entity);
	EvictEntityFromMap(PerEntityItemStacksChanged,  Entity);
	EvictEntityFromMap(PerEntityItemAttached,            Entity);
	EvictEntityFromMap(PerEntityItemDetached,            Entity);
	EvictEntityFromMap(PerEntityItemAttachmentChanged,   Entity);
	EvictEntityFromMap(PerSlotItemSlotChanged,           Entity);
}
