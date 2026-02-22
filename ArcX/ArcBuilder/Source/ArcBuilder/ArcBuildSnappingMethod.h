// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "ArcBuildSnappingMethod.generated.h"

class UStaticMeshComponent;

/**
 * Base snapping method for building placement.
 * Subclasses define different snap behaviors (sockets, bounding box, etc.).
 * Configured per building definition via instanced struct.
 */
USTRUCT(BlueprintType)
struct ARCBUILDER_API FArcBuildSnappingMethod
{
	GENERATED_BODY()

public:
	/**
	 * Compute the snapped transform given the raw placement data.
	 * @param HitResult     The trace hit result from targeting.
	 * @param PreviewMesh   The preview actor's static mesh component.
	 * @param TargetMesh    The target actor's static mesh component (may be null).
	 * @param TargetTags    Tags on the target actor (for filtering).
	 * @param OutTransform  Filled with the snapped transform if snapping is applied.
	 * @return True if snapping was applied, false to fall back to grid/raw placement.
	 */
	virtual bool ComputeSnappedTransform(
		const FHitResult& HitResult,
		UStaticMeshComponent* PreviewMesh,
		UStaticMeshComponent* TargetMesh,
		const FGameplayTagContainer& TargetTags,
		FTransform& OutTransform) const;

	virtual ~FArcBuildSnappingMethod() = default;
};

/**
 * Socket-based snapping. Matches sockets between the preview and target meshes.
 * Extracted from existing HandleTargetingCompleted() logic.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Socket Snapping"))
struct ARCBUILDER_API FArcBuildSnappingMethod_Socket : public FArcBuildSnappingMethod
{
	GENERATED_BODY()

public:
	/** Only snap to sockets with matching names between preview and target. */
	UPROPERTY(EditAnywhere)
	bool bMatchSocketsByName = true;

	/** Snap to the closest socket rather than the first match found. */
	UPROPERTY(EditAnywhere)
	bool bSnapToClosestSocket = false;

	/** Tags required on the target actor for socket snapping to engage. */
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer SnappingRequiredTags;

	virtual bool ComputeSnappedTransform(
		const FHitResult& HitResult,
		UStaticMeshComponent* PreviewMesh,
		UStaticMeshComponent* TargetMesh,
		const FGameplayTagContainer& TargetTags,
		FTransform& OutTransform) const override;
};

/**
 * Bounding-box edge/face snapping.
 * Snaps the preview to the nearest face of the target's bounding box.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Bounding Box Snapping"))
struct ARCBUILDER_API FArcBuildSnappingMethod_BoundingBox : public FArcBuildSnappingMethod
{
	GENERATED_BODY()

public:
	virtual bool ComputeSnappedTransform(
		const FHitResult& HitResult,
		UStaticMeshComponent* PreviewMesh,
		UStaticMeshComponent* TargetMesh,
		const FGameplayTagContainer& TargetTags,
		FTransform& OutTransform) const override;
};
