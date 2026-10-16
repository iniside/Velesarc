// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataMaps/ArcTerrainDataMapTypes.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "Stamps/ArcTerrainPatchStamp.h" // FArcTerrainPatchDirectWeight
#include "ArcTerrainTexturePatchModifier.generated.h"

class UTexture2D;

/** Shape of the falloff mask multiplied into a texture patch stamp. */
UENUM()
enum class EArcTerrainStampMaskShape : uint8
{
	/** No mask: the footprint rectangle is a hard clip. */
	None,

	/** Inward ramp from every footprint edge, with optional corner rounding. */
	Box,

	/** Radial gradient inside the footprint rectangle. */
	Circle
};

/**
 * ArcTerrain stamp backed by a texture patch: an optional heightmap plus any number of
 * per-channel weight layers. Height and weights are independent — either alone or both together.
 *
 * Despite the Modifier suffix, this is an ArcTerrain stamp component, not a MeshPartition modifier.
 * It feeds the existing descriptor/composite path used by GPU preview and final terrain builds.
 */
UCLASS(ClassGroup = (ArcTerrain), meta = (BlueprintSpawnableComponent, DisplayName = "Terrain Texture Patch Modifier"))
class ARCTERRAIN_API UArcTerrainTexturePatchModifier : public UArcTerrainStampComponent
{
	GENERATED_BODY()

public:
	/** Heightmap texture. If null, this stamp contributes no height and acts as weight-only. */
	UPROPERTY(EditAnywhere, Category = "Source")
	TObjectPtr<UTexture2D> HeightTexture;

	/** Texture channel decoded as height. */
	UPROPERTY(EditAnywhere, Category = "Source")
	EArcTerrainSourceChannel HeightSourceChannel = EArcTerrainSourceChannel::R;

	/** Take the height zero point from the heightmap's border-ring mean, so the stamp contributes 0 at its own edge. */
	UPROPERTY(EditAnywhere, Category = "Source")
	bool bAutoHeightZero = false;

	/** height (cm) = (texel - HeightZeroInEncoding) * HeightEncodingScale. */
	UPROPERTY(EditAnywhere, Category = "Source", meta = (EditCondition = "!bAutoHeightZero"))
	float HeightZeroInEncoding = 0.5f;

	/** height (cm) = (texel - HeightZeroInEncoding) * HeightEncodingScale. */
	UPROPERTY(EditAnywhere, Category = "Source", meta = (Units = "cm"))
	float HeightEncodingScale = 1000.0f;

	/** Weight layers stamped onto the terrain (per-channel source textures). */
	UPROPERTY(EditAnywhere, Category = "Weights")
	TArray<FArcTerrainPatchDirectWeight> WeightLayers;

	/** Gaea Erosion2 node, Deposits output. Decoded value is CENTIMETRES of soil: 0 = bare rock, and
	 *  EncodingScale is the cm a full-strength (1.0) texel means. Read as fertility and rooting depth. */
	UPROPERTY(EditAnywhere, Category = "Data Maps")
	FArcTerrainDataMapSource Deposits;

	/** Gaea Erosion2 node, Wear output, 0..1: 0 = untouched ground, 1 = fully carved streak. Where
	 *  water scoured, not how much material it removed — that is Deposits. Read as material breakup
	 *  and debris. */
	UPROPERTY(EditAnywhere, Category = "Data Maps")
	FArcTerrainDataMapSource Wear;

	/** Gaea Erosion2 node, Flow output. Normalized flow accumulation, 0..1: 0 = ridge, no upstream
	 *  area; 1 = main channel. Read as drainage, replacing the hydrology solver where present. */
	UPROPERTY(EditAnywhere, Category = "Data Maps")
	FArcTerrainDataMapSource Flow;

	/** Gaea Derive > Soil node. Substrate permeability, 0..1: 0 = impermeable, water pools; 1 = drains
	 *  freely. With Flow this gives real moisture instead of a slope proxy. */
	UPROPERTY(EditAnywhere, Category = "Data Maps")
	FArcTerrainDataMapSource Soil;

	/** Gaea Simulate > Scree node. Loose-material density, 0..1: 0 = none, 1 = full scree cover. Read
	 *  as scatter for rock that is part of the terrain. */
	UPROPERTY(EditAnywhere, Category = "Data Maps")
	FArcTerrainDataMapSource Scree;

