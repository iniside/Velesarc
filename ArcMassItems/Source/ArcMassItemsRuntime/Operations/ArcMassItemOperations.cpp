// Copyright Lukasz Baran. All Rights Reserved.

#include "Operations/ArcMassItemOperations.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Fragments/ArcMassItemEventFragment.h"
#include "Signals/ArcMassItemSignals.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemInstance.h"
#include "Items/ArcItemScalableFloat.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "MassSignalSubsystem.h"

namespace ArcMassItems
{

namespace Private
{
	FArcMassItemStoreFragment* GetStore(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
	{
		FMassEntityView EntityView(EntityManager, Entity);
		return EntityView.GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
	}

	void SetEvent(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcMassItemEvent& Event)
	{
		FMassEntityView EntityView(EntityManager, Entity);
		FArcMassItemEventFragment* EventFragment = EntityView.GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
		if (!EventFragment)
		{
			EventFragment = &EntityManager.AddSparseElementToEntity<FArcMassItemEventFragment>(Entity);
		}
		EventFragment->Events = { Event };
	}

	void FireSignal(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FName SignalName)
	{
		UWorld* World = EntityManager.GetWorld();
		if (!World)
		{
			return;
		}
		UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
		if (SignalSubsystem)
		{
			SignalSubsystem->SignalEntity(SignalName, Entity);
		}
	}

	FName GetSignalForEventType(EArcMassItemEventType Type)
	{
		switch (Type)
		{
		case EArcMassItemEventType::Added: return UE::ArcMassItems::Signals::ItemAdded;
		case EArcMassItemEventType::Removed: return UE::ArcMassItems::Signals::ItemRemoved;
		case EArcMassItemEventType::SlotChanged: return UE::ArcMassItems::Signals::ItemSlotChanged;
		case EArcMassItemEventType::StacksChanged: return UE::ArcMassItems::Signals::ItemStacksChanged;
		case EArcMassItemEventType::Attached: return UE::ArcMassItems::Signals::ItemAttached;
		case EArcMassItemEventType::Detached: return UE::ArcMassItems::Signals::ItemDetached;
		default: return FName();
		}
	}

	void EmitEvent(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcMassItemEvent& Event)
	{
		SetEvent(EntityManager, Entity, Event);
		FireSignal(EntityManager, Entity, GetSignalForEventType(Event.Type));
	}
}

FArcItemId AddItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcItemSpec& Spec)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity);
	if (!Store)
	{
		return FArcItemId();
	}

	FArcMassReplicatedItem NewItem(Spec);
	FArcItemData* Data = NewItem.ToItem();
	FArcItemId ItemId = Spec.ItemId.IsValid() ? Spec.ItemId : FArcItemId::Generate();
	Data->ItemId = ItemId;
	Data->SetStacks(Spec.Amount);

	Store->ReplicatedItems.AddItem(MoveTemp(NewItem));

	FArcMassItemEvent Event;
	Event.ItemId = ItemId;
	Event.Type = EArcMassItemEventType::Added;
	Private::EmitEvent(EntityManager, Entity, Event);

	return ItemId;
}

bool RemoveItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcItemId ItemId, int32 StacksToRemove)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity);
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
		Private::EmitEvent(EntityManager, Entity, Event);
		return true;
	}

	// Before removing the item itself, remove all its attachments
	const TArray<FArcItemId>& Attachments = Data->GetAttachedItems();
	TArray<FArcItemId> AttachmentsCopy(Attachments);
	for (const FArcItemId& AttachId : AttachmentsCopy)
	{
		RemoveItem(EntityManager, Entity, AttachId);
	}

	FGameplayTag CurrentSlot = Data->GetSlotId();
	if (CurrentSlot.IsValid())
	{
		Store->SlottedItems.Remove(CurrentSlot);
	}

	int32 Index = Store->ReplicatedItems.FindIndexById(ItemId);
	Store->ReplicatedItems.RemoveItemAt(Index);

	FArcMassItemEvent Event;
	Event.ItemId = ItemId;
	Event.Type = EArcMassItemEventType::Removed;
	Private::EmitEvent(EntityManager, Entity, Event);

	return true;
}

bool AddItemToSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcItemId ItemId, FGameplayTag SlotTag)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity);
	if (!Store)
	{
		return false;
	}

	FArcMassReplicatedItem* Item = Store->ReplicatedItems.FindById(ItemId);
	if (!Item)
	{
		return false;
	}

	if (Store->SlottedItems.Contains(SlotTag))
	{
		return false;
	}

	FArcItemData* Data = Item->ToItem();
	if (!Data)
	{
		return false;
	}

	Data->Slot = SlotTag;
	Store->ReplicatedItems.MarkItemDirty(*Item);
	Store->SlottedItems.Add(SlotTag, ItemId);

	FArcMassItemEvent Event;
	Event.ItemId = ItemId;
	Event.Type = EArcMassItemEventType::SlotChanged;
	Event.SlotTag = SlotTag;
	Private::EmitEvent(EntityManager, Entity, Event);

	return true;
}

