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

#include "ArcAddItemToQuickBarCommand.h"

#include "QuickBar/ArcQuickBarComponent.h"
#include "Items/ArcItemsStoreComponent.h"
#include "ArcCoreGameplayTags.h"

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

	const FArcItemData* ItemData = QuickBarComponent->FindSlotData(QuickBar, QuickSlot);
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
	
	if (ItemSlotComponent->IsOnAnySlot(ItemId) == false)
	{
		return;
	}

	const bool bIsOnAnyQuickSlot = QuickBarComponent->IsItemOnAnyQuickSlot(ItemId);
	TPair<FGameplayTag, FGameplayTag> QuickBarQuickSlot = QuickBarComponent->FindQuickSlotForItemId(ItemId);
	
	
	if (bIsOnAnyQuickSlot)
	{
		QuickBarComponent->RemoveItemFromBarByItemId(ItemId);
	}

	bool bIsTaken = QuickBarComponent->IsItemOnQuickSlot(QuickBar, QuickSlot);
	if (bIsTaken == true)
	{
		/**
		 * This is not replicated to server by QuickBar we need to also call it on server.
		 */
		QuickBarComponent->RemoveItemFromBar(QuickBar, QuickSlot);
	}

	const FArcItemData* ItemData = QuickBarComponent->FindSlotData(QuickBar, QuickSlot);
	if (ItemData != nullptr)
	{
		if (QuickBarQuickSlot.Key.IsValid() && QuickBarQuickSlot.Value.IsValid())
		{
			QuickBarComponent->AddItemToBarOrRegisterDelegate(QuickBarQuickSlot.Key, QuickBarQuickSlot.Value, ItemData->GetItemId());
		}
	}

	/**
	 * If there is item slotted we will now add it to QuickSlot/
	 * Otherwise we will register delegate waitning on @link ItemId,
	 * till the slot is replicated.
	 */
	QuickBarComponent->AddItemToBarOrRegisterDelegate(QuickBar, QuickSlot, ItemId);
}

bool FArcAddItemToQuickBarCommand::Execute()
{	
	UArcItemsStoreComponent* ISC = QuickBarComponent->GetItemStoreComponent(QuickBar);

	const FArcItemData* OldItemData = ISC->GetItemFromSlot(ItemSlot);
	const FArcItemData* ItemToSlot = ISC->GetItemPtr(ItemId);
	
	if (OldItemData && ItemToSlot && ItemToSlot->GetSlotId().IsValid())
	{
		ISC->ChangeItemSlot(OldItemData->GetItemId(), ItemToSlot->GetSlotId());
	}
	if (ISC->IsOnAnySlot(ItemId) == false)
	{
		ISC->AddItemToSlot(ItemId, ItemSlot);
		QuickBarComponent->AddItemToBarOrRegisterDelegate(QuickBar, QuickSlot, ItemId);
	}
	else
	{
		QuickBarComponent->AddItemToBarOrRegisterDelegate(QuickBar, QuickSlot, ItemId);
	}
	
	const bool bIsOnAnyQuickSlot = QuickBarComponent->IsItemOnAnyQuickSlot(ItemId);
	TPair<FGameplayTag, FGameplayTag> ExistingQuickBarSlot = QuickBarComponent->FindQuickSlotForItemId(ItemId);
	
	// If it was on any quick bar slot, remove it first to correctly trigger events, like input unbind.
	if (bIsOnAnyQuickSlot)
	{
		QuickBarComponent->RemoveItemFromBarByItemId(ItemId);
	}

	bool bIsTaken = QuickBarComponent->IsItemOnQuickSlot(QuickBar, QuickSlot);
	if (bIsTaken == true)
	{
		/**
		 * This is not replicated to server by QuickBar we need to also call it on server.
		 */
		QuickBarComponent->RemoveItemFromBar(QuickBar, QuickSlot);
	}
	
	// Find item, which might be at designated quick slot.
	const FArcItemData* ItemData = QuickBarComponent->FindSlotData(QuickBar, QuickSlot);
	if (ItemData != nullptr)
	{
		if (ExistingQuickBarSlot.Key.IsValid() && ExistingQuickBarSlot.Value.IsValid())
		{
			QuickBarComponent->AddItemToBarOrRegisterDelegate(ExistingQuickBarSlot.Key, ExistingQuickBarSlot.Value, ItemData->GetItemId());
		}
	}
	
	
	
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

	if (QuickBarComponent->IsQuickBarLocked())
	{
		return false;
	}
	//const FArcItemData* ItemData = QuickBarComponent->FindSlotData(QuickBar, QuickSlot);
	//if (ItemData != nullptr)
	//{
	//	if (ItemData->GetItemId() == ItemId)
	//	{
	//		return false;
	//	}
	//}

	return true;
}

