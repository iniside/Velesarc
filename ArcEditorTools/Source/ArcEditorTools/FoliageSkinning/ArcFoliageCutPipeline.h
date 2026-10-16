// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "FoliageSkinning/ArcFoliageMaskTracer.h"

class UStaticMesh;
class UTexture2D;
class UMaterialInterface;

namespace UE::Geometry { class FDynamicMesh3; }

/**
 * Shared source -> cut half of the foliage card converters (plan Step 1).
 *
 * Takes a foliage-card UStaticMesh, resolves the opacity mask per material slot, traces the opacity
 * contour once per unique mask texture (Arc::FoliageMask) and cuts the card geometry to it
 * (Arc::FoliageCut::CutMeshToMask), producing a cut FDynamicMesh3 with MaterialID / UVs preserved.
 * The output is deliberately output-agnostic so both the skinned and the static-mesh converters can
 * build their asset on top of the same cut path.
 *
 * Editor-only. Reports NOTHING to the message log — each caller maps the ECutPipelineResult to its
 * own FText and logs / toasts as appropriate.
 */
namespace Arc::FoliageCut
{
	/** Inputs for the source -> cut pipeline, decoupled from any particular converter's settings object. */
	struct FArcCutPipelineParams
	{
		/** When false, the full source geometry is passed through unchanged (no mask trace / cut). */
		bool bUseOpacityMask = true;

		/** Explicit mask texture forced on every slot; null = auto-detect the mask per material. */
		UTexture2D* MaskTextureOverride = nullptr;

		/** White-level threshold override; < 0 = per-material OpacityMaskClipValue. */
		float WhiteLevelOverride = -1.0f;

		/** Trace inputs shared by every slot. The WhiteLevel field is ignored (resolved per material). */
		Arc::FoliageMask::FArcMaskTraceParams TraceTemplate;

		/** Copy the never-masked sections through into the output unchanged. */
		bool bPassThroughUnmaskedSections = true;
	};

	/** Outcome of BuildCutMeshFromStaticMesh; the caller maps each value to its own message. */
	enum class ECutPipelineResult : uint8
	{
		Ok,
		NoMeshDescription,
		NoMaskResolved,
		ZeroClosedCurves,
		ZeroGeometry
	};

	/**
	 * Build the cut FDynamicMesh3 for @p StaticMesh: FMeshDescription -> FDynamicMesh3, per-slot mask
	 * resolve + trace, card cut composition (single- vs multi-texture), and the no-mask full-mesh path.
	 *
	 * @param StaticMesh        Source foliage card (LOD0 mesh description required).
	 * @param Params            Mask / cut inputs.
	 * @param OutCutMesh        Receives the cut geometry (attributes + MaterialID enabled) on Ok.
	 * @param OutAssetMaterials Receives the source material slots (in order) for asset creation.
	 * @return Ok on success; otherwise the failing stage.
	 */
	ECutPipelineResult BuildCutMeshFromStaticMesh(
		const UStaticMesh& StaticMesh,
		const FArcCutPipelineParams& Params,
		UE::Geometry::FDynamicMesh3& OutCutMesh,
		TArray<UMaterialInterface*>& OutAssetMaterials);
}
