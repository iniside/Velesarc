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
#include "Equipment/Fragments/ArcItemFragment_ItemAttachment.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "Templates/SubclassOf.h"

#include "ArcItemFragment_ChaosClothComponent.generated.h"

class UChaosClothComponent;
class UChaosClothAsset;
/**
 * 
 */
USTRUCT(meta = (DisplayName = "Item Attachment - Chaos Cloth", Category = "Item Attachment", ToolTip = "Attaches a Chaos Cloth simulation component to the character when this item is equipped. References a ChaosClothAsset and an optional custom ClothComponent class. Processed by FArcAttachmentHandler_ChaosCloth for cloth physics on capes, skirts, banners, etc."))
struct ARCMASSITEMSRUNTIME_API FArcItemFragment_ChaosClothComponent : public FArcItemFragment_ItemAttachment
{
	GENERATED_BODY()

public:
	FArcItemFragment_ChaosClothComponent();

	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Client"))
	TSoftClassPtr<UChaosClothComponent> ClothComponentClass;

	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UChaosClothAsset> ClothAsset;
};

//UCLASS(Blueprintable, BlueprintType)
//class UArcChaosClothComponent : public UChaosClothComponent
//{
//	GENERATED_BODY()
//};