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

bool FArcItemStackMethod::TryStackSpec(TArray<FArcItemSpec>& ExistingSpecs, FArcItemSpec&& InSpec) const
{
	return false;
}

FArcItemId FArcItemStackMethod_CanNotStack::StackCheck(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec, uint16& OutNewStacks, uint16& OutRemainingStacks) const
{
	return FArcItemId::InvalidId;
}

bool FArcItemStackMethod_CanNotStack::CanStack(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const
{
	return false;
}

bool FArcItemStackMethod_CanNotStack::CanAdd(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const
{
	return true;
}

bool FArcItemStackMethod_CanNotStack::TryStackSpec(TArray<FArcItemSpec>& ExistingSpecs, FArcItemSpec&& InSpec) const
{
	return false;
}

///////////////////////////////
FArcItemId FArcItemStackMethod_CanNotStackUnique::StackCheck(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec, uint16& OutNewStacks, uint16& OutRemainingStacks) const
{
	return FArcItemId::InvalidId;
}

bool FArcItemStackMethod_CanNotStackUnique::CanStack(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const
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

bool FArcItemStackMethod_CanNotStackUnique::TryStackSpec(TArray<FArcItemSpec>& ExistingSpecs, FArcItemSpec&& InSpec) const
{
	const FPrimaryAssetId InDefId = InSpec.GetItemDefinitionId();
	for (const FArcItemSpec& Existing : ExistingSpecs)
	{
		if (Existing.GetItemDefinitionId() == InDefId)
		{
			// Duplicate of unique item — reject silently
			return true;
		}
	}
	return false;
}

namespace ArcItemStackMethodInternal
{
	bool MergeSpecIntoArray(
		TArray<FArcItemSpec>& ExistingSpecs,
		FArcItemSpec&& InSpec,
		int32 MaxStacks)
	{
		const FPrimaryAssetId InDefId = InSpec.GetItemDefinitionId();
		bool bFoundMatch = false;
		int32 RemainingAmount = static_cast<int32>(InSpec.Amount);

		for (int32 i = 0; i < ExistingSpecs.Num() && RemainingAmount > 0; ++i)
		{
			if (ExistingSpecs[i].GetItemDefinitionId() != InDefId)
			{
				continue;
			}

			bFoundMatch = true;

			const int32 CurrentAmount = static_cast<int32>(ExistingSpecs[i].Amount);
			if (CurrentAmount >= MaxStacks)
			{
				continue;
			}

			const int32 Space = MaxStacks - CurrentAmount;
			const int32 ToAdd = FMath::Min(Space, RemainingAmount);
			ExistingSpecs[i].Amount = static_cast<uint16>(CurrentAmount + ToAdd);
			RemainingAmount -= ToAdd;
		}

		if (!bFoundMatch)
		{
			return false;
		}

		if (RemainingAmount > 0)
		{
			InSpec.Amount = static_cast<uint16>(RemainingAmount);
			ExistingSpecs.Add(MoveTemp(InSpec));
		}

		return true;
	}
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
			OutNewStacks = NewStacks;
			OutRemainingStacks = 0;
		}
		return ItemData->GetItemId();
	}

	return FArcItemId::InvalidId;
}

bool FArcItemStackMethod_StackByType::CanStack(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const
{
	const FArcItemData* ItemData = Owner->GetItemByDefinition(InSpec.GetItemDefinition());
	if (ItemData)
	{
		return true;
	}

	return false;
}

bool FArcItemStackMethod_StackByType::CanAdd(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const
{
	if (!Owner)
	{
		return false;
	}

	return true;
}

bool FArcItemStackMethod_StackByType::TryStackSpec(TArray<FArcItemSpec>& ExistingSpecs, FArcItemSpec&& InSpec) const
{
	const int32 Max = FMath::FloorToInt(MaxStacks.GetValue());
	return ArcItemStackMethodInternal::MergeSpecIntoArray(ExistingSpecs, MoveTemp(InSpec), Max);
}
