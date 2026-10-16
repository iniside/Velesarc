// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "Assets/ArcTerrainBakedField.h"
#include "ArcTerrainBakedStamp.generated.h"

/**
 * Terrain stamp composited from a baked scalar field (height) plus optional named weight-map layers.
 * Single class covering both authoring workflows (CPU heightmap graph and TextureGraph): the actual
 * bake is delegated to ArcEditorTools via the static OnBakeRequested delegate — this Runtime class
 * carries no editor-module dependency and just stores the resulting CPU fields (non-transient) and
 * hands COPIES to the subsystem on demand.
 */
UCLASS(ClassGroup = (ArcTerrain), meta = (BlueprintSpawnableComponent, DisplayName = "Terrain Baked Stamp"))
class ARCTERRAIN_API UArcTerrainBakedStamp : public UArcTerrainStampComponent
{
	GENERATED_BODY()

public:
	/** Fired by Bake() so ArcEditorTools can evaluate BakeSource and fill BakedField/BakedLayers.
	 *  Returns true only when the stamp's baked fields were successfully (re)filled. Declared here
	 *  (Runtime) and bound in FArcEditorToolsModule::StartupModule — one-way dep. */
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnArcTerrainBakeRequested, UArcTerrainBakedStamp&);
	static FOnArcTerrainBakeRequested OnBakeRequested;

	/** Ask ArcEditorTools to (re)bake BakeSource into BakedField/BakedLayers, then re-composite. When the
	 *  delegate is unbound (ArcEditorTools not loaded) a toast explains why nothing happened. */
	UFUNCTION(CallInEditor, Category = "Bake")
	void Bake();

#if WITH_EDITORONLY_DATA
	/** Source to bake from: a UArcHeightmapGraph or a UTextureGraph. Soft UObject ref because both are
	 *  editor-module types this Runtime class must not hard-reference. */
	UPROPERTY(EditAnywhere, Category = "Bake")
	TSoftObjectPtr<UObject> BakeSource;

	/** Which TextureGraph Output node supplies the heightfield (TextureGraph sources only; every other
	 *  Output becomes a layer). Ignored for UArcHeightmapGraph sources. */
	UPROPERTY(EditAnywhere, Category = "Bake")
	FName HeightOutputName;
#endif // WITH_EDITORONLY_DATA

	/** Multiplier decoding baked height texels into stamp-local cm (fed to the composite shader's
	 *  EncodingScale). Default 1.0: heightmap-graph bakes are already cm-valued. TextureGraph bakes
	 *  are normalized 0-1 — set e.g. 5000 to match the old modifier's HeightRange. */
	UPROPERTY(EditAnywhere, Category = "Bake", meta = (ForceUnits = "cm"))
	float HeightScale = 1.0f;

	/** Maps a baked layer name -> the terrain channel it paints into. */
	UPROPERTY(EditAnywhere, Category = "Channels", meta = (GetOptions = "GetTerrainChannels"))
	TMap<FName, FName> LayerChannelMap;

	/** Baked heightfield (non-transient: persisted in the package so builds never touch a UObject). */
	UPROPERTY()
	FArcTerrainBakedField BakedField;

	/** Baked weight-map layers keyed by bake-source layer name (non-transient). */
	UPROPERTY()
	TMap<FName, FArcTerrainBakedField> BakedLayers;

	// --- Falloff (semantics: FArcStampSourceDesc EdgeMask / CircleMask) ---

	/** Width (cm) of the inward ramp from the footprint edge. 0 = hard vertical clip. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Falloff", meta = (ClampMin = "0.0", UIMax = "5000.0", ForceUnits = "cm"))
	float EdgeFalloffBand = 1000.0f;

	/** Sharpness exponent of the edge mask: pow(saturate(dist/band), sharpness). Higher = sharper. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Falloff", meta = (ClampMin = "0.1", UIMin = "1.0", UIMax = "16.0"))
	float EdgeSharpness = 4.0f;

	/** Multiply a circular blend mask (smoothstep edge) into the stamp — height AND weights. */
	UPROPERTY(EditAnywhere, Category = "Falloff")
	bool bCircleMask = false;

	/** Patch-local XY offset (cm) of the circle center from the footprint center. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (EditCondition = "bCircleMask", ForceUnits = "cm"))
	FVector2D CircleCenterOffset = FVector2D::ZeroVector;

	/** Circle radius (cm). 0 = auto: the ellipse inscribed in the footprint. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm", EditCondition = "bCircleMask"))
	float CircleRadius = 0.0f;

	/** Width (cm) of the smoothstep ramp inward from the circle edge. 0 = hard clip at the circle. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.0", UIMax = "5000.0", ForceUnits = "cm", EditCondition = "bCircleMask"))
	float CircleFalloffBand = 1000.0f;

	//~ Begin UArcTerrainStampComponent interface
	virtual bool GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const override;
	//~ End UArcTerrainStampComponent interface
};
