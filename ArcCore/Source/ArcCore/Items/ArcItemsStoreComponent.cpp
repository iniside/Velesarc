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

#include "ArcItemsStoreComponent.h"

#include "ArcCoreGameplayTags.h"
#include "ArcItemsHelpers.h"
#include "ArcItemsStoreSubsystem.h"
#include "Engine/ActorChannel.h"
#include "Engine/NetConnection.h"
#include "Engine/PackageMapClient.h"
#include "Net/UnrealNetwork.h"

#include "ArcItemsSubsystem.h"
#include "ArcWorldDelegates.h"

#include "ArcCore/Items/ArcItemDefinition.h"
#include "ArcCore/Items/ArcItemData.h"
#include "ArcCore/Items/ArcItemsComponent.h"
#include "Fragments/ArcItemFragment_SocketSlots.h"
#include "Fragments/ArcItemFragment_Stacks.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY(LogArcItems);

const FName UArcItemsStoreComponent::NAME_ActorFeatureName(TEXT("ArcItemsStoreComponent"));

UArcItemsStoreComponentBlueprint::UArcItemsStoreComponentBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

void UArcItemsStoreComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = false;

	// TODO: Only replicate items to owner ?
	Params.Condition = COND_OwnerOnly;
	
	DOREPLIFETIME_WITH_PARAMS_FAST(UArcItemsStoreComponent, ItemsArray, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UArcItemsStoreComponent, ItemNum, Params);
	
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void UArcItemsStoreComponent::LockItem(const FArcItemId& InItem)
{
	if (HasAuthority())
	{
		return;
	}

	LockedItems.Add(InItem);

	UArcItemsSubsystem::Get(this)->BroadcastActorOnItemStateChanged(GetOwner(), this, InItem, true);
}

void UArcItemsStoreComponent::UnlockItem(const FArcItemId& InItem)
{
	if (HasAuthority())
    {
    	return;
    }
	
	LockedItems.Remove(InItem);

	UArcItemsSubsystem::Get(this)->BroadcastActorOnItemStateChanged(GetOwner(), this, InItem, false);
}

bool UArcItemsStoreComponent::IsItemLocked(const FArcItemId& InItem) const
{
	if (HasAuthority())
	{
		return false;
	}

	return LockedItems.Contains(InItem);
}

void UArcItemsStoreComponent::LockSlot(const FGameplayTag& InSlot)
{
	if (HasAuthority())
	{
		return;
	}
	
	LockedSlots.AddUnique(InSlot);
	
	UArcItemsSubsystem::Get(this)->BroadcastActorOnItemSlotStateChanged(GetOwner(), this, InSlot, true);
}

void UArcItemsStoreComponent::UnlockSlot(const FGameplayTag& InSlot)
{
	if (HasAuthority())
    {
    	return;
    }
	
	LockedSlots.Remove(InSlot);
	
	UArcItemsSubsystem::Get(this)->BroadcastActorOnItemSlotStateChanged(GetOwner(), this, InSlot, false);
}

bool UArcItemsStoreComponent::IsSlotLocked(const FGameplayTag& InSlot) const
{
	if (HasAuthority())
	{
		return false;
	}
	
	return LockedSlots.Contains(InSlot);
}

void UArcItemsStoreComponent::LockAttachmentSlot(const FArcItemId& OwnerId
	, const FGameplayTag& InSlot)
{
	if (HasAuthority())
	{
		return;
	}
	
	LockedAttachmentSlots.FindOrAdd(OwnerId).AddUnique(InSlot);

	UArcItemsSubsystem::Get(this)->BroadcastActorOnItemAttachmentSlotStateChanged(GetOwner(), this, OwnerId, InSlot, true);
}

void UArcItemsStoreComponent::UnlockAttachmentSlot(const FArcItemId& OwnerId
	, const FGameplayTag& InSlot)
{
	if (HasAuthority())
	{
		return;
	}
	
	LockedAttachmentSlots.FindOrAdd(OwnerId).Remove(InSlot);
	
	UArcItemsSubsystem::Get(this)->BroadcastActorOnItemAttachmentSlotStateChanged(GetOwner(), this, OwnerId, InSlot, false);
}

