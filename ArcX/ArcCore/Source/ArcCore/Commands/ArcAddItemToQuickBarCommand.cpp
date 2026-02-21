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

#include "ArcAddItemToQuickBarCommand.h"

#include "QuickBar/ArcQuickBarComponent.h"
#include "Items/ArcItemsStoreComponent.h"
#include "ArcCoreGameplayTags.h"
#include "Core/ArcCoreAssetManager.h"
#include "Items/ArcItemsHelpers.h"

bool FArcAddItemToQuickBarCommand::CanSendCommand() const
{
	if (QuickBarComponent == nullptr)
	{
		return false;
	}

	if (QuickBar.IsValid() == false)
	{
		return false;
	}

	if (QuickSlot.IsValid() == false)
	{
		return false;
	}
	
	if (ItemId.IsValid() == false)
	{
		return false;
	}

	const FArcItemData* ItemData = QuickBarComponent->FindQuickSlotItem(QuickBar, QuickSlot);
	if (ItemData != nullptr)
	{
		if (ItemData->GetItemId() == ItemId)
		{
			return false;
		}
	}
	
	return true;	
}

void FArcAddItemToQuickBarCommand::PreSendCommand()
{
	UArcItemsStoreComponent* ItemSlotComponent = QuickBarComponent->GetItemStoreComponent(QuickBar);
	
	//if (ItemSlotComponent->IsOnAnySlot(ItemId) == false)
	//{
	//	return;
	//}

	ItemsStoreComponent->LockItem(ItemId);
}

bool FArcAddItemToQuickBarCommand::Execute()
{	
	UArcItemsStoreComponent* ISC = QuickBarComponent->GetItemStoreComponent(QuickBar);

	FArcQuickBar OutQuickBar;
	UArcQuickBarComponent::BP_GetQuickBar(QuickBarComponent, QuickBar, OutQuickBar);
	int32 QuickSlotIdx = OutQuickBar.Slots.IndexOfByKey(QuickSlot);

	FGameplayTag ItemSlot = QuickSlotIdx != INDEX_NONE ? OutQuickBar.Slots[QuickSlotIdx].DefaultItemSlotId : FGameplayTag::EmptyTag;

	TPair<FGameplayTag, FGameplayTag> ExistingQuickBarSlot = QuickBarComponent->FindQuickSlotForItemId(ItemId);
	if (ExistingQuickBarSlot.Key.IsValid() && ExistingQuickBarSlot.Value.IsValid())
	{
		QuickBarComponent->RemoveQuickSlot(ExistingQuickBarSlot.Key, ExistingQuickBarSlot.Value);
	}
	
	bool bIsTaken = QuickBarComponent->IsItemOnQuickSlot(QuickBar, QuickSlot);

	FArcItemId ExistingItemId = QuickBarComponent->GetItemId(QuickBar, QuickSlot);
	if (bIsTaken && ExistingItemId.IsValid() && ExistingQuickBarSlot.Key.IsValid() && ExistingQuickBarSlot.Value.IsValid())
	{
		QuickBarComponent->RemoveQuickSlot(QuickBar, QuickSlot);
		//  We need to swap items.

		QuickBarComponent->AddAndActivateQuickSlot(ExistingQuickBarSlot.Key, ExistingQuickBarSlot.Value, ExistingItemId);
	}
	
	const FArcItemData* OldItemData = ISC->GetItemPtr(ExistingItemId);
	const FArcItemData* ItemToSlot = ISC->GetItemPtr(ItemId);
	
	if (OldItemData && ItemToSlot && ItemToSlot->GetSlotId().IsValid())
	{
		ISC->ChangeItemSlot(OldItemData->GetItemId(), ItemToSlot->GetSlotId());
	}
	
	if (ISC->IsOnAnySlot(ItemId) == false)
	{
		ISC->AddItemToSlot(ItemId, ItemSlot);
	}
	
	QuickBarComponent->AddAndActivateQuickSlot(QuickBar, QuickSlot, ItemId);
	
	//if (const FArcItemFragment_QuickBarItems* Fragment = ArcItemsHelper::FindFragment<FArcItemFragment_QuickBarItems>(ItemToSlot))
	//{
	//	int32 ChildBarIdx = OutQuickBar.Slots.IndexOfByPredicate([this](const FArcQuickBar& QuickBar)
	//		{
	//			if (QuickBar.ParentQuickSlot == QuickSlot)
	//			{
	//				return true;
	//			}
	//			return false;
	//		});
	//	
	//	if (ChildBarIdx != INDEX_NONE)
	//	{
	//		for (const FArcQuickBarItem& Item : Fragment->ItemsToAdd)
	//		{
	//			FArcItemSpec Spec = FArcItemSpec::NewItem(Item.Item, 1, 1);
	//			ISC->AddItem(Spec, FArcItemId());
	//			ISC->AddItemToSlot(ItemId, FArcCoreGameplayTags::Get().Item_SlotActive);
	//			
	//			QuickBarComponent->AddAndActivateQuickSlot(ExistingQuickBarSlot.Key, ExistingQuickBarSlot.Value, ExistingItemId);
	//		}
	//	}
	//}
	return true;
}