bool RemoveItemFromSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcItemId ItemId)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity);
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

	Data->Slot = FGameplayTag();
	Store->ReplicatedItems.MarkItemDirty(*Item);
	Store->SlottedItems.Remove(CurrentSlot);

	FArcMassItemEvent Event;
	Event.ItemId = ItemId;
	Event.Type = EArcMassItemEventType::SlotChanged;
	Private::EmitEvent(EntityManager, Entity, Event);

	return true;
}

bool ModifyStacks(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcItemId ItemId, int32 Delta)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity);
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
	Private::EmitEvent(EntityManager, Entity, Event);

	return true;
}

// --- Attachment operations ---

bool AttachItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcItemId OwnerId, FArcItemId AttachmentId, FGameplayTag AttachSlot)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity);
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

	AttachmentData->OwnerId = OwnerId;
	AttachmentData->AttachedToSlot = AttachSlot;
	OwnerData->AttachedItems.Add(AttachmentId);

	Store->ReplicatedItems.MarkItemDirty(*OwnerItem);
	Store->ReplicatedItems.MarkItemDirty(*AttachmentItem);

	FArcMassItemEvent Event;
	Event.ItemId = AttachmentId;
	Event.Type = EArcMassItemEventType::Attached;
	Event.SlotTag = AttachSlot;
	Private::EmitEvent(EntityManager, Entity, Event);

	return true;
}

bool DetachItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcItemId AttachmentId)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity);
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
			OwnerData->AttachedItems.Remove(AttachmentId);
			Store->ReplicatedItems.MarkItemDirty(*OwnerItem);
		}
	}

	AttachmentData->OldOwnerId = AttachmentData->OwnerId;
	AttachmentData->OwnerId = FArcItemId();
	AttachmentData->OldAttachedToSlot = AttachmentData->AttachedToSlot;
	AttachmentData->AttachedToSlot = FGameplayTag();

	Store->ReplicatedItems.MarkItemDirty(*AttachmentItem);

	FArcMassItemEvent Event;
	Event.ItemId = AttachmentId;
	Event.Type = EArcMassItemEventType::Detached;
	Private::EmitEvent(EntityManager, Entity, Event);

	return true;
}

// --- Instance data operations ---

bool SetItemInstances(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcItemId ItemId, const FArcItemInstanceArray& Instances)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity);
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

const FArcItemInstanceArray* GetItemInstances(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcItemId ItemId)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity);
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

float GetScalableValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcItemId ItemId, const FArcScalableCurveFloat& ScalableFloat)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity);
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

bool SetLevel(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcItemId ItemId, uint8 NewLevel)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity);
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

bool AddDynamicTag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcItemId ItemId, FGameplayTag Tag)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity);
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

bool RemoveDynamicTag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcItemId ItemId, FGameplayTag Tag)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity);
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

const FArcItemData* FindItemByDefinition(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FPrimaryAssetId& DefinitionId)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity);
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

TArray<const FArcItemData*> FindItemsByTag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FGameplayTagContainer& Tags)
{
	TArray<const FArcItemData*> Result;

	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity);
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

bool MoveItem(FMassEntityManager& EntityManager, FMassEntityHandle FromEntity, FMassEntityHandle ToEntity, FArcItemId ItemId)
{
	FArcMassItemStoreFragment* FromStore = Private::GetStore(EntityManager, FromEntity);
	FArcMassItemStoreFragment* ToStore = Private::GetStore(EntityManager, ToEntity);
	if (!FromStore || !ToStore)
	{
		return false;
	}

	int32 SourceIndex = FromStore->ReplicatedItems.FindIndexById(ItemId);
	if (SourceIndex == INDEX_NONE)
	{
		return false;
	}

	FArcMassReplicatedItem MovedItem = MoveTemp(FromStore->ReplicatedItems.Items[SourceIndex]);
	FArcItemData* Data = MovedItem.ToItem();

	// Clean up slot in source if slotted
	if (Data)
	{
		FGameplayTag CurrentSlot = Data->GetSlotId();
		if (CurrentSlot.IsValid())
		{
			FromStore->SlottedItems.Remove(CurrentSlot);
			Data->Slot = FGameplayTag();
		}
	}

	FromStore->ReplicatedItems.RemoveItemAt(SourceIndex);

	ToStore->ReplicatedItems.AddItem(MoveTemp(MovedItem));

	// Emit Removed on source
	FArcMassItemEvent RemovedEvent;
	RemovedEvent.ItemId = ItemId;
	RemovedEvent.Type = EArcMassItemEventType::Removed;
	Private::EmitEvent(EntityManager, FromEntity, RemovedEvent);

	// Emit Added on target
	FArcMassItemEvent AddedEvent;
	AddedEvent.ItemId = ItemId;
	AddedEvent.Type = EArcMassItemEventType::Added;
	Private::EmitEvent(EntityManager, ToEntity, AddedEvent);

	return true;
}

}
