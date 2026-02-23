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

#include "ArcBuildingDefinition.generated.h"

class UMassEntityConfigAsset;

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
 * Identified by a GUID-based primary asset ID, following the same
 * pattern as UArcRecipeDefinition and UArcLootTable.
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	FGameplayTagContainer BuildingTags;

	// ---- Spawning ----

	/** Actor class to spawn when placing. Used as fallback if MassEntityConfig is not set. */
	UPROPERTY(EditAnywhere, Category = "Building|Spawning")
	TSoftClassPtr<AActor> ActorClass;

	/** Mass entity config. If set, PlaceObject() spawns a Mass VisEntity instead of an actor. */
	UPROPERTY(EditAnywhere, Category = "Building|Spawning", meta = (DisplayThumbnail = false))
	TObjectPtr<UMassEntityConfigAsset> MassEntityConfig;

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
	 * Default: FArcBuildResourceConsumption_ItemStore (looks for UArcItemsStoreComponent on owner).
	 */
	UPROPERTY(EditAnywhere, Category = "Building|Consumption")
	TInstancedStruct<FArcBuildResourceConsumption> ResourceConsumption;

	// ---- Snapping ----

	/** Snapping method. If not set, falls through to grid snapping. */
	UPROPERTY(EditAnywhere, Category = "Building|Snapping")
	TInstancedStruct<FArcBuildSnappingMethod> SnappingMethod;

	// ---- Placement Requirements ----

	/** Additional placement requirements (line of sight, foundation check, etc.). */
	UPROPERTY(EditAnywhere, Category = "Building|Requirements",
		meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FArcBuildRequirement>> BuildRequirements;

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
	 * Tries ActorClass first, then falls back to the MassEntityConfig's visualization trait actor class.
	 * Returns nullptr if neither provides a valid actor class.
	 */
	UClass* ResolvePreviewActorClass() const;
};
