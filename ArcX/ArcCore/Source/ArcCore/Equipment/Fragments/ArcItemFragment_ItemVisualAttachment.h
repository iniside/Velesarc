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
#include "ArcItemFragment_ItemAttachment.h"
#include "ArcNamedPrimaryAssetId.h"
#include "Items/ArcItemInstance.h"

#include "ArcItemFragment_ItemVisualAttachment.generated.h"

USTRUCT()
struct ARCCORE_API FArcItemInstance_ItemVisualAttachment : public FArcItemInstance_ItemData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId VisualItem;

public:
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemInstance_ItemVisualAttachment::StaticStruct();
	}
	
	virtual TSharedPtr<FArcItemInstance> Duplicate() const override
	{
		TSharedPtr<FArcItemInstance_ItemVisualAttachment> SharedPtr = ArcItems::AllocateInstance<FArcItemInstance_ItemVisualAttachment>();
		SharedPtr->VisualItem = VisualItem;
		return SharedPtr;
	}

	virtual bool Equals(const FArcItemInstance& Other) const override
	{
		const FArcItemInstance_ItemVisualAttachment& Instance = static_cast<const FArcItemInstance_ItemVisualAttachment&>(Other);
		return VisualItem == Instance.VisualItem;
	}
};

/**
 *  This fragment contains item, which have visual attachemnt fragments.
 *  Use it instead of using direct link to actors/components, for viaul definition.
 *
 *  Items, linked by this fragment, should not be instantiated within  @class UArcItemsStoreComponent.
 *  As they should only contains data relevelant to  @class UArcItemAttachmentComponent
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Item Attachment - Visual Item Attachment"))
struct ARCCORE_API FArcItemFragment_ItemVisualAttachment : public FArcItemFragment_ItemInstanceBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId DefaultVisualItem;

	virtual UScriptStruct* GetItemInstanceType() const override
	{
		return FArcItemInstance_ItemVisualAttachment::StaticStruct();
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemFragment_ItemVisualAttachment::StaticStruct();
	}
	
	virtual ~FArcItemFragment_ItemVisualAttachment() override = default;
};