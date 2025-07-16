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

#include "Commands/ArcAddItemToItemSocketCommand.h"

#include "Items/ArcItemsComponent.h"
#include "Items/ArcItemsStoreComponent.h"

bool FArcAddItemToItemSocketCommand::CanSendCommand() const
{
	if (ToComponent == nullptr)
	{
		return false;
	}
	
	if (AttachmentItem.IsValid() == false)
	{
		return false;
	}
	
	if (TargetItem.IsValid() == false)
	{
		return false;
	}

	if (SocketSlot.IsValid() == false)
	{
		return false;
	}
	
	return true;
}

void FArcAddItemToItemSocketCommand::PreSendCommand()
{
	
}

bool FArcAddItemToItemSocketCommand::Execute()
{
	if (ToComponent == nullptr)
	{
		return false;
	}
	
	const FArcItemData* CurrentlyAttachedItem = ToComponent->FindAttachedItemOnSlot(TargetItem, SocketSlot);
		
	if (CurrentlyAttachedItem)
	{
		ToComponent->DetachItemFrom(TargetItem, CurrentlyAttachedItem->GetItemId());
	}
	
	ToComponent->InternalAttachToItem(TargetItem, AttachmentItem, SocketSlot);
	
	return true;
}
