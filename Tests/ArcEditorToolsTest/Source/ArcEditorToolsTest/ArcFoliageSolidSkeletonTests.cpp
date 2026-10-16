// Copyright Lukasz Baran. All Rights Reserved.

// ArcFoliageSolidSkeletonTests.cpp
// Proves the no-opacity path's core claim: Arc::FoliageSkeleton::BuildClumpSkeletonAndBind produces a
// valid bone chain AND writes skin weights on a FULL, un-cut solid mesh (not just a mask-cut card). The
// test box is built with 8 SHARED vertices (2 triangles per face, all faces indexing the same 8 verts):
// the builder clumps by VERTEX connectivity (FMeshConnectedComponents::FindConnectedVertices in
// ArcFoliageSkeletonBuilder.cpp), so a naively per-face-duplicated 24-vert box would be 6 disjoint
// islands and fail the single-clump assertion below.

#if WITH_EDITOR

#include "CQTest.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "FoliageSkinning/ArcFoliageSkeletonBuilder.h"
#include "ReferenceSkeleton.h"
#include "SkeletalMeshAttributes.h"

TEST_CLASS(ArcFoliageSolidSkeleton, "ArcEditorTools.FoliageSolidSkeleton")
{
	TEST_METHOD(BuildClumpSkeletonAndBind_SolidWeldedBox_OneClumpFullChainAndSkinWeights)
	{
		using namespace UE::Geometry;
		using namespace Arc::FoliageSkeleton;

		// Tall box, ~10x10x100 cm, Z from 0 to 100. 8 SHARED vertices; 12 triangles (2 per face, 6 faces)
		// all indexing into those same 8 verts, so the whole box welds into a single connected island.
		FDynamicMesh3 Box;

		const int32 V000 = Box.AppendVertex(FVector3d(0.0, 0.0, 0.0));
		const int32 V100 = Box.AppendVertex(FVector3d(10.0, 0.0, 0.0));
		const int32 V110 = Box.AppendVertex(FVector3d(10.0, 10.0, 0.0));
		const int32 V010 = Box.AppendVertex(FVector3d(0.0, 10.0, 0.0));
		const int32 V001 = Box.AppendVertex(FVector3d(0.0, 0.0, 100.0));
		const int32 V101 = Box.AppendVertex(FVector3d(10.0, 0.0, 100.0));
		const int32 V111 = Box.AppendVertex(FVector3d(10.0, 10.0, 100.0));
		const int32 V011 = Box.AppendVertex(FVector3d(0.0, 10.0, 100.0));

		// Bottom (Z=0)
		Box.AppendTriangle(V000, V110, V100);
		Box.AppendTriangle(V000, V010, V110);
		// Top (Z=100)
		Box.AppendTriangle(V001, V101, V111);
		Box.AppendTriangle(V001, V111, V011);
		// Front (Y=0)
		Box.AppendTriangle(V000, V100, V101);
		Box.AppendTriangle(V000, V101, V001);
		// Back (Y=10)
		Box.AppendTriangle(V010, V011, V111);
		Box.AppendTriangle(V010, V111, V110);
		// Left (X=0)
		Box.AppendTriangle(V000, V001, V011);
		Box.AppendTriangle(V000, V011, V010);
		// Right (X=10)
		Box.AppendTriangle(V100, V110, V111);
		Box.AppendTriangle(V100, V111, V101);

		Box.EnableAttributes();

		FArcSkeletonBuildParams Params;
		FReferenceSkeleton RefSkeleton;
		FArcSkeletonBuildStats Stats;

		const bool bOk = BuildClumpSkeletonAndBind(Box, Params, RefSkeleton, Stats);

		ASSERT_THAT(IsTrue(bOk, TEXT("BuildClumpSkeletonAndBind must succeed on a solid welded box")));
		ASSERT_THAT(IsTrue(Stats.ClumpCount == 1,
			TEXT("The 8-shared-vertex box is one connected vertex island and must produce exactly one clump")));
		ASSERT_THAT(IsTrue(Stats.BoneCount >= 1 + Params.MinBonesPerClump,
			TEXT("BoneCount must cover root + a full per-clump chain (>= MinBonesPerClump), not root-only")));
		ASSERT_THAT(IsTrue(RefSkeleton.GetRawBoneNum() == Stats.BoneCount,
			TEXT("FReferenceSkeleton bone count must match OutStats.BoneCount exactly")));

		// The bind must have actually written skin weights onto the mesh's attribute set, not just built a
		// skeleton in isolation. This is the SAME profile-name accessor the builder itself uses: it calls
		// SkinBinding::CreateConstrainedSkinWeights(InOutMesh, ..., FSkeletalMeshAttributes::DefaultSkinWeightProfileName,
		// ...) (ArcFoliageSkeletonBuilder.cpp), and FDynamicMeshAttributeSet exposes the written result via
		// HasSkinWeightsAttribute(FName)/GetSkinWeightsAttribute(FName) (DynamicMeshAttributeSet.h).
		ASSERT_THAT(IsTrue(Box.HasAttributes(),
			TEXT("Attribute set must exist after bind (EnableAttributes was called before build)")));
		const bool bHasSkinWeights = Box.Attributes()->HasSkinWeightsAttribute(FSkeletalMeshAttributes::DefaultSkinWeightProfileName);
		ASSERT_THAT(IsTrue(bHasSkinWeights,
			TEXT("Bind must write the default-profile skin-weight attribute (FSkeletalMeshAttributes::DefaultSkinWeightProfileName) onto the mesh")));
	}
};

#endif // WITH_EDITOR
