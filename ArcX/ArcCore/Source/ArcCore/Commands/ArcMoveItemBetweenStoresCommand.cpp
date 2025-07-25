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



#include "ArcMoveItemBetweenStoresCommand.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsComponent.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/ArcItemsStoreComponent.h"

#include "Items/ArcItemStackMethod.h"
#include "Items/Fragments/ArcItemFragment_Stacks.h"

bool FArcMoveItemBetweenStoresCommand::CanSendCommand() const
{
	if (TargetStore == SourceStore)
	{
		return false;
	}
	return true;
}

void FArcMoveItemBetweenStoresCommand::PreSendCommand()
{
	
}

bool FArcMoveItemBetweenStoresCommand::Execute()
{
	const FArcItemData* ItemData = SourceStore->GetItemPtr(ItemId);
	if (!ItemData)
	{
		return false;
	}

	FArcItemCopyContainerHelper Copy = SourceStore->GetItemCopyHelper(ItemId);
	Copy.SlotId = FGameplayTag::EmptyTag;
	
	TargetStore->AddItemDataInternal(Copy);

	SourceStore->DestroyItem(ItemId);
	
	return true;
}

