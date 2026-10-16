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

#include "Equipment/ArcMassEquipmentUtils.h"

#include "Equipment/ArcMassEquipmentFragment.h"
#include "Equipment/ArcMassEquipmentSlotPreset.h"
#include "Equipment/ArcMassEquipmentTypes.h"
#include "Fragments/ArcMassItemStoreFragment.h"

#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMassEquipment, Log, All);

namespace ArcMassEquipmentUtilsImpl
{
	const FArcMassEquipmentSlot* FindSlot(const UArcMassEquipmentSlotPreset* Preset, const FGameplayTag& InSlotId)
	{
		if (Preset == nullptr)
		{
			return nullptr;
		}

		const FGameplayTag SlotId = InSlotId;
		const int32 Idx = Preset->EquipmentSlots.IndexOfByPredicate([SlotId](const FArcMassEquipmentSlot& InSlot)
		{
			return InSlot.SlotId == SlotId;
		});

		if (Idx == INDEX_NONE)
		{
			return nullptr;
		}

		return &Preset->EquipmentSlots[Idx];
	}

	const FArcMassEquipmentSharedFragment* GetEquipmentSharedFragment(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			return nullptr;
		}

		const FMassEntityView View(EntityManager, Entity);
		return View.GetConstSharedFragmentDataPtr<FArcMassEquipmentSharedFragment>();
	}
}

