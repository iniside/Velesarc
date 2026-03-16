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

#include "Commands/ArcUseItemCommand.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/ArcItemAbilityHelpers.h"
#include "AbilitySystem/Fragments/ArcItemFragment_UseAbility.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsStoreComponent.h"

DEFINE_LOG_CATEGORY(LogArcUseItemCommand);

bool FArcUseItemCommand::CanSendCommand() const
{
	if (ItemsStore == nullptr)
	{
		return false;
	}

	if (!ItemId.IsValid())
	{
		return false;
	}

	if (ItemsStore->IsPending(ItemId))
	{
		UE_LOG(LogArcUseItemCommand, Log, TEXT("Item %s is currently pending."), *ItemId.ToString());
		return false;
	}

	const FArcItemData* ItemData = ItemsStore->GetItemPtr(ItemId);
	if (ItemData == nullptr)
	{
		UE_LOG(LogArcUseItemCommand, Log, TEXT("Item %s not found in store."), *ItemId.ToString());
		return false;
	}

	const FArcItemFragment_UseAbility* UseFragment = ItemData->FindSpecFragment<FArcItemFragment_UseAbility>();
	if (UseFragment == nullptr)
	{
		UE_LOG(LogArcUseItemCommand, Log, TEXT("Item %s has no UseAbility fragment."), *ItemId.ToString());
		return false;
	}

	if (UseFragment->UseAbilityClass == nullptr)
	{
		UE_LOG(LogArcUseItemCommand, Log, TEXT("Item %s UseAbility fragment has no ability class."), *ItemId.ToString());
		return false;
	}

	// Verify the ability is granted on the owner's ASC
	AActor* Owner = ItemsStore->GetOwner();
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	if (ASC == nullptr)
	{
		UE_LOG(LogArcUseItemCommand, Log, TEXT("No ASC found on store owner."));
		return false;
	}

	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(UseFragment->UseAbilityClass);
	if (Spec == nullptr)
	{
		UE_LOG(LogArcUseItemCommand, Log, TEXT("Ability %s not granted on ASC."), *GetNameSafe(UseFragment->UseAbilityClass));
		return false;
	}

	return true;
}

void FArcUseItemCommand::PreSendCommand()
{
	if (ItemsStore)
	{
		PendingItemIds.Add(ItemId);
		ItemsStore->AddPendingItems(PendingItemIds);
		CaptureExpectedVersions(ItemsStore);
	}
}

void FArcUseItemCommand::CommandConfirmed(bool bSuccess)
{
	if (!bSuccess && ItemsStore)
	{
		ItemsStore->RemovePendingItems(PendingItemIds);
	}
}

bool FArcUseItemCommand::Execute()
{
	if (ItemsStore == nullptr)
	{
		return false;
	}

	if (!ValidateVersions(ItemsStore))
	{
		UE_LOG(LogArcUseItemCommand, Log, TEXT("FArcUseItemCommand::Execute version mismatch, rejecting."));
		return false;
	}

	const FArcItemData* ItemData = ItemsStore->GetItemPtr(ItemId);
	if (ItemData == nullptr)
	{
		UE_LOG(LogArcUseItemCommand, Log, TEXT("Item not found on server."));
		return false;
	}

	const FArcItemFragment_UseAbility* UseFragment = ItemData->FindSpecFragment<FArcItemFragment_UseAbility>();
	if (UseFragment == nullptr || UseFragment->UseAbilityClass == nullptr)
	{
		UE_LOG(LogArcUseItemCommand, Log, TEXT("UseAbility fragment missing or invalid on server."));
		return false;
	}

	AActor* Owner = ItemsStore->GetOwner();
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	if (ASC == nullptr)
	{
		UE_LOG(LogArcUseItemCommand, Log, TEXT("No ASC found on server."));
		return false;
	}

	// Verify ability is granted on server
	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(UseFragment->UseAbilityClass);
	if (Spec == nullptr)
	{
		UE_LOG(LogArcUseItemCommand, Log, TEXT("Ability %s not granted on server ASC."), *GetNameSafe(UseFragment->UseAbilityClass));
		return false;
	}

	const int32 ActivatedCount = UArcItemAbilityHelpers::SendItemUseEvent(
		ASC, ItemsStore, ItemId, UseFragment->ActivationEventTag);

	if (ActivatedCount == 0)
	{
		UE_LOG(LogArcUseItemCommand, Log, TEXT("No ability handled the event for tag %s."), *UseFragment->ActivationEventTag.ToString());
		return false;
	}

	return true;
}
