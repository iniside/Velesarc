/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcItemsArray.h"

#include "UObject/UObjectGlobals.h"

#include "Items/ArcItemDefinition.h"
#include "ArcItemData.h"
#include "ArcItemsComponent.h"
#include "ArcItemsStoreComponent.h"
#include "Core/ArcCoreAssetManager.h"

#include "Items/ArcItemDefinition.h"

DEFINE_LOG_CATEGORY(LogItemArray);


void FArcItemCopyContainerHelper::CopyPersistentInstances(FArcItemSpec& OutSpec, const FArcItemData* InData)
{
	for (const FInstancedStruct& Instance : InData->ItemInstances.Data)
	{
		const FArcItemInstance* InstancePtr = Instance.GetPtr<FArcItemInstance>();
		if (InstancePtr && InstancePtr->ShouldPersist())
		{
			OutSpec.InitialInstanceData.Add(Instance);
		}
	}
}

FArcItemSpec FArcItemCopyContainerHelper::ToSpec(const FArcItemData* InData)
{
	FArcItemSpec Spec = InData->Spec;
	Spec.ItemId = InData->GetItemId();
	Spec.Amount = InData->GetStacks();
	Spec.Level = InData->GetLevel();
	CopyPersistentInstances(Spec, InData);
	return Spec;
}

FArcItemCopyContainerHelper FArcItemCopyContainerHelper::New(UArcItemsStoreComponent* InItemsStore, const FArcItemData* InData)
{
	FArcItemCopyContainerHelper Container;
	Container.Item = InData->Spec;
	Container.Item.ItemId = InData->GetItemId();
	Container.Item.Amount = InData->GetStacks();
	Container.Item.Level = InData->GetLevel();
	Container.SlotId = InData->GetSlotId();
	CopyPersistentInstances(Container.Item, InData);

	TArray<const FArcItemData*> AttachedItems = InItemsStore->GetItemsArray().GetItemsAttachedTo(InData->GetItemId());

	for (const FArcItemData* Item : AttachedItems)
	{
		if (Item)
		{
			int32 Idx = Container.ItemAttachments.Add( { Item->GetItemId(), Item->Spec, Item->GetAttachSlot() });
			Container.ItemAttachments[Idx].Item.ItemId = Item->GetItemId();
			Container.ItemAttachments[Idx].Item.Amount = Item->GetStacks();
			Container.ItemAttachments[Idx].Item.Level = Item->GetLevel();
			CopyPersistentInstances(Container.ItemAttachments[Idx].Item, Item);
		}
	}

	return Container;
}

FArcItemCopyContainerHelper FArcItemCopyContainerHelper::New(const FArcItemData* InData)
{
	FArcItemCopyContainerHelper Container;
	Container.Item = InData->Spec;
	Container.Item.ItemId = InData->GetItemId();
	Container.Item.Amount = InData->GetStacks();
	Container.Item.Level = InData->GetLevel();
	Container.SlotId = InData->GetSlotId();
	CopyPersistentInstances(Container.Item, InData);

	return Container;
}

FArcItemCopyContainerHelper FArcItemCopyContainerHelper::FromSpec(const FArcItemSpec& Item)
{
	FArcItemCopyContainerHelper Container;
	Container.ItemId = Item.ItemId.IsValid() ? Item.ItemId : FArcItemId::Generate();
	Container.Item = Item;
	Container.Item.ItemId = Item.ItemId.IsValid() ? Item.ItemId : FArcItemId::Generate();
	Container.SlotId = FGameplayTag();;

	return Container;
}

