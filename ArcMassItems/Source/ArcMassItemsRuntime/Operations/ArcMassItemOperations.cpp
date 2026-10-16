// Copyright Lukasz Baran. All Rights Reserved.

#include "Operations/ArcMassItemOperations.h"
#include "Operations/ArcMassItemTransaction.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Fragments/ArcMassItemEventFragment.h"
#include "Signals/ArcMassItemSignals.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemStackMethod.h"
#include "Items/ArcItemInstance.h"
#include "Items/ArcItemScalableFloat.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Items/Fragments/ArcItemFragment_SocketSlots.h"
#include "Equipment/Fragments/ArcItemFragment_ItemVisualAttachment.h"
#include "Attachments/ArcMassItemAttachmentFragments.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "MassSignalSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMassItemOps, Log, All);

namespace ArcMassItems
{

namespace Private
{
	FArcMassItemStoreFragment* GetStore(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType)
	{
		checkf(StoreType && StoreType->IsChildOf(FArcMassItemStoreFragment::StaticStruct()),
			TEXT("StoreType '%s' must derive from FArcMassItemStoreFragment"),
			StoreType ? *StoreType->GetName() : TEXT("null"));
		FStructView View = EntityManager.GetMutableSparseElementDataForEntity(StoreType, Entity);
		return View.GetPtr<FArcMassItemStoreFragment>();
	}

}

FName GetSignalForEventType(EArcMassItemEventType Type)
{
	switch (Type)
	{
	case EArcMassItemEventType::Added:             return UE::ArcMassItems::Signals::ItemAdded;
	case EArcMassItemEventType::Removed:           return UE::ArcMassItems::Signals::ItemRemoved;
	case EArcMassItemEventType::SlotAssigned:      return UE::ArcMassItems::Signals::ItemSlotAssigned;
	case EArcMassItemEventType::SlotCleared:       return UE::ArcMassItems::Signals::ItemSlotCleared;
	case EArcMassItemEventType::SlotChanged:       return UE::ArcMassItems::Signals::ItemSlotChanged;
	case EArcMassItemEventType::StacksChanged:     return UE::ArcMassItems::Signals::ItemStacksChanged;
	case EArcMassItemEventType::Attached:          return UE::ArcMassItems::Signals::ItemAttached;
	case EArcMassItemEventType::Detached:          return UE::ArcMassItems::Signals::ItemDetached;
	case EArcMassItemEventType::AttachmentChanged: return UE::ArcMassItems::Signals::ItemAttachmentChanged;
	default: return FName();
	}
}

// --- Mass-safe slot notification helpers ---

namespace Private
{
	const FGameplayTagContainer* FindGrantedTags(const FArcItemData* ItemData)
	{
		const FArcItemDefinitionData* Definition = ItemData->GetItemDefinition();
		const FArcItemFragment_Tags* TagsFragment = Definition ? Definition->FindFragment<FArcItemFragment_Tags>() : nullptr;
		return TagsFragment ? &TagsFragment->GrantedTags : nullptr;
	}

	void NotifySingleItemSlotAdded(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcItemData* ItemData, const FGameplayTag& SlotId)
	{
		ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(ItemData
			, [SlotId](const FArcItemData* Data, const FArcItemFragment_ItemInstanceBase* Fragment)
			{
				Fragment->OnItemAddedToSlot(Data, SlotId);
			});

		if (const FGameplayTagContainer* GrantedTags = FindGrantedTags(ItemData))
		{
			ArcAbilities::AddOwnedTags(EntityManager, Entity, *GrantedTags);
		}
	}

	void NotifySingleItemSlotRemoved(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcItemData* ItemData, const FGameplayTag& SlotId)
	{
		if (const FGameplayTagContainer* GrantedTags = FindGrantedTags(ItemData))
		{
			ArcAbilities::RemoveOwnedTags(EntityManager, Entity, *GrantedTags);
		}

		ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(ItemData
			, [SlotId](const FArcItemData* Data, const FArcItemFragment_ItemInstanceBase* Fragment)
			{
				Fragment->OnItemRemovedFromSlot(Data, SlotId);
			});
	}
}

void NotifyFragmentsSlotAddedMass(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemData* ItemData, const FGameplayTag& SlotId)
{
	Private::NotifySingleItemSlotAdded(EntityManager, Entity, ItemData, SlotId);

	TArray<const FArcItemData*> Attached = GetAttachedItems(EntityManager, Entity, StoreType, ItemData->GetItemId());
	for (const FArcItemData* AttachedItem : Attached)
	{
		Private::NotifySingleItemSlotAdded(EntityManager, Entity, AttachedItem, AttachedItem->GetAttachSlot());
	}
}

void NotifyFragmentsSlotRemovedMass(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemData* ItemData, const FGameplayTag& SlotId)
{
	TArray<const FArcItemData*> Attached = GetAttachedItems(EntityManager, Entity, StoreType, ItemData->GetItemId());
	for (const FArcItemData* AttachedItem : Attached)
	{
		Private::NotifySingleItemSlotRemoved(EntityManager, Entity, AttachedItem, AttachedItem->GetAttachSlot());
	}

	Private::NotifySingleItemSlotRemoved(EntityManager, Entity, ItemData, SlotId);
}

namespace Private
{
	void ActivateItemData(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType,
		FArcMassItemStoreFragment& Store, FArcMassReplicatedItem& Item, FArcItemData& Data)
	{
		if (Data.IsActive() || !Data.GetSlotId().IsValid())
		{
			return;
		}

		Data.bIsActive = true;
		Store.ReplicatedItems.MarkItemDirty(Item);
		NotifyFragmentsSlotAddedMass(EntityManager, Entity, StoreType, &Data, Data.GetSlotId());
	}

