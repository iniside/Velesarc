/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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

#include "ArcDepositItemToCraftStationCommand.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemsArray.h"
#include "Items/ArcItemsStoreComponent.h"
#include "ArcCraft/Station/ArcCraftStationComponent.h"

bool FArcDepositItemToCraftStationCommand::CanSendCommand() const
{
	if (!SourceStore || !CraftStation)
	{
		return false;
	}

	if (!ItemId.IsValid())
	{
		return false;
	}

	if (SourceStore->IsPending(ItemId))
	{
		return false;
	}

	const FArcItemData* ItemData = SourceStore->GetItemPtr(ItemId);
	if (!ItemData)
	{
		return false;
	}

	return true;
}

void FArcDepositItemToCraftStationCommand::PreSendCommand()
{
	if (SourceStore)
	{
		PendingItemIds.Add(ItemId);
		SourceStore->AddPendingItems(PendingItemIds);
		CaptureExpectedVersions(SourceStore);
	}
}

void FArcDepositItemToCraftStationCommand::CommandConfirmed(bool bSuccess)
{
	if (!bSuccess && SourceStore)
	{
		SourceStore->RemovePendingItems(PendingItemIds);
	}
}

bool FArcDepositItemToCraftStationCommand::Execute()
{
	if (!SourceStore || !CraftStation)
	{
		return false;
	}

	if (!ValidateVersions(SourceStore))
	{
		return false;
	}

	const FArcItemData* ItemData = SourceStore->GetItemPtr(ItemId);
	if (!ItemData)
	{
		return false;
	}

	// Determine how many stacks to transfer
	const int32 ItemStacks = static_cast<int32>(ItemData->GetStacks());
	const int32 StacksToTransfer = (Stacks <= 0 || Stacks >= ItemStacks)
		? ItemStacks
		: Stacks;

	// Create spec from the item data, preserving instance data
	FArcItemSpec Spec = FArcItemCopyContainerHelper::ToSpec(ItemData);
	Spec.Amount = static_cast<uint16>(StacksToTransfer);

	// Deposit into the crafting station (station resolves entity vs actor storage)
	const bool bDeposited = CraftStation->DepositItem(Spec, SourceStore->GetOwner());
	if (!bDeposited)
	{
		return false;
	}

	// Remove from source store
	if (StacksToTransfer >= ItemStacks)
	{
		SourceStore->DestroyItem(ItemId);
	}
	else
	{
		SourceStore->RemoveItem(ItemId, StacksToTransfer);
	}

	return true;
}
