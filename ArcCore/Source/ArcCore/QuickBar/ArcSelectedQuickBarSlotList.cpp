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

#include "QuickBar/ArcSelectedQuickBarSlotList.h"

#include "ArcQuickBarComponent.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Items/ArcItemsStoreComponent.h"
#include "QuickSlot/ArcQuickSlotHandler.h"

void FArcSelectedQuickBarSlotList::HandlePendingAddedQuickSlots()
{
	UArcCoreAbilitySystemComponent* ArcASC = QuickBar->GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	UArcQuickBarSubsystem* QuickBarSubsystem = UArcQuickBarSubsystem::Get(QuickBar.Get());

	TArray<PendingSlot> Copy = PendingAddQuickSlotItems;
	for (const PendingSlot& Pending : Copy)
	{
		UArcItemsStoreComponent* ItemsStoreComponent = QuickBar->GetItemStoreComponent(Pending.BarId);

		const int32 BarIdx = QuickBar->QuickBars.IndexOfByKey(Pending.BarId);
		const FArcQuickBarSlot* QuickSlotPtr = QuickBar->FindQuickSlot(Pending.BarId, Pending.QuickSlotId);
	
		const FArcItemData* ItemData = ItemsStoreComponent->GetItemPtr(Pending.ItemId);

		if (!ItemData)
		{
			return;
		}
	
		for (const FInstancedStruct& IS : QuickSlotPtr->SelectedHandlers)
		{
			IS.GetPtr<FArcQuickSlotHandler>()->OnSlotSelected(ArcASC
				, QuickBar.Get()
				, ItemData
				, &QuickBar->QuickBars[BarIdx]
				, QuickSlotPtr);
		}
	
		QuickBarSubsystem->BroadcastActorOnQuickSlotActivated(QuickBar->GetOwner()
		, QuickBar.Get()
		, Pending.BarId
		, Pending.QuickSlotId
		, Pending.ItemId);

		QuickBarSubsystem->OnQuickSlotActivated.Broadcast(QuickBar.Get(), Pending.BarId, Pending.QuickSlotId, Pending.ItemId);

		int32 Idx = PendingAddQuickSlotItems.IndexOfByKey(Pending.ItemId);
		PendingAddQuickSlotItems.RemoveAt(Idx);
	}
		
	PendingAddQuickSlotItems.Empty();
}

void FArcSelectedQuickBarSlotList::RemoveQuickSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
{
	for (int32 Idx = 0; Idx < Items.Num(); Idx++)
	{
		if (Items[Idx].BarId == BarId && Items[Idx].QuickSlotId == QuickSlotId)
		{
			Edit().Remove(Idx);
			return;
		}
	}
}

void FArcSelectedQuickBarSlotList::ActivateQuickSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
{
	const int32 Index = IndexOf(BarId, QuickSlotId);
	if (Index == INDEX_NONE)
	{
		return;
	}

	Items[Index].bIsSlotActive = true;
	Edit().Edit(Index);
}

bool FArcSelectedQuickBarSlotList::IsQuickSlotSelected(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId) const
{
	const int32 Index = IndexOf(BarId, QuickSlotId);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	return Items[Index].bIsSlotActive;
}

void FArcSelectedQuickBarSlotList::DeactivateQuickSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
{
	const int32 Index = IndexOf(BarId, QuickSlotId);
	if (Index == INDEX_NONE)
	{
		return;
	}

	Items[Index].bIsSlotActive = false;

	Edit().Edit(Index);
}

TArray<FGameplayTag> FArcSelectedQuickBarSlotList::GetAllActiveSlots(const FGameplayTag& InBarId) const
{
	TArray<FGameplayTag> OutSlots;
	for (const FArcSelectedQuickBarSlot& QuickSlot : Items)
	{
		if (QuickSlot.BarId == InBarId)
		{
			if (QuickSlot.bIsSlotActive)
			{
				OutSlots.Add(QuickSlot.QuickSlotId);	
			}
		}
	}
	return OutSlots;
}

FGameplayTag FArcSelectedQuickBarSlotList::FindFirstSelectedSlot(const FGameplayTag& InBarId) const
{
	for (const FArcSelectedQuickBarSlot& QuickSlot : Items)
	{
		if (QuickSlot.BarId == InBarId)
		{
			if (QuickSlot.bIsSlotActive)
			{
				return QuickSlot.QuickSlotId;	
			}
		}
	}

	return FGameplayTag::EmptyTag;
}

int32 FArcSelectedQuickBarSlotList::IndexOf(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId) const
{
	return Items.IndexOfByPredicate([BarId, QuickSlotId](const FArcSelectedQuickBarSlot& QuickSlotItem)
	{
		return QuickSlotItem.BarId == BarId && QuickSlotItem.QuickSlotId == QuickSlotId;
	});
}

FArcSelectedQuickBarSlot* FArcSelectedQuickBarSlotList::Find(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId)
{
	return Items.FindByPredicate([BarId, QuickSlotId](const FArcSelectedQuickBarSlot& Slot)
	{
		return BarId == Slot.BarId && QuickSlotId == Slot.QuickSlotId;
	});
}

const FArcSelectedQuickBarSlot* FArcSelectedQuickBarSlotList::Find(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId) const
{
	return Items.FindByPredicate([BarId, QuickSlotId](const FArcSelectedQuickBarSlot& Slot)
	{
		return BarId == Slot.BarId && QuickSlotId == Slot.QuickSlotId;
	});
}

const FGameplayTag FArcSelectedQuickBarSlotList::FindItemSlot(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId) const
{
	const FArcSelectedQuickBarSlot* QuickBarSlot = Find(BarId, QuickSlotId);
	if (QuickBarSlot != nullptr)
	{
		return QuickBarSlot->ItemSlot;
	}

	return FGameplayTag::EmptyTag;
}

const FArcItemId FArcSelectedQuickBarSlotList::FindItemId(const FGameplayTag& BarId, const FGameplayTag& QuickSlotId) const
{
	const FArcSelectedQuickBarSlot* QuickBarSlot = Find(BarId, QuickSlotId);
	if (QuickBarSlot != nullptr)
	{
		return QuickBarSlot->AssignedItemId;
	}

	return FArcItemId::InvalidId;
}

bool FArcSelectedQuickBarSlotList::IsItemOnAnySlot(const FArcItemId& InItemId) const
{
	for (const FArcSelectedQuickBarSlot& Item : Items)
	{
		if (Item.AssignedItemId == InItemId)
		{
			return true;
		}
	}

	return false;
}

TPair<FGameplayTag, FGameplayTag> FArcSelectedQuickBarSlotList::FindItemBarAndSlot(const FArcItemId& InItemId) const
{
	for (const FArcSelectedQuickBarSlot& Item : Items)
	{
		if (Item.AssignedItemId == InItemId)
		{
			return TPair<FGameplayTag, FGameplayTag>(Item.BarId, Item.QuickSlotId);
		}
	}

	return TPair<FGameplayTag, FGameplayTag>();
}