	void DeactivateItemData(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType,
		FArcMassItemStoreFragment& Store, FArcMassReplicatedItem& Item, FArcItemData& Data)
	{
		if (!Data.IsActive())
		{
			return;
		}

		Data.bIsActive = false;
		Store.ReplicatedItems.MarkItemDirty(Item);
		NotifyFragmentsSlotRemovedMass(EntityManager, Entity, StoreType, &Data, Data.GetSlotId());
	}
}

bool ActivateItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return false;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	FArcItemData* Data = Item ? Item->ToItem() : nullptr;
	if (!Data || !Data->GetSlotId().IsValid())
	{
		return false;
	}

	Private::ActivateItemData(EntityManager, Entity, StoreType, *Store, *Item, *Data);
	return true;
}

bool DeactivateItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return false;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	FArcItemData* Data = Item ? Item->ToItem() : nullptr;
	if (!Data)
	{
		return false;
	}

	Private::DeactivateItemData(EntityManager, Entity, StoreType, *Store, *Item, *Data);
	return true;
}

namespace Private
{
	// Shared tail of AddItem: place the new item in the store, initialize it, create
	// default socket children, and emit the Added event. Spec.Amount must already
	// reflect the stacks this new item should carry.
	FArcItemId PlaceNewItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType,
		FArcMassItemStoreFragment* Store, FArcMassItemTransaction& Tx, const FArcItemSpec& Spec, uint16 Stacks)
	{
		FArcMassReplicatedItem NewItem(Spec);
		FArcItemData* Data = NewItem.ToItem();
		FArcItemId ItemId = Spec.ItemId.IsValid() ? Spec.ItemId : FArcItemId::Generate();
		Data->ItemId = ItemId;
		Data->SetStacks(Stacks);

		Store->ReplicatedItems.AddItem(MoveTemp(NewItem));
		// OnItemInitialize fragments read the owning entity, so it is set before InitializeMass.
		Data->MassEntityHandle = Entity;
		Data->InitializeMass(EntityManager.GetWorld());
		Data->OnItemAdded();

		if (const FArcItemFragment_SocketSlots* SocketSlots = ArcItemsHelper::GetFragment<FArcItemFragment_SocketSlots>(Data))
		{
			for (const FArcSocketSlot& Slot : SocketSlots->GetSocketSlots())
			{
				if (Slot.DefaultSocketItemDefinition.IsValid())
				{
					FArcItemSpec ChildSpec = FArcItemSpec::NewItem(Slot.DefaultSocketItemDefinition, 1, 1);
					FArcItemId ChildId = AddItem(EntityManager, Entity, StoreType, ChildSpec);
					if (ChildId.IsValid())
					{
						AttachItem(EntityManager, Entity, StoreType, ItemId, ChildId, Slot.SlotId);
					}
				}
			}
		}

		FArcMassItemEvent Event;
		Event.ItemId = ItemId;
		Event.Type = EArcMassItemEventType::Added;
		Tx.Emit(MoveTemp(Event));

		return ItemId;
	}
}

FArcItemId AddItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FArcItemSpec& Spec)
{
	FArcMassItemTransaction Tx(EntityManager, Entity, StoreType);

	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return FArcItemId();
	}

	const FArcItemDefinitionData* Def = Spec.GetItemDefinition();

	// Apply stacking logic from the item definition's stack method
	if (Def)
	{
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();
		if (StackMethod)
		{
			if (!StackMethod->CanAddMass(EntityManager, Entity, StoreType, Spec))
			{
				return FArcItemId::InvalidId;
			}

			if (StackMethod->CanStackMass(EntityManager, Entity, StoreType, Spec))
			{
				uint16 NewStacks = 0;
				uint16 RemainingStacks = 0;
				FArcItemId ExistingId = StackMethod->StackCheckMass(EntityManager, Entity, StoreType, Spec, NewStacks, RemainingStacks);

				UE_LOGF(LogArcMassItemOps, Log,"AddItem stack path: Def=%ls Spec.Amount=%d -> Existing=%ls NewStacks=%d Remaining=%d",
					*Spec.GetItemDefinitionId().ToString(), static_cast<int32>(Spec.Amount),
					ExistingId.IsValid() ? *ExistingId.ToString() : TEXT("none"),
					static_cast<int32>(NewStacks), static_cast<int32>(RemainingStacks));

				if (ExistingId.IsValid())
				{
					FArcMassReplicatedItem* ExistingItem = Store->ReplicatedItems.FindById(ExistingId);
					if (ExistingItem)
					{
						FArcItemData* ExistingData = ExistingItem->ToItem();
						if (ExistingData)
						{
							const uint16 OldStacks = ExistingData->GetStacks();
							ExistingData->SetStacks(NewStacks);
							Store->ReplicatedItems.MarkItemDirty(*ExistingItem);

							FArcMassItemEvent Event;
							Event.ItemId = ExistingId;
							Event.Type = EArcMassItemEventType::StacksChanged;
							Event.StackDelta = static_cast<int32>(NewStacks) - static_cast<int32>(OldStacks);
							Tx.Emit(MoveTemp(Event));
						}
					}

					if (RemainingStacks == 0)
					{
						return ExistingId;
					}
				}

				// Overflow: remaining stacks become a new item
				if (RemainingStacks > 0)
				{
					FArcItemSpec OverflowSpec = Spec;
					OverflowSpec.Amount = RemainingStacks;

					return Private::PlaceNewItem(EntityManager, Entity, StoreType, Store, Tx, OverflowSpec, RemainingStacks);
				}
			}
		}
	}

	// Create new item (no stacking or stacking not applicable)
	UE_LOGF(LogArcMassItemOps, Log,"AddItem fresh item: Def=%ls Spec.Amount=%d (HasStackMethod=%d)",
		*Spec.GetItemDefinitionId().ToString(), static_cast<int32>(Spec.Amount),
		Def && Def->GetStackMethod<FArcItemStackMethod>() ? 1 : 0);
	return Private::PlaceNewItem(EntityManager, Entity, StoreType, Store, Tx, Spec, Spec.Amount);
}

