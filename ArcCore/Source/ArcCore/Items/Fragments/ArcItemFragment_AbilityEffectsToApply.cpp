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

#include "Items/Fragments/ArcItemFragment_AbilityEffectsToApply.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"

void FArcItemFragment_AbilityEffectsToApply::OnItemInitialize(const FArcItemData* InItem) const
{
	FArcItemInstance_EffectToApply* Instance = ArcItems::FindMutableInstance<FArcItemInstance_EffectToApply>(InItem);
	if (Instance == nullptr)
	{
		return;
	}
	
	int32 SpecIdx = Instance->DefaultEffectSpecs.SpecsArray.IndexOfByKey(InItem->GetItemId());
	if (SpecIdx == INDEX_NONE)
	{
		FArcMapAbilityEffectSpecContainer::AddAbilityEffectSpecs(this
			, InItem->GetItemId()
			, InItem->GetItemsStoreComponent()
			, Instance->DefaultEffectSpecs);
	}
	else
	{
		UE_LOG(LogItemEntry
			, Log
			, TEXT("%s Effect spec already initialized.")
			, *InItem->GetItemDefinition()->GetName());
	}
}

void FArcItemFragment_AbilityEffectsToApply::OnItemChanged(const FArcItemData* InItem) const
{
	const TArray<const FArcItemData*>& ItemsInSockets = InItem->GetItemsInSockets();
	const FArcItemData* OwnerItemPtr = InItem->GetItemsStoreComponent()->GetItemPtr(InItem->GetOwnerId());
	
	TArray<FArcItemId> ExistingItems;
	ExistingItems.Add(InItem->GetItemId());
	
	for (const FArcItemData* SocketItem : ItemsInSockets)
	{
		ExistingItems.Add(SocketItem->GetItemId());
	}
	
	FArcItemInstance_EffectToApply* Instance = nullptr;
	if (OwnerItemPtr)
	{
		Instance = ArcItems::FindMutableInstance<FArcItemInstance_EffectToApply>(OwnerItemPtr);
	}
	else
	{
		Instance = ArcItems::FindMutableInstance<FArcItemInstance_EffectToApply>(InItem);	
	}
	
	if (Instance == nullptr)
	{
		if (InItem->GetItemsStoreComponent()->GetOwnerRole() == ENetRole::ROLE_Authority)
		{
			ArcItems::AddInstance<FArcItemInstance_EffectToApply>(OwnerItemPtr);
			Instance = ArcItems::FindMutableInstance<FArcItemInstance_EffectToApply>(OwnerItemPtr);
			if (Instance == nullptr)
			{
				return;
			}
		}
		if (InItem->GetItemsStoreComponent()->GetOwnerRole() < ENetRole::ROLE_Authority)
		{
			return;
		}
	}
	// Try to find index of item which does not match any of item Ids.
	int32 MissingIdx = Instance->DefaultEffectSpecs.SpecsArray.IndexOfByPredicate([&ExistingItems](const FArcEffectSpecItem& InItem)
	{
		for (const FArcItemId& Id : ExistingItems)
		{
			if (InItem.ItemId == Id)
			{
				return false;
			}
		}
		return true;
	});

	if (MissingIdx != INDEX_NONE)
	{
		Instance->DefaultEffectSpecs.SpecsArray.RemoveAt(MissingIdx);	
	}
	
	const FArcItemData* OwnerItemData = InItem->GetItemsStoreComponent()->GetItemPtr(InItem->GetOwnerId());
	if (OwnerItemData && Instance)
	{
		int32 SpecIdx = Instance->DefaultEffectSpecs.SpecsArray.IndexOfByKey(InItem->GetItemId());
		if (SpecIdx == INDEX_NONE)
		{
			FArcMapAbilityEffectSpecContainer::AddAbilityEffectSpecs(this
				, InItem->GetItemId()
				, InItem->GetItemsStoreComponent()
				, Instance->DefaultEffectSpecs);
			
		}
		else
		{
			UE_LOG(LogItemEntry
				, Log
				, TEXT("%s Effect spec already initialized.")
				, *InItem->GetItemDefinition()->GetName());
		}
	}
	for (const FArcItemData* SocketItem : ItemsInSockets)
	{
		int32 SpecIdx = Instance->DefaultEffectSpecs.SpecsArray.IndexOfByKey(SocketItem->GetItemId());
		if (SpecIdx == INDEX_NONE)
		{
			FArcMapAbilityEffectSpecContainer::AddAbilityEffectSpecs(this
				, SocketItem->GetItemId()
				, SocketItem->GetItemsStoreComponent()
				, Instance->DefaultEffectSpecs);
			
		}
		else
		{
			UE_LOG(LogItemEntry
				, Log
				, TEXT("%s Effect spec already initialized.")
				, *SocketItem->GetItemDefinition()->GetName());
		}
	}
	
}