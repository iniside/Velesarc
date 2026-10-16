// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Polygon2.h"

#include "FoliageSkinning/ArcFoliageToSkinnedMeshSettings.h"

class UTexture2D;
class UMaterialInterface;

/**
 * Mask -> contour extraction for the foliage card converter.
 *
 * This is a trimmed port of the mask-tracing chain in Epic's experimental
 * ProceduralVegetationEditor (PVImporter_Texture2D.cpp). It samples a texture's
 * opacity channel into a black/white grid, traces the boundary with PCG's
 * Marching Squares, and cleans the resulting closed curves (drop tiny islands,
 * smooth, simplify). The plant-hierarchy / triangulation stages of the original
 * are deliberately NOT ported — Step 3 (card cutter) triangulates per source
 * triangle instead.
 */
namespace Arc::FoliageMask
{
	enum class ETraceResult : uint8
	{
		Success,
		InvalidSourceTexture,
		InvalidSampleSize,
		InvalidTextureSize,
		NoClosedCurves
	};

	/** Inputs controlling how a mask texture is sampled and traced. */
	struct FArcMaskTraceParams
	{
		/** Square resolution the mask is rasterized at before tracing. Clamped to the source texture size. */
		int32 SampleResolution = 512;

		/** Invert the sampled channel (treat dark as opaque) before thresholding. */
		bool bInvert = false;

		/** Threshold above which a sampled texel counts as opaque (typically the material's OpacityMaskClipValue). */
		float WhiteLevel = 0.333f;

		/** Which texture channel supplies the opacity value. */
		EArcMaskChannel Channel = EArcMaskChannel::Alpha;

		/** Minimum UV-space bounding-box area; curves smaller than this are discarded as noise. */
		float MinCurveBoundsArea = 0.0004f;

		/** Number of neighbour-averaging smoothing passes over each traced curve. */
		int32 SmoothingIterations = 2;

		/** Douglas-Peucker-style simplification strength (see mapping in the .cpp). */
		float SimplificationAmount = 1.0f;
	};

	/**
	 * Sample @p Texture's mask channel, trace its opaque region's perimeter, and return the
	 * cleaned closed contour curves in UV space.
	 *
	 * Output contract (V convention): each contour vertex is (col/(W-1), row/(H-1)), i.e. UV
	 * (0,0) = the texture's top-left texel and V grows downward with pixel rows. This matches the
	 * PV source verbatim; downstream (Step 3) accounts for it when stamping into 3D.
	 *
	 * @return Success when at least one closed curve survived; otherwise the failing stage.
	 */
	ETraceResult TracePerimeterCurves(
		UTexture2D* Texture,
		const FArcMaskTraceParams& Params,
		TArray<UE::Geometry::FPolygon2f>& OutCurves);

	/**
	 * Walk @p Material's texture parameters and return the most likely opacity-mask texture, using a
	 * case-insensitive priority substring match on the parameter name:
	 * Translucency > Opacity > Mask > Alpha. Returns nullptr when nothing matches (the caller then
	 * falls back to an explicit override).
	 */
	UTexture2D* ResolveMaskTexture(const UMaterialInterface* Material);

	/** The material's OpacityMaskClipValue — the natural white-level threshold for a masked material. */
	float ResolveWhiteLevel(const UMaterialInterface* Material);
}
