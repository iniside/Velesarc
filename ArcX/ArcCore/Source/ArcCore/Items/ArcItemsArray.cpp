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

FArcItemDataInternal::FArcItemDataInternal(const FArcItemDataInternal& Other)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FArcItemDataInternal Deep Copy");

	TMap<const UScriptStruct*, const FArcScalableFloatItemFragment*> ScalableFloatFragmentsTempCopy;
	if (ItemPtr.IsValid() && ItemId == Other.ItemId)
	{
		ScalableFloatFragmentsTempCopy = MoveTemp(ItemPtr->ScalableFloatFragments);
	}
		
	ItemId = Other.ItemId;
	OwningArray = Other.OwningArray;
	
	if (Equals(Other.ItemPtr) == false)
	{
		if (Other.ItemPtr.IsValid() == false)
		{
			ItemPtr.Reset();
			return;
		}
		if (ItemPtr.IsValid() == false)
		{
			ItemPtr = MakeShareable(new FArcItemData);
		}

		*ItemPtr.Get() = *Other.ItemPtr.Get();

		UE_LOG(LogItemArray, Log, TEXT("FArcItemDataInternal Deep Copy"))
	}
}

FArcItemDataInternal::FArcItemDataInternal(FArcItemDataInternal&& Other)
{
	ItemId = Other.ItemId;
	ItemPtr = MoveTemp(Other.ItemPtr);
	OwningArray = MoveTemp(Other.OwningArray);
	
	UE_LOG(LogItemArray, Log, TEXT("FArcItemDataInternal Move"))
}

FArcItemDataInternal::FArcItemDataInternal(FArcItemData&& Other)
{
	ItemId = Other.GetItemId();
	ItemPtr = MakeShareable(new FArcItemData(MoveTemp(Other)));
}

void FArcItemDataInternal::SetItemData(TSharedPtr<FArcItemData>& InItem
									   , const FArcItemSpec& InSpec)
{
	ItemPtr = InItem;
	ItemId = ItemPtr->GetItemId();

	const UArcItemDefinition* ItemDef = UArcCoreAssetManager::Get().GetAsset<UArcItemDefinition>(InSpec.GetItemDefinitionId());

	// Fallback: use the definition set directly on the spec (e.g. from tests or manually created items)
	if (ItemDef == nullptr)
	{
		ItemDef = InSpec.GetItemDefinition();
	}

	ItemPtr->SetItemSpec(InSpec);

	TArray<const FArcItemFragment_ItemInstanceBase*> InstancesToMake;
	if (ItemDef != nullptr)
	{
		InstancesToMake = ItemDef->GetFragmentsOfType<FArcItemFragment_ItemInstanceBase>();
	}
	for (const FArcItemFragment_ItemInstanceBase* IIB : InstancesToMake)
	{
		if (IIB->GetCreateItemInstance() == false)
		{
			continue;
		}
		
		if (UScriptStruct* InstanceType = IIB->GetItemInstanceType())
		{
			int32 InitialDataIdx = InSpec.InitialInstanceData.IndexOfByPredicate([InstanceType](const TSharedPtr<FArcItemInstance>& Other)
			{
				return Other->GetScriptStruct() == InstanceType;
			});
			
			int32 Idx = ItemPtr->ItemInstances.Data.AddDefaulted();
			{
				TSharedPtr<FArcItemInstance> SharedPtr = ArcItems::AllocateInstance(InstanceType);

				if (InitialDataIdx != INDEX_NONE)
				{
					InstanceType->GetCppStructOps()->Copy(SharedPtr.Get(), InSpec.InitialInstanceData[InitialDataIdx].Get(), 0);
				}

				ItemPtr->ItemInstances.Data[Idx] = SharedPtr;
			}
		}
	}

	TArray<const FArcItemFragment_ItemInstanceBase*> SpecFragments = InSpec.GetSpecFragments();
	for (const FArcItemFragment_ItemInstanceBase* Fragment : SpecFragments)
	{
		if (Fragment->GetCreateItemInstance() == false)
		{
			continue;
		}

		
		if (UScriptStruct* InstanceType = Fragment->GetItemInstanceType())
		{
			int32 InitialDataIdx = InSpec.InitialInstanceData.IndexOfByPredicate([InstanceType](const TSharedPtr<FArcItemInstance>& Other)
			{
				return Other->GetScriptStruct() == InstanceType;
			});
			
			TSharedPtr<FArcItemInstance> SharedPtr = ArcItems::AllocateInstance(InstanceType);
			if (InitialDataIdx != INDEX_NONE)
			{
				InstanceType->GetCppStructOps()->Copy(SharedPtr.Get(), InSpec.InitialInstanceData[InitialDataIdx].Get(), 0);
			}

			int32 Idx = ItemPtr->ItemInstances.Data.AddDefaulted();
			ItemPtr->ItemInstances.Data[Idx] = SharedPtr;
		}
	}

	ItemPtr->SetItemInstances(ItemPtr->ItemInstances);
}

