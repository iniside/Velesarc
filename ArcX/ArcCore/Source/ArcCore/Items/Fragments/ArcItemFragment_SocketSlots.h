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
#include "ArcCore/Items/Fragments/ArcItemFragment.h"

#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"

#include "ArcItemFragment_SocketSlots.generated.h"

class UArcItemDefinition;

USTRUCT()
struct ARCCORE_API FArcSocketSlot
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data", meta = (Categories = "QuickSlotId"))
	FGameplayTag SlotId;

	UPROPERTY(EditAnywhere)
	FPrimaryAssetType ItemType;

	/** Only items with these tags will match this slot.
	 * Ignored if empty.
	 */
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer RequiredItemTags;
	
	UPROPERTY(EditAnywhere)
	FText SlotName;
	
	/** Optional. If set this item will be auto attached when owner is added. */
	UPROPERTY(EditAnywhere, Category = "Data")
	TObjectPtr<UArcItemDefinition> DefaultSocketItemDefinition;

	FArcSocketSlot()
		: SlotId()
		, DefaultSocketItemDefinition(nullptr)
	{
	}
};

UCLASS()
class UArcItemSocketSlotsPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data", meta = (ForceInlineRow, ShowOnlyInnerProperties, TitleProperty = "SlotId"))
	TArray<FArcSocketSlot> Slots;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Socket Slots"))
struct ARCCORE_API FArcItemFragment_SocketSlots : public FArcItemFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Data", meta = (ForceInlineRow, ShowOnlyInnerProperties, TitleProperty = "SlotId"))
	TArray<FArcSocketSlot> Slots;

	UPROPERTY(EditAnywhere, Category = "Data")
	TObjectPtr<UArcItemSocketSlotsPreset> Preset;
	
public:
	const TArray<FArcSocketSlot>& GetSocketSlots() const
	{
		if (Preset)
		{
			return Preset->Slots;
		}

		return Slots;
	}
	
	virtual ~FArcItemFragment_SocketSlots() override = default;
};