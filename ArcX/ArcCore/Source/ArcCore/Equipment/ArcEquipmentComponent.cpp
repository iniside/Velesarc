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



#include "ArcEquipmentComponent.h"

#include "ArcCoreUtils.h"
#include "Engine/Engine.h"
#include "Player/ArcCorePlayerController.h"

#include "Commands/ArcEquipItemCommand.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "Core/ArcCoreAssetManager.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY(LogArcEquipmentComponent);

// Sets default values for this component's properties
UArcEquipmentComponent::UArcEquipmentComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	
	// ...
}

// Called when the game starts
void UArcEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

UArcItemsStoreComponent* UArcEquipmentComponent::GetItemsStore() const
{
	if (ItemsStore)
	{
		return ItemsStore;
	}

	ItemsStore = Arcx::Utils::GetComponent<UArcItemsStoreComponent>(GetOwner(), ItemsStoreClass);

	return ItemsStore;
}

bool UArcEquipmentComponent::CanEquipItem(const UArcItemDefinition* InItemDefinition, const FArcItemData* InItemData, const FGameplayTag& InSlotId) const
{
	if (InSlotId.IsValid() == false)
	{
		UE_LOG(LogArcEquipmentComponent, Error, TEXT("Invalid Slot"))
		return false;
	}

	if (EquipmentSlotPreset == nullptr)
	{
		UE_LOG(LogArcEquipmentComponent, Error, TEXT("No SlotPreset"))
		return false;
	}

	FGameplayTag SlotId = InSlotId;
	int32 EquipSlotIdx = EquipmentSlotPreset->EquipmentSlots.IndexOfByPredicate([SlotId](const FArcEquipmentSlot& InSlot)
	{
		return InSlot.SlotId == SlotId;
	});

	if (EquipSlotIdx == INDEX_NONE)
	{
		UE_LOG(LogArcEquipmentComponent, Error, TEXT("Not Equipment Slot for %s"), *InSlotId.ToString())
		return false;
	}

	const FArcEquipmentSlotCondition* CustomCondition = EquipmentSlotPreset->EquipmentSlots[EquipSlotIdx].CustomCondition.GetPtr<FArcEquipmentSlotCondition>();
	bool bCustomConditionPassed = true;
	if (CustomCondition != nullptr)
	{
		bCustomConditionPassed = CustomCondition->CanEquip(GetOwner(), InItemDefinition, InItemData);
	}

	const FArcItemFragment_Tags* TagsFragment = InItemDefinition->FindFragment<FArcItemFragment_Tags>();
	bool bTagsPassed = false;
	if (TagsFragment != nullptr)
	{
		if (TagsFragment->ItemTags.Num() > 0
			&& EquipmentSlotPreset->EquipmentSlots[EquipSlotIdx].RequiredTags.Num() > 0)
		{
			bTagsPassed = TagsFragment->ItemTags.HasAll(EquipmentSlotPreset->EquipmentSlots[EquipSlotIdx].RequiredTags);
		}
	}

	//UE_LOGFMT(LogArcEquipmentComponent, Log, "Custom Condition {bCustomConditionPassed}, Tags {bTagsPassed}", bCustomConditionPassed, bTagsPassed);
	return bCustomConditionPassed && bTagsPassed;
}

bool UArcEquipmentComponent::CanEquipItem(const FArcItemId& InItemId, UArcItemsStoreComponent* ItemStore, const FGameplayTag& InSlotId) const
{
	if (ItemStore == nullptr)
	{
		return false;
	}

	const FArcItemData* InItemData = ItemStore->GetItemPtr(InItemId);
	if (InItemData == nullptr)
	{
		UE_LOG(LogArcEquipmentComponent, Error, TEXT("Invalid Item"))
		return false;
	}

	return CanEquipItem(InItemData->GetItemDefinition(), InItemData, InSlotId);
}

bool UArcEquipmentComponent::CanEquipItem(const FPrimaryAssetId& ItemDefinitionId, const FGameplayTag& InSlotId) const
{
	UArcItemDefinition* ItemDef = UArcCoreAssetManager::GetAsset<UArcItemDefinition>(ItemDefinitionId, false);
	
	return CanEquipItem(ItemDef, nullptr, InSlotId);
}

void UArcEquipmentComponent::GetTagsForSlot(const FGameplayTag& InSlotId
											, FGameplayTagContainer& OutRequiredTags
											, FGameplayTagContainer& OutIgnoreTags) const
{
	FGameplayTag SlotId = InSlotId;
	int32 EquipSlotIdx = EquipmentSlotPreset->EquipmentSlots.IndexOfByPredicate([SlotId](const FArcEquipmentSlot& InSlot)
	{
		return InSlot.SlotId == SlotId;
	});
	
	if (EquipSlotIdx == INDEX_NONE)
	{
		UE_LOG(LogArcEquipmentComponent, Error, TEXT("Not Equipment Slot for %s"), *InSlotId.ToString())
		return;
	}

	OutRequiredTags = EquipmentSlotPreset->EquipmentSlots[EquipSlotIdx].RequiredTags;
	OutIgnoreTags = EquipmentSlotPreset->EquipmentSlots[EquipSlotIdx].IgnoreTags;
}

void UArcEquipmentComponent::EquipItem(UArcItemsStoreComponent* FromItemsStore
	, const FArcItemId& InNewItem
	, const FGameplayTag& ToSlot)
{
}

TArray<const FArcItemData*> UArcEquipmentComponent::GetItemsFromStoreForSlot(const FGameplayTag& InSlotId) const
{
	TArray<const FArcItemData*> Items;

	UArcItemsStoreComponent* FromItemsStore = GetItemsStore();
	
	if (FromItemsStore == nullptr || EquipmentSlotPreset == nullptr)
	{
		return Items;
	}

	TArray<const FArcItemData*> StoreItems = FromItemsStore->GetItems();
	for (const FArcItemData* ItemData : StoreItems)
	{
		if (ItemData == nullptr)
		{
			continue;
		}

		if (CanEquipItem(ItemData->GetItemId(), FromItemsStore, InSlotId))
		{
			Items.Add(ItemData);
		}
	}

	return Items;
}

TArray<FArcEquipmentSlot> UArcEquipmentComponent::GetMatchingSlots(const FGameplayTag& InTag) const
{
	TArray<FArcEquipmentSlot> Slots;

	for (const FArcEquipmentSlot& ES : EquipmentSlotPreset->EquipmentSlots)
	{
		if (ES.SlotId.MatchesTag(InTag))
		{
			Slots.Add(ES);
		}
	}

	return Slots;
}

const TArray<FArcEquipmentSlot>& UArcEquipmentComponent::GetEquipmentSlots() const
{
	if (EquipmentSlotPreset)
	{
		return EquipmentSlotPreset->EquipmentSlots;
	}

	static TArray<FArcEquipmentSlot> Slots;

	return Slots;
}

void UArcEquipmentSlotDefaultItems::AddItemsToEquipment(UArcEquipmentComponent* InEquipmentComponent)
{
	
}