bool FArcItemDataInternal::operator==(const FArcItemDataInternal& Other) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FArcItemDataInternal::operator==");
	// When used as replicated property and we want to detect changes of the intance
	// data we need to compare the data stored in the inner structs.
	if (ItemId != Other.ItemId)
	{
		UE_LOG(LogItemArray, Log, TEXT("FArcItemDataInternal operator== %s ids different"), *ItemPtr->GetItemDefinition()->GetName())
		return false;
	}

	if (ItemPtr.IsValid() != Other.ItemPtr.IsValid())
	{
		bItemDataChanged = true;
		return false;
	}

	if (ItemPtr->ItemInstances != Other.ItemPtr->ItemInstances)
	{
		bItemInstancesChanged = true;
		UE_LOG(LogItemArray, Log, TEXT("FArcItemDataInternal operator== %s item instanced different"), *ItemPtr->GetItemDefinition()->GetName())
	}
	
	if (*ItemPtr.Get() != *Other.ItemPtr.Get())
	{
		if (OwningArray)
		{
			OwningArray->ChangedItems.FindOrAdd(ItemId) = *ItemPtr;
		}
		bItemDataChanged = true;
		UE_LOG(LogItemArray, Log, TEXT("FArcItemDataInternal operator== %s item data different"), *ItemPtr->GetItemDefinition()->GetName())
		return false;
	}

	return true;
}

bool FArcItemDataInternal::Equals(const TSharedPtr<FArcItemData>& OtherItemPtr)
{
	if (ItemPtr.IsValid() != OtherItemPtr.IsValid())
	{
		//bItemDataChanged = true;
		return false;
	}

	if (ItemPtr->ItemInstances != OtherItemPtr->ItemInstances)
	{
		//bItemInstancesChanged = true;
		UE_LOG(LogItemArray, Log, TEXT("FArcItemDataInternal Equals %s item instanced different"), *ItemPtr->GetItemDefinition()->GetName())
		return false;
	}
	
	if (*ItemPtr.Get() != *OtherItemPtr.Get())
	{
		if (OwningArray)
		{
			OwningArray->ChangedItems.FindOrAdd(ItemId) = *ItemPtr;
		}
		//bItemDataChanged = true;
		UE_LOG(LogItemArray, Log, TEXT("FArcItemDataInternal::Equals %s item data different"), *ItemPtr->GetItemDefinition()->GetName())
		return false;
	}
	
	return true;
}

