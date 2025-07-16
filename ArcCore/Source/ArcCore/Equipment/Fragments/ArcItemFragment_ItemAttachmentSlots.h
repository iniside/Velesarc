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



#pragma once

#include "Equipment/ArcSocketArray.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcItemFragment_ItemAttachment.h"

#include "ArcItemFragment_ItemAttachmentSlots.generated.h"

USTRUCT(BlueprintType)
struct ARCCORE_API FArcItemAttachmentSlotEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta=(Categories="SlotId"))
	FGameplayTag SlotId;

	/**
	 * Mapping of ItemSlot -> Possible Sockets.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (TitleProperty = "Socket: {SocketName}"))
	TArray<FArcSocketArray> AttachmentSockets;
	
	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcCore.ArcAttachmentHandler", ShowTreeView, ExcludeBaseStruct))
	TArray<FInstancedStruct> Handlers;
};

/*
 * Represents visual sockets of to which other items can be attached. Ie sockets on
 * backpack or sockets on weapon.
 *
 * These are sockets On item, to which OTHER items can be attached.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Item Attachment - Item Attachment Slots", Category = "Item Attachment"))
struct ARCCORE_API FArcItemFragment_ItemAttachmentSlots : public FArcItemFragment_ItemAttachment
{
	GENERATED_BODY()

public:
	/**
	 * @brief Handlers used by this attachment slot on item.
	 */
	UPROPERTY(EditAnywhere)
	TArray<FArcItemAttachmentSlot> AttachmentSlots;
	
	virtual ~FArcItemFragment_ItemAttachmentSlots() override = default;

	const FArcItemAttachmentSlot* FindSlotEntry(const FGameplayTag& InSlotId) const
	{
		for (const FArcItemAttachmentSlot& Entry : AttachmentSlots)
		{
			if (Entry.SlotId == InSlotId)
			{
				return &Entry;
			}
		}

		return nullptr;
	}
};