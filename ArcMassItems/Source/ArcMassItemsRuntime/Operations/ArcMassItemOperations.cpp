// Copyright Lukasz Baran. All Rights Reserved.

#include "Operations/ArcMassItemOperations.h"
#include "Operations/ArcMassItemTransaction.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Fragments/ArcMassItemEventFragment.h"
#include "Signals/ArcMassItemSignals.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemStackMethod.h"
#include "Items/ArcItemInstance.h"
#include "Items/ArcItemScalableFloat.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Items/Fragments/ArcItemFragment_SocketSlots.h"
#include "Equipment/Fragments/ArcItemFragment_ItemVisualAttachment.h"
#include "Attachments/ArcMassItemAttachmentFragments.h"
#include "Core/ArcCoreAssetManager.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "MassSignalSubsystem.h"

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

FName GetSignalForEventType(EArcMassItemEventType Type, const UScriptStruct* StoreType)
{
	FName BaseSignal;
	switch (Type)
	{
	case EArcMassItemEventType::Added:             BaseSignal = UE::ArcMassItems::Signals::ItemAdded;             break;
	case EArcMassItemEventType::Removed:           BaseSignal = UE::ArcMassItems::Signals::ItemRemoved;           break;
	case EArcMassItemEventType::SlotChanged:       BaseSignal = UE::ArcMassItems::Signals::ItemSlotChanged;       break;
	case EArcMassItemEventType::StacksChanged:     BaseSignal = UE::ArcMassItems::Signals::ItemStacksChanged;     break;
	case EArcMassItemEventType::Attached:          BaseSignal = UE::ArcMassItems::Signals::ItemAttached;          break;
	case EArcMassItemEventType::Detached:          BaseSignal = UE::ArcMassItems::Signals::ItemDetached;          break;
	case EArcMassItemEventType::VisualAttached:    BaseSignal = UE::ArcMassItems::Signals::ItemVisualAttached;    break;
	case EArcMassItemEventType::VisualDetached:    BaseSignal = UE::ArcMassItems::Signals::ItemVisualDetached;    break;
	case EArcMassItemEventType::AttachmentChanged: BaseSignal = UE::ArcMassItems::Signals::ItemAttachmentChanged; break;
	default: return FName();
	}
	return UE::ArcMassItems::Signals::GetSignalName(BaseSignal, StoreType);
}

// --- Mass-safe slot notification helpers ---

void NotifyFragmentsSlotAddedMass(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemData* ItemData, const FGameplayTag& SlotId)
{
	// Notify item's own fragments
	ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(ItemData
		, [SlotId](const FArcItemData* Data, const FArcItemFragment_ItemInstanceBase* Fragment)
		{
			Fragment->OnItemAddedToSlot(Data, SlotId);
		});

	// Notify attached items' fragments
	TArray<const FArcItemData*> Attached = GetAttachedItems(EntityManager, Entity, StoreType, ItemData->GetItemId());
	for (const FArcItemData* AttachedItem : Attached)
	{
		ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(AttachedItem
			, [](const FArcItemData* Data, const FArcItemFragment_ItemInstanceBase* Fragment)
			{
				Fragment->OnItemAddedToSlot(Data, Data->GetAttachSlot());
			});
	}
}

void NotifyFragmentsSlotRemovedMass(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemData* ItemData, const FGameplayTag& SlotId)
{
	// Notify attached items first
	TArray<const FArcItemData*> Attached = GetAttachedItems(EntityManager, Entity, StoreType, ItemData->GetItemId());
	for (const FArcItemData* AttachedItem : Attached)
	{
		ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(AttachedItem
			, [](const FArcItemData* Data, const FArcItemFragment_ItemInstanceBase* Fragment)
			{
				Fragment->OnItemRemovedFromSlot(Data, Data->GetAttachSlot());
			});
	}

	// Notify item's own fragments
	ArcItemsHelper::ForEachFragment<FArcItemFragment_ItemInstanceBase>(ItemData
		, [SlotId](const FArcItemData* Data, const FArcItemFragment_ItemInstanceBase* Fragment)
		{
			Fragment->OnItemRemovedFromSlot(Data, SlotId);
		});
}

