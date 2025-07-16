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
#include "GameplayTagContainer.h"
#include "Components/SkeletalMeshComponent.h"
#include "Equipment/ArcAttachmentHandler.h"
#include "Templates/SubclassOf.h"
#include "ArcItemFragment_SkeletalMeshAttachment.generated.h"

class UAnimInstance;
class USkeletalMeshComponent;
/**
 *
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Item Attachment - Skeletal Mesh Attachment", Category = "Item Attachment"))
struct ARCCORE_API FArcItemFragment_SkeletalMeshAttachment : public FArcItemFragment_ItemAttachment
{
	GENERATED_BODY()

public:
	/*
	 * Possible attach sockets have tags, which describe what kind of attachment will fit
	 * them These are tags which describe this attachment are checked by sockets.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
	FGameplayTagContainer AttachTags;

	/*
	 * SlotId of other item we want to attach. Does not imply any requriments as any any
	 * attach slot can customize how items are attached on it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (Categories = "SlotId"))
	FGameplayTag SlotId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
	bool bUseLeaderPose = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	/*
	 * Anim instance which will be set to ATTACHED skeletal mesh component, if not valid,
	 * skeletal mesh component will just use master pose.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
	TSoftClassPtr<UAnimInstance> SkeletalMeshAnimInstance;

	virtual ~FArcItemFragment_SkeletalMeshAttachment() override = default;
};

USTRUCT()
struct ARCCORE_API FArcAttachmentHandler_SkeletalMesh : public FArcAttachmentHandlerCommon
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
		return FArcItemFragment_SkeletalMeshAttachment::StaticStruct();
	}
	
	virtual ~FArcAttachmentHandler_SkeletalMesh() override = default;
};