bool UArcItemsStoreComponent::IsAttachmentSlotLocked(const FArcItemId& OwnerId, const FGameplayTag& InSlot) const
{
	if (HasAuthority())
	{
		return false;
	}
	
	if (const TArray<FGameplayTag>* Array = LockedAttachmentSlots.Find(OwnerId))
	{
		return Array->Contains(InSlot);
	}
	
	return false;
}

const FArcItemData* UArcItemsStoreComponent::GetItemByDefinition(const UArcItemDefinition* InItemType)
{
	return ItemsArray(InItemType);
}

// Sets default values for this component's properties
UArcItemsStoreComponent::UArcItemsStoreComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every
	// frame.  You can turn these features off to improve performance if you don't need
	// them.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// ItemsComponentsArray.Owner = this;

	bWantsInitializeComponent = true;
}

void UArcItemsStoreComponent::OnRegister()
{
	ItemsArray.SetOwner(this);

	Super::OnRegister();

	RegisterInitStateFeature();
}

void UArcItemsStoreComponent::InitializeComponent()
{
	ItemsArray.SetOwner(this);
	Super::InitializeComponent();
}

// Called when the game starts
void UArcItemsStoreComponent::BeginPlay()
{
	ItemsArray.SetOwner(this);

	Super::BeginPlay();

	UArcWorldDelegates::Get(this)->BroadcastActorOnComponentBeginPlay(
		FArcComponentChannelKey(GetOwner(), GetClass())
		, this);
	UArcWorldDelegates::Get(this)->OnAnyComponentBeginPlayDynamic.Broadcast(this);
	UArcWorldDelegates::Get(this)->BroadcastClassOnComponentBeginPlay(GetClass(), this);
	TryToChangeInitState(FArcCoreGameplayTags::Get().InitState_GameplayReady);
}

int32 UArcItemsStoreComponent::SetupItem(int32 ItemIdx
										 , const FArcItemId& OwnerItemId)
{
	ItemsArray[ItemIdx]->Initialize(this);
	
	PostAdd(ItemsArray[ItemIdx]);

	return ItemIdx;
}

FArcItemId UArcItemsStoreComponent::AddItem(const FArcItemSpec& InItem
											, const FArcItemId& OwnerItemId)
{
	UGameInstance* GameInstance = GetGameInstance<UGameInstance>();
	UArcItemsSubsystem* ItemsSubsystem = GameInstance->GetSubsystem<UArcItemsSubsystem>();//::Get(this);
	
	if (bUseSubsystemForItemStore)
	{
		UArcItemsStoreSubsystem* ItemsStoreSubsystem = GameInstance->GetSubsystem<UArcItemsStoreSubsystem>();
		if (ItemsStoreSubsystem != nullptr)
		{
			FArcItemId ItemId = ItemsStoreSubsystem->AddItem(InItem, OwnerItemId, this);
			
			FArcItemData* Entry = ItemsStoreSubsystem->GetItemPtr(ItemId);
			
			ItemsSubsystem->BroadcastOnItemAddedToStore(this, Entry);
			ItemsSubsystem->OnItemAddedToStoreDynamic.Broadcast(this, Entry->GetItemId());
			ItemsSubsystem->BroadcastActorOnItemAddedToStore(GetOwner(), this, Entry);
			ItemsSubsystem->BroadcastActorOnItemAddedToStoreMap(GetOwner(), Entry->GetItemId(), this, Entry);

			return ItemId;
		}
	}
	
	bool bAlreadyExists = false;
	
	int32 Idx = ItemsArray.AddItem(InItem, bAlreadyExists);

	ItemNum = ItemsArray.Num();
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ItemNum, this);
	
	SetupItem(Idx, OwnerItemId);

	//MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ItemsArray, this);
	
	FArcItemData* Entry = ItemsArray[Idx];

	const FArcItemFragment_SocketSlots* ES = ArcItems::GetFragment<FArcItemFragment_SocketSlots>(Entry);
	if (ES != nullptr)
	{
		for (const FArcSocketSlot& SS : ES->GetSocketSlots())
		{
			if (IsValid(SS.DefaultSocketItemDefinition))
			{
				InternalAttachToItemNew(Entry->GetItemId(), SS.DefaultSocketItemDefinition, SS.SlotId);
			}
		}
	}
	
	ItemsSubsystem->BroadcastOnItemAddedToStore(this, Entry);
	ItemsSubsystem->OnItemAddedToStoreDynamic.Broadcast(this, Entry->GetItemId());
	ItemsSubsystem->BroadcastActorOnItemAddedToStore(GetOwner(), this, Entry);
	ItemsSubsystem->BroadcastActorOnItemAddedToStoreMap(GetOwner(), Entry->GetItemId(), this, Entry);

	Entry->OnItemAdded();
	
	return Entry->GetItemId();
}