namespace Arc::MassEquipment
{
	bool IsSlotAvailable(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcMassEquipmentSlot& Slot)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			return false;
		}

		for (const FInstancedStruct& Condition : Slot.AvailabilityConditions)
		{
			const FArcMassEquipmentSlotAvailability* C = Condition.GetPtr<FArcMassEquipmentSlotAvailability>();
			if (C != nullptr && !C->IsAvailable(Entity, EntityManager))
			{
				return false;
			}
		}

		return true;
	}

	bool IsSlotAvailable(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FGameplayTag& InSlotId)
	{
		const FArcMassEquipmentSharedFragment* Shared = ArcMassEquipmentUtilsImpl::GetEquipmentSharedFragment(EntityManager, Entity);
		if (Shared == nullptr || Shared->Preset == nullptr)
		{
			return false;
		}

		const FArcMassEquipmentSlot* Slot = ArcMassEquipmentUtilsImpl::FindSlot(Shared->Preset, InSlotId);
		if (Slot == nullptr)
		{
			return false;
		}

		return IsSlotAvailable(EntityManager, Entity, *Slot);
	}

	bool CanEquip(FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FArcItemDefinitionData* InItemDefinition,
		const FArcItemData* InItemData,
		const FGameplayTag& InSlotId)
	{
		if (InSlotId.IsValid() == false)
		{
			UE_LOGFMT(LogArcMassEquipment, Error, "Invalid Slot");
			return false;
		}

		if (InItemDefinition == nullptr)
		{
			UE_LOGFMT(LogArcMassEquipment, Error, "Invalid ItemDefinition");
			return false;
		}

		const FArcMassEquipmentSharedFragment* Shared = ArcMassEquipmentUtilsImpl::GetEquipmentSharedFragment(EntityManager, Entity);
		if (Shared == nullptr || Shared->Preset == nullptr)
		{
			UE_LOGFMT(LogArcMassEquipment, Error, "No SlotPreset on entity");
			return false;
		}

		const FArcMassEquipmentSlot* Slot = ArcMassEquipmentUtilsImpl::FindSlot(Shared->Preset, InSlotId);
		if (Slot == nullptr)
		{
			UE_LOGFMT(LogArcMassEquipment, Error, "Not Equipment Slot for {SlotId}", InSlotId.ToString());
			return false;
		}

		if (!IsSlotAvailable(EntityManager, Entity, *Slot))
		{
			UE_LOGFMT(LogArcMassEquipment, Verbose, "Slot {SlotId} not available", InSlotId.ToString());
			return false;
		}

		bool bCustomConditionPassed = true;
		const FArcMassEquipmentSlotCondition* CustomCondition = Slot->CustomCondition.GetPtr<FArcMassEquipmentSlotCondition>();
		if (CustomCondition != nullptr)
		{
			bCustomConditionPassed = CustomCondition->CanEquip(Entity, EntityManager, InItemDefinition, InItemData);
		}

		bool bTagsPassed = true;
		const FArcItemFragment_Tags* TagsFragment = InItemDefinition->FindFragment<FArcItemFragment_Tags>();
		if (TagsFragment != nullptr)
		{
			if (TagsFragment->ItemTags.Num() > 0
				&& Slot->RequiredTags.Num() > 0)
			{
				bTagsPassed = TagsFragment->ItemTags.HasAll(Slot->RequiredTags);
			}
		}

		return bCustomConditionPassed && bTagsPassed;
	}

	bool CanEquip(FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType,
		const FArcItemId& InItemId,
		const FGameplayTag& InSlotId)
	{
		const UScriptStruct* EffectiveStoreType = StoreType ? StoreType : FArcMassItemStoreFragment::StaticStruct();
		if (EffectiveStoreType->IsChildOf(FArcMassItemStoreFragment::StaticStruct()) == false)
		{
			UE_LOGFMT(LogArcMassEquipment, Error, "StoreType {StoreType} must derive from FArcMassItemStoreFragment", EffectiveStoreType->GetName());
			return false;
		}

		if (!EntityManager.IsEntityValid(Entity))
		{
			return false;
		}

		const FConstStructView StoreView = EntityManager.GetSparseElementDataForEntity(EffectiveStoreType, Entity);
		const FArcMassItemStoreFragment* Store = StoreView.GetPtr<FArcMassItemStoreFragment>();
		if (Store == nullptr)
		{
			UE_LOGFMT(LogArcMassEquipment, Error, "No store fragment of type {StoreType} on entity", EffectiveStoreType->GetName());
			return false;
		}

		// FindById is non-const on FArcMassReplicatedItemArray; cast away const for the lookup only.
		FArcMassReplicatedItemArray& ItemsArray = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems);
		const FArcMassReplicatedItem* ReplicatedItem = ItemsArray.FindById(InItemId);
		if (ReplicatedItem == nullptr)
		{
			UE_LOGFMT(LogArcMassEquipment, Error, "Invalid Item");
			return false;
		}

		const FArcItemData* ItemData = ReplicatedItem->ToItem();
		if (ItemData == nullptr)
		{
			UE_LOGFMT(LogArcMassEquipment, Error, "Invalid Item data");
			return false;
		}

		return CanEquip(EntityManager, Entity, ItemData->GetItemDefinition(), ItemData, InSlotId);
	}

	bool CanEquip(FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FPrimaryAssetId& ItemDefinitionId,
		const FGameplayTag& InSlotId)
	{
		return CanEquip(EntityManager, Entity, FArcItemDefinitionData::Find(ItemDefinitionId), nullptr, InSlotId);
	}

	void GetTagsForSlot(FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FGameplayTag& InSlotId,
		FGameplayTagContainer& OutRequiredTags,
		FGameplayTagContainer& OutIgnoreTags)
	{
		const FArcMassEquipmentSharedFragment* Shared = ArcMassEquipmentUtilsImpl::GetEquipmentSharedFragment(EntityManager, Entity);
		if (Shared == nullptr || Shared->Preset == nullptr)
		{
			UE_LOGFMT(LogArcMassEquipment, Error, "No SlotPreset on entity");
			return;
		}

		const FArcMassEquipmentSlot* Slot = ArcMassEquipmentUtilsImpl::FindSlot(Shared->Preset, InSlotId);
		if (Slot == nullptr)
		{
			UE_LOGFMT(LogArcMassEquipment, Error, "Not Equipment Slot for {SlotId}", InSlotId.ToString());
			return;
		}

		OutRequiredTags = Slot->RequiredTags;
		OutIgnoreTags = Slot->IgnoreTags;
	}
}
