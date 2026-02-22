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

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "Components/ActorComponent.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "ArcNamedPrimaryAssetId.h"
#include "Tasks/TargetingTask.h"
#include "Types/TargetingSystemTypes.h"
#include "ArcBuilderComponent.generated.h"

ARCBUILDER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Build_Requirement_Item_Fail);

class UStaticMesh;
class UTargetingPreset;
class UArcItemDefinition;
class UArcBuildingDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArcBuilderComponent_OnPlacementCompleted, UArcItemDefinition*, InItemDef, FGameplayTag, Tag);

/**
 * Item fragment that references a UArcBuildingDefinition.
 * Stored on UArcItemDefinition to associate items with building data.
 */
USTRUCT(BlueprintType)
struct FArcItemFragment_BuilderData : public FArcItemFragment
{
	GENERATED_BODY()

public:
	/** Reference to the building definition data asset. */
	UPROPERTY(EditAnywhere, meta = (AllowedClasses = "/Script/ArcBuilder.ArcBuildingDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId BuildingDefinitionId;
};

/**
 * Builder component for survival-style building placement.
 *
 * Uses UArcBuildingDefinition data assets for building configuration,
 * supports both actor spawning and Mass VisEntity spawning,
 * and delegates snapping/requirements/consumption to pluggable instanced structs.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARCBUILDER_API UArcBuilderComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Config")
	TObjectPtr<UTargetingPreset> PlacementTargetingPreset;

	FTargetingRequestHandle TargetingRequestHandle;

	FVector PlacementOffsetLocation = FVector::ZeroVector;
	FRotator PlacementOffsetRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category = "Config")
	TObjectPtr<AActor> TemporaryPlacementActor;

	/** Cached building definition for current placement session. */
	UPROPERTY()
	TWeakObjectPtr<UArcBuildingDefinition> CurrentBuildingDef;

	TWeakObjectPtr<UArcItemDefinition> CurrentItemDef;

	bool bDidMeetPlaceRequirements = true;

	bool bUsePlacementGrid = false;

	// If true, we will use grid relative to location of current object or location.
	bool bUseRelativeGrid = false;
	bool bAlignGridToSurfaceNormal = true;

	FVector RelativeGridOrigin = FVector::ZeroVector;
	FQuat RelativeGridRotation = FQuat::Identity;

	int32 CurrentGridSize = 100;

	FVector FinalPlacementLocation = FVector::ZeroVector;
	FQuat FinalPlacementRotation = FQuat::Identity;

public:
	// Sets default values for this component's properties
	UArcBuilderComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category = "Arc Builder")
	bool DoesMeetItemRequirement(UArcItemDefinition* InItemDef) const;

	UFUNCTION(BlueprintCallable, Category = "Arc Builder")
	void SetUsePlacementGrid(bool bNewUsePlacementGrid)
	{
		bUsePlacementGrid = bNewUsePlacementGrid;
	}

	UFUNCTION(BlueprintCallable, Category = "Arc Builder")
	void SetUseRelativeGrid(bool bInUseRelativeGrid)
	{
		bUseRelativeGrid = bInUseRelativeGrid;
	}

	UFUNCTION(BlueprintCallable, Category = "Arc Builder")
	void SetAlignGridToSurfaceNormal(bool bInAlignGridToSurfaceNormal)
	{
		bAlignGridToSurfaceNormal = bInAlignGridToSurfaceNormal;
	}

	UFUNCTION(BlueprintCallable, Category = "Arc Builder")
	void SetNewGridSize(int32 NewGridSize)
	{
		CurrentGridSize = NewGridSize;
	}

	int32 GetGridSize() const
	{
		return CurrentGridSize;
	}

	UFUNCTION(BlueprintCallable, Category = "Arc Builder")
	void SetPlacementOffsetLocation(const FVector& NewLocation);

	UFUNCTION(BlueprintCallable, Category = "Arc Builder")
	void BeginPlacement(UArcItemDefinition* InBuilderData);

	/** Begin placement directly from a building definition (no item definition needed). */
	UFUNCTION(BlueprintCallable, Category = "Arc Builder")
	void BeginPlacementFromDefinition(UArcBuildingDefinition* InBuildingDef);

	UFUNCTION(BlueprintCallable, Category = "Arc Builder")
	void EndPlacement();

	void HandleTargetingCompleted(FTargetingRequestHandle InTargetingRequestHandle);

	UFUNCTION(BlueprintCallable, Category = "Arc Builder")
	void PlaceObject();

private:
	/** Resolve and cache a UArcBuildingDefinition from an item definition's fragment. */
	UArcBuildingDefinition* ResolveBuildingDefinition(UArcItemDefinition* InItemDef) const;
};

UCLASS()
class UArcTargetingTask_BuildTrace : public UTargetingTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Target Trace Selection | Collision Data")
	float SphereRadius = 400.f;

public:
	UArcTargetingTask_BuildTrace(const FObjectInitializer& ObjectInitializer);

	/** Evaluation function called by derived classes to process the targeting request */
	virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;
};