void UArcItemsStoreComponent::AddLoadedItem(UArcItemsStoreComponent* NewItemStoreComponent, FArcItemData&& InItem)
{
	
}

void UArcItemsStoreComponent::MoveItemFrom(const FArcItemId& ItemId, UArcItemsStoreComponent* FromItemsStore)
{
	const FArcItemData* ItemData = GetItemPtr(ItemId);
	if (ItemData != nullptr)
	{
		UE_LOGFMT(LogArcItems, Error, "Item {0} Already exists", ItemId.ToString());
		return;
	}

	FArcItemData* ItemDataToMove = FromItemsStore->GetItemPtr(ItemId);
	if (ItemDataToMove == nullptr)
	{
		UE_LOGFMT(LogArcItems, Error, "Item {0} does not exists", ItemId.ToString());
		return;
	}

	FArcItemDataInternal* InternalItem = FromItemsStore->ItemsArray.GetInternalItem(ItemId);
	if (InternalItem == nullptr)
	{
		return;
	}

	const TArray<FArcItemId>& AttachedItems = ItemDataToMove->GetAttachedItems();
	TArray<FArcItemDataInternal*> InternalAttachedItems;
	for (const FArcItemId& AttachedItemId : AttachedItems)
	{
		FArcItemDataInternal* InternalAttachedItem = FromItemsStore->ItemsArray.GetInternalItem(AttachedItemId);
		if (InternalAttachedItem == nullptr)
		{
			continue;
		}

		InternalAttachedItems.Add(InternalAttachedItem);
	}
	
	ItemsArray.AddInternalItem(this, MoveTemp(*InternalItem), InternalAttachedItems);
	
	FromItemsStore->ItemsArray.RemoveItem(ItemId);

	for (const FArcItemId& AttachedItemId : AttachedItems)
	{
		FromItemsStore->ItemsArray.RemoveItem(AttachedItemId);
	}
}

TArray<FArcItemId> UArcItemsStoreComponent::AddItemDataInternal(FArcItemCopyContainerHelper& InContainer)
{
	TArray<FArcItemId> ItemIds = ItemsArray.AddItemCopyInternal(this, InContainer);

	//MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ItemsArray, this);

	for (const FArcItemId& ItemId : ItemIds)
	{
		FArcItemData* AddedItem = GetItemPtr(ItemId);
		AddedItem->Initialize(this);
		PostAdd(AddedItem);
	}
	return ItemIds;
}

void UArcItemsStoreComponent::RemoveItem(const FArcItemId& Item, int32 Stacks, bool bRemoveOnZeroStacks)
{
	int32 Idx = ItemsArray.IndexOf(Item);
	if (Idx <= INDEX_NONE)
	{
		return;
	}

	UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(this);
	
	FArcItemData* ItemData = GetItemPtr(Item);

	FArcItemInstance_Stacks* StacksInstance = ArcItems::FindMutableInstance<FArcItemInstance_Stacks>(ItemData);
	if (StacksInstance != nullptr)
	{
		const int32 StackNum = StacksInstance->GetStacks();

		bool bRemove = (StackNum <= Stacks);
		if (Stacks < 0)
		{
			bRemove = true;
		}
		
		if (ItemData != nullptr && bRemove && bRemoveOnZeroStacks)
		{
			ItemsSubsystem->BroadcastActorOnItemRemovedFromStoreMap(GetOwner(), Item, this, ItemData);

			ItemData->OnPreRemove();
	
			PreRemove(ItemData);

			// TODO:: Remove Attached Items ?
			const TArray<const FArcItemData*> SocketItems = GetAttachedItems(Item);
			for (const FArcItemData* SocketedItem : SocketItems)
			{
				RemoveItem(SocketedItem->GetItemId(), 1);
			}

			ItemsArray.RemoveItem(Item);
			ItemsArray.MarkArrayDirty();
			ItemsArray.MarkItemDirtyIdx(Idx);

		}
		else
		{
			const int32 RemainingStacks = StacksInstance->GetStacks() - Stacks;

			StacksInstance->SetStacks(RemainingStacks);
			ItemsArray.MarkItemDirtyIdx(Idx);
			GetOwner()->ForceNetUpdate();
		}
	}
	else
	{
	}
}

