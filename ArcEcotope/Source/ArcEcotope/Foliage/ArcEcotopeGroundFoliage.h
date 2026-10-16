// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "ArcEcotopeGroundFoliage.generated.h"

class UMaterialInterface;
class UStaticMesh;

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcEcotopeGroundFoliageMesh
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;
};

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcEcotopeFoliageRenderSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadows")
	bool bCastShadow = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadows")
	bool bCastDynamicShadow = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadows")
	bool bCastContactShadow = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadows")
	bool bCastShadowAsTwoSided = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadows")
	bool bCastFarShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Culling", meta = (ClampMin = "0"))
	int32 InstanceMinDrawDistance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Culling", meta = (ClampMin = "0", ToolTip = "Ignored on a Nanite mesh: the Nanite proxy reads only InstanceMinDrawDistance and InstanceEndCullDistance."))
	int32 InstanceStartCullDistance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Culling", meta = (ClampMin = "0"))
	int32 InstanceEndCullDistance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Culling", meta = (ClampMin = "0.0"))
	float MinDrawDistance = 0.0f;

	/** Zero draws at any distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Culling", meta = (ClampMin = "0.0"))
	float MaxDrawDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Position Offset")
	bool bEvaluateWorldPositionOffset = true;

	/** Zero keeps world position offset evaluated at every distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Position Offset", meta = (ClampMin = "0"))
	int32 WorldPositionOffsetDisableDistance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lighting")
	bool bAffectDynamicIndirectLighting = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lighting")
	bool bAffectDistanceFieldLighting = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering")
	bool bReceivesDecals = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering")
	bool bVisibleInRayTracing = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOD", meta = (ToolTip = "Ignored on a Nanite mesh: the Nanite proxy reads only InstanceMinDrawDistance and InstanceEndCullDistance."))
	bool bUseGpuLodSelection = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LOD", meta = (ClampMin = "0.01", ToolTip = "Ignored on a Nanite mesh: the Nanite proxy reads only InstanceMinDrawDistance and InstanceEndCullDistance."))
	float InstanceLODDistanceScale = 1.0f;

	bool operator==(const FArcEcotopeFoliageRenderSettings& Other) const
	{
		return bCastShadow == Other.bCastShadow
			&& bCastDynamicShadow == Other.bCastDynamicShadow
			&& bCastContactShadow == Other.bCastContactShadow
			&& bCastShadowAsTwoSided == Other.bCastShadowAsTwoSided
			&& bCastFarShadow == Other.bCastFarShadow
			&& InstanceMinDrawDistance == Other.InstanceMinDrawDistance
			&& InstanceStartCullDistance == Other.InstanceStartCullDistance
			&& InstanceEndCullDistance == Other.InstanceEndCullDistance
			&& MinDrawDistance == Other.MinDrawDistance
			&& MaxDrawDistance == Other.MaxDrawDistance
			&& bEvaluateWorldPositionOffset == Other.bEvaluateWorldPositionOffset
			&& WorldPositionOffsetDisableDistance == Other.WorldPositionOffsetDisableDistance
			&& bAffectDynamicIndirectLighting == Other.bAffectDynamicIndirectLighting
			&& bAffectDistanceFieldLighting == Other.bAffectDistanceFieldLighting
			&& bReceivesDecals == Other.bReceivesDecals
			&& bVisibleInRayTracing == Other.bVisibleInRayTracing
			&& bUseGpuLodSelection == Other.bUseGpuLodSelection
			&& InstanceLODDistanceScale == Other.InstanceLODDistanceScale;
	}

	bool operator!=(const FArcEcotopeFoliageRenderSettings& Other) const
	{
		return !(*this == Other);
	}
};

/**
 * One grass or small ground plant drawn by the runtime GPU scatter. It carries only what that
 * scatter consumes: nothing here is simulated, hashed or serialized into a bake.
 */
UCLASS(BlueprintType)
class ARCECOTOPE_API UArcEcotopeGroundFoliage : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TArray<FArcEcotopeGroundFoliageMesh> Meshes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering")
	FArcEcotopeFoliageRenderSettings RenderSettings;

	/**
	 * Plants per square metre. This does NOT set the instance count: the scatter derives a world
	 * lattice step from it and the count is GridX * GridY, because every random draw is keyed on the
	 * world lattice cell and the two cannot disagree.
	 *
	 * Editing it re-anchors that lattice and therefore re-randomises the whole patch. That is an
	 * authoring-time event, not a runtime one.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter", meta = (ClampMin = "0.01"))
	float DensityPerSquareMeter = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter", meta = (ClampMin = "0.01"))
	float MinScale = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter", meta = (ClampMin = "0.01"))
	float MaxScale = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxYawDeg = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter")
	bool bAlignToNormal = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxSlopeDegrees = 35.0f;

	/** NAME_None disables the layer filter entirely; any other name requires a cell whose weights have resolved. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	FName TerrainLayerChannel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinLayerWeight = 0.25f;

	/** -1 places the plant on open ground only, +1 under deep shade only, 0 is indifferent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float ShadePreference = 0.0f;

	/** 0 ignores the canopy entirely, whatever ShadePreference says. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShadeStrength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinMoisture = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forest", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinFertility = 0.0f;
};

/** One ground foliage asset placed on a simulation volume, with a per-volume density trim. */
USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcEcotopeGroundFoliageEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foliage")
	TObjectPtr<UArcEcotopeGroundFoliage> Foliage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foliage")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foliage", meta = (ClampMin = "0.01"))
	float DensityScale = 1.0f;
};
