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
#include "ChaosClothAsset/ClothComponent.h"
#include "Equipment/ArcAttachmentHandler.h"
#include "Templates/SubclassOf.h"

#include "ArcItemFragment_ChaosClothComponent.generated.h"

class UChaosClothComponent;
class UChaosClothAsset;
/**
 * 
 */
USTRUCT()
struct ARCCORE_API FArcItemFragment_ChaosClothComponent : public FArcItemFragment_ItemAttachment
{
	GENERATED_BODY()

public:
	FArcItemFragment_ChaosClothComponent();

	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Client"))
	TSoftClassPtr<UChaosClothComponent> ClothComponentClass;

	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UChaosClothAsset> ClothAsset;
};

USTRUCT()
struct ARCCORE_API FArcAttachmentHandler_ChaosCloth : public FArcAttachmentHandlerCommon
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Data", meta = (NoCategoryGrouping))
	TSubclassOf<UChaosClothComponent> ComponentClass;

	UPROPERTY(EditAnywhere, Category = "Data", meta = (NoCategoryGrouping))
	double WorldScale = 1.0;
	
public:
	virtual bool HandleItemAddedToSlot(UArcItemAttachmentComponent* InAttachmentComponent
									   , const FArcItemData* InItem
									   , const FArcItemData* InOwnerItem) const override;

	virtual void HandleItemRemovedFromSlot(UArcItemAttachmentComponent* InAttachmentComponent
										   , const FArcItemData* InItem
										   , const FGameplayTag& SlotId
										   , const FArcItemData* InOwnerItem) const override;;

	virtual void HandleItemAttach(UArcItemAttachmentComponent* InAttachmentComponent
							  , const FArcItemId InItemId
							  , const FArcItemId InOwnerItem) const override;

	virtual void HandleItemDetach(UArcItemAttachmentComponent* InAttachmentComponent
								  , const FArcItemId InItemId
								  , const FArcItemId InOwnerItem) const override;

	virtual UScriptStruct* SupportedItemFragment() const override
	{
		return FArcItemFragment_ChaosClothComponent::StaticStruct();
	}
	
	virtual ~FArcAttachmentHandler_ChaosCloth() override = default;
};

//UCLASS(Blueprintable, BlueprintType)
//class UArcChaosClothComponent : public UChaosClothComponent
//{
//	GENERATED_BODY()
//};