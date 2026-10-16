// Copyright Lukasz Baran. All Rights Reserved.

// ArcFoliageStaticCutTests.cpp
// Proves the cut core the static-mesh conversion path relies on (plan Step 6): Arc::FoliageCut::CutMeshToMask
// actually trims a source card mesh to an inset UV-space mask polygon. Builds a raw unit quad FDynamicMesh3
// (like ArcFoliageSolidSkeletonTests.cpp), populates the primary UV overlay explicitly, because CutMeshToMask
// passes triangles with unset/degenerate UVs through UNCHANGED (see ArcFoliageCardCutter.h) -- without real
// UVs this test would "pass" for the wrong reason (no cut happened at all).

#if WITH_EDITOR

#include "CQTest.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "FoliageSkinning/ArcFoliageCardCutter.h"
#include "Polygon2.h"

namespace ArcFoliageStaticCutTests_Private
{
	// Sum of per-triangle areas (0.5*|cross(B-A, C-A)|) over every triangle in Mesh.
	double SumTriangleAreas(const UE::Geometry::FDynamicMesh3& Mesh)
	{
		double TotalArea = 0.0;
		for (const int32 TriangleID : Mesh.TriangleIndicesItr())
		{
			FVector3d VertexA;
			FVector3d VertexB;
			FVector3d VertexC;
			Mesh.GetTriVertices(TriangleID, VertexA, VertexB, VertexC);
			const FVector3d Cross = FVector3d::CrossProduct(VertexB - VertexA, VertexC - VertexA);
			TotalArea += 0.5 * Cross.Length();
		}
		return TotalArea;
	}
}

TEST_CLASS(ArcFoliageStaticCut, "ArcEditorTools.FoliageStaticCut")
{
	TEST_METHOD(CutMeshToMask_InsetSquareOnUnitQuad_TrimsAreaAndPreservesMaterialID)
	{
		using namespace UE::Geometry;
		using namespace ArcFoliageStaticCutTests_Private;

		// Unit quad card in the XY plane, 0..100 cm, two triangles sharing 4 verts.
		FDynamicMesh3 SourceQuad;

		const int32 V00 = SourceQuad.AppendVertex(FVector3d(0.0, 0.0, 0.0));
		const int32 V10 = SourceQuad.AppendVertex(FVector3d(100.0, 0.0, 0.0));
		const int32 V11 = SourceQuad.AppendVertex(FVector3d(100.0, 100.0, 0.0));
		const int32 V01 = SourceQuad.AppendVertex(FVector3d(0.0, 100.0, 0.0));

		const int32 Tri0 = SourceQuad.AppendTriangle(V00, V10, V11);
		const int32 Tri1 = SourceQuad.AppendTriangle(V00, V11, V01);

		SourceQuad.EnableAttributes();
		SourceQuad.Attributes()->EnableMaterialID();
		SourceQuad.Attributes()->GetMaterialID()->SetValue(Tri0, 0);
		SourceQuad.Attributes()->GetMaterialID()->SetValue(Tri1, 0);

		// Populate primary UV overlay 0..1 mapped to the quad corners; MANDATORY per CutMeshToMask's doc --
		// unset UVs are passed through unchanged, which would make the trim assertion meaningless.
		FDynamicMeshUVOverlay* const UVOverlay = SourceQuad.Attributes()->PrimaryUV();
		const int32 UV00 = UVOverlay->AppendElement(FVector2f(0.0f, 0.0f));
		const int32 UV10 = UVOverlay->AppendElement(FVector2f(1.0f, 0.0f));
		const int32 UV11 = UVOverlay->AppendElement(FVector2f(1.0f, 1.0f));
		const int32 UV01 = UVOverlay->AppendElement(FVector2f(0.0f, 1.0f));
		UVOverlay->SetTriangle(Tri0, FIndex3i(UV00, UV10, UV11));
		UVOverlay->SetTriangle(Tri1, FIndex3i(UV00, UV11, UV01));

		// Synthetic inset square in UV space (0.25,0.25) .. (0.75,0.75), used as the traced mask curve.
		FPolygon2f InsetSquare;
		InsetSquare.AppendVertex(FVector2f(0.25f, 0.25f));
		InsetSquare.AppendVertex(FVector2f(0.75f, 0.25f));
		InsetSquare.AppendVertex(FVector2f(0.75f, 0.75f));
		InsetSquare.AppendVertex(FVector2f(0.25f, 0.75f));

		Arc::FoliageCut::FArcCardCutParams Params;
		Params.MaskedSectionIndices = {0};
		Params.bPassThroughUnmaskedSections = false;

		FDynamicMesh3 OutMesh;
		Arc::FoliageCut::FArcCardCutStats Stats;

		const bool bOk = Arc::FoliageCut::CutMeshToMask(SourceQuad, {InsetSquare}, Params, OutMesh, Stats);

		ASSERT_THAT(IsTrue(bOk, TEXT("CutMeshToMask must succeed and emit at least one triangle")));
		ASSERT_THAT(IsTrue(OutMesh.TriangleCount() > 0,
			TEXT("Cut output must contain at least one triangle")));

		const double AreaSource = SumTriangleAreas(SourceQuad);
		const double AreaOut = SumTriangleAreas(OutMesh);

		// Not a triangle-count comparison: constrained-Delaunay retriangulation of the clipped loop can emit
		// MORE triangles than the source quad. Area is the invariant that actually proves a trim happened.
		ASSERT_THAT(IsTrue(AreaOut > 0.0, TEXT("Cut mesh must have positive area")));
		ASSERT_THAT(IsTrue(AreaOut < AreaSource,
			TEXT("Cut mesh trimmed to the inset mask must have strictly less area than the source quad")));

		const FDynamicMeshMaterialAttribute* const OutMaterialID = OutMesh.Attributes()->GetMaterialID();
		ASSERT_THAT(IsTrue(OutMaterialID != nullptr,
			TEXT("Cut output must carry a MaterialID attribute")));
		for (const int32 TriangleID : OutMesh.TriangleIndicesItr())
		{
			ASSERT_THAT(IsTrue(OutMaterialID->GetValue(TriangleID) == 0,
				TEXT("Every cut triangle must preserve the source section's MaterialID (0)")));
		}
	}
};

#endif // WITH_EDITOR