FArcItemDataInternal& FArcItemDataInternal::operator=(const FArcItemDataInternal& Other)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FArcItemDataInternal::operator=");

	OwningArray = Other.OwningArray;
	if (Equals(Other.ItemPtr) == false)
	{
		TMap<const UScriptStruct*, const FArcScalableFloatItemFragment*> ScalableFloatFragmentsTempCopy;
		if (ItemPtr.IsValid() && ItemId == Other.ItemId)
		{
			ScalableFloatFragmentsTempCopy = MoveTemp(ItemPtr->ScalableFloatFragments);
		}

		ItemId = Other.ItemId;

		if (Other.ItemPtr.IsValid() == false)
		{
			ItemPtr.Reset();
			return *this;
		}
		if (ItemPtr.IsValid() == false)
		{
			ItemPtr = MakeShareable(new FArcItemData);
		}

		*ItemPtr.Get() = *Other.ItemPtr.Get();

		if (ItemPtr.IsValid() && ItemId == Other.ItemId)
		{
			ItemPtr->ScalableFloatFragments = MoveTemp(ScalableFloatFragmentsTempCopy);
		}
		
		UE_LOG(LogItemArray, Log, TEXT("FArcItemDataInternal::operator= Deep Copy"))
	}
	
	return *this;
}

FArcItemDataInternal& FArcItemDataInternal::operator=(FArcItemDataInternal&& Other)
{
	ItemId = Other.ItemId;
	ItemPtr = MoveTemp(Other.ItemPtr);
	OwningArray = MoveTemp(Other.OwningArray);
	
	UE_LOG(LogItemArray, Log, TEXT("FArcItemDataInternal operator= Move"))

	ItemPtr->SetItemInstances(ItemPtr->ItemInstances);
	return *this;
}

void FArcItemDataInternal::PreReplicatedRemove(const FArcItemsArray& InArraySerializer)
{
	OnItemRemovedDelegate.Broadcast(ItemId);
	if (ItemPtr.IsValid())
	{
		ItemPtr->PreReplicatedRemove(InArraySerializer);
		InArraySerializer.RemoveWeakCachedItem(ItemId);
		InArraySerializer.RemoveCachedItem(ItemId);
	}
}

void FArcItemDataInternal::PostReplicatedAdd(const FArcItemsArray& InArraySerializer)
{
	if (ItemPtr.IsValid())
	{
		InArraySerializer.AddWeakCachedItem(ItemId, ItemPtr);
		InArraySerializer.AddCachedItem(ItemId, ItemPtr.Get());
		//ItemPtr->ItemId = ItemId;
		ItemPtr->PostReplicatedAdd(InArraySerializer);
	}
}

void FArcItemDataInternal::PostReplicatedChange(const FArcItemsArray& InArraySerializer)
{

	if (ItemPtr.IsValid() && bItemDataChanged)
	{
		bItemDataChanged = false;
		bItemInstancesChanged = false;
		ItemPtr->PostReplicatedChange(InArraySerializer);
	}

	if (bItemInstancesChanged)
	{
		bItemInstancesChanged = false;
		if (ItemPtr.IsValid())
		{
			ItemPtr->OnItemChanged();
		}
	}
}

void FArcItemDataInternal::AddStructReferencedObjects(FReferenceCollector& Collector) const
{
	//if (ItemPtr.IsValid())
	//{
	//	TObjectPtr<const UArcItemDefinition> Def = GetItem()->GetItemDefinition();
	//	Collector.AddReferencedObject<UArcItemDefinition>(Def);
	//}
}