////////////////////////////////////////////////////////
bool FArcAddItemToQuickBarNoItemSlotCommand::CanSendCommand() const
{
	if (QuickBarComponent == nullptr)
	{
		return false;
	}

	if (QuickBar.IsValid() == false)
	{
		return false;
	}

	if (QuickSlot.IsValid() == false)
	{
		return false;
	}

	if (ItemId.IsValid() == false)
	{
		return false;
	}

	if (QuickBarComponent->IsQuickSlotLocked(QuickBar, QuickSlot))
	{
		return false;
	}
	
	if (QuickBarComponent->IsQuickBarLocked())
	{
		return false;
	}

	if (SourceItemsStoreComponent && SourceItemsStoreComponent->IsItemLocked(ItemId))
	{
		return false;
	}
	
	return true;
}

void FArcAddItemToQuickBarNoItemSlotCommand::PreSendCommand()
{
	UArcItemsStoreComponent* ItemSlotComponent = QuickBarComponent->GetItemStoreComponent(QuickBar);

	if (QuickBarComponent->GetNetMode() == ENetMode::NM_Client)
	{
		QuickBarComponent->SetLockQuickBar();
		QuickBarComponent->LockQuickSlots(QuickBar, QuickSlot);
		SourceItemsStoreComponent->LockItem(ItemId);
	}
}

bool FArcAddItemToQuickBarNoItemSlotCommand::Execute()
{
	UArcItemsStoreComponent* ISC = QuickBarComponent->GetItemStoreComponent(QuickBar);

	const FArcItemData* ItemToSlot = ISC->GetItemPtr(ItemId);

	const bool bIsOnAnyQuickSlot = QuickBarComponent->IsItemOnAnyQuickSlot(ItemId);
	
	// If it was on any quick bar slot, remove it first to correctly trigger events, like input unbind.
	if (bIsOnAnyQuickSlot)
	{
		TPair<FGameplayTag, FGameplayTag> QuickBarSlot = QuickBarComponent->FindQuickSlotForItemId(ItemId);
		QuickBarComponent->RemoveQuickSlot(QuickBarSlot.Key, QuickBarSlot.Value);
	}

	bool bIsTaken = QuickBarComponent->IsItemOnQuickSlot(QuickBar, QuickSlot);
	if (bIsTaken == true)
	{
		const FArcItemData* ExistingItemData = QuickBarComponent->FindQuickSlotItem(QuickBar, QuickSlot);
		if (ExistingItemData != nullptr)
		{
			ISC->RemoveItemFromSlot(ExistingItemData->GetItemId());
		}
		
		/**
		 * This is not replicated to server by QuickBar we need to also call it on server.
		 */
		QuickBarComponent->RemoveQuickSlot(QuickBar, QuickSlot);
	}
	
	const FArcItemData* FoundItemData = ISC->GetItemPtr(ItemId);
	if (FoundItemData == nullptr)
	{
		return false;
	}
	
	if (ISC->GetItemPtr(ItemId) == nullptr)
	{
		if (SourceItemsStoreComponent)
		{
			FArcItemCopyContainerHelper Copy = SourceItemsStoreComponent->GetItemCopyHelper(ItemId);
			ISC->AddItemDataInternal(Copy);
		}
	}

	if (ISC->IsOnAnySlot(ItemId) == false)
	{
		ISC->AddItemToSlot(ItemId, FArcCoreGameplayTags::Get().Item_SlotActive);
	}
	
	QuickBarComponent->AddAndActivateQuickSlot(QuickBar, QuickSlot, ItemId);
	

	return true;
}

bool FArcAddNewItemToQuickBarCommand::CanSendCommand() const
{
	return true;
}

void FArcAddNewItemToQuickBarCommand::PreSendCommand()
{
	
}