void UArcItemsStoreComponent::DestroyItem(const FArcItemId& ItemId)
{
	TArray<const FArcItemData*> AttachedItems = GetItemsAttachedTo(ItemId);

	for (int32 Idx = AttachedItems.Num() - 1; Idx > -1; Idx--)
	{
		DestroyItem(AttachedItems[Idx]->GetItemId());
	}
	
	FArcItemData* ItemData = GetItemPtr(ItemId);
	if (ItemData->GetOwnerId().IsValid())
	{
		ItemData->DetachFromItem();
	}

	RemoveItemFromSlot(ItemId);
	PreRemove(ItemData);
	ItemData->OnPreRemove();
	
	ItemsArray.RemoveItem(ItemId);

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ItemsArray, this);
}

const FArcItemData* UArcItemsStoreComponent::GetItemPtr(const FArcItemId& Handle) const
{
	if (bUseSubsystemForItemStore)
	{
		UArcItemsStoreSubsystem* ItemsSubsystem = GetGameInstance<UGameInstance>()->GetSubsystem<UArcItemsStoreSubsystem>();
		if (ItemsSubsystem != nullptr)
		{
			return ItemsSubsystem->GetItemPtr(Handle);
		}
	}
	
	return ItemsArray[Handle];
}

FArcItemData* UArcItemsStoreComponent::GetItemPtr(const FArcItemId& Handle)
{
	if (bUseSubsystemForItemStore)
	{
		UArcItemsStoreSubsystem* ItemsSubsystem = GetGameInstance<UGameInstance>()->GetSubsystem<UArcItemsStoreSubsystem>();
		if (ItemsSubsystem != nullptr)
		{
			return ItemsSubsystem->GetItemPtr(Handle);
		}
	}
	
	return ItemsArray[Handle];
}

const TWeakPtr<FArcItemData>& UArcItemsStoreComponent::GetWeakItemPtr(const FArcItemId& Handle) const
{
	return ItemsArray.GetWeakItem(Handle);
}

TWeakPtr<FArcItemData>& UArcItemsStoreComponent::GetWeakItemPtr(const FArcItemId& Handle)
{
	return ItemsArray.GetWeakItem(Handle);
}

const FArcItemDataInternal* UArcItemsStoreComponent::GetInternalItem(const FArcItemId& InItemId) const
{
	return ItemsArray.FindItemDataInternal(InItemId);
}

TArray<FArcItemCopyContainerHelper> UArcItemsStoreComponent::GetAllInternalItems() const
{
	return ItemsArray.GetAllInternalItems();
}

FArcItemCopyContainerHelper UArcItemsStoreComponent::GetItemCopyHelper(const FArcItemId& InItemId) const
{
	if (bUseSubsystemForItemStore)
	{
		UArcItemsStoreSubsystem* ItemsSubsystem = GetGameInstance<UGameInstance>()->GetSubsystem<UArcItemsStoreSubsystem>();
		if (ItemsSubsystem != nullptr)
		{
			return ItemsSubsystem->GetItem(InItemId);
		}
	}
	
	return ItemsArray.GetItemCopyHelper(InItemId);
}

const TSharedPtr<FArcItemData>& UArcItemsStoreComponent::GetItemSharedPtr(const FArcItemId& Handle) const
{
	for (const FArcItemDataInternalWrapper& Item : ItemsArray.Items)
	{
		if (Item.ToItem()->GetItemId() == Handle)
		{
			return Item.GetInternalItem()->GetItem();
		}
	}

	static const TSharedPtr<FArcItemData> Invalid = nullptr;
	return Invalid;
}

bool UArcItemsStoreComponent::Contains(const UArcItemDefinition* InItemDefinition) const
{
	return ItemsArray.Contains(InItemDefinition);
}

const FArcItemData* UArcItemsStoreComponent::GetItemByIdx(int32 Idx) const
{
	return ItemsArray[Idx];
}

