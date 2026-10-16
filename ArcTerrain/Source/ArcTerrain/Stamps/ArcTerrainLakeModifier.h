// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "Assets/ArcTerrainBakedField.h"
#include "ArcTerrainLakeModifier.generated.h"

/**
 * Terrain stamp that carves an authored lake basin: a bowl of revolution around the component,
 * blended with Min so the terrain is deepened inside the shoreline and untouched outside it. No
 * solve and no readback — the basin is a closed-form function of the knobs below.
 *
 * The bowl is rasterised ONCE into a cached field with its own ContentId and regenerated when a
 * shape knob changes; GetSourceDescriptor only hands out a refcount to it, because it runs per stamp
 * on several per-cell paths.
 *
 * Place the component AT the intended water surface: field values are stamp-local cm, so the
 * component's own plane (offset by WaterSurfaceZ) is where the water sits, and a Min blend can only
 * carve terrain that is above it.
 *
 * The water surface itself is not terrain — WaterSurfaceZ is recorded for a later water-rendering
 * step; this component emits nothing but the basin.
 */
UCLASS(ClassGroup = (ArcTerrain), meta = (BlueprintSpawnableComponent, DisplayName = "Terrain Lake Modifier"))
class ARCTERRAIN_API UArcTerrainLakeModifier : public UArcTerrainStampComponent
{
	GENERATED_BODY()

public:
	UArcTerrainLakeModifier();

	// --- Basin ---

	/** Water surface, stamp-local cm: the plane the bowl is measured down from, and the height a later
	 *  water-rendering step draws the surface at. 0 = the component's own plane. */
	UPROPERTY(EditAnywhere, Category = "Lake|Basin", meta = (ForceUnits = "cm"))
	float WaterSurfaceZ = 0.0f;

	/** Depth of the bowl at its centre, below the water surface. */
	UPROPERTY(EditAnywhere, Category = "Lake|Basin",
		meta = (ClampMin = "0.0", UIMax = "50000.0", ForceUnits = "cm"))
	float BasinDepth = 5000.0f;

	/** Where the bed reaches the water surface — the waterline — as a fraction of the mask geometry
	 *  (the circle/inscribed ellipse when bCircleMask is set, the footprint rectangle otherwise). The
	 *  remainder is the rim that rises back above the terrain. */
	UPROPERTY(EditAnywhere, Category = "Lake|Basin", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ShorelineExtent = 0.8f;

	/** Bowl profile: depth = BasinDepth * pow(1 - (r/shore)^2, exponent). 1 = parabolic; below 1
	 *  flattens the bed and steepens the banks, above 1 does the opposite. */
	UPROPERTY(EditAnywhere, Category = "Lake|Basin", meta = (ClampMin = "0.25", ClampMax = "4.0"))
	float BasinProfileExponent = 1.0f;

	/** How far above the water surface the field sits beyond the shoreline — the Min blend then keeps
	 *  the terrain there. Terrain rising higher than this inside the footprint IS cut down to it. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Lake|Basin",
		meta = (ClampMin = "0.0", UIMax = "200000.0", ForceUnits = "cm"))
	float RimClearance = 20000.0f;

	/** Texels per side of the basin field. The bowl is smooth, so a modest raster resolves it exactly;
	 *  capped at 2048 (16 MB per field) because every regeneration copies the whole field into the undo
	 *  buffer, whose default budget is 256 MB — ~16 regenerations at the cap evict all older history. */
	UPROPERTY(EditAnywhere, Category = "Lake|Basin", meta = (ClampMin = "64", ClampMax = "2048"))
	int32 CarveResolution = 512;

	// --- Falloff (semantics: FArcStampSourceDesc EdgeMask / CircleMask) ---

	/** Width (cm) of the inward ramp from the footprint edge. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.0", UIMax = "20000.0", ForceUnits = "cm"))
	float EdgeFalloffBand = 2000.0f;

	/** Sharpness exponent of the edge mask: pow(saturate(dist/band), sharpness). Higher = sharper. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.1", UIMin = "1.0", UIMax = "16.0"))
	float EdgeSharpness = 2.0f;

	/** Round lake: the mask AND the bowl take their shape from the circle below. Cleared, both follow
	 *  the footprint rectangle. */
	UPROPERTY(EditAnywhere, Category = "Falloff")
	bool bCircleMask = true;

	/** Patch-local XY offset (cm) of the circle center from the footprint center. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (EditCondition = "bCircleMask", ForceUnits = "cm"))
	FVector2D CircleCenterOffset = FVector2D::ZeroVector;

	/** Circle radius (cm). 0 = auto: the ellipse inscribed in the footprint. */
	UPROPERTY(EditAnywhere, Category = "Falloff",
		meta = (ClampMin = "0.0", ForceUnits = "cm", EditCondition = "bCircleMask"))
	float CircleRadius = 0.0f;

	/** Width (cm) of the smoothstep ramp inward from the circle edge — the shore fades the carve out
	 *  into the surrounding terrain over it. */
	UPROPERTY(EditAnywhere, Category = "Falloff",
		meta = (ClampMin = "0.0", UIMax = "20000.0", ForceUnits = "cm", EditCondition = "bCircleMask"))
	float CircleFalloffBand = 5000.0f;

	/** Rebuild the basin field and re-composite. Idempotent: an unchanged knob set rebuilds the same
	 *  raster under the same ContentId, so nothing downstream re-bakes. */
	UFUNCTION(CallInEditor, Category = "Lake")
	void RegenerateBasin();

	//~ Begin UObject / UActorComponent / UArcTerrainStampComponent interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void OnRegister() override;
	virtual bool GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject / UActorComponent / UArcTerrainStampComponent interface

private:
	/** Rasterise the bowl into a new field — the modifier's ONLY mutation of BasinField. Its ContentId
	 *  is hashed from the shape parameters, so it changes exactly when the raster does: that id is
	 *  folded into the cell collision key, which IS the composite-input key the section keys
	 *  fold, and an id that moved without the content would re-bake the whole footprint. */
	void BuildBasinField();

	/** Immutable once built and replaced wholesale, so every descriptor hands out a refcount instead of
	 *  copying the whole raster. Serialized by hand (a TSharedPtr is no UPROPERTY), which also puts it
	 *  in the transaction buffer so undo restores the previous basin. */
	TSharedPtr<const FArcTerrainBakedField> BasinField;
};
