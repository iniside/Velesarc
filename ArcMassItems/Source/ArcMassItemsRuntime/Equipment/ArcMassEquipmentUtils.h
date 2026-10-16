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



#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemTypes.h"
#include "Equipment/ArcMassEquipmentTypes.h"

struct FMassEntityHandle;
struct FMassEntityManager;
struct FArcItemData;
struct FArcItemDefinitionData;
class UScriptStruct;

namespace Arc::MassEquipment
{
	ARCMASSITEMSRUNTIME_API bool CanEquip(FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FArcItemDefinitionData* InItemDefinition,
		const FArcItemData* InItemData,
		const FGameplayTag& InSlotId);

	/** Item-id overload — looks up FArcItemData inside the given store fragment type and forwards. */
	ARCMASSITEMSRUNTIME_API bool CanEquip(FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType,
		const FArcItemId& InItemId,
		const FGameplayTag& InSlotId);

	ARCMASSITEMSRUNTIME_API bool CanEquip(FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FPrimaryAssetId& ItemDefinitionId,
		const FGameplayTag& InSlotId);

	/** Copies RequiredTags / IgnoreTags out of the slot definition for a given slot id. */
	ARCMASSITEMSRUNTIME_API void GetTagsForSlot(FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FGameplayTag& InSlotId,
		FGameplayTagContainer& OutRequiredTags,
		FGameplayTagContainer& OutIgnoreTags);

	/** Returns true if ALL availability conditions on the slot pass for the given entity (empty list → true). */
	ARCMASSITEMSRUNTIME_API bool IsSlotAvailable(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcMassEquipmentSlot& Slot);

	/** Slot-id overload — resolves the preset on the entity, finds the slot, then forwards to the Slot overload. */
	ARCMASSITEMSRUNTIME_API bool IsSlotAvailable(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FGameplayTag& InSlotId);
}
