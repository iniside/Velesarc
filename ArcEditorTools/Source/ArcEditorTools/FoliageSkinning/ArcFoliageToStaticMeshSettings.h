// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "FoliageSkinning/ArcFoliageToSkinnedMeshSettings.h"

#include "ArcFoliageToStaticMeshSettings.generated.h"

class UTexture2D;

/**
 * Options for converting a foliage card UStaticMesh into a Nanite-enabled UStaticMesh
 * whose geometry is cut to the opacity mask (no skeleton, no bones, no wind).
 * Persisted per-project-per-user so the Content Browser dialog remembers the last
 * settings used.
 */
UCLASS(Config = EditorPerProjectUserSettings)
class ARCEDITORTOOLS_API UArcFoliageToStaticMeshSettings : public UObject
{
	GENERATED_BODY()

public:
	// --- Mask ---

	/** When true (default), trace the material's opacity mask and cut the geometry to it (foliage-card path).
	 *  When false, skip mask tracing and cutting entirely and use the full source mesh — for solid meshes that
	 *  have no opacity texture (e.g. a fully modeled plant). All Mask/Contour settings below are ignored in that case. */
	UPROPERTY(EditAnywhere, Config, Category = "Mask", meta = (ToolTip = "Trace and cut to the opacity mask (foliage cards). Turn OFF to convert a solid mesh with no opacity texture."))
	bool bUseOpacityMask = true;

	/** Explicit mask texture to sample. If null, the tool auto-detects one from the material (Translucency/Opacity/Mask/Alpha param names). Not persisted to config (object refs can't be Config); intentionally per-invocation. */
	UPROPERTY(EditAnywhere, Category = "Mask", meta = (ToolTip = "Explicit mask texture override. Leave unset to auto-detect from the material's texture parameters.", EditCondition = "bUseOpacityMask"))
	TObjectPtr<UTexture2D> MaskTextureOverride = nullptr;

	/** Resolution to sample the mask texture at when tracing its contour. */
	UPROPERTY(EditAnywhere, Config, Category = "Mask", meta = (ToolTip = "Square sample resolution used when rasterizing the mask for contour tracing.", EditCondition = "bUseOpacityMask"))
	int32 SampleResolution = 512;

	/** Threshold above which a sampled texel is considered opaque. -1 = use the material's OpacityMaskClipValue. */
	UPROPERTY(EditAnywhere, Config, Category = "Mask", meta = (ToolTip = "Opacity threshold for the mask. -1 uses the source material's OpacityMaskClipValue.", EditCondition = "bUseOpacityMask"))
	float WhiteLevelOverride = -1.0f;

	/** Which channel of the mask texture to sample. Auto picks the highest-variance channel. */
	UPROPERTY(EditAnywhere, Config, Category = "Mask", meta = (ToolTip = "Texture channel to sample from the mask. Auto detects the channel that carries the mask (grayscale masks live in RGB, packed masks in Alpha).", EditCondition = "bUseOpacityMask"))
	EArcMaskChannel Channel = EArcMaskChannel::Auto;

	/** Invert the mask before tracing (treat black as opaque). */
	UPROPERTY(EditAnywhere, Config, Category = "Mask", meta = (ToolTip = "Invert the mask before tracing its contour.", EditCondition = "bUseOpacityMask"))
	bool bInvertMask = false;

	// --- Contour ---

	/** Number of smoothing passes applied to the traced contour curves. */
	UPROPERTY(EditAnywhere, Config, Category = "Contour", meta = (ToolTip = "Smoothing iterations applied to traced contour curves before simplification.", EditCondition = "bUseOpacityMask"))
	int32 SmoothingIterations = 2;

	/** Controls how aggressively traced contours are simplified (fewer triangles, less exact fit). */
	UPROPERTY(EditAnywhere, Config, Category = "Contour", meta = (ToolTip = "Contour simplification amount; higher values reduce triangle count at the cost of mask fidelity.", EditCondition = "bUseOpacityMask"))
	float SimplificationAmount = 1.0f;

	/** Minimum UV-space bounds area (UV^2) for a traced curve to be kept; smaller islands are discarded as noise. */
	UPROPERTY(EditAnywhere, Config, Category = "Contour", meta = (ToolTip = "Minimum UV-space bounds area (UV^2) for a traced curve to survive; smaller curves are discarded.", EditCondition = "bUseOpacityMask"))
	float MinCurveBoundsArea = 0.0004f;

	// --- Output ---

	/** Enable Nanite on the generated static mesh asset. */
	UPROPERTY(EditAnywhere, Config, Category = "Output", meta = (ToolTip = "Enable Nanite settings on the generated static mesh."))
	bool bEnableNanite = true;

	/** Nanite shape-preservation technique for the generated static mesh. Voxelize keeps foliage from thinning out in the distance. */
	UPROPERTY(EditAnywhere, Config, Category = "Output", meta = (EditCondition = "bEnableNanite", ToolTip = "Nanite shape preservation for the generated mesh. Voxelize preserves perceived volume in the distance (recommended for foliage)."))
	ENaniteShapePreservation NaniteShapePreservation = ENaniteShapePreservation::Voxelize;

	/** Copy unmasked material sections through unchanged (e.g. seed heads with no alpha mask). */
	UPROPERTY(EditAnywhere, Config, Category = "Output", meta = (ToolTip = "Pass sections with no resolved mask through unchanged instead of failing them.", EditCondition = "bUseOpacityMask"))
	bool bPassThroughUnmaskedSections = true;

	/** Suffix appended to the source asset name for the generated static mesh asset. */
	UPROPERTY(EditAnywhere, Config, Category = "Output", meta = (ToolTip = "Suffix appended to the source asset name for the generated asset."))
	FString AssetSuffix = "_Nanite";

	/** Save the generated packages to disk immediately after creation. */
	UPROPERTY(EditAnywhere, Config, Category = "Output", meta = (ToolTip = "Save generated packages to disk immediately after creation."))
	bool bSavePackages = true;
};
