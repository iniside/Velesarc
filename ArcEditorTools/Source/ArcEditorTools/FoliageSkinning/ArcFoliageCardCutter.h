// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Polygon2.h"

namespace UE::Geometry
{
	class FDynamicMesh3;
}

/**
 * Card cutter for the foliage card -> skinned mesh converter (plan Step 3).
 *
 * Stamps the traced opacity-mask contours (Step 2 output, UV space) onto the source
 * card mesh, per source triangle: each masked-section triangle's UV footprint clips
 * the traced curves (Sutherland-Hodgman), the clipped loops are triangulated with a
 * single constrained Delaunay + NonZeroWinding fill, and the resulting 2D triangles
 * are mapped back to 3D through the source triangle's exact UV->position affine map.
 * The result is a mesh that covers exactly the opaque part of the mask while reusing
 * the original UVs (and therefore the original textures) verbatim.
 */
namespace Arc::FoliageCut
{
	struct FArcCardCutParams
	{
		/** Material/section indices (source-mesh MaterialID values) whose triangles are cut against the traced mask. */
		TSet<int32> MaskedSectionIndices;

		/** Copy triangles of non-masked sections into the output unchanged (e.g. seed heads); when false they are dropped. */
		bool bPassThroughUnmaskedSections = true;

		/**
		 * Post-stamp seam weld tolerance (world units / cm). Adjacent source triangles clip their shared UV edge to
		 * mathematically identical points, but the affine maps and clip corner derivations differ per triangle, so
		 * seam vertices agree only to floating-point noise; this tolerance absorbs it.
		 */
		double WeldTolerance = 1.e-4;
	};

	struct FArcCardCutStats
	{
		/** Masked-section triangles that went through the clip+stamp path. */
		int32 StampedSourceTris = 0;

		/** Triangles copied into the output unchanged (unmasked sections). */
		int32 PassedThroughTris = 0;

		/** Triangles with degenerate/unset UVs — passed through unchanged. */
		int32 SkippedDegenerate = 0;

		/** Triangles whose UVs span a UV-tile boundary — passed through unchanged (with a log warning). */
		int32 SkippedTileSpanning = 0;

		/** Total triangles emitted into the output mesh (before welding). */
		int32 EmittedTris = 0;
	};

	/**
	 * Cut @p SourceMesh against the traced mask contours.
	 *
	 * @param SourceMesh    Source card mesh; must have an attribute set with UV overlay 0 populated. MaterialID
	 *                      attribute carries section indices (missing attribute = every triangle is section 0).
	 * @param TracedCurves  Closed contour curves in UV space from Arc::FoliageMask::TracePerimeterCurves. Windings
	 *                      are re-normalized internally by containment parity (outers CCW, holes CW) — the marching
	 *                      squares convention is not trusted.
	 * @param Params        Which sections are masked + pass-through/weld behavior.
	 * @param OutMesh       Cleared and filled with the cut mesh: positions, UV overlay 0 (original-texture UVs),
	 *                      normals overlay (source triangle normals), per-triangle MaterialID = source section index,
	 *                      seams welded with FMergeCoincidentMeshEdges.
	 * @param OutStats      Per-category triangle counts.
	 * @return true when the output mesh contains at least one triangle.
	 */
	ARCEDITORTOOLS_API bool CutMeshToMask(
		const UE::Geometry::FDynamicMesh3& SourceMesh,
		const TArray<UE::Geometry::FPolygon2f>& TracedCurves,
		const FArcCardCutParams& Params,
		UE::Geometry::FDynamicMesh3& OutMesh,
		FArcCardCutStats& OutStats);
}