bool RemoveItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, int32 StacksToRemove)
{
	FArcMassItemTransaction Tx(EntityManager, Entity, StoreType);

	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return false;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	if (!Item)
	{
		return false;
	}

	FArcItemData* Data = Item->ToItem();
	if (!Data)
	{
		return false;
	}

	if (StacksToRemove > 0 && Data->GetStacks() > static_cast<uint16>(StacksToRemove))
	{
		Data->SetStacks(Data->GetStacks() - static_cast<uint16>(StacksToRemove));
		Store->ReplicatedItems.MarkItemDirty(*Item);

		FArcMassItemEvent Event;
		Event.ItemId = ItemId;
		Event.Type = EArcMassItemEventType::StacksChanged;
		Event.StackDelta = -StacksToRemove;
		Tx.Emit(MoveTemp(Event));
		return true;
	}

	// Full removal: delegate to DestroyItem so every removal path shares the same
	// lifecycle — OnPreRemove, detach-from-owner, slot clearing, recursive teardown,
	// and a single Removed event.
	return DestroyItem(EntityManager, Entity, StoreType, ItemId);
}

bool AddItemToSharedSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag SlotTag)
{
	FArcMassItemTransaction Tx(EntityManager, Entity, StoreType);

	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return false;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	if (!Item)
	{
		UE_LOGF(LogArcMassItemOps, Warning, "AddItemToSharedSlot: item %ls not in store", *ItemId.ToString());
		return false;
	}

	FArcItemData* Data = Item->ToItem();
	if (!Data)
	{
		return false;
	}

	const FGameplayTag PreviousSlot = Data->GetSlotId();

	Private::DeactivateItemData(EntityManager, Entity, StoreType, *Store, *Item, *Data);

	Data->Slot = SlotTag;
	Store->ReplicatedItems.MarkItemDirty(*Item);

	if (!QuickBar::IsItemActivationDeferred(EntityManager, Entity, StoreType, SlotTag))
	{
		Private::ActivateItemData(EntityManager, Entity, StoreType, *Store, *Item, *Data);
	}

	FArcMassItemEvent Event;
	Event.ItemId = ItemId;
	Event.Type = PreviousSlot.IsValid() ? EArcMassItemEventType::SlotChanged : EArcMassItemEventType::SlotAssigned;
	Event.OldSlotTag = PreviousSlot;
	Event.SlotTag = SlotTag;
	Tx.Emit(MoveTemp(Event));

	return true;
}

bool AddItemToSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag SlotTag)
{
	const FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return false;
	}

	// Check slot occupancy by scanning item data (SlottedItems map removed)
	for (const FArcMassReplicatedItem& ExistingItem : Store->ReplicatedItems.Items)
	{
		const FArcItemData* ExistingData = ExistingItem.ToItem();
		if (ExistingData && ExistingData->GetSlotId() == SlotTag)
		{
			UE_LOGF(LogArcMassItemOps, Warning, "AddItemToSlot: slot %ls already holds %ls (%ls); rejected %ls",
				*SlotTag.ToString(), *ExistingData->GetItemDefinitionId().ToString(),
				*ExistingData->GetItemId().ToString(), *ItemId.ToString());
			return false;
		}
	}

	return AddItemToSharedSlot(EntityManager, Entity, StoreType, ItemId, SlotTag);
}

bool RemoveItemFromSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId)
{
	FArcMassItemTransaction Tx(EntityManager, Entity, StoreType);

	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return false;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	if (!Item)
	{
		return false;
	}

	FArcItemData* Data = Item->ToItem();
	if (!Data)
	{
		return false;
	}

	FGameplayTag CurrentSlot = Data->GetSlotId();
	if (!CurrentSlot.IsValid())
	{
		return false;
	}

	Private::DeactivateItemData(EntityManager, Entity, StoreType, *Store, *Item, *Data);

	Data->Slot = FGameplayTag();
	Store->ReplicatedItems.MarkItemDirty(*Item);

	FArcMassItemEvent Event;
	Event.ItemId = ItemId;
	Event.Type = EArcMassItemEventType::SlotCleared;
	Event.OldSlotTag = CurrentSlot;
	// SlotTag stays default (invalid) — item is now unslotted
	Tx.Emit(MoveTemp(Event));

	return true;
}

