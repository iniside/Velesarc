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

#include "CoreMinimal.h"
#include "ArcNamedPrimaryAssetId.h"
#include "Engine/DataAsset.h"
#include "Engine/GameInstance.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Interface.h"
#include "UObject/PrimaryAssetId.h"

#include "ArcUserFacingExperienceDefinition.generated.h"

class UWorld;
class UCommonSession_HostSessionRequest;
class UTexture2D;
class UUserWidget;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcUserFacingExperienceDefinitionData
{
	GENERATED_BODY()
};

//////////////////////////////////////////////////////////////////////
// UArcUserFacingExperienceDefinition

UCLASS(BlueprintType)
class ARCCORE_API UArcUserFacingExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Experience, meta = (AllowedTypes = "Map"))
	FPrimaryAssetId MapID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Experience, meta = (AllowedTypes = "ArcExperienceDefinition"))
	FArcNamedPrimaryAssetId ExperienceID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Experience, meta = (BaseStruct = "/Script/ArcCore.ArcUserFacingExperienceDefinitionData"))
	TArray<FInstancedStruct> CustomData;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Experience)
	TMap<FString, FString> ExtraArgs;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Experience)
	FText TileTitle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Experience)
	FText TileSubTitle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Experience)
	FText TileDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = UI
		, meta = (AllowPrivateAccess = "true", DisplayThumbnail = "true", DisplayName = "Image", AllowedClasses =
			"/Script/Engine.Texture,/Script/Engine.MaterialInterface,/Script/Engine.SlateTextureAtlasInterface",
			DisallowedClasses = "/Script/MediaAssets.MediaTexture"))
	TSoftObjectPtr<UObject> TileIcon;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Experience, AssetRegistrySearchable)
	bool bIsVisible = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Experience)
	bool bIsDefaultExperience;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Experience)
	int32 MaxPlayerCount = 16;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = LoadingScreen)
	TSoftClassPtr<UUserWidget> LoadingScreenWidget;

public:
	UFUNCTION(BlueprintCallable, BlueprintPure = false)
	UCommonSession_HostSessionRequest* CreateHostingRequest() const;
};