void FArcAddItemToQuickBarNoItemSlotCommand::PreSendCommand()
{
	UArcItemsStoreComponent* ItemSlotComponent = QuickBarComponent->GetItemStoreComponent(QuickBar);

	//if (ItemSlotComponent->IsOnAnySlot(ItemId) == false)
	//{
	//	return;
	//}

	if (QuickBarComponent->GetNetMode() == ENetMode::NM_Client)
	{
		QuickBarComponent->SetLockQuickBar();	
	}
	
	//const bool bIsOnAnyQuickSlot = QuickBarComponent->IsItemOnAnyQuickSlot(ItemId);
	//TPair<FGameplayTag, FGameplayTag> QuickBarQuickSlot = QuickBarComponent->FindQuickSlotForItemId(ItemId);


	//if (bIsOnAnyQuickSlot)
	//{
	//	QuickBarComponent->RemoveItemFromBarByItemId(ItemId);
	//}

	//bool bIsTaken = QuickBarComponent->IsItemOnQuickSlot(QuickBar, QuickSlot);
	//if (bIsTaken == true)
	//{
	//	/**
	//	 * This is not replicated to server by QuickBar we need to also call it on server.
	//	 */
	//	QuickBarComponent->RemoveItemFromBar(QuickBar, QuickSlot);
	//}

	//const FArcItemData* ItemData = QuickBarComponent->FindSlotData(QuickBar, QuickSlot);
	//if (ItemData != nullptr)
	//{
	//	if (QuickBarQuickSlot.Key.IsValid() && QuickBarQuickSlot.Value.IsValid())
	//	{
	//		QuickBarComponent->AddItemToBarOrRegisterDelegate(QuickBarQuickSlot.Key, QuickBarQuickSlot.Value, ItemData->GetItemId());
	//	}
	//}

	/**
	 * If there is item slotted we will now add it to QuickSlot/
	 * Otherwise we will register delegate waitning on @link ItemId,
	 * till the slot is replicated.
	 */
	//QuickBarComponent->AddItemToBarOrRegisterDelegate(QuickBar, QuickSlot, ItemId);
}

bool FArcAddItemToQuickBarNoItemSlotCommand::Execute()
{
	UArcItemsStoreComponent* ISC = QuickBarComponent->GetItemStoreComponent(QuickBar);

	const FArcItemData* ItemToSlot = ISC->GetItemPtr(ItemId);

	const bool bIsOnAnyQuickSlot = QuickBarComponent->IsItemOnAnyQuickSlot(ItemId);
	
	// If it was on any quick bar slot, remove it first to correctly trigger events, like input unbind.
	if (bIsOnAnyQuickSlot)
	{
		QuickBarComponent->RemoveItemFromBarByItemId(ItemId);
	}

	bool bIsTaken = QuickBarComponent->IsItemOnQuickSlot(QuickBar, QuickSlot);
	if (bIsTaken == true)
	{
		/**
		 * This is not replicated to server by QuickBar we need to also call it on server.
		 */
		QuickBarComponent->RemoveItemFromBar(QuickBar, QuickSlot);
	}
	
	const FArcItemData* ExistingItemData = QuickBarComponent->FindSlotData(QuickBar, QuickSlot);
	if (ExistingItemData != nullptr)
	{
		ISC->RemoveItemFromSlot(ExistingItemData->GetItemId());
	}

	// Find item, which might be at designated quick slot.
	const FArcItemData* ItemData = QuickBarComponent->FindSlotData(QuickBar, QuickSlot);
	if (ItemData != nullptr)
	{
		TPair<FGameplayTag, FGameplayTag> ExistingQuickBarSlot = QuickBarComponent->FindQuickSlotForItemId(ItemId);
		//if (ExistingQuickBarSlot.Key.IsValid() && ExistingQuickBarSlot.Value.IsValid())
		//{
		//	QuickBarComponent->AddItemToBarOrRegisterDelegate(ExistingQuickBarSlot.Key, ExistingQuickBarSlot.Value, ItemData->GetItemId());
		//}
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

		QuickBarComponent->AddItemToBarOrRegisterDelegate(QuickBar, QuickSlot, ItemId);
	}
	else
	{
		QuickBarComponent->AddItemToBarOrRegisterDelegate(QuickBar, QuickSlot, ItemId);
	}

	return true;
}
