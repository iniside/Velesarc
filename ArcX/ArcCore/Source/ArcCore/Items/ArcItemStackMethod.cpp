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



#include "ArcItemStackMethod.h"

#include "GameFramework/Actor.h"
#include "ArcItemsComponent.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemDefinition.h"

#include "Items/ArcItemsStoreComponent.h"

FArcItemId FArcItemStackMethod_CanNotStack::StackCheck(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec, uint16& OutNewStacks, uint16& OutRemainingStacks) const
{
	return FArcItemId::InvalidId;
}

bool FArcItemStackMethod_CanNotStack::CanStack() const
{
	return false;
}

bool FArcItemStackMethod_CanNotStack::CanAdd(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const
{
	return true;
}

///////////////////////////////
FArcItemId FArcItemStackMethod_CanNotStackUnique::StackCheck(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec, uint16& OutNewStacks, uint16& OutRemainingStacks) const
{
	return FArcItemId::InvalidId;
}

bool FArcItemStackMethod_CanNotStackUnique::CanStack() const
{
	return false;
}

bool FArcItemStackMethod_CanNotStackUnique::CanAdd(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const
{
	if (!Owner)
	{
		return false;
	}
	const FArcItemData* ItemData = Owner->GetItemByDefinition(InSpec.GetItemDefinition());
	if (ItemData)
	{
		return false;
	}

	return true;
}

///////////////////////////////
FArcItemId FArcItemStackMethod_StackByType::StackCheck(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec, uint16& OutNewStacks, uint16& OutRemainingStacks) const
{
	const FArcItemData* ItemData = Owner->GetItemByDefinition(InSpec.GetItemDefinition());
	if (ItemData)
	{
		const uint16 Stacks = ItemData->GetStacks();
		const uint16 Max = FMath::FloorToInt(MaxStacks.GetValue());
		const uint16 NewStacks = Stacks + InSpec.Amount;
		if (NewStacks >= Max)
		{
			uint16 Remaining = NewStacks - Max;
			OutRemainingStacks = Remaining;
			OutNewStacks = Max;
		}
		else
		{
			OutNewStacks = InSpec.Amount;
			OutRemainingStacks = 0;
		}
		return ItemData->GetItemId();
	}

	return FArcItemId::InvalidId;
}

bool FArcItemStackMethod_StackByType::CanStack() const
{
	return false;
}

bool FArcItemStackMethod_StackByType::CanAdd(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const
{
	if (!Owner)
	{
		return false;
	}

	const FArcItemData* ItemData = Owner->GetItemByDefinition(InSpec.GetItemDefinition());
	if (ItemData)
	{
		return false;
	}

	return true;
}