TArray<FArcItemId> FArcItemCopyContainerHelper::AddItems(UArcItemsStoreComponent* InItemsStore)
{
	TArray<FArcItemId> OutIds;

	OutIds.Add(Item.ItemId);
	
	InItemsStore->AddItem(Item, FArcItemId::InvalidId);

	if (SlotId.IsValid())
	{
		InItemsStore->AddItemToSlot(Item.ItemId, SlotId);	
	}
	
	for (FArcAttachedItemHelper& AttachedItem : ItemAttachments)
	{
		
		OutIds.Add(AttachedItem.Item.ItemId);

		InItemsStore->AddItem(AttachedItem.Item, Item.ItemId);
		InItemsStore->InternalAttachToItem(Item.ItemId, AttachedItem.Item.ItemId, AttachedItem.SlotId);
	}
	
	return OutIds;
}

FArcGenericItemIdDelegate FArcItemDataInternalWrapper::OnItemRemovedDelegate = FArcGenericItemIdDelegate();

void FArcItemDataInternalWrapper::PreReplicatedRemove(const FArcItemsArray& InArraySerializer)
{
	FArcItemData* Data = ToItem();
	if (Data)
	{
		OnItemRemovedDelegate.Broadcast(Data->GetItemId());
		Data->PreReplicatedRemove(InArraySerializer);
		InArraySerializer.RemoveCachedItem(Data->GetItemId());
	}
}

void FArcItemDataInternalWrapper::PostReplicatedAdd(const FArcItemsArray& InArraySerializer)
{
	FArcItemData* Data = ToItem();
	if (Data)
	{
		InArraySerializer.AddCachedItem(Data->GetItemId(), Data);
		Data->PostReplicatedAdd(InArraySerializer);
	}
}

void FArcItemDataInternalWrapper::PostReplicatedChange(const FArcItemsArray& InArraySerializer)
{
	FArcItemData* Data = ToItem();
	if (Data)
	{
		Data->PostReplicatedChange(InArraySerializer);
	}
}

void FArcItemsArray::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices
										 , int32 FinalSize)
{
	if (RemovedIndices.Num())
	{
	}
}

void FArcItemsArray::PostReplicatedAdd(const TArrayView<int32>& AddedIndices
									   , int32 FinalSize)
{
	if (AddedIndices.Num())
	{
	}
}

void FArcItemsArray::PostReplicatedChange(const TArrayView<int32>& ChangedIndices
										  , int32 FinalSize)
{
	if (ChangedIndices.Num())
	{
	}
}

void FArcItemsArray::AddInternalItem(UArcItemsStoreComponent* NewItemStoreComponent, FInstancedStruct&& InItem
	, TArray<FInstancedStruct>& AttachedItems)
{
	FArcItemDataInternalWrapper Wrapper;
	Wrapper.ItemData = MoveTemp(InItem);
	FArcItemData* Data = Wrapper.ToItem();
	Data->Initialize(NewItemStoreComponent);

	int32 Idx = Items.Add(MoveTemp(Wrapper));
	FArcItemData* AddedData = Items[Idx].ToItem();
	AddCachedItem(AddedData->GetItemId(), AddedData);

	for (FInstancedStruct& AttachedItem : AttachedItems)
	{
		FArcItemDataInternalWrapper AttachWrapper;
		AttachWrapper.ItemData = MoveTemp(AttachedItem);
		FArcItemData* AttachData = AttachWrapper.ToItem();
		AttachData->Initialize(NewItemStoreComponent);

		int32 AttachIdx = Items.Add(MoveTemp(AttachWrapper));
		FArcItemData* AddedAttach = Items[AttachIdx].ToItem();
		AddCachedItem(AddedAttach->GetItemId(), AddedAttach);

		AddedAttach->AttachToItem(AddedData->ItemId, AddedAttach->GetAttachSlot());
		Edit().MarkItemDirty(Items[AttachIdx]);
	}

	Edit().MarkItemDirty(Items[Idx]);
}

TArray<FArcItemCopyContainerHelper> FArcItemsArray::GetAllInternalItems() const
{
	TArray<FArcItemCopyContainerHelper> Out;
	Out.Reserve(Items.Num());

	for (const FArcItemDataInternalWrapper& Wrapper : Items)
	{
		const FArcItemData* Data = Wrapper.ToItem();
		if (Data && Data->GetOwnerId().IsValid())
		{
			continue;
		}

		Out.Add(FArcItemCopyContainerHelper::New(Owner, Data));
	}

	return Out;
}

