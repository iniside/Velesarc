// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/ObjectSaveContext.h"
#include "ArcBuildIngredient.h"
#include "ArcBuildSnappingMethod.h"
#include "ArcBuildResourceConsumption.h"
#include "ArcBuildRequirement.h"
#include "ArcBuildPostPlaceAction.h"

#include "ArcBuildingDefinition.generated.h"

class UArcMassEntityConfigAsset;
class UArcTargetingPreset;

USTRUCT()
struct ARCBUILDER_API FArcBuildSocketInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer SocketTags;

	UPROPERTY(EditAnywhere)
	TArray<FName> SocketNames;
};

/**
 * Primary data asset defining a building piece.
 * Identified by a GUID-based primary asset ID.
 */
UCLASS(BlueprintType, meta = (LoadBehavior = "LazyOnDemand"))
class ARCBUILDER_API UArcBuildingDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// ---- Identity ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building")
	FGuid BuildingId;

	UPROPERTY(EditAnywhere, Category = "Building")
	FPrimaryAssetType BuildingType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	FText DisplayName;

	// Short UI description shown in build menus.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	FText Description;

	// Menu icon; UObject so both UTexture2D and ArcSvg assets work (same convention as FArcItemFragment_Icon).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	TSoftObjectPtr<UObject> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	FGameplayTagContainer BuildingTags;

	// ---- Spawning ----

	/** Actor class to spawn when placing. Used as fallback if MassEntityConfig is not set. */
	UPROPERTY(EditAnywhere, Category = "Building|Spawning")
	TSoftClassPtr<AActor> ActorClass;

	/** Mass entity config. If set, PlaceObject() spawns a Mass VisEntity instead of an actor. */
	UPROPERTY(EditAnywhere, Category = "Building|Spawning", meta = (DisplayThumbnail = false))
	TObjectPtr<UArcMassEntityConfigAsset> MassEntityConfig;

	// ---- Visual / Preview ----

	UPROPERTY(EditAnywhere, Category = "Building|Preview")
	TObjectPtr<UMaterialInterface> RequirementMeetMaterial;

	UPROPERTY(EditAnywhere, Category = "Building|Preview")
	TObjectPtr<UMaterialInterface> RequirementFailedMaterial;

	UPROPERTY(EditAnywhere, Category = "Building|Preview")
	FVector ApproximateSize = FVector(100.0, 100.0, 100.0);

	// ---- Grid ----

	UPROPERTY(EditAnywhere, Category = "Building|Grid")
	int32 DefaultGridSize = 100;

	// ---- Sockets ----

	UPROPERTY(EditAnywhere, Category = "Building|Sockets")
	FArcBuildSocketInfo SocketInfo;

	// ---- Ingredients ----

	/** Items required to place this building. Empty = free placement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Ingredients",
		meta = (BaseStruct = "/Script/ArcBuilder.ArcBuildIngredient", ExcludeBaseStruct))
	TArray<FInstancedStruct> Ingredients;

	// ---- Resource Consumption ----

	/**
	 * Strategy for checking and consuming building resources.
	 * Base struct = free placement; FArcBuildResourceConsumption_MassStore draws
	 * ingredients from an FArcMassItemStoreFragment on the instigator entity.
	 */
	UPROPERTY(EditAnywhere, Category = "Building|Consumption")
	TInstancedStruct<FArcBuildResourceConsumption> ResourceConsumption;

	// ---- Targeting ----

	/**
	 * Per-building override for the placement targeting pipeline. When set, the
	 * builder subsystem seeds its placement loop with this preset; otherwise it
	 * falls back to the subsystem's DefaultPlacementPreset.
	 */
	UPROPERTY(EditAnywhere, Category = "Building|Targeting")
	TObjectPtr<UArcTargetingPreset> PlacementTargetingPreset;

	// ---- Snapping ----

	/** Snapping method. If not set, falls through to grid snapping. */
	UPROPERTY(EditAnywhere, Category = "Building|Snapping")
	TInstancedStruct<FArcBuildSnappingMethod> SnappingMethod;

	// ---- Placement Requirements ----

	/** Additional placement requirements (line of sight, foundation check, etc.). */
	UPROPERTY(EditAnywhere, Category = "Building|Requirements",
		meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FArcBuildRequirement>> BuildRequirements;

	// ---- Post-Place Actions ----

	/**
	 * Actions the server place command runs around entity spawn: BuildSpawnOverrides
	 * before SpawnEntityWithOverrides, PostPlaced after (see FArcBuildPostPlaceAction).
	 * E.g. FArcBuildPostPlaceAction_IngredientStats transfers consumed-item stats onto
	 * the placed entity's attributes.
	 */
	UPROPERTY(EditAnywhere, Category = "Building|PostPlace",
		meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FArcBuildPostPlaceAction>> PostPlaceActions;

	// ---- Primary Asset ----

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void PostInitProperties() override;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Building")
	void RegenerateBuildingId();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	// ---- Helpers ----

	bool HasMassEntityConfig() const { return MassEntityConfig != nullptr; }

	/**
	 * Resolve an actor class for preview placement.
	 * Tries ActorClass first, then falls back to the MassEntityConfig's engine
	 * UMassVisualizationTrait (HighResTemplateActor). Returns nullptr if neither provides a class.
	 * Note: this only covers the class-based path. Ghosts built from the Arc visualization traits
	 * (mesh/skinned/composite/unified) are assembled component-wise by ArcBuilderGhostPreview,
	 * which calls this first and falls through to trait extraction when it returns nullptr.
	 */
	UClass* ResolvePreviewActorClass() const;
};