bool ModifyStacks(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, int32 Delta)
{
	FArcMassItemTransaction Tx(EntityManager, Entity, StoreType);

	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return false;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	if (!Item)
	{
		return false;
	}

	FArcItemData* Data = Item->ToItem();
	if (!Data)
	{
		return false;
	}

	int32 NewStacks = static_cast<int32>(Data->GetStacks()) + Delta;
	if (NewStacks < 0)
	{
		return false;
	}

	Data->SetStacks(static_cast<uint16>(NewStacks));
	Store->ReplicatedItems.MarkItemDirty(*Item);

	FArcMassItemEvent Event;
	Event.ItemId = ItemId;
	Event.Type = EArcMassItemEventType::StacksChanged;
	Event.StackDelta = Delta;
	Tx.Emit(MoveTemp(Event));

	return true;
}

// --- Attachment operations ---

bool AttachItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId OwnerId, FArcItemId AttachmentId, FGameplayTag AttachSlot)
{
	// Reject invalid ids and self-attach before any work.
	if (!OwnerId.IsValid() || !AttachmentId.IsValid())
	{
		return false;
	}
	if (OwnerId == AttachmentId)
	{
		return false;
	}

	FArcMassItemTransaction Tx(EntityManager, Entity, StoreType);

	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return false;
	}

	FArcMassReplicatedItem* OwnerItem = Store->ReplicatedItems.FindById(OwnerId);
	FArcMassReplicatedItem* AttachmentItem = Store->ReplicatedItems.FindById(AttachmentId);
	if (!OwnerItem || !AttachmentItem)
	{
		return false;
	}

	FArcItemData* OwnerData = OwnerItem->ToItem();
	FArcItemData* AttachmentData = AttachmentItem->ToItem();
	if (!OwnerData || !AttachmentData)
	{
		return false;
	}

	// Reject if attachment already has an owner.
	if (AttachmentData->OwnerId.IsValid())
	{
		return false;
	}

	// Reject if the requested attach slot is already occupied.
	if (AttachSlot.IsValid() && FindAttachedItemOnSlot(EntityManager, Entity, StoreType, OwnerId, AttachSlot) != nullptr)
	{
		return false;
	}

	// Reject if attaching would create an ownership cycle.
	// Walk the owner's ancestor chain; if AttachmentId appears, attaching would create a cycle.
	{
		TSet<FArcItemId> Visited;
		FArcItemId WalkId = OwnerData->OwnerId;
		while (WalkId.IsValid())
		{
			if (WalkId == AttachmentId)
			{
				return false;
			}
			if (Visited.Contains(WalkId))
			{
				// Pre-existing broken cycle — stop walking.
				break;
			}
			Visited.Add(WalkId);
			FArcMassReplicatedItem* WalkItem = Store->ReplicatedItems.FindById(WalkId);
			if (!WalkItem || !WalkItem->ToItem())
			{
				break;
			}
			WalkId = WalkItem->ToItem()->OwnerId;
		}
	}

	AttachmentData->OwnerId = OwnerId;
	AttachmentData->AttachedToSlot = AttachSlot;
	OwnerData->AttachedItems.Add(AttachmentId);

	// Hierarchy cache maintenance — FArcItemData heap blocks are stable across
	// store-array mutation, so caching raw pointers is safe (cycles rejected above).
	AttachmentData->OwnerItemCachedPtr = OwnerData;
	OwnerData->ItemsInSockets.Add(AttachmentData);
	OwnerData->RebuildScalableFloatFragments();

	Store->ReplicatedItems.MarkItemDirty(*OwnerItem);
	Store->ReplicatedItems.MarkItemDirty(*AttachmentItem);

	// Fragment lifecycle callbacks on the attachment's fragments.
	ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(AttachmentData
		, [OwnerData](const FArcItemData* AttachData, const FArcItemFragment_ItemInstanceBase* Fragment)
		{
			Fragment->OnItemAttachedTo(AttachData, OwnerData);
		});

	if (OwnerData->IsActive())
	{
		Private::NotifySingleItemSlotAdded(EntityManager, Entity, AttachmentData, AttachSlot);
	}

	FArcMassItemEvent Event;
	Event.ItemId = AttachmentId;
	Event.Type = EArcMassItemEventType::Attached;
	Event.SlotTag = AttachSlot;
	Tx.Emit(MoveTemp(Event));

	return true;
}

