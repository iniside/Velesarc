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



#include "Commands/ArcEquipItemCommand.h"

#include "Core/ArcCoreAssetManager.h"
#include "Equipment/ArcEquipmentComponent.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsComponent.h"
#include "Items/ArcItemsStoreComponent.h"

DEFINE_LOG_CATEGORY(LogArcEquipItemCommand);

bool FArcEquipItemCommand::CanSendCommand() const
{
	if (EquipmentComponent == nullptr)
	{
		return false;
	}
	
	if (ItemsStore == nullptr)
	{
		return false;	
	}

	if (ItemsStore->IsSlotLocked(SlotId))
	{
		UE_LOG(LogArcEquipItemCommand, Log, TEXT("%s Is currently edited."), *SlotId.ToString())
		return false;
	}

	if (ItemsStore->IsItemLocked(Item))
	{
		UE_LOG(LogArcEquipItemCommand, Log, TEXT("Item %s Is currently edited."), *Item.ToString())
		return false;
	}
	
	const FArcItemData* ItemPtr = ItemsStore->GetItemPtr(Item);
	
	if (ItemPtr == nullptr)
	{
		UE_LOG(LogArcEquipItemCommand, Log, TEXT("Can Equip Item Failed."))
		return false;
	}

	const FArcItemData* SlotEntry = ItemsStore->GetItemFromSlot(SlotId);
	if (SlotEntry != nullptr)
	{
		if (ItemPtr->GetItemId() == SlotEntry->GetItemId())
		{
			UE_LOG(LogArcEquipItemCommand, Log, TEXT("CanSendCommand Item Already Equipped CannotSend. %s"), *ItemPtr->GetItemDefinition()->GetName())
			return false;
		}
	}
	
	return true;
}

void FArcEquipItemCommand::PreSendCommand()
{
	if (ItemsStore->GetNetMode() == ENetMode::NM_Client)
	{
		ItemsStore->LockSlot(SlotId);
		ItemsStore->LockItem(Item);
	}
}

bool FArcEquipItemCommand::Execute()
{	
	if (ItemsStore == nullptr)
	{
		return false;
	}

	static int32 CommandIdx = 0;
	CommandIdx++;
	UE_LOG(LogArcEquipItemCommand, Log, TEXT("FArcEquipItemCommand::Execute %d"), CommandIdx)
	
	const FArcItemData* ItemPtr = ItemsStore->GetItemPtr(Item);
	if (ItemPtr == nullptr)
	{
		UE_LOG(LogArcEquipItemCommand, Log, TEXT("Authority new item is invalid."))
		return false;
	}
	
	const FArcItemData* SlotEntry = ItemsStore->GetItemFromSlot(SlotId);
	if (SlotEntry != nullptr)
	{
		if (SlotEntry->GetItemId() == ItemPtr->GetItemId())
		{
			UE_LOG(LogArcEquipItemCommand, Log, TEXT("Authority Item Already Equipped %s"), *ItemPtr->GetItemDefinition()->GetName())
			return false;
		}
		
		UE_LOG(LogArcEquipItemCommand, Log, TEXT("1. Remove Item %s From %s"), *GetNameSafe(SlotEntry->GetItemDefinition()), *GetNameSafe(ItemsStore))
		
		ItemsStore->RemoveItemFromSlot(SlotEntry->GetItemId());
	}
	
	UE_LOG(LogArcEquipItemCommand, Log, TEXT("4. Add Item %s Slot %s To %s From %s"), *GetNameSafe(ItemPtr->GetItemDefinition()), *SlotId.ToString(), *GetNameSafe(ItemsStore), *GetNameSafe(ItemsStore))
	ItemsStore->AddItemToSlot(Item, SlotId);

	EquipmentComponent->EquipItem(ItemsStore, Item, SlotId);
	
	return true;	
}

bool FArcEquipNewItemCommand::CanSendCommand() const
{
	if (EquipmentComponent == nullptr)
	{
		return false;
	}
	
	if (ItemsStore == nullptr)
	{
		return false;	
	}

	if (ItemDefinitionId.IsValid() == false)
	{
		UE_LOG(LogArcEquipItemCommand, Log, TEXT("ItemDefinitionId is not valid."))
		return false;
	}
	if (ItemsStore->IsSlotLocked(SlotId))
	{
		UE_LOG(LogArcEquipItemCommand, Log, TEXT("%s Is currently edited."), *SlotId.ToString())
		return false;
	}

	if (EquipmentComponent->CanEquipItem(ItemDefinitionId, SlotId) == false)
	{
		UE_LOG(LogArcEquipItemCommand, Log, TEXT("Cannot equip item %s to slot %s"), *ItemDefinitionId.ToString(), *SlotId.ToString())
		return false;
	}
	
	return true;
}

void FArcEquipNewItemCommand::PreSendCommand()
{
	ItemsStore->LockSlot(SlotId);
}

bool FArcEquipNewItemCommand::Execute()
{	
	if (ItemsStore == nullptr)
	{
		return false;
	}

	static int32 CommandIdx = 0;
	CommandIdx++;
	UE_LOG(LogArcEquipItemCommand, Log, TEXT("FArcEquipItemCommand::Execute %d"), CommandIdx)

	const FArcItemData* SlotEntry = ItemsStore->GetItemFromSlot(SlotId);
	if (SlotEntry != nullptr)
	{		
		UE_LOG(LogArcEquipItemCommand, Log, TEXT("1. Remove Item %s From %s"), *GetNameSafe(SlotEntry->GetItemDefinition()), *GetNameSafe(ItemsStore))
		if (bRemoveExistingItemFromStore)
		{
			ItemsStore->RemoveItem(SlotEntry->GetItemId(), -1, true);
		}
		else
		{
			ItemsStore->RemoveItemFromSlot(SlotEntry->GetItemId());	
		}
	}
	
	FArcItemSpec Spec = FArcItemSpec::NewItem(ItemDefinitionId, 1, 1);
	Spec.SetItemDefinition(ItemDefinitionId).SetAmount(1);
	FArcItemId NewItem = ItemsStore->AddItem(Spec, FArcItemId::InvalidId);
	
	
	UE_LOG(LogArcEquipItemCommand, Log, TEXT("4. Add Item %s Slot %s To %s From %s"), *GetNameSafe(Spec.GetItemDefinition()), *SlotId.ToString(), *GetNameSafe(ItemsStore), *GetNameSafe(ItemsStore))
	ItemsStore->AddItemToSlot(NewItem, SlotId);

	EquipmentComponent->EquipItem(ItemsStore, NewItem, SlotId);
	
	return true;	
}