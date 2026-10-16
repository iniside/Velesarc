// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "Assets/ArcTerrainBakedField.h"
#include "Noise/ArcTerrainNoiseParams.h"

#include "ArcTerrainNoiseModifier.generated.h"

/**
 * Terrain stamp that raises procedural relief over its footprint: fractal noise rasterised once into
 * a cached field and composited like any other baked source. No solve, no readback, no GPU noise —
 * the composite consumes rasters, so the field is built on the CPU and the shader only ever samples
 * it.
 *
 * The field is a pure function of the knobs, so it is NOT serialized: OnRegister rebuilds it, and
 * because its ContentId is hashed from those same knobs the rebuilt raster carries the id the last
 * bake already used and nothing downstream re-bakes. That is the whole reason it is safe to leave
 * the raster out of the map.
 *
 * Field values are stamp-local cm about the component's own plane, so an Additive blend lifts and
 * lowers the surface around the component's Z.
 */
UCLASS(ClassGroup = (ArcTerrain), meta = (BlueprintSpawnableComponent, DisplayName = "Terrain Noise Modifier"))
class ARCTERRAIN_API UArcTerrainNoiseModifier : public UArcTerrainStampComponent
{
	GENERATED_BODY()

public:
	UArcTerrainNoiseModifier();

	UPROPERTY(EditAnywhere, Category = "Noise", meta = (ShowOnlyInnerProperties))
	FArcTerrainNoiseParams Noise;

	/** Texels per side of the noise field, and so the ceiling on the detail the stamp can carry
	 *  whatever Octaves asks for: the shortest wavelength that survives rasterisation is two texels,
	 *  2 * CoverageXY / (FieldResolution - 1). The raster is resident for the stamp's whole life, and
	 *  the source cache holds a second copy of it plus a GPU texture. */
	UPROPERTY(EditAnywhere, Category = "Noise|Field", meta = (ClampMin = "64", ClampMax = "4096"))
	int32 FieldResolution = 2048;

	// --- Falloff (semantics: FArcStampSourceDesc EdgeMask / CircleMask) ---

	/** Width (cm) of the inward ramp from the footprint edge. Without it the noise ends on a cliff. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.0", UIMax = "50000.0", ForceUnits = "cm"))
	float EdgeFalloffBand = 10000.0f;

	/** Sharpness exponent of the edge mask: pow(saturate(dist/band), sharpness). Higher = sharper. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.1", UIMin = "1.0", UIMax = "16.0"))
	float EdgeSharpness = 2.0f;

	/** Corner rounding radius (cm) of the rect edge mask; 0 = square corners. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float CornerRadius = 0.0f;

	/** Additionally fade the stamp out through a circle / inscribed ellipse. */
	UPROPERTY(EditAnywhere, Category = "Falloff")
	bool bCircleMask = false;

	/** Patch-local XY offset (cm) of the circle center from the footprint center. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (EditCondition = "bCircleMask", ForceUnits = "cm"))
	FVector2D CircleCenterOffset = FVector2D::ZeroVector;

	/** Circle radius (cm). 0 = auto: the ellipse inscribed in the footprint. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.0", ForceUnits = "cm", EditCondition = "bCircleMask"))
	float CircleRadius = 0.0f;

	/** Width (cm) of the smoothstep ramp inward from the circle edge. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.0", UIMax = "50000.0", ForceUnits = "cm", EditCondition = "bCircleMask"))
	float CircleFalloffBand = 20000.0f;

	/** Rebuild the noise field and re-composite. Idempotent: an unchanged knob set rebuilds the same
	 *  raster under the same ContentId, so nothing downstream re-bakes. */
	UFUNCTION(CallInEditor, Category = "Noise")
	void RegenerateNoise();

	//~ Begin UActorComponent / UArcTerrainStampComponent interface
	virtual void OnRegister() override;
	virtual bool GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif
	//~ End UActorComponent / UArcTerrainStampComponent interface

private:
	/** Rasterise the noise into a new field — the modifier's ONLY mutation of NoiseField. */
	void BuildNoiseField();

	/** Immutable once built and replaced wholesale, so every descriptor hands out a refcount instead
	 *  of copying the whole raster. */
	TSharedPtr<const FArcTerrainBakedField> NoiseField;
};