bool DetachItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId AttachmentId)
{
	FArcMassItemTransaction Tx(EntityManager, Entity, StoreType);

	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return false;
	}

	FArcMassReplicatedItem* AttachmentItem = Store->ReplicatedItems.FindById(AttachmentId);
	if (!AttachmentItem)
	{
		return false;
	}

	FArcItemData* AttachmentData = AttachmentItem->ToItem();
	if (!AttachmentData)
	{
		return false;
	}

	FArcItemId OwnerId = AttachmentData->OwnerId;
	FArcMassReplicatedItem* OwnerItem = Store->ReplicatedItems.FindById(OwnerId);
	if (OwnerItem)
	{
		FArcItemData* OwnerData = OwnerItem->ToItem();
		if (OwnerData)
		{
			if (OwnerData->IsActive())
			{
				Private::NotifySingleItemSlotRemoved(EntityManager, Entity, AttachmentData, AttachmentData->GetAttachSlot());
			}

			OwnerData->AttachedItems.Remove(AttachmentId);

			// Hierarchy cache cleanup mirrors AttachItem.
			OwnerData->ItemsInSockets.Remove(AttachmentData);
			OwnerData->RebuildScalableFloatFragments();

			ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(AttachmentData
				, [OwnerData](const FArcItemData* AttachData, const FArcItemFragment_ItemInstanceBase* Fragment)
				{
					Fragment->OnItemDetachedFrom(AttachData, OwnerData);
				});

			Store->ReplicatedItems.MarkItemDirty(*OwnerItem);
		}
	}

	AttachmentData->OwnerItemCachedPtr = nullptr;

	AttachmentData->OldOwnerId = AttachmentData->OwnerId;
	AttachmentData->OwnerId = FArcItemId();
	AttachmentData->OldAttachedToSlot = AttachmentData->AttachedToSlot;
	AttachmentData->AttachedToSlot = FGameplayTag();

	Store->ReplicatedItems.MarkItemDirty(*AttachmentItem);

	FArcMassItemEvent Event;
	Event.ItemId = AttachmentId;
	Event.Type = EArcMassItemEventType::Detached;
	Tx.Emit(MoveTemp(Event));

	return true;
}

// --- Instance data operations ---

bool SetItemInstances(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, const FArcItemInstanceArray& Instances)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return false;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	if (!Item)
	{
		return false;
	}

	FArcItemData* Data = Item->ToItem();
	if (!Data)
	{
		return false;
	}

	Data->SetItemInstances(Instances);
	Store->ReplicatedItems.MarkItemDirty(*Item);

	return true;
}

const FArcItemInstanceArray* GetItemInstances(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return nullptr;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	if (!Item)
	{
		return nullptr;
	}

	FArcItemData* Data = Item->ToItem();
	if (!Data)
	{
		return nullptr;
	}

	return &Data->ItemInstances;
}

// --- Scalable value ---

float GetScalableValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, const FArcScalableCurveFloat& ScalableFloat)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return 0.f;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	if (!Item)
	{
		return 0.f;
	}

	FArcItemData* Data = Item->ToItem();
	if (!Data)
	{
		return 0.f;
	}

	const UScriptStruct* OwnerType = ScalableFloat.GetOwnerType();
	if (!OwnerType)
	{
		return 0.f;
	}

	const uint8* FragmentMemory = Data->FindSpecFragment(const_cast<UScriptStruct*>(OwnerType));
	if (!FragmentMemory)
	{
		return 0.f;
	}

	const FArcScalableFloatItemFragment* Fragment = reinterpret_cast<const FArcScalableFloatItemFragment*>(FragmentMemory);
	return ScalableFloat.GetValue(Fragment, Data->GetLevel());
}

// --- Level and tags ---

bool SetLevel(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, uint8 NewLevel)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return false;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	if (!Item)
	{
		return false;
	}

	FArcItemData* Data = Item->ToItem();
	if (!Data)
	{
		return false;
	}

	Data->Level = NewLevel;
	Store->ReplicatedItems.MarkItemDirty(*Item);

	return true;
}

bool AddDynamicTag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag Tag)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return false;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	if (!Item)
	{
		return false;
	}

	FArcItemData* Data = Item->ToItem();
	if (!Data)
	{
		return false;
	}

	Data->DynamicTags.AddTag(Tag);
	Data->IncrementVersion();
	Store->ReplicatedItems.MarkItemDirty(*Item);

	return true;
}

bool RemoveDynamicTag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag Tag)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return false;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	if (!Item)
	{
		return false;
	}

	FArcItemData* Data = Item->ToItem();
	if (!Data)
	{
		return false;
	}

	Data->DynamicTags.RemoveTag(Tag);
	Data->IncrementVersion();
	Store->ReplicatedItems.MarkItemDirty(*Item);

	return true;
}

// --- Query operations ---

const FArcItemData* FindItemById(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return nullptr;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	return Item ? Item->ToItem() : nullptr;
}

const FArcItemData* FindItemByDefinition(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FPrimaryAssetId& DefinitionId)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return nullptr;
	}

	for (FArcMassReplicatedItem& Item : Store->ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (Data && Data->GetItemDefinitionId() == DefinitionId)
		{
			return Data;
		}
	}

	return nullptr;
}

TArray<const FArcItemData*> FindItemsByDefinition(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FPrimaryAssetId& DefinitionId)
{
	TArray<const FArcItemData*> Result;

	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return Result;
	}

	for (FArcMassReplicatedItem& Item : Store->ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (Data && Data->GetItemDefinitionId() == DefinitionId)
		{
			Result.Add(Data);
		}
	}

	return Result;
}

