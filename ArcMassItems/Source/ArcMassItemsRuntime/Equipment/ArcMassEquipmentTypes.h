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

#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"
#include "Tags/ArcGameplayTagRequirements.h"
#include "ArcMassEquipmentTypes.generated.h"

struct FMassEntityManager;
struct FArcItemDefinitionData;
struct FArcItemData;

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassEquipmentSlotCondition
{
	GENERATED_BODY()

	virtual ~FArcMassEquipmentSlotCondition() = default;
	virtual bool CanEquip(FMassEntityHandle OwnerEntity, FMassEntityManager& EntityManager, const FArcItemDefinitionData* InItemDefinition, const FArcItemData* InItemData) const { return true; }
};

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassEquipmentSlotAvailability
{
	GENERATED_BODY()

	virtual ~FArcMassEquipmentSlotAvailability() = default;
	// Read-only over replicated/derivable entity state ONLY (owned tags, EquippedItems). No server-only data.
	virtual bool IsAvailable(FMassEntityHandle OwnerEntity, FMassEntityManager& EntityManager) const { return true; }
};

// Replaces the removed OwnerRequiredTags/OwnerIgnoreTags fields.
USTRUCT(meta = (DisplayName = "Owner Tags"))
struct ARCMASSITEMSRUNTIME_API FArcMassEquipmentSlotAvailability_OwnerTags : public FArcMassEquipmentSlotAvailability
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcGameplayTagRequirements OwnerTags;   // RequireTags / IgnoreTags / TagQuery

	virtual bool IsAvailable(FMassEntityHandle OwnerEntity, FMassEntityManager& EntityManager) const override;
};

// Belt/backpack: slot usable only when a prerequisite slot is occupied.
USTRUCT(meta = (DisplayName = "Require Slot Occupied"))
struct ARCMASSITEMSRUNTIME_API FArcMassEquipmentSlotAvailability_RequireSlotOccupied : public FArcMassEquipmentSlotAvailability
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (Categories = "SlotId"))
	FGameplayTag RequiredSlotId;

	virtual bool IsAvailable(FMassEntityHandle OwnerEntity, FMassEntityManager& EntityManager) const override;
};

USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassEquipmentSlot
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "SlotId"))
	FGameplayTag SlotId;

	/* If set item must have all of those tags. */
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer RequiredTags;

	/* If set item must have none of those tags. */
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer IgnoreTags;

	/* Custom implemented condition which must be meet for item to be fit into this slot. */
	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcMassEquipmentSlotCondition"))
	FInstancedStruct CustomCondition;

	/* Availability conditions: slot is usable for an entity only when ALL conditions hold. */
	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcMassEquipmentSlotAvailability", ExcludeBaseStruct))
	TArray<FInstancedStruct> AvailabilityConditions;
};
