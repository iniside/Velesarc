// Copyright Lukasz Baran. All Rights Reserved.

// ArcPCGTerrainDataTests.cpp
// TIER 1 pure-core CQTest coverage for UPCGTerrainData::ProjectPoint (bilinear O(1) sampling over an
// FArcTerrainSurfaceCache).
//
// All fixtures drive an exact TILTED PLANE Z(localX,localY) = A*localX + B*localY + C across the
// FULL (RenderQuads+3)^2 tile, apron included. A plane is affine, so:
//   * the finite-difference normal is EXACT everywhere (interior, edge, and apron one-sided taps —
//     same constant slope), pinning the apron-unpack + normal math without needing a curved surface;
//   * bilinear sampling of an affine field is exact at ANY fractional coordinate, not only at grid
//     vertices, so height/position assertions can use non-vertex-aligned query points too.
// Every test drives the PRODUCTION ArcTerrainSurface::BuildCellCache / UPCGTerrainData::ProjectPoint —
// no hand-rolled parallel sampling path.
//
// No layer-weight coverage: CPU layer-weight sampling was removed from this seam entirely (weights are
// sampled GPU-side), so there is no weight code left here to pin.

#include "CQTest.h"

#include "Surface/ArcTerrainSurfaceSample.h"
#include "Terrain/PCGTerrainData.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcPCGTerrainDataTest, Log, All);

namespace ArcPCGTerrainDataTestsLocal
{
	// Fills a (RenderQuads+3)^2 row-major apron tile with an exact tilted plane
	// Z(localX,localY) = A*localX + B*localY + C. Texel (Tx,Ty) -> localX=(Tx-1)*SpacingCm,
	// localY=(Ty-1)*SpacingCm (apron ring at Tx/Ty in {0, RenderQuads+2}), matching the interior
	// vertex convention (c,r) -> texel (c+1,r+1) documented on FArcTerrainSurfaceCellCache.
	TArray<float> MakeTiltedPlaneHeights(int32 RenderQuads, float SpacingCm, float A, float B, float C)
	{
		const int32 TileRes = RenderQuads + 3;
		TArray<float> Heights;
		Heights.SetNumUninitialized(TileRes * TileRes);
		for (int32 Ty = 0; Ty < TileRes; ++Ty)
		{
			const float LocalY = static_cast<float>(Ty - 1) * SpacingCm;
			for (int32 Tx = 0; Tx < TileRes; ++Tx)
			{
				const float LocalX = static_cast<float>(Tx - 1) * SpacingCm;
				Heights[Ty * TileRes + Tx] = A * LocalX + B * LocalY + C;
			}
		}
		return Heights;
	}

	// Builds a single-cell (Key (0,0)) FArcTerrainSurfaceCache wrapping a tilted-plane BuildCellCache
	// output. CellBounds is the max-EXCLUSIVE [0,1)x[0,1) rect the production ProjectPoint code
	// expects (see PCGTerrainData.cpp GetLocalExtentCm).
	TSharedPtr<FArcTerrainSurfaceCache> MakeSingleCellCache(
		int32 RenderQuads,
		double CellSize,
		float A,
		float B,
		float C,
		const FTransform& TerrainTransform)
	{
		const float SpacingCm = static_cast<float>(CellSize / static_cast<double>(RenderQuads));
		const TArray<float> Heights = MakeTiltedPlaneHeights(RenderQuads, SpacingCm, A, B, C);

		TSharedPtr<FArcTerrainSurfaceCache> Cache = MakeShared<FArcTerrainSurfaceCache>();
		Cache->CellSize = CellSize;
		Cache->QuadsPerCell = RenderQuads;
		Cache->CellBounds = FIntRect(FIntPoint(0, 0), FIntPoint(1, 1));
		Cache->TerrainTransform = TerrainTransform;

		FArcTerrainSurfaceCellCache Cell = ArcTerrainSurface::BuildCellCache(
			FIntPoint(0, 0), Heights, RenderQuads, CellSize);
		Cache->Cells.Add(MoveTemp(Cell));
		Cache->KeyToIndex.Add(FIntPoint(0, 0), 0);

		return Cache;
	}
}