TArray<const FArcItemData*> FindItemsByTag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FGameplayTagContainer& Tags)
{
	TArray<const FArcItemData*> Result;

	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return Result;
	}

	for (FArcMassReplicatedItem& Item : Store->ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (!Data)
		{
			continue;
		}

		if (Data->DynamicTags.HasAll(Tags))
		{
			Result.Add(Data);
		}
	}

	return Result;
}

// --- Transfer operations ---

bool MoveItem(FMassEntityManager& EntityManager, FMassEntityHandle FromEntity, FMassEntityHandle ToEntity, FArcItemId ItemId, const UScriptStruct* FromStoreType, const UScriptStruct* ToStoreType)
{
	FArcMassItemTransaction FromTx(EntityManager, FromEntity, FromStoreType);
	FArcMassItemTransaction ToTx(EntityManager, ToEntity, ToStoreType);

	FArcMassItemStoreFragment* FromStore = Private::GetStore(EntityManager, FromEntity, FromStoreType);
	FArcMassItemStoreFragment* ToStore = Private::GetStore(EntityManager, ToEntity, ToStoreType);
	if (!FromStore || !ToStore)
	{
		return false;
	}

	// If the root item has an external owner in the source store, detach it first so the
	// owner's AttachedItems and ItemsInSockets don't hold a stale id after the move.
	// Transaction nesting on (FromEntity, FromStoreType) is safe — inner Emit forwards to the outer FromTx.
	{
		FArcMassReplicatedItem* RootItem = FromStore->ReplicatedItems.FindById(ItemId);
		if (RootItem)
		{
			const FArcItemData* RootData = RootItem->ToItem();
			if (RootData && RootData->GetOwnerId().IsValid())
			{
				DetachItem(EntityManager, FromEntity, FromStoreType, ItemId);
			}
		}
	}

	{
		FArcMassReplicatedItem* RootItem = FromStore->ReplicatedItems.FindById(ItemId);
		FArcItemData* RootData = RootItem ? RootItem->ToItem() : nullptr;
		if (RootData)
		{
			Private::DeactivateItemData(EntityManager, FromEntity, FromStoreType, *FromStore, *RootItem, *RootData);
		}
	}

	// Collect all descendants recursively before any mutation
	TArray<FArcItemId> Descendants;
	TSet<FArcItemId> Visited;
	TArray<FArcItemId> Pending;
	Pending.Add(ItemId);

	while (Pending.Num() > 0)
	{
		FArcItemId CurrentId = Pending.Pop();
		FArcMassReplicatedItem* CurrentItem = FromStore->ReplicatedItems.FindById(CurrentId);
		if (!CurrentItem)
		{
			continue;
		}
		const FArcItemData* CurrentData = CurrentItem->ToItem();
		if (!CurrentData)
		{
			continue;
		}
		for (const FArcItemId& AttachId : CurrentData->GetAttachedItems())
		{
			if (!Visited.Contains(AttachId))
			{
				Visited.Add(AttachId);
				Descendants.Add(AttachId);
				Pending.Add(AttachId);
			}
		}
	}

	// Move descendants first, then the parent
	for (const FArcItemId& DescendantId : Descendants)
	{
		int32 DescIndex = FromStore->ReplicatedItems.FindIndexById(DescendantId);
		if (DescIndex == INDEX_NONE)
		{
			continue;
		}

		FArcMassReplicatedItem MovedItem = MoveTemp(FromStore->ReplicatedItems.Items[DescIndex]);
		FArcItemData* DescData = MovedItem.ToItem();
		FGameplayTag DescPriorSlot;
		if (DescData)
		{
			DescPriorSlot = DescData->GetSlotId();
			DescData->Slot = FGameplayTag();
		}

		FromStore->ReplicatedItems.RemoveItemAt(DescIndex);
		ToStore->ReplicatedItems.AddItem(MoveTemp(MovedItem));
		// FArcItemData heap blocks survive the array move — pointers captured pre-move stay valid.
		if (DescData)
		{
			DescData->MassEntityHandle = ToEntity;
		}

		FArcMassItemEvent RemovedEvent;
		RemovedEvent.ItemId = DescendantId;
		RemovedEvent.Type = EArcMassItemEventType::Removed;
		FromTx.Emit(MoveTemp(RemovedEvent));

		FArcMassItemEvent AddedEvent;
		AddedEvent.ItemId = DescendantId;
		AddedEvent.Type = EArcMassItemEventType::Added;
		ToTx.Emit(MoveTemp(AddedEvent));

		if (DescPriorSlot.IsValid())
		{
			FArcMassItemEvent SlotClearedEvent;
			SlotClearedEvent.ItemId = DescendantId;
			SlotClearedEvent.Type = EArcMassItemEventType::SlotCleared;
			SlotClearedEvent.OldSlotTag = DescPriorSlot;
			FromTx.Emit(MoveTemp(SlotClearedEvent));
		}
	}

	// Move the parent item
	int32 SourceIndex = FromStore->ReplicatedItems.FindIndexById(ItemId);
	if (SourceIndex == INDEX_NONE)
	{
		return false;
	}

	FArcMassReplicatedItem MovedItem = MoveTemp(FromStore->ReplicatedItems.Items[SourceIndex]);
	FArcItemData* Data = MovedItem.ToItem();

	FGameplayTag PriorSlot;
	if (Data)
	{
		PriorSlot = Data->GetSlotId();
		Data->Slot = FGameplayTag();
	}

	FromStore->ReplicatedItems.RemoveItemAt(SourceIndex);
	ToStore->ReplicatedItems.AddItem(MoveTemp(MovedItem));
	// FArcItemData heap blocks survive the array move — pointers captured pre-move stay valid.
	if (Data)
	{
		Data->MassEntityHandle = ToEntity;
	}

	FArcMassItemEvent RemovedEvent;
	RemovedEvent.ItemId = ItemId;
	RemovedEvent.Type = EArcMassItemEventType::Removed;
	FromTx.Emit(MoveTemp(RemovedEvent));

	FArcMassItemEvent AddedEvent;
	AddedEvent.ItemId = ItemId;
	AddedEvent.Type = EArcMassItemEventType::Added;
	ToTx.Emit(MoveTemp(AddedEvent));

	if (PriorSlot.IsValid())
	{
		FArcMassItemEvent SlotClearedEvent;
		SlotClearedEvent.ItemId = ItemId;
		SlotClearedEvent.Type = EArcMassItemEventType::SlotCleared;
		SlotClearedEvent.OldSlotTag = PriorSlot;
		FromTx.Emit(MoveTemp(SlotClearedEvent));
	}

	return true;
}

