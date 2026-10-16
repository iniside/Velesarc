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
#include "Net/Serialization/FastArraySerializer.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcNamedPrimaryAssetId.h"

#include "ArcSocketArray.generated.h"

USTRUCT(BlueprintType)
struct ARCCORE_API FArcSocketArray
{
	GENERATED_BODY()

public:
	/**
	 * Item must have these tagsl
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FGameplayTagContainer SocketTags;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName SocketName;
};

USTRUCT()
struct ARCCORE_API FArcItemAttachmentSlot
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data", meta = (NoCategoryGrouping, Categories = "SlotId"))
	FGameplayTag SlotId;

	UPROPERTY(EditAnywhere, meta = (AllowedTypes = "ArcItem"))
	FArcNamedPrimaryAssetId DefaultVisualItem;
	
	/*
	 * All of those will be executed if something is added to this slot.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Arc Core", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcMassAttachmentHandler"))
	TArray<FInstancedStruct> Handlers;
};

/**
 * Custom data which can be attached to @link FArcItemAttachment
 * To add some custom data associated with attachment. Like custom Material Parameters, scale etc.
 */
USTRUCT()
struct ARCCORE_API FArcItemAttachmentCustomData
{
	GENERATED_BODY()

	virtual ~FArcItemAttachmentCustomData() = default;
};

