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

#include "AbilitySystem/ArcItemAbilityHelpers.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "ArcGameplayEffectContext.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemsStoreComponent.h"

const FArcItemData* UArcItemAbilityHelpers::GetItemData(UArcItemsStoreComponent* ItemsStore, FArcItemId ItemId)
{
	if (ItemsStore == nullptr || !ItemId.IsValid())
	{
		return nullptr;
	}
	return ItemsStore->GetItemPtr(ItemId);
}

const UArcItemDefinition* UArcItemAbilityHelpers::GetItemDefinition(UArcItemsStoreComponent* ItemsStore, FArcItemId ItemId)
{
	const FArcItemData* ItemData = GetItemData(ItemsStore, ItemId);
	if (ItemData == nullptr)
	{
		return nullptr;
	}
	return ItemData->GetItemDefinition();
}

bool UArcItemAbilityHelpers::ConsumeStacks(UArcItemsStoreComponent* ItemsStore, FArcItemId ItemId, int32 Count)
{
	if (Count <= 0 || !HasEnoughStacks(ItemsStore, ItemId, Count))
	{
		return false;
	}
	ItemsStore->RemoveItem(ItemId, Count, true);
	return true;
}

int32 UArcItemAbilityHelpers::GetStackCount(UArcItemsStoreComponent* ItemsStore, FArcItemId ItemId)
{
	const FArcItemData* ItemData = GetItemData(ItemsStore, ItemId);
	if (ItemData == nullptr)
	{
		return 0;
	}
	return static_cast<int32>(ItemData->GetStacks());
}

bool UArcItemAbilityHelpers::HasEnoughStacks(UArcItemsStoreComponent* ItemsStore, FArcItemId ItemId, int32 Required)
{
	return GetStackCount(ItemsStore, ItemId) >= Required;
}

FGameplayEffectContextHandle UArcItemAbilityHelpers::MakeItemEffectContext(UAbilitySystemComponent* ASC, UArcItemsStoreComponent* ItemsStore, FArcItemId ItemId)
{
	if (ASC == nullptr)
	{
		return FGameplayEffectContextHandle();
	}
	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	FArcGameplayEffectContext* ArcContext = static_cast<FArcGameplayEffectContext*>(ContextHandle.Get());
	if (ArcContext && ItemsStore)
	{
		ArcContext->SetSourceItemId(ItemId);
		ArcContext->SetItemStoreComponent(ItemsStore);

		const FArcItemData* ItemData = ItemsStore->GetItemPtr(ItemId);
		if (ItemData)
		{
			ArcContext->SetSourceItemPtr(const_cast<FArcItemData*>(ItemData));
			ArcContext->SetSourceItemDef(ItemData->GetItemDefinition());
		}
	}
	return ContextHandle;
}

FArcGameplayEffectContext* UArcItemAbilityHelpers::GetItemDataFromContext(FGameplayEffectContextHandle ContextHandle)
{
	if (!ContextHandle.IsValid())
	{
		return nullptr;
	}
	return static_cast<FArcGameplayEffectContext*>(ContextHandle.Get());
}

int32 UArcItemAbilityHelpers::SendItemUseEvent(UAbilitySystemComponent* ASC, UArcItemsStoreComponent* ItemsStore, FArcItemId ItemId, FGameplayTag EventTag)
{
	if (ASC == nullptr || !EventTag.IsValid())
	{
		return 0;
	}

	FGameplayEffectContextHandle ContextHandle = MakeItemEffectContext(ASC, ItemsStore, ItemId);

	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.Instigator = ASC->GetOwner();
	EventData.ContextHandle = ContextHandle;

	return ASC->HandleGameplayEvent(EventTag, &EventData);
}