void UArcItemsStoreComponent::MarkItemDirtyById(const FArcItemId& InItemId)
{
	if (GetOwnerRole() < ENetRole::ROLE_Authority)
	{
		return;
	}

	ItemsArray.MarkItemDirtyHandle(InItemId);

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass
    			, ItemsArray
    			, this);
	
	GetOwner()->ForceNetUpdate();
}

void UArcItemsStoreComponent::MarkItemInstanceDirtyById(const FArcItemId& InItemId
	, UScriptStruct* InScriptStruct)
{
	if (GetOwnerRole() < ENetRole::ROLE_Authority)
	{
		return;
	}

	ItemsArray.MarkItemInstanceDirtyHandle(InItemId, InScriptStruct);
}

const FArcItemData* UArcItemsStoreComponent::FindAttachedItemOnSlot(const FArcItemId& InOwnerId
															  , const FGameplayTag& InAttachSlot) const
{
	const TArray<FArcItemDataInternalWrapper>& Items = ItemsArray.Items;
	for (const FArcItemDataInternalWrapper& Item : Items)
	{
		if (Item.ToItem()->GetOwnerId() == InOwnerId)
		{
			const FArcItemData* ItemData = Item.ToItem();
			
			if (ItemData == nullptr)
			{
				continue;
			}

			if (ItemData->GetAttachSlot() == InAttachSlot)
			{
				return ItemData;
			}
		}
	}

	return nullptr;
}

TArray<const FArcItemData*> UArcItemsStoreComponent::GetAttachedItems(const FArcItemId& InOwnerId) const
{
	TArray<const FArcItemData*> Out;

	const TArray<FArcItemDataInternalWrapper>& Items = ItemsArray.Items;
	for (const FArcItemDataInternalWrapper& Item : Items)
	{
		if (Item.ToItem()->GetOwnerId() == InOwnerId)
		{
			const FArcItemData* ItemData = Item.ToItem();
			if (ItemData == nullptr)
			{
				continue;
			}
			Out.Add(ItemData);
		}
	}

	return Out;
}

FArcItemId UArcItemsStoreComponent::InternalAttachToItemNew(const FArcItemId& OwnerId
	, const UArcItemDefinition* AttachmentDefinition
	, const FGameplayTag& InAttachSlot)
{
	const FArcItemData* OwnerData = GetItemPtr(OwnerId);
	if (OwnerData == nullptr)
	{
		return FArcItemId();
	}

	FArcItemId Id = FArcItemId::Generate();
	FArcItemSpec AttachmentSpec;
	AttachmentSpec.SetItemId(Id).SetItemDefinition(AttachmentDefinition->GetPrimaryAssetId()).SetAmount(1);
	
	FArcItemId AttachmentId = AddItem(AttachmentSpec, OwnerData->GetItemId());
	
	const FArcItemData* AttachmentData = GetItemPtr(AttachmentId);
	if (AttachmentData == nullptr)
	{
		return FArcItemId();
	}

	
	const_cast<FArcItemData*>(AttachmentData)->AttachToItem(OwnerId, InAttachSlot);
	
	//ItemsArray.a(OwnerId, AttachmentId);
	
	MarkItemDirtyById(AttachmentId);

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass
			, ItemsArray
			, this);

	return AttachmentId;
}

FArcItemId UArcItemsStoreComponent::InternalAttachToItem(const FArcItemId& OwnerId
	, const FArcItemId& AttachedId
	, const FGameplayTag& InAttachSlot)
{
	const FArcItemData* OwnerData = GetItemPtr(OwnerId);
	if (OwnerData == nullptr)
	{
		return FArcItemId();
	}

	if (InAttachSlot.IsValid())
	{
		const FArcItemFragment_SocketSlots* AttachSlots = ArcItems::FindFragment<FArcItemFragment_SocketSlots>(OwnerData);
		if (AttachSlots == nullptr)
		{
			return FArcItemId::InvalidId;
		}

		bool bFoundSlot = false;
		for (const FArcSocketSlot& AttachSlot : AttachSlots->GetSocketSlots())
		{
			if (AttachSlot.SlotId == InAttachSlot)
			{
				bFoundSlot = true;
				break;
			}
		}
		
		if (bFoundSlot == false)
		{
			return FArcItemId::InvalidId;
		}
	}
	
	const FArcItemData* AttachmentData = GetItemPtr(AttachedId);
	if (AttachmentData == nullptr)
	{
		return FArcItemId();
	}

	const_cast<FArcItemData*>(AttachmentData)->AttachToItem(OwnerId, InAttachSlot);
	
	//OwnedItems.AttachToItem(OwnerId, AttachedId);

	//MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass
	//		, OwnedItems
	//		, this);
	
	return AttachedId;
}

