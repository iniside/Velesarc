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
#include "Equipment/ArcAttachmentHandler.h"
#include "Templates/SubclassOf.h"
#include "ArcItemFragment_ActorAttachment.generated.h"

USTRUCT(BlueprintType)
struct ARCCORE_API FArcItemAttachmentData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data", meta = (NoCategoryGrouping, AssetBundles = "Client"))
	TSoftClassPtr<class AActor> ActorClass;
};

/**
 *
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Item Attachment - Actor Attachment"))
struct ARCCORE_API FArcItemFragment_ActorAttachment : public FArcItemFragment_ItemAttachment
{
	GENERATED_BODY()

public:
	/*
	 * Possible attach sockets have tags, which describe what kind of attachment will fit
	 * them These are tags which describe this attachment are checked by sockets.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (NoCategoryGrouping))
	FGameplayTagContainer AttachTags;

	/*
	 * SlotId of other item we want to attach. Does not imply any requriments as any any
	 * attach slot can customize how items are attached on it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (NoCategoryGrouping, Categories = "SlotId"))
	FGameplayTag SlotId;

	/*
	 * Or maybe TSubclassOf<USkeltalMeshComponent> ?
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (NoCategoryGrouping))
	bool bAttachToCharacterMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (NoCategoryGrouping, Categories = "SlotId"))
	FTransform RelativeTransform;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (NoCategoryGrouping, AssetBundles = "Client"))
	TSoftClassPtr<class AActor> ActorClass;

	virtual ~FArcItemFragment_ActorAttachment() override = default;
};

USTRUCT()
struct ARCCORE_API FArcAttachmentHandler_Actor : public FArcAttachmentHandlerCommon
{
	GENERATED_BODY()

public:
	virtual bool HandleItemAddedToSlot(UArcItemAttachmentComponent* InAttachmentComponent
									   , const FArcItemData* InItem
									   , const FArcItemData* InOwnerItem) const override;

	virtual void HandleItemRemovedFromSlot(UArcItemAttachmentComponent* InAttachmentComponent
										   , const FArcItemData* InItem
										   , const FGameplayTag& SlotId
										   , const FArcItemData* InOwnerItem) const override;

	virtual void HandleItemAttach(UArcItemAttachmentComponent* InAttachmentComponent
								  , const FArcItemId InItemId
								  , const FArcItemId InOwnerItem) const override;
	
	virtual void HandleItemAttachmentChanged(UArcItemAttachmentComponent* InAttachmentComponent
								  , const FArcItemAttachment& ItemAttachment) const override;

	virtual UScriptStruct* SupportedItemFragment() const override
	{
		return FArcItemFragment_ActorAttachment::StaticStruct();
	}
	
private:
	void HandleOnItemLoaded(UArcItemAttachmentComponent* InAttachmentComponent, const FArcItemId InItemId) const;
	
public:
	virtual ~FArcAttachmentHandler_Actor() override = default;
};
