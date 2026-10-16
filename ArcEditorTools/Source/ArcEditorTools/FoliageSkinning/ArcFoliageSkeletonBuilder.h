// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FReferenceSkeleton;

namespace UE::Geometry
{
	class FDynamicMesh3;
}

/**
 * Clump skeleton builder + skin binder for the foliage card -> skinned mesh converter (plan Step 4).
 *
 * Connected vertex islands of the cut mesh (Step 3 output) are treated as grass clumps (KMeans-merged
 * down to MaxClumps when there are more islands than that). Each clump gets a PCA-derived blade axis
 * (dominant eigenvector, sign-flipped so the low-Z end is the base) and a straight bone chain along it,
 * parented under a shared "root" bone at the origin. Skin weights are then bound per clump with
 * UE::Geometry::SkinBinding::CreateConstrainedSkinWeights so a blade can only ever weight to its own
 * clump's bones — never to the root or a neighbouring clump.
 */
namespace Arc::FoliageSkeleton
{
	struct FArcSkeletonBuildParams
	{
		/** Lower bound on chain length per clump — even short clumps get a bendable chain. */
		int32 MinBonesPerClump = 3;

		/** Upper bound on chain length per clump. */
		int32 MaxBonesPerClump = 5;

		/** Desired spacing between chain bones along the clump axis (world units / cm). */
		float BoneSpacing = 25.0f;

		/** Skeleton-size bound: when the mesh has more vertex islands than this, islands are KMeans-merged into clumps. */
		int32 MaxClumps = 64;

		/** Max skin-weight influences per vertex. */
		int32 MaxInfluences = 4;

		/** Skin bind stiffness in [0,1] (SkinBinding::FBindSettings::Stiffness). */
		float BindStiffness = 0.2f;
	};

	struct FArcSkeletonBuildStats
	{
		/** Clumps after island detection (and KMeans merge, when it ran). */
		int32 ClumpCount = 0;

		/** Total bones in the reference skeleton, root included. */
		int32 BoneCount = 0;
	};

	/**
	 * Build the per-clump skeleton for @p InOutMesh and bind skin weights onto it.
	 *
	 * @param InOutMesh   Cut foliage mesh. Skin weights (profile FSkeletalMeshAttributes::DefaultSkinWeightProfileName)
	 *                    and bone attributes (names/parents/poses mirroring @p OutRefSkeleton) are written onto its
	 *                    attribute set, so downstream CreateSkeletalMeshAsset conversion validates.
	 * @param Params      Clump/bone/bind tuning.
	 * @param OutRefSkeleton  Reset and rebuilt: "root" at origin + one "clump{i}_{j}" chain per clump, parent-local
	 *                    transforms, bone +X aligned to the clump axis.
	 * @param OutStats    Clump/bone counts.
	 * @return true when the mesh had at least one vertex and the skeleton + weights were written.
	 */
	ARCEDITORTOOLS_API bool BuildClumpSkeletonAndBind(
		UE::Geometry::FDynamicMesh3& InOutMesh,
		const FArcSkeletonBuildParams& Params,
		FReferenceSkeleton& OutRefSkeleton,
		FArcSkeletonBuildStats& OutStats);
}
