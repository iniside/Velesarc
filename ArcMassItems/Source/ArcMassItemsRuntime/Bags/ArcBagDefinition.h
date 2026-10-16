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
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcBagDefinition.generated.h"

struct FArcItemData;

/**
 * Base struct for custom bag acceptance conditions.
 * Subclass in C++ or Blueprint to implement per-item acceptance logic.
 */
USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcBagAcceptanceCondition
{
	GENERATED_BODY()

	virtual ~FArcBagAcceptanceCondition() = default;
	virtual bool CanAccept(const FArcItemData& Item) const { return true; }
};

/**
 * Data asset describing a bag's capacity and item acceptance rules.
 */
UCLASS()
class ARCMASSITEMSRUNTIME_API UArcBagDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Number of item slots this bag provides. */
	UPROPERTY(EditAnywhere)
	int32 SlotCount = 1;

	/** Display name shown in UI. */
	UPROPERTY(EditAnywhere)
	FText DisplayName;

	/** If set, an item must have ALL of these tags to be accepted. */
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer RequiredTags;

	/** If set, an item must have NONE of these tags to be accepted. */
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer IgnoreTags;

	/** Custom condition struct implementing FArcBagAcceptanceCondition. */
	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcBagAcceptanceCondition"))
	FInstancedStruct CustomCondition;
};