FArcItemCopyContainerHelper FArcItemsArray::GetItemCopyHelper(const FArcItemId& InItemId) const
{
	if (ItemsMap.Contains(InItemId))
	{
		FArcItemData* ItemPtr = ItemsMap[InItemId];
		if (ItemPtr)
		{
			return FArcItemCopyContainerHelper::New(Owner, ItemPtr);
		}
	}

	return FArcItemCopyContainerHelper();
}

TArray<FArcItemId> FArcItemsArray::AddItemCopyInternal(UArcItemsStoreComponent* NewItemStoreComponent
													   , FArcItemCopyContainerHelper& InContainer)
{
	return InContainer.AddItems(NewItemStoreComponent);
}

int32 FArcItemsArray::AddItem(const FArcItemSpec& NewItemSpec
							  , bool& bAlreadyExists)
{
	FArcItemDataInternalWrapper Wrapper(NewItemSpec);
	int32 Idx = Items.Add(MoveTemp(Wrapper));
	Edit().MarkItemDirty(Items[Idx]);

	FArcItemData* Data = Items[Idx].ToItem();
	AddCachedItem(Data->GetItemId(), Data);

	return Items.Num() - 1;
}

void FArcItemsArray::RemoveItem(const FArcItemId& InItemId)
{
	RemoveCachedItem(InItemId);

	int32 Idx = IndexOf(InItemId);
	FArcItemData* Data = Items[Idx].ToItem();
	if (Data)
	{
		Data->Deinitialize();
	}

	MarkItemDirtyHandle(InItemId);

	Items.RemoveAt(Idx);
	Edit().MarkArrayDirty();
}

void FArcItemsArray::RemoveItem(int32 Idx)
{
	FArcItemData* Data = Items[Idx].ToItem();
	if (Data)
	{
		RemoveCachedItem(Data->GetItemId());
	}
	MarkItemDirtyIdx(Idx);
	Edit().MarkArrayDirty();

	Items.RemoveAt(Idx);
}

void FArcItemsArray::AddItemInstance(const FArcItemData* ToItem, UScriptStruct* InInstanceType)
{
	for (FArcItemDataInternalWrapper& Wrapper : Items)
	{
		FArcItemData* Data = Wrapper.ToItem();
		if (Data && Data->GetItemId() == ToItem->GetItemId())
		{
			FInstancedStruct NewInstance;
			NewInstance.InitializeAs(InInstanceType, nullptr);

			Data->ItemInstances.Data.Add(MoveTemp(NewInstance));
			Data->SetItemInstances(Data->ItemInstances);
			MarkItemDirtyHandle(ToItem->GetItemId());
			break;
		}
	}
}

void FArcItemsArray::MarkItemDirtyIdx(int32 Idx)
{
	if (Items.IsValidIndex(Idx) == false)
	{
		return;
	}

	//Items[Idx].ToItem()->ForceRep++;
	//Items[Idx].Rep++;
	MarkItemDirty(Items[Idx]);
	Edit().Edit(Idx);
	Edit().MarkItemDirty(Items[Idx]);
}

void FArcItemsArray::MarkItemDirtyHandle(const FArcItemId& Handle)
{
	int32 Idx = IndexOf(Handle);
	//Items[Idx].Rep++;
	//Items[Idx].ToItem()->ForceRep++;
	
	//MarkItemDirty(Items[Idx]);
	
	Edit().Edit(Idx);
	MarkItemDirty(Items[Idx]);
	Edit().MarkItemDirty(Items[Idx]);
}

void FArcItemsArray::MarkItemInstanceDirtyHandle(const FArcItemId& Handle
	, UScriptStruct* InScriptStruct)
{
	int32 Idx = IndexOf(Handle);
	//Items[Idx].ToItem()->ForceRep++;
}
