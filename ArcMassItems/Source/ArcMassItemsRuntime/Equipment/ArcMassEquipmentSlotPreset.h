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

#include "Engine/DataAsset.h"
#include "Equipment/ArcMassEquipmentTypes.h"
#include "Items/ArcItemSpec.h"
#include "ArcMassEquipmentSlotPreset.generated.h"

UCLASS()
class ARCMASSITEMSRUNTIME_API UArcMassEquipmentSlotPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TArray<FArcMassEquipmentSlot> EquipmentSlots;
};

USTRUCT()
struct FArcMassEquipmentSlotDefaultItem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta = (Categories = "QuickSlotId"))
	FGameplayTag SlotId;

	/* If set item must have all of those tags. */
	UPROPERTY(EditAnywhere, meta = (AllowedTypes = "ArcItem"))
	FArcNamedPrimaryAssetId ItemId;
};

UCLASS()
class ARCMASSITEMSRUNTIME_API UArcMassEquipmentSlotDefaultItems : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TArray<FArcMassEquipmentSlotDefaultItem> EquipmentSlots;
};