void InitializeItemData(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	if (!Item)
	{
		return;
	}

	FArcItemData* Data = Item->ToItem();
	if (!Data)
	{
		return;
	}

	Data->MassEntityHandle = Entity;
	Data->InitializeMass(EntityManager.GetWorld());
	Data->OnItemAdded();
}

// --- Query: definition-level tag fragment search ---

const FArcItemData* FindItemByDefinitionTags(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FGameplayTagContainer& Tags)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return nullptr;
	}

	for (FArcMassReplicatedItem& Item : Store->ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (!Data)
		{
			continue;
		}
		const FArcItemFragment_Tags* TagFrag = ArcItemsHelper::GetFragment<FArcItemFragment_Tags>(Data);
		if (TagFrag && TagFrag->ItemTags.HasAll(Tags))
		{
			return Data;
		}
	}

	return nullptr;
}

TArray<const FArcItemData*> FindItemsByDefinitionTags(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FGameplayTagContainer& Tags)
{
	TArray<const FArcItemData*> Result;

	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return Result;
	}

	for (FArcMassReplicatedItem& Item : Store->ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (!Data)
		{
			continue;
		}
		const FArcItemFragment_Tags* TagFrag = ArcItemsHelper::GetFragment<FArcItemFragment_Tags>(Data);
		if (TagFrag && TagFrag->ItemTags.HasAll(Tags))
		{
			Result.Add(Data);
		}
	}

	return Result;
}

// --- Count items by definition ---

int32 CountItemsByDefinition(const FArcMassItemStoreFragment& Store, const FPrimaryAssetId& DefinitionId)
{
	int32 Total = 0;
	for (const FArcMassReplicatedItem& Item : Store.ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (Data && Data->GetItemDefinitionId() == DefinitionId)
		{
			Total += Data->GetStacks();
		}
	}

	return Total;
}

int32 CountItemsByDefinition(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FPrimaryAssetId& DefinitionId)
{
	const FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	return Store ? CountItemsByDefinition(*Store, DefinitionId) : 0;
}

int32 CountItemsByTags(const FArcMassItemStoreFragment& Store, const FGameplayTagContainer& RequiredTags, const FGameplayTagContainer& DenyTags)
{
	int32 Total = 0;
	for (const FArcMassReplicatedItem& Item : Store.ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (!Data)
		{
			continue;
		}

		const FGameplayTagContainer& ItemTags = Data->ItemAggregatedTags;
		if (RequiredTags.IsEmpty() || ItemTags.HasAll(RequiredTags))
		{
			if (DenyTags.IsEmpty() || !ItemTags.HasAny(DenyTags))
			{
				Total += Data->GetStacks();
			}
		}
	}

	return Total;
}

// --- Contains ---

bool Contains(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FPrimaryAssetId& DefinitionId)
{
	return FindItemByDefinition(EntityManager, Entity, StoreType, DefinitionId) != nullptr;
}

// --- Change item slot ---

bool ChangeItemSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag NewSlot)
{
	FArcMassItemTransaction Tx(EntityManager, Entity, StoreType);

	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return false;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	if (!Item)
	{
		return false;
	}

	FArcItemData* Data = Item->ToItem();
	if (!Data)
	{
		return false;
	}

	for (FArcMassReplicatedItem& ExistingItem : Store->ReplicatedItems.Items)
	{
		const FArcItemData* ExistingData = ExistingItem.ToItem();
		if (ExistingData && ExistingData->GetItemId() != ItemId && ExistingData->GetSlotId() == NewSlot)
		{
			return false;
		}
	}

	FGameplayTag OldSlot = Data->GetSlotId();
	Private::DeactivateItemData(EntityManager, Entity, StoreType, *Store, *Item, *Data);

	Data->Slot = NewSlot;
	Store->ReplicatedItems.MarkItemDirty(*Item);

	if (!QuickBar::IsItemActivationDeferred(EntityManager, Entity, StoreType, NewSlot))
	{
		Private::ActivateItemData(EntityManager, Entity, StoreType, *Store, *Item, *Data);
	}

	FArcMassItemEvent Event;
	Event.ItemId = ItemId;
	if (!OldSlot.IsValid())
	{
		Event.Type = EArcMassItemEventType::SlotAssigned;
	}
	else if (!NewSlot.IsValid())
	{
		Event.Type = EArcMassItemEventType::SlotCleared;
	}
	else
	{
		Event.Type = EArcMassItemEventType::SlotChanged;
	}
	Event.OldSlotTag = OldSlot;
	Event.SlotTag = NewSlot;
	Tx.Emit(MoveTemp(Event));

	return true;
}

