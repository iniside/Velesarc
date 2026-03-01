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

#include "ArcWithdrawItemFromCraftStationCommand.h"

#include "Items/ArcItemId.h"
#include "Items/ArcItemsStoreComponent.h"
#include "ArcCraft/Station/ArcCraftStationComponent.h"

bool FArcWithdrawItemFromCraftStationCommand::CanSendCommand() const
{
	if (!CraftStation || !TargetStore)
	{
		return false;
	}

	if (ItemIndex < 0)
	{
		return false;
	}

	return true;
}

bool FArcWithdrawItemFromCraftStationCommand::Execute()
{
	if (!CraftStation || !TargetStore)
	{
		return false;
	}

	FArcItemSpec WithdrawnSpec;
	bool bSuccess = false;

	if (bWithdrawOutput)
	{
		bSuccess = CraftStation->WithdrawOutputItem(ItemIndex, Stacks, WithdrawnSpec, TargetStore->GetOwner());
	}
	else
	{
		bSuccess = CraftStation->WithdrawInputItem(ItemIndex, Stacks, WithdrawnSpec, TargetStore->GetOwner());
	}

	if (!bSuccess)
	{
		return false;
	}

	// Add to target store
	TargetStore->AddItem(WithdrawnSpec, FArcItemId::InvalidId);

	return true;
}