	/** Gaea Derive > RockMap node. Rock hardness, 0..1: 0 = soft, weathered; 1 = hard bedrock. Read as
	 *  material and rock type where there is no soil. */
	UPROPERTY(EditAnywhere, Category = "Data Maps")
	FArcTerrainDataMapSource RockMap;

	/** Gaea Derive > FlowMap node, R channel. Terrain-local X of the flow direction, decoded to
	 *  [-1,1]: texel 0 = -1, 0.5 = still, 1 = +1. Read as anisotropy — oriented scree, laid grass,
	 *  roads following valleys. */
	UPROPERTY(EditAnywhere, Category = "Data Maps")
	FArcTerrainDataMapSource FlowMapX = FArcTerrainDataMapSource(2.0f, 0.5f);

	/** Gaea Derive > FlowMap node, G channel. Terrain-local Y of the flow direction, decoded to
	 *  [-1,1]: texel 0 = -1, 0.5 = still, 1 = +1. Read as anisotropy — oriented scree, laid grass,
	 *  roads following valleys. */
	UPROPERTY(EditAnywhere, Category = "Data Maps")
	FArcTerrainDataMapSource FlowMapY = FArcTerrainDataMapSource(2.0f, 0.5f);

	/** Gaea Simulate > Rivers node, Rivers mask output, 0..1: 0 = dry ground, 1 = river channel. Read
	 *  as permanent water for placement exclusion and shoreline dressing. */
	UPROPERTY(EditAnywhere, Category = "Data Maps")
	FArcTerrainDataMapSource Rivers;

	/** Blend every mask above against other modifiers' masks of the SAME type, using this stamp's
	 *  BlendMode. Min/Max are hard here — MinMaxSoftness is a height-domain cm value and does not
	 *  apply to masks. Off: these masks replace whatever earlier modifiers wrote. */
	UPROPERTY(EditAnywhere, Category = "Data Maps")
	bool bBlendDataMaps = false;

	// --- Falloff (semantics: FArcStampSourceDesc EdgeMask / CircleMask) ---

	/** Mask multiplied into height, weights, carve and data maps alike. */
	UPROPERTY(EditAnywhere, Category = "Falloff")
	EArcTerrainStampMaskShape MaskShape = EArcTerrainStampMaskShape::None;

	/** Width (cm) of the inward ramp from the footprint edge. 0 = hard vertical clip. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.0", ForceUnits = "cm",
		EditCondition = "MaskShape == EArcTerrainStampMaskShape::Box", EditConditionHides))
	float EdgeFalloffBand = 1000.0f;

	/** Sharpness exponent of the edge mask: smoothstep(pow(saturate(dist/band), sharpness)). Higher = sharper. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.1",
		EditCondition = "MaskShape == EArcTerrainStampMaskShape::Box", EditConditionHides))
	float EdgeSharpness = 4.0f;

	/** Corner rounding radius (cm) of the rect edge mask. 0 = square corners. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.0", ForceUnits = "cm",
		EditCondition = "MaskShape == EArcTerrainStampMaskShape::Box", EditConditionHides))
	float CornerRadius = 0.0f;

	/** Patch-local XY offset (cm) of the circle center from the footprint center. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ForceUnits = "cm",
		EditCondition = "MaskShape == EArcTerrainStampMaskShape::Circle", EditConditionHides))
	FVector2D CircleCenterOffset = FVector2D::ZeroVector;

	/** Circle radius (cm). 0 = auto: the ellipse inscribed in the footprint. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm",
		EditCondition = "MaskShape == EArcTerrainStampMaskShape::Circle", EditConditionHides))
	float CircleRadius = 0.0f;

	/** Width (cm) of the smoothstep ramp inward from the circle edge. 0 = hard clip at the circle. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.0", UIMax = "5000.0", ForceUnits = "cm",
		EditCondition = "MaskShape == EArcTerrainStampMaskShape::Circle", EditConditionHides))
	float CircleFalloffBand = 1000.0f;

	//~ Begin UArcTerrainStampComponent interface
	virtual bool GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const override;
	//~ End UArcTerrainStampComponent interface

#if WITH_EDITOR
	/** Every mask property, in EArcTerrainDataMap order, as pointers into this component. The one
	 *  place the mask fields are enumerated; the returned set is fixed, so entries with a null
	 *  Texture are present and the caller filters. */
	TArray<TPair<EArcTerrainDataMap, const FArcTerrainDataMapSource*>> GetDataMapSources() const;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Footprint wire box plus the geometry of the MaskShape actually in effect. */
	virtual void DrawStampVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI) const override;
#endif
};