// --- Destroy item ---

bool DestroyItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId)
{
	FArcMassItemTransaction Tx(EntityManager, Entity, StoreType);

	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return false;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	if (!Item)
	{
		return false;
	}

	FArcItemData* Data = Item->ToItem();
	if (!Data)
	{
		return false;
	}

	// Recursively destroy all attached items
	const TArray<FArcItemId>& Attachments = Data->GetAttachedItems();
	TArray<FArcItemId> AttachmentsCopy(Attachments);
	for (const FArcItemId& AttachId : AttachmentsCopy)
	{
		DestroyItem(EntityManager, Entity, StoreType, AttachId);
	}

	// Detach from owner if attached
	if (Data->GetOwnerId().IsValid())
	{
		DetachItem(EntityManager, Entity, StoreType, ItemId);
	}

	// Remove from slot if slotted
	if (Data->GetSlotId().IsValid())
	{
		RemoveItemFromSlot(EntityManager, Entity, StoreType, ItemId);
	}

	Data->OnPreRemove();

	int32 Index = Store->ReplicatedItems.FindIndexById(ItemId);
	Store->ReplicatedItems.RemoveItemAt(Index);

	FArcMassItemEvent Event;
	Event.ItemId = ItemId;
	Event.Type = EArcMassItemEventType::Removed;
	Tx.Emit(MoveTemp(Event));

	return true;
}

// --- Is on any slot ---

bool IsOnAnySlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return false;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	if (!Item)
	{
		return false;
	}

	const FArcItemData* Data = Item->ToItem();
	if (!Data)
	{
		return false;
	}

	return Data->GetSlotId().IsValid();
}

// --- Attachment query APIs ---

TArray<const FArcItemData*> GetAttachedItems(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId OwnerId)
{
	TArray<const FArcItemData*> Result;

	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return Result;
	}

	for (FArcMassReplicatedItem& Item : Store->ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (Data && Data->GetOwnerId() == OwnerId)
		{
			Result.Add(Data);
		}
	}

	return Result;
}

const FArcItemData* FindAttachedItemOnSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId OwnerId, FGameplayTag AttachSlot)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return nullptr;
	}

	for (FArcMassReplicatedItem& Item : Store->ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (Data && Data->GetOwnerId() == OwnerId && Data->GetAttachSlot() == AttachSlot)
		{
			return Data;
		}
	}

	return nullptr;
}

const FArcItemData* GetItemAttachedTo(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId OwnerId, FGameplayTag AttachSlot)
{
	return FindAttachedItemOnSlot(EntityManager, Entity, StoreType, OwnerId, AttachSlot);
}

bool SetVisualItemAttachment(
	FMassEntityManager& EntityManager, FMassEntityHandle Entity,
	const UScriptStruct* StoreType,
	FArcItemId ItemId, const FPrimaryAssetId& NewVisualDefId)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store) return false;

	FArcMassReplicatedItem* RepItem = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
	if (!RepItem) return false;
	FArcItemData* ItemData = const_cast<FArcItemData*>(RepItem->ToItem());
	if (!ItemData) return false;

	// Update the FArcItemInstance_ItemVisualAttachment so the next read of
	// GetVisualItem returns the new def.
	if (FArcItemInstance_ItemVisualAttachment* Inst =
			ArcItemsHelper::FindMutableInstance<FArcItemInstance_ItemVisualAttachment>(ItemData))
	{
		Inst->VisualItem = NewVisualDefId;
	}

	// Update the attachment record's old/new visual fields so handlers
	// know what changed. Look up the state fragment.
	FMassEntityView View(EntityManager, Entity);
	FArcMassItemAttachmentStateFragment* StateFrag =
		View.GetSparseFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
	if (!StateFrag) return false;

	FArcMassItemAttachment* Existing = StateFrag->ActiveAttachments.FindByPredicate(
		[ItemId](const FArcMassItemAttachment& A) { return A.ItemId == ItemId; });
	if (!Existing) return false;

	Existing->OldVisualItemDefinition = Existing->VisualItemDefinition;
	Existing->VisualItemDefinition = NewVisualDefId;

	// Emit AttachmentChanged.
	FArcMassItemTransaction Tx(EntityManager, Entity, StoreType);
	FArcMassItemEvent Event;
	Event.ItemId = ItemId;
	Event.Type = EArcMassItemEventType::AttachmentChanged;
	Tx.Emit(MoveTemp(Event));
	return true;
}

}
