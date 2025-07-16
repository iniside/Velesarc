/**
 * This file is part of ArcX.
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

#include "Commands/ArcSetItemOnSlotCommand.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsComponent.h"
#include "Items/ArcItemsStoreComponent.h"

bool FArcSetItemOnSlotCommand::CanSendCommand() const
{
	if (Item.IsValid() == false || SlotId.IsValid() == false || ItemsStoreComponent == nullptr)
	{
		return false;
	}
	
	return true;
}

void FArcSetItemOnSlotCommand::PreSendCommand()
{
}

bool FArcSetItemOnSlotCommand::Execute()
{
	if (ItemsStoreComponent)
	{
		bool bOnAnySlot = ItemsStoreComponent->IsOnAnySlot(Item);
		if (bOnAnySlot == true)
		{
			ItemsStoreComponent->RemoveItemFromSlot(Item);
		}
		ItemsStoreComponent->AddItemToSlot(Item
			, SlotId);
	}
	
	return true;
}