void UArcItemsStoreComponent::DetachItemFrom(const FArcItemId& InOwnerId
	, const FArcItemId& InAttachmentId)
{
	const FArcItemData* OwnerData = GetItemPtr(InOwnerId);
	if (OwnerData == nullptr)
	{
		return;
	}

	const FArcItemData* AttachmentData = GetItemPtr(InAttachmentId);
	if (AttachmentData == nullptr)
	{
		return;
	}

	const_cast<FArcItemData*>(AttachmentData)->DetachFromItem();
}

void UArcItemsStoreComponent::AddItemToSlot(const FArcItemId& InItemId
											, const FGameplayTag& InSlotId)
{
	FArcItemData* Item = GetItemPtr(InItemId);
	if (Item != nullptr)
	{
		if (Item->GetOwnerId().IsValid())
		{
			UE_LOGFMT(LogArcItems, Error, "Item {0} Is attached to other item and cannot be added to standalone slot {1}", Item->GetItemDefinition()->GetName(), InSlotId.ToString());
			return;
		}
		
		UE_LOGFMT(LogArcItems, Log, "Add Item {0} to Slot {1}", Item->GetItemDefinition()->GetName(), InSlotId.ToString());
		Item->AddToSlot(InSlotId);

		UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(this);
		ItemsSubsystem->BroadcastActorOnItemChangedStore(GetOwner(), this, Item);
		ItemsSubsystem->BroadcastActorOnItemChangedStoreMap(GetOwner(), InItemId, this, Item);

		ItemsSubsystem->OnItemAddedToSlotDynamic.Broadcast(this, InSlotId, Item->GetItemId());
		ItemsSubsystem->BroadcastActorOnAddedToSlot(GetOwner(), this, InSlotId, Item);
		ItemsSubsystem->BroadcastActorOnItemAddedToSlotMap(GetOwner(), InItemId, this, Item);
		ItemsSubsystem->BroadcastActorOnAddedToSlotMap(GetOwner(), InSlotId, this, Item);
		
		MarkItemDirtyById(Item->GetItemId());
	}
}

void UArcItemsStoreComponent::RemoveItemFromSlot(const FArcItemId& InItemId)
{
	FArcItemData* Item = GetItemPtr(InItemId);
	if (Item != nullptr)
	{
		if (Item->GetOwnerId().IsValid())
		{
			UE_LOGFMT(LogArcItems, Error, "Item {0} Is attached to other item and cannot be removed from standalone slot {1} attachslot {2}", Item->GetItemDefinition()->GetName(), Item->GetSlotId().ToString(), Item->GetAttachSlot().ToString());
			return;
		}
		
		UE_LOGFMT(LogArcItems, Log, "Remove Item {0} from Slot {1}", Item->GetItemDefinition()->GetName(), Item->GetSlotId().ToString());
		UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(this);

		ItemsSubsystem->BroadcastActorOnItemChangedStore(GetOwner(), this, Item);
		ItemsSubsystem->BroadcastActorOnItemChangedStoreMap(GetOwner(), InItemId, this, Item);

		ItemsSubsystem->OnItemRemovedFromSlotDynamic.Broadcast(this, Item->GetSlotId(), Item->GetItemId());
		ItemsSubsystem->BroadcastActorOnRemovedFromSlot(GetOwner(), this, Item->GetSlotId(), Item);
		ItemsSubsystem->BroadcastActorOnRemovedFromSlotMap(GetOwner(), Item->GetSlotId(), this, Item);
		
		Item->RemoveFromSlot(FGameplayTag::EmptyTag);
		MarkItemDirtyById(Item->GetItemId());
	}
}

