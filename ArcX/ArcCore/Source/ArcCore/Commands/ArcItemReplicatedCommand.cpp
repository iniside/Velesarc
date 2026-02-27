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

#include "Commands/ArcItemReplicatedCommand.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemsStoreComponent.h"

void FArcItemReplicatedCommand::CaptureExpectedVersions(const UArcItemsStoreComponent* Store)
{
	if (!Store)
	{
		return;
	}

	ExpectedVersions.Reset(PendingItemIds.Num());
	for (const FArcItemId& ItemId : PendingItemIds)
	{
		if (const FArcItemData* ItemData = Store->GetItemPtr(ItemId))
		{
			ExpectedVersions.Emplace(ItemId, ItemData->GetVersion());
		}
	}
}

bool FArcItemReplicatedCommand::ValidateVersions(const UArcItemsStoreComponent* Store) const
{
	if (!Store || ExpectedVersions.IsEmpty())
	{
		return true;
	}

	for (const FArcItemExpectedVersion& Entry : ExpectedVersions)
	{
		const FArcItemData* ItemData = Store->GetItemPtr(Entry.ItemId);
		if (!ItemData)
		{
			return false;
		}

		if (ItemData->GetVersion() != Entry.Version)
		{
			return false;
		}
	}

	return true;
}
