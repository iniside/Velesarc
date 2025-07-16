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
#include "ArcItemFragment_ItemAttachment.h"
#include "Equipment/ArcAttachmentHandler.h"


#include "ArcItemFragment_StaticMeshAttachment.generated.h"
class UStaticMesh;

/**
 * Used with @link UArcItemAttachmentComponent and @link FArcAttachmentHandler_StaticMeshComponent
 * To attach static mesh @link FArcItemFragment_StaticMeshAttachment#StaticMeshAttach
 * when Item is added to slot.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Item Attachment - Static Mesh Attachment", ShowTooltip, Category = "Item Attachment"))
struct ARCCORE_API FArcItemFragment_StaticMeshAttachment : public FArcItemFragment_ItemAttachment
{
	GENERATED_BODY()

public:
	/*
	 * Possible attach sockets have tags, which describe what kind of attachment will fit
	 * them These are tags which describe this attachment are checked by sockets.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
	FGameplayTagContainer AttachTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UStaticMesh> StaticMeshAttachClass;
	
	virtual ~FArcItemFragment_StaticMeshAttachment() override = default;
};

USTRUCT()
struct ARCCORE_API FArcAttachmentHandler_StaticMesh : public FArcAttachmentHandlerCommon
{
	GENERATED_BODY()
	
public:
	virtual bool HandleItemAddedToSlot(UArcItemAttachmentComponent* InAttachmentComponent
									   , const FArcItemData* InItem
									   , const FArcItemData* InOwnerItem) const override;

	virtual void HandleItemRemovedFromSlot(UArcItemAttachmentComponent* InAttachmentComponent
										   , const FArcItemData* InItem
										   , const FGameplayTag& SlotId
										   , const FArcItemData* InOwnerItem) const override
	{
	};

	virtual void HandleItemAttach(UArcItemAttachmentComponent* InAttachmentComponent
							  , const FArcItemId InItemId
							  , const FArcItemId InOwnerItem) const override;

	virtual UScriptStruct* SupportedItemFragment() const override
	{
		return FArcItemFragment_StaticMeshAttachment::StaticStruct();
	}
	
	virtual ~FArcAttachmentHandler_StaticMesh() override = default;
};