TEST_CLASS(ArcPCGTerrainDataProjectPoint, "ArcPCGExtension.Terrain.ProjectPoint")
{
	// Identity TerrainTransform: bilinear-sampled height at both a grid vertex and an arbitrary
	// interior point must equal the exact plane value (bilinear of an affine field is exact anywhere).
	TEST_METHOD(ProjectPoint_IdentityTransform_BilinearHeightMatchesPlane)
	{
		using namespace ArcPCGTerrainDataTestsLocal;

		const int32 RenderQuads = 4;
		const double CellSize = 400.0; // spacing = 100 cm
		const float A = 0.3f;
		const float B = -0.2f;
		const float C = 50.0f;

		const TSharedPtr<FArcTerrainSurfaceCache> Cache = MakeSingleCellCache(RenderQuads, CellSize, A, B, C, FTransform::Identity);

		UPCGTerrainData* Data = NewObject<UPCGTerrainData>();
		FPCGTerrainDataProps Props;
		Data->Initialize(Cache, Props);

		// Grid vertex (c=1,r=3) -> localX=100, localY=300.
		{
			const float LocalX = 100.0f;
			const float LocalY = 300.0f;
			const float ExpectedHeight = A * LocalX + B * LocalY + C;

			FPCGPoint OutPoint;
			const FTransform Query(FVector(LocalX, LocalY, 10000.0));
			const bool bOk = Data->ProjectPoint(Query, FBox(ForceInit), FPCGProjectionParams(), OutPoint, nullptr);

			ASSERT_THAT(IsTrue(bOk, TEXT("Interior query must project successfully")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(OutPoint.Transform.GetLocation().Z, static_cast<double>(ExpectedHeight), 1e-2),
				TEXT("Bilinear height at a grid vertex must equal the exact plane value")));
		}

		// Arbitrary non-vertex-aligned interior point.
		{
			const float LocalX = 37.5f;
			const float LocalY = 222.3f;
			const float ExpectedHeight = A * LocalX + B * LocalY + C;

			FPCGPoint OutPoint;
			const FTransform Query(FVector(LocalX, LocalY, 10000.0));
			const bool bOk = Data->ProjectPoint(Query, FBox(ForceInit), FPCGProjectionParams(), OutPoint, nullptr);

			ASSERT_THAT(IsTrue(bOk, TEXT("Interior query must project successfully")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(OutPoint.Transform.GetLocation().Z, static_cast<double>(ExpectedHeight), 1e-2),
				TEXT("Bilinear height at a non-vertex point must equal the exact plane value (affine field)")));
		}
	}

	// Non-identity TerrainTransform (rotation + non-uniform scale + translation): the projected
	// WORLD position must be the local plane point pushed through TransformPosition, and the WORLD
	// normal (read off the point's Z rotation axis) must be the local plane normal pushed through
	// TransformVectorNoScale — pinning that ProjectPoint applies the cache's TerrainTransform, not an
	// implicit identity.
	TEST_METHOD(ProjectPoint_NonIdentityTerrainTransform_PushesPositionAndNormalThroughTransform)
	{
		using namespace ArcPCGTerrainDataTestsLocal;

		const int32 RenderQuads = 4;
		const double CellSize = 400.0;
		const float A = 0.3f;
		const float B = -0.2f;
		const float C = 50.0f;

		const FTransform TerrainTransform(
			FRotator(0.0, 30.0, 0.0).Quaternion(),
			FVector(500.0, -300.0, 1000.0),
			FVector(2.0, 1.5, 1.0));

		const TSharedPtr<FArcTerrainSurfaceCache> Cache = MakeSingleCellCache(RenderQuads, CellSize, A, B, C, TerrainTransform);

		UPCGTerrainData* Data = NewObject<UPCGTerrainData>();
		FPCGTerrainDataProps Props;
		Data->Initialize(Cache, Props);

		const float LocalX = 150.0f;
		const float LocalY = 250.0f;
		const float PlaneHeight = A * LocalX + B * LocalY + C;
		const FVector LocalPlanePoint(LocalX, LocalY, static_cast<double>(PlaneHeight));
		const FVector3f LocalNormal3f = FVector3f(-A, -B, 1.0f).GetSafeNormal();

		const FVector ExpectedWorldPos = TerrainTransform.TransformPosition(LocalPlanePoint);
		const FVector ExpectedWorldNormal = TerrainTransform.TransformVectorNoScale(FVector(LocalNormal3f)).GetSafeNormal();

		// Query world location: any world point whose inverse-transform XY matches (LocalX,LocalY);
		// Local.Z is never read by ProjectPoint, so an arbitrary query Z is fine.
		const FVector WorldQueryPos = TerrainTransform.TransformPosition(FVector(LocalX, LocalY, 10000.0));

		FPCGPoint OutPoint;
		const FTransform Query(WorldQueryPos);
		const bool bOk = Data->ProjectPoint(Query, FBox(ForceInit), FPCGProjectionParams(), OutPoint, nullptr);

		ASSERT_THAT(IsTrue(bOk, TEXT("Query must project successfully")));
		ASSERT_THAT(IsTrue(OutPoint.Transform.GetLocation().Equals(ExpectedWorldPos, 1e-2),
			TEXT("World position must be the local plane point pushed through TerrainTransform.TransformPosition")));

		const FVector ActualWorldNormal = OutPoint.Transform.GetUnitAxis(EAxis::Z);
		ASSERT_THAT(IsTrue(ActualWorldNormal.Equals(ExpectedWorldNormal, 1e-3),
			TEXT("World normal (point's Z rotation axis) must be the local plane normal pushed through TerrainTransform.TransformVectorNoScale")));
	}

	// Boundary/clamp behaviour (CellBounds is max-EXCLUSIVE): a point clearly outside the single
	// cell's cell-space rect must be rejected; a point exactly on the far max edge must still be
	// owned by the last cell (the MaxCellX/Y - UE_KINDA_SMALL_NUMBER clamp), not lost.
	TEST_METHOD(ProjectPoint_Boundary_OutsideRejectedFarEdgeOwnedByLastCell)
	{
		using namespace ArcPCGTerrainDataTestsLocal;

		const int32 RenderQuads = 4;
		const double CellSize = 400.0;
		const float A = 0.3f;
		const float B = -0.2f;
		const float C = 50.0f;

		const TSharedPtr<FArcTerrainSurfaceCache> Cache = MakeSingleCellCache(RenderQuads, CellSize, A, B, C, FTransform::Identity);

		UPCGTerrainData* Data = NewObject<UPCGTerrainData>();
		FPCGTerrainDataProps Props;
		Data->Initialize(Cache, Props);

		// Clearly outside: local XY in a non-existent cell (CellFX = 2.0 > MaxCellX = 1.0).
		{
			FPCGPoint OutPoint;
			const FTransform Query(FVector(2.0 * CellSize, 0.0, 10000.0));
			const bool bOk = Data->ProjectPoint(Query, FBox(ForceInit), FPCGProjectionParams(), OutPoint, nullptr);
			ASSERT_THAT(IsTrue(!bOk, TEXT("Point outside CellBounds must be rejected")));
		}

		// Exactly on the far max edge of the single cell: owned by the last cell, not lost.
		{
			const float ExpectedHeight = A * static_cast<float>(CellSize) + B * static_cast<float>(CellSize) + C;

			FPCGPoint OutPoint;
			const FTransform Query(FVector(CellSize, CellSize, 10000.0));
			const bool bOk = Data->ProjectPoint(Query, FBox(ForceInit), FPCGProjectionParams(), OutPoint, nullptr);
			ASSERT_THAT(IsTrue(bOk, TEXT("Point exactly on the far max edge must be accepted (owned by the last cell)")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(OutPoint.Transform.GetLocation().Z, static_cast<double>(ExpectedHeight), 1e-1),
				TEXT("Far-edge height must equal the plane's edge value (within the clamp epsilon)")));
		}
	}
};
