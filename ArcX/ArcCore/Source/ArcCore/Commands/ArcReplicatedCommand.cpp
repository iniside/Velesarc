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

#include "Commands/ArcReplicatedCommand.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemsStoreComponent.h"

void FArcReplicatedCommand::CaptureExpectedVersions(const UArcItemsStoreComponent* Store)
{
	if (!Store)
	{
		return;
	}

	TArray<FArcItemId> Items;
	GetPendingItems(Items);

	ExpectedVersions.Empty(Items.Num());
	for (const FArcItemId& ItemId : Items)
	{
		if (const FArcItemData* ItemData = Store->GetItemPtr(ItemId))
		{
			ExpectedVersions.Add(ItemId, ItemData->GetVersion());
		}
	}
}

bool FArcReplicatedCommand::ValidateVersions(const UArcItemsStoreComponent* Store) const
{
	if (!Store || ExpectedVersions.IsEmpty())
	{
		return true;
	}

	for (const auto& Pair : ExpectedVersions)
	{
		const FArcItemData* ItemData = Store->GetItemPtr(Pair.Key);
		if (!ItemData)
		{
			return false;
		}

		if (ItemData->GetVersion() != Pair.Value)
		{
			return false;
		}
	}

	return true;
}