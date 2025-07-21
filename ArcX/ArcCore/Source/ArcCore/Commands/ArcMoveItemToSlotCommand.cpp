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

#include "Commands/ArcMoveItemToSlotCommand.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsComponent.h"
#include "Items/ArcItemsStoreComponent.h"

DEFINE_LOG_CATEGORY(LogArcMoveItemToSlotCommand);

bool FArcMoveItemToSlotCommand::CanSendCommand() const
{
	if (ItemsStore == nullptr)
	{
		return false;
	}
	return true;
}

void FArcMoveItemToSlotCommand::PreSendCommand()
{
}

bool FArcMoveItemToSlotCommand::Execute()
{
	if (ItemsStore)
	{
		const FArcItemData* ItemEntry = ItemsStore->GetItemFromSlot(SlotId);
		if (ItemEntry != nullptr)
		{
			UE_LOG(LogArcMoveItemToSlotCommand, Log, TEXT("1. Remove Item %s From %s"), *ItemEntry->GetItemId().ToString(), *GetNameSafe(ItemsStore))
			
			ItemsStore->RemoveItemFromSlot(ItemEntry->GetItemId());
		}
	}
	
	UE_LOG(LogArcMoveItemToSlotCommand, Log, TEXT("4. Add Item %s Slot %s To %s From %s"), *Item.ToString(), *SlotId.ToString(), *GetNameSafe(ItemsStore), *GetNameSafe(ItemsStore))
	ItemsStore->AddItemToSlot(Item
		, SlotId);
		
	return true;
}