bool FArcAddNewItemToQuickBarCommand::Execute()
{
	UArcItemsStoreComponent* ISC = QuickBarComponent->GetItemStoreComponent(QuickBar);

	UArcItemDefinition* ItemDef = UArcCoreAssetManager::GetAsset<UArcItemDefinition>(ItemDefinitionId, false);

	FArcItemSpec Spec = FArcItemSpec::NewItem(ItemDef, 1, 1);
	Spec.SetItemDefinition(ItemDefinitionId).SetAmount(1);
	
	FArcItemId ItemId = ISC->AddItem(Spec, FArcItemId::InvalidId);

	const bool bIsOnAnyQuickSlot = QuickBarComponent->IsItemOnAnyQuickSlot(ItemId);
	
	// If it was on any quick bar slot, remove it first to correctly trigger events, like input unbind.
	if (bIsOnAnyQuickSlot)
	{
		TPair<FGameplayTag, FGameplayTag> QuickBarSlot = QuickBarComponent->FindQuickSlotForItemId(ItemId);
		QuickBarComponent->RemoveQuickSlot(QuickBarSlot.Key, QuickBarSlot.Value);
	}

	bool bIsTaken = QuickBarComponent->IsItemOnQuickSlot(QuickBar, QuickSlot);
	if (bIsTaken == true)
	{
		const FArcItemData* ExistingItemData = QuickBarComponent->FindQuickSlotItem(QuickBar, QuickSlot);
		if (ExistingItemData != nullptr)
		{
			ISC->RemoveItemFromSlot(ExistingItemData->GetItemId());
			ISC->RemoveItem(ExistingItemData->GetItemId(), -1);
		}
		
		/**
		 * This is not replicated to server by QuickBar we need to also call it on server.
		 */
		QuickBarComponent->RemoveQuickSlot(QuickBar, QuickSlot);
	}
	// Find item, which might be at designated quick slot.
	
	
	FArcQuickBar OutQuickBar;
	UArcQuickBarComponent::BP_GetQuickBar(QuickBarComponent, QuickBar, OutQuickBar);
	int32 QuickSlotIdx = OutQuickBar.Slots.IndexOfByKey(QuickSlot);

	FGameplayTag ItemSlot = QuickSlotIdx != INDEX_NONE ? OutQuickBar.Slots[QuickSlotIdx].DefaultItemSlotId : FGameplayTag::EmptyTag;
	
	FArcItemId ExistingItemId = QuickBarComponent->GetItemId(QuickBar, QuickSlot);
	const FArcItemData* OldItemData = ISC->GetItemPtr(ExistingItemId);
	const FArcItemData* ItemToSlot = ISC->GetItemPtr(ItemId);
	
	if (OldItemData && ItemToSlot && ItemToSlot->GetSlotId().IsValid())
	{
		ISC->ChangeItemSlot(OldItemData->GetItemId(), ItemToSlot->GetSlotId());
	}
	if (ISC->IsOnAnySlot(ItemId) == false)
	{
		ISC->AddItemToSlot(ItemId, ItemSlot);
	}
	QuickBarComponent->AddAndActivateQuickSlot(QuickBar, QuickSlot, ItemId);
	
	return true;
}

bool FArcRemoveItemFromQuickSlotCommand::CanSendCommand() const
{
	if (QuickBarComponent == nullptr)
	{
		return false;
	}

	if (QuickBar.IsValid() == false)
	{
		return false;
	}

	if (QuickSlot.IsValid() == false)
	{
		return false;
	}

	if (bRemoveFromItemSlot && QuickBarComponent->IsQuickSlotLocked(QuickBar, QuickSlot))
	{
		return false;
	}

	if (QuickBarComponent->IsQuickBarLocked())
	{
		return false;
	}

	FArcItemId ItemId = QuickBarComponent->GetItemId(QuickBar, QuickSlot);
	if (!ItemId.IsValid())
	{
		return false;
	}

	if (bRemoveFromItemSlot)
	{
		UArcItemsStoreComponent* ISC = QuickBarComponent->GetItemStoreComponent(QuickBar);
	
		if (ISC->IsItemLocked(QuickBarComponent->GetItemId(QuickBar, QuickSlot)))
		{
			return false;
		}
	}
	
	return true;
}

void FArcRemoveItemFromQuickSlotCommand::PreSendCommand()
{
}

bool FArcRemoveItemFromQuickSlotCommand::Execute()
{
	UArcItemsStoreComponent* ISC = QuickBarComponent->GetItemStoreComponent(QuickBar);

	QuickBarComponent->RemoveQuickSlot(QuickBar, QuickSlot);
	if (bRemoveFromItemSlot)
	{
		ISC->RemoveItemFromSlot(QuickBarComponent->GetItemId(QuickBar, QuickSlot));
	}
	
	return true;
}