FArcItemId AddItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FArcItemSpec& Spec)
{
	FArcMassItemTransaction Tx(EntityManager, Entity, StoreType);

	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return FArcItemId();
	}

	const UArcItemDefinition* Def = Spec.GetItemDefinition();

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

					FArcMassReplicatedItem NewItem(OverflowSpec);
					FArcItemData* Data = NewItem.ToItem();
					FArcItemId ItemId = OverflowSpec.ItemId.IsValid() ? OverflowSpec.ItemId : FArcItemId::Generate();
					Data->ItemId = ItemId;
					Data->SetStacks(RemainingStacks);

					Store->ReplicatedItems.AddItem(MoveTemp(NewItem));
					Data->InitializeMass(EntityManager.GetWorld());
					Data->MassEntityHandle = Entity;
					Data->OnItemAdded();

					if (const FArcItemFragment_SocketSlots* SocketSlots = ArcItemsHelper::GetFragment<FArcItemFragment_SocketSlots>(Data))
					{
						for (const FArcSocketSlot& Slot : SocketSlots->GetSocketSlots())
						{
							if (Slot.DefaultSocketItemDefinition)
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
		}
	}

// Create new item (no stacking or stacking not applicable)
FArcMassReplicatedItem NewItem(Spec);
FArcItemData* Data = NewItem.ToItem();
FArcItemId ItemId = Spec.ItemId.IsValid() ? Spec.ItemId : FArcItemId::Generate();
Data->ItemId = ItemId;
Data->SetStacks(Spec.Amount);

Store->ReplicatedItems.AddItem(MoveTemp(NewItem));
Data->InitializeMass(EntityManager.GetWorld());
Data->MassEntityHandle = Entity;
Data->OnItemAdded();

if (const FArcItemFragment_SocketSlots* SocketSlots = ArcItemsHelper::GetFragment<FArcItemFragment_SocketSlots>(Data))
{
	for (const FArcSocketSlot& Slot : SocketSlots->GetSocketSlots())
	{
		if (Slot.DefaultSocketItemDefinition)
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

	// Remove all attachments before removing the item itself
	const TArray<FArcItemId>& Attachments = Data->GetAttachedItems();
	TArray<FArcItemId> AttachmentsCopy(Attachments);
	for (const FArcItemId& AttachId : AttachmentsCopy)
	{
		RemoveItem(EntityManager, Entity, StoreType, AttachId);
	}

	int32 Index = Store->ReplicatedItems.FindIndexById(ItemId);
	Store->ReplicatedItems.RemoveItemAt(Index);

	FArcMassItemEvent Event;
	Event.ItemId = ItemId;
	Event.Type = EArcMassItemEventType::Removed;
	Tx.Emit(MoveTemp(Event));

	return true;
}

bool AddItemToSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag SlotTag)
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

	// Check slot occupancy by scanning item data (SlottedItems map removed)
	for (const FArcMassReplicatedItem& ExistingItem : Store->ReplicatedItems.Items)
	{
		const FArcItemData* ExistingData = ExistingItem.ToItem();
		if (ExistingData && ExistingData->GetSlotId() == SlotTag)
		{
			return false;
		}
	}

	FArcItemData* Data = Item->ToItem();
	if (!Data)
	{
		return false;
	}

	Data->Slot = SlotTag;
	Store->ReplicatedItems.MarkItemDirty(*Item);

	NotifyFragmentsSlotAddedMass(EntityManager, Entity, StoreType, Data, SlotTag);

	FArcMassItemEvent Event;
	Event.ItemId = ItemId;
	Event.Type = EArcMassItemEventType::SlotChanged;
	Event.SlotTag = SlotTag;
	Tx.Emit(MoveTemp(Event));

	return true;
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

	NotifyFragmentsSlotRemovedMass(EntityManager, Entity, StoreType, Data, CurrentSlot);

	Data->Slot = FGameplayTag();
	Store->ReplicatedItems.MarkItemDirty(*Item);

	FArcMassItemEvent Event;
	Event.ItemId = ItemId;
	Event.Type = EArcMassItemEventType::SlotChanged;
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

	AttachmentData->OwnerId = OwnerId;
	AttachmentData->AttachedToSlot = AttachSlot;
	OwnerData->AttachedItems.Add(AttachmentId);

	Store->ReplicatedItems.MarkItemDirty(*OwnerItem);
	Store->ReplicatedItems.MarkItemDirty(*AttachmentItem);

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
		if (DescData)
		{
			DescData->Slot = FGameplayTag();
		}

		FromStore->ReplicatedItems.RemoveItemAt(DescIndex);
		ToStore->ReplicatedItems.AddItem(MoveTemp(MovedItem));

		FArcMassItemEvent RemovedEvent;
		RemovedEvent.ItemId = DescendantId;
		RemovedEvent.Type = EArcMassItemEventType::Removed;
		FromTx.Emit(MoveTemp(RemovedEvent));

		FArcMassItemEvent AddedEvent;
		AddedEvent.ItemId = DescendantId;
		AddedEvent.Type = EArcMassItemEventType::Added;
		ToTx.Emit(MoveTemp(AddedEvent));
	}

	// Move the parent item
	int32 SourceIndex = FromStore->ReplicatedItems.FindIndexById(ItemId);
	if (SourceIndex == INDEX_NONE)
	{
		return false;
	}

	FArcMassReplicatedItem MovedItem = MoveTemp(FromStore->ReplicatedItems.Items[SourceIndex]);
	FArcItemData* Data = MovedItem.ToItem();

	if (Data)
	{
		Data->Slot = FGameplayTag();
	}

	FromStore->ReplicatedItems.RemoveItemAt(SourceIndex);
	ToStore->ReplicatedItems.AddItem(MoveTemp(MovedItem));

	FArcMassItemEvent RemovedEvent;
	RemovedEvent.ItemId = ItemId;
	RemovedEvent.Type = EArcMassItemEventType::Removed;
	FromTx.Emit(MoveTemp(RemovedEvent));

	FArcMassItemEvent AddedEvent;
	AddedEvent.ItemId = ItemId;
	AddedEvent.Type = EArcMassItemEventType::Added;
	ToTx.Emit(MoveTemp(AddedEvent));

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

	Data->InitializeMass(EntityManager.GetWorld());
	Data->OnItemAdded();
}