void FArcItemCopyContainerHelper::CopyPersistentInstances(FArcItemSpec& OutSpec, const FArcItemData* InData)
{
	for (const FArcItemInstanceInternal& Instance : InData->ItemInstances.Data)
	{
		if (Instance.Data->ShouldPersist())
		{
			OutSpec.InitialInstanceData.Add(Instance.Data->Duplicate());
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

FArcItemCopyContainerHelper FArcItemCopyContainerHelper::New(UArcItemsStoreComponent* InItemsStore, const FArcItemDataInternal& InDataInternal)
{
	FArcItemCopyContainerHelper Container;
	Container.Item = InDataInternal.GetItem()->Spec;
	Container.Item.ItemId = InDataInternal.GetItem()->GetItemId();
	Container.SlotId = InDataInternal.GetItem()->GetSlotId();
	CopyPersistentInstances(Container.Item, InDataInternal.GetItem().Get());

	TArray<const FArcItemDataInternal*> AttachedItems = InItemsStore->GetItemsArray().GetInternalItemsAttachedTo(InDataInternal.GetItemId());

	for (const FArcItemDataInternal* Item : AttachedItems)
	{
		if (Item)
		{
			int32 Idx = Container.ItemAttachments.Add( {Item->GetItem()->GetItemId(), Item->GetItem()->Spec, Item->GetItem()->GetAttachSlot() });
			Container.ItemAttachments[Idx].Item.ItemId = Item->GetItem()->GetItemId();
			CopyPersistentInstances(Container.ItemAttachments[Idx].Item, Item->GetItem().Get());
		}
	}

	return Container;
}

FArcItemCopyContainerHelper FArcItemCopyContainerHelper::New(UArcItemsStoreComponent* InItemsStore, const FArcItemData* InData)
{
	FArcItemCopyContainerHelper Container;
	Container.Item = InData->Spec;
	Container.Item.ItemId = InData->GetItemId();
	Container.SlotId = InData->GetSlotId();
	CopyPersistentInstances(Container.Item, InData);

	TArray<const FArcItemData*> AttachedItems = InItemsStore->GetItemsArray().GetItemsAttachedTo(InData->GetItemId());

	for (const FArcItemData* Item : AttachedItems)
	{
		if (Item)
		{
			int32 Idx = Container.ItemAttachments.Add( { Item->GetItemId(), Item->Spec, Item->GetAttachSlot() });
			Container.ItemAttachments[Idx].Item.ItemId = Item->GetItemId();
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

FArcItemDataInternalWrapper::FArcItemDataInternalWrapper(const FArcItemSpec& InItem)
{
	TSharedPtr<FArcItemData> NewEntry = FArcItemData::NewFromSpec(InItem);
	Item.SetItemData(NewEntry
		, InItem);
}

FArcItemDataInternalWrapper::FArcItemDataInternalWrapper(const FArcItemDataInternalWrapper& InItem)
{
	UE_LOG(LogItemArray, Log, TEXT("FArcItemDataInternalWrapper ctor Copy"))
	Item = InItem.Item;
}

FArcItemDataInternalWrapper::FArcItemDataInternalWrapper(FArcItemDataInternalWrapper&& InItem)
{
	UE_LOG(LogItemArray, Log, TEXT("FArcItemDataInternalWrapper ctor Move"))
	Item = MoveTemp(InItem.Item);
}

FArcItemDataInternalWrapper::FArcItemDataInternalWrapper(FArcItemDataInternal&& InternalItem)
{
	Item = MoveTemp(InternalItem);
}

FArcItemDataInternalWrapper& FArcItemDataInternalWrapper::operator=(const FArcItemDataInternalWrapper& Other)
{
	UE_LOG(LogItemArray, Log, TEXT("FArcItemDataInternalWrapper operator= DeepCopy"))
	*Item.ItemPtr.Get() = *Other.Item.ItemPtr.Get();

	return *this;
}

FArcItemDataInternalWrapper& FArcItemDataInternalWrapper::operator=(FArcItemDataInternalWrapper&& Other)
{
	UE_LOG(LogItemArray, Log, TEXT("FArcItemDataInternalWrapper operator= Move"))
	Item = MoveTemp(Other.Item);
	
	return *this;
}

void FArcItemDataInternalWrapper::PreReplicatedRemove(const FArcItemsArray& InArraySerializer)
{
	Item.PreReplicatedRemove(InArraySerializer);
}

void FArcItemDataInternalWrapper::PostReplicatedAdd(const FArcItemsArray& InArraySerializer)
{
	Item.OwningArray = &InArraySerializer;

	Item.PostReplicatedAdd(InArraySerializer);
}

void FArcItemDataInternalWrapper::PostReplicatedChange(const FArcItemsArray& InArraySerializer)
{
	Item.PostReplicatedChange(InArraySerializer);
}

bool FArcItemDataInternalWrapper::operator==(const FArcItemDataInternalWrapper& Other) const
{
	return Item == Other.Item;
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

void FArcItemsArray::AddInternalItem(UArcItemsStoreComponent* NewItemStoreComponent, FArcItemDataInternal&& InItem
	, TArray<FArcItemDataInternal*>& AttachedItems)
{
	FArcItemDataInternalWrapper Item(MoveTemp(InItem));

	Item.GetInternalItem()->GetItem()->Initialize(NewItemStoreComponent);
	
	int32 Idx = Items.Add(MoveTemp(Item));

	AddCachedItem(Items[Idx].Item.GetItem()->GetItemId(), Items[Idx].Item.GetItem().Get());
	AddWeakCachedItem(Items[Idx].Item.GetItem()->GetItemId(), Items[Idx].Item.GetItem());
	
	for (FArcItemDataInternal* AttachedItem : AttachedItems)
	{
		FArcItemDataInternalWrapper InternalAttachedItem(MoveTemp(*AttachedItem));

		InternalAttachedItem.GetInternalItem()->GetItem()->Initialize(NewItemStoreComponent);
	
		int32 AttachIdx = Items.Add(MoveTemp(InternalAttachedItem));
		AddCachedItem(Items[AttachIdx].Item.GetItem()->GetItemId(), Items[AttachIdx].Item.GetItem().Get());
		AddWeakCachedItem(Items[AttachIdx].Item.GetItem()->GetItemId(), Items[AttachIdx].Item.GetItem());
		
		Items[AttachIdx].ToItem()->AttachToItem(Items[Idx].ToItem()->ItemId, Items[AttachIdx].ToItem()->GetAttachSlot());

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
		if (Wrapper.ToItem()->GetOwnerId().IsValid())
		{
			continue;
		}
		
		Out.Add(FArcItemCopyContainerHelper::New(Owner, *Wrapper.GetInternalItem()));
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
	FArcItemDataInternalWrapper Item(NewItemSpec);
	int32 Idx = Items.Add(MoveTemp(Item));
	Edit().MarkItemDirty(Items[Idx]);
	
	AddCachedItem(Items[Idx].Item.GetItem()->GetItemId(), Items[Idx].Item.GetItem().Get());
	AddWeakCachedItem(Items[Idx].Item.GetItem()->GetItemId(), Items[Idx].Item.GetItem());
	
	return Items.Num() - 1;;
}

void FArcItemsArray::RemoveItem(const FArcItemId& InItemId)
{
	RemoveCachedItem(InItemId);
	RemoveWeakCachedItem(InItemId);
	
	int32 Idx = IndexOf(InItemId);
	if (Items[Idx].Item.GetItem())
	{
		Items[Idx].Item.GetItem()->Deinitialize();	
	}
	

	//Copy.SetItemInstance(nullptr);
	//Copy.SetItemSpec(nullptr);
	
	MarkItemDirtyHandle(InItemId);

	//Entry.Reset();
	//Items[Idx].Item.Reset();
	
	Items.RemoveAt(Idx);
	Edit().MarkArrayDirty();
}

void FArcItemsArray::RemoveItem(int32 Idx)
{
	RemoveCachedItem(Items[Idx].Item.GetItemId());
	RemoveWeakCachedItem(Items[Idx].Item.GetItemId());
	;
	MarkItemDirtyIdx(Idx);
	Edit().MarkArrayDirty();

	Items.RemoveAt(Idx);
}

void FArcItemsArray::AddItemInstance(const FArcItemData* ToItem, UScriptStruct* InInstanceType)
{
	for (FArcItemDataInternalWrapper& Wrapper : Items)
	{
		if (Wrapper.Item.GetItem()->GetItemId() == ToItem->GetItemId())
		{
			TSharedPtr<FArcItemInstance> SharedPtr = ArcItems::AllocateInstance(InInstanceType);
			
			int32 Idx = Wrapper.Item.GetItem()->ItemInstances.Data.AddDefaulted();
			
			Wrapper.Item.GetItem()->ItemInstances.Data[Idx].Data = SharedPtr;
			Wrapper.Item.GetItem()->SetItemInstances(Wrapper.Item.GetItem()->ItemInstances);
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
