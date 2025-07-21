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


#include "GameplayEffectUIData.h"
#include "ArcGEC_UITextIcon.generated.h"

/**
 * 
 */
UCLASS()
class ARCCORE_API UArcGEC_UITextIcon : public UGameplayEffectUIData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere
		, BlueprintReadOnly
		, Category = UI
		, meta = (AllowPrivateAccess = "true", DisplayThumbnail = "true", DisplayName = "Image", AllowedClasses =
			"/Script/Engine.Texture,/Script/Engine.MaterialInterface,/Script/Engine.SlateTextureAtlasInterface",
			DisallowedClasses = "/Script/MediaAssets.MediaTexture"))
	TSoftObjectPtr<UObject> ItemIcon;

	UPROPERTY(EditAnywhere
		, BlueprintReadOnly
		, Category = UI
		, AssetRegistrySearchable)
	FText ItemName;

	UPROPERTY(EditAnywhere
		, BlueprintReadOnly
		, Category = UI
		, AssetRegistrySearchable)
	FText ItemDescription;
};
