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
#include "ArcItemFragment_CustomizableObject.generated.h"

class UCustomizableObjectInstance;
class UCustomizableObject;
class USkeletalMesh;

USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMutableIntParameter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString ParameterName;

	UPROPERTY(EditAnywhere)
	FString SelectedOption;
};

USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMutableSkeletalMeshParameter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString ParameterName;

	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Client"))
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditAnywhere)
	int32 RangeIndex = -1;
};

USTRUCT(meta = (DisplayName = "Item Attachment - Customizable Object", Category = "Item Attachment", ToolTip = "Integrates a Mutable CustomizableObject with the item attachment system. Allows items to modify character appearance through CustomizableObject parameters (e.g., armor pieces that alter body mesh, clothing that changes material slots). Processed by FArcMassAttachmentHandler_CustomizableObject."))
struct ARCMASSITEMSRUNTIME_API FArcItemFragment_CustomizableObject : public FArcItemFragment_ItemAttachment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "CustomizableObject", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UCustomizableObject> CustomizableObject;

	UPROPERTY(EditAnywhere, Category = "CustomizableObject")
	FArcMutableIntParameter IntParameter;

	UPROPERTY(EditAnywhere, Category = "CustomizableObject")
	FArcMutableSkeletalMeshParameter SkeletalMeshParameter;
};