void UArcItemsStoreComponent::ChangeItemSlot(const FArcItemId& InItem
	, const FGameplayTag& InNewSlot)
{
	FArcItemData* Item = GetItemPtr(InItem);
	if (Item != nullptr)
	{
		Item->ChangeSlot(InNewSlot);
		MarkItemDirtyById(Item->GetItemId());
		
		UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(this);
		
		ItemsSubsystem->BroadcastActorOnItemChangedStore(GetOwner(), this, Item);
		ItemsSubsystem->BroadcastActorOnItemChangedStoreMap(GetOwner(), InItem, this, Item);
		
		ItemsSubsystem->BroadcastActorOnSlotChanged(GetOwner(), this, InNewSlot, Item);
		ItemsSubsystem->BroadcastActorOnItemSlotChangedMap(GetOwner(), Item->GetItemId(), this, Item);
	}
}

const FArcItemData* UArcItemsStoreComponent::GetItemFromSlot(const FGameplayTag& InSlotId)
{
	return ItemsArray.GetItemFromSlot(InSlotId);
}

bool UArcItemsStoreComponent::IsOnAnySlot(const FArcItemId& InItemId) const
{
	const FArcItemData* ItemData = GetItemPtr(InItemId);
	if (ItemData && ItemData->GetSlotId().IsValid())
	{
		return true;
	}
	return false;
}

TArray<const FArcItemData*> UArcItemsStoreComponent::GetItemsAttachedTo(const FArcItemId& InItemId) const
{
	return ItemsArray.GetItemsAttachedTo(InItemId);
}

const FArcItemData* UArcItemsStoreComponent::GetItemAttachedTo(const FArcItemId& InOwnerItemId
	, const FGameplayTag& AttachSlot) const
{
	for (const FArcItemDataInternalWrapper& Item : ItemsArray.Items)
	{
		if (Item.ToItem()->GetOwnerId() == InOwnerItemId && Item.ToItem()->GetAttachSlot() == AttachSlot)
		{
			return Item.ToItem();
		}
	}
	return nullptr;
}

TArray<const FArcItemDataInternal*> UArcItemsStoreComponent::GetInternalAttachtedItems(const FArcItemId& InItemId) const
{
	return ItemsArray.GetInternalItemsAttachedTo(InItemId);
}

TArray<const FArcItemData*> UArcItemsStoreComponent::GetAllItemsOnSlots() const
{
	return ItemsArray.GetAllItemsOnSlots();
}


int32 UArcItemsStoreComponent::GetItemStacks(const UArcItemDefinition* ItemType) const
{
	UE_LOG(LogTemp, Warning, TEXT("UArcItemsStoreComponent::GetItemStacks Not Implemented"));
	return 0;
}

void UArcItemsStoreComponent::AddDynamicTag(const FArcItemId& Item
	, const FGameplayTag& InTag)
{
	if (FArcItemData* ItemPtr = GetItemPtr(Item))
	{
		ItemPtr->AddDynamicTag(InTag);
		MarkItemDirtyById(Item);
	}
}

void UArcItemsStoreComponent::RemoveDynamicTag(const FArcItemId& Item
	, const FGameplayTag& InTag)
{
	if (FArcItemData* ItemPtr = GetItemPtr(Item))
	{
		ItemPtr->RemoveDynamicTag(InTag);
		MarkItemDirtyById(Item);
	}
}

void UArcItemsStoreComponent::BP_GetItems(TArray<FArcItemDataHandle>& OutItems)
{
	OutItems.Reserve(ItemsArray.Num());

	for (FArcItemDataInternalWrapper& Item : ItemsArray.Items)
	{
		OutItems.Add(Item.ToSharedPtr());
	}
}

bool UArcItemsStoreComponent::BP_GetItem(const FArcItemId& ItemId, FArcItemDataHandle& Item)
{
	TWeakPtr<FArcItemData>& ItemPtr = ItemsArray.GetWeakItem(ItemId);
	
	if (ItemPtr.IsValid())
	{
		Item = FArcItemDataHandle(ItemPtr);
		return true;
	}

	return false;
}

bool UArcItemsStoreComponent::BP_GetItemFromSlot(const FGameplayTag& ItemSlotId
	, FArcItemDataHandle& Item)
{
	TWeakPtr<FArcItemData>& ItemPtr = ItemsArray.GetWeakItemFromSlot(ItemSlotId);
	if (ItemPtr.IsValid())
	{
		Item = ItemPtr;
		return true;
	}

	return false;
}