// --- Query: pointer-based overload ---

const FArcItemData* FindItemByDefinition(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const UArcItemDefinition* Definition)
{
	if (!Definition)
	{
		return nullptr;
	}
	return FindItemByDefinition(EntityManager, Entity, StoreType, Definition->GetPrimaryAssetId());
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

int32 CountItemsByDefinition(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FPrimaryAssetId& DefinitionId)
{
	FArcMassItemStoreFragment* Store = Private::GetStore(EntityManager, Entity, StoreType);
	if (!Store)
	{
		return 0;
	}

	int32 Total = 0;
	for (FArcMassReplicatedItem& Item : Store->ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (Data && Data->GetItemDefinitionId() == DefinitionId)
		{
			Total += Data->GetStacks();
		}
	}

	return Total;
}

// --- Contains ---

bool Contains(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const UArcItemDefinition* Definition)
{
	return FindItemByDefinition(EntityManager, Entity, StoreType, Definition) != nullptr;
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
	if (OldSlot.IsValid())
	{
		NotifyFragmentsSlotRemovedMass(EntityManager, Entity, StoreType, Data, OldSlot);
	}

	Data->Slot = NewSlot;
	Store->ReplicatedItems.MarkItemDirty(*Item);

	NotifyFragmentsSlotAddedMass(EntityManager, Entity, StoreType, Data, NewSlot);

	FArcMassItemEvent Event;
	Event.ItemId = ItemId;
	Event.Type = EArcMassItemEventType::SlotChanged;
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
	FArcItemId ItemId, UArcItemDefinition* NewVisualDef)
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
		UArcCoreAssetManager& AM = UArcCoreAssetManager::Get();
		Inst->VisualItem = AM.GetPrimaryAssetIdForObject(NewVisualDef);
	}

	// Update the attachment record's old/new visual fields so handlers
	// know what changed. Look up the state fragment.
	FMassEntityView View(EntityManager, Entity);
	FArcMassItemAttachmentStateFragment* StateFrag =
		View.GetFragmentDataPtr<FArcMassItemAttachmentStateFragment>();
	if (!StateFrag) return false;

	FArcMassItemAttachment* Existing = StateFrag->ActiveAttachments.FindByPredicate(
		[ItemId](const FArcMassItemAttachment& A) { return A.ItemId == ItemId; });
	if (!Existing) return false;

	Existing->OldVisualItemDefinition = Existing->VisualItemDefinition;
	Existing->VisualItemDefinition = NewVisualDef;

	// Emit AttachmentChanged.
	FArcMassItemTransaction Tx(EntityManager, Entity, StoreType);
	FArcMassItemEvent Event;
	Event.ItemId = ItemId;
	Event.Type = EArcMassItemEventType::AttachmentChanged;
	Tx.Emit(MoveTemp(Event));
	return true;
}

}
