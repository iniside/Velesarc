// Copyright Lukasz Baran. All Rights Reserved.

#include "FoliageSkinning/ArcFoliageCardCutter.h"

#include "BoxTypes.h"
#include "CompGeom/Delaunay2.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"
#include "IndexTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcFoliageCardCutter, Log, All);

namespace ArcFoliageCardCutterCpp
{
	using UE::Geometry::FAxisAlignedBox2d;
	using UE::Geometry::FAxisAlignedBox2f;
	using UE::Geometry::FDelaunay2;
	using UE::Geometry::FDynamicMesh3;
	using UE::Geometry::FDynamicMeshAttributeSet;
	using UE::Geometry::FDynamicMeshMaterialAttribute;
	using UE::Geometry::FDynamicMeshNormalOverlay;
	using UE::Geometry::FDynamicMeshUVOverlay;
	using UE::Geometry::FIndex2i;
	using UE::Geometry::FIndex3i;
	using UE::Geometry::FMergeCoincidentMeshEdges;
	using UE::Geometry::FPolygon2f;

	/** 2D UV area below which a source triangle's UV footprint is considered degenerate. */
	constexpr double DegenerateUVAreaTolerance = 1.e-12;

	/** 2D area below which a clipped loop is discarded as a numerical sliver. */
	constexpr double MinClippedLoopArea = 1.e-14;

	/** Squared distance below which consecutive clipped-loop points are collapsed. */
	constexpr double DuplicatePointToleranceSq = 1.e-24;

	/** Slack allowed when deciding whether a triangle's UVs fit inside a single UV tile. */
	constexpr double TileFitTolerance = 1.e-4;

	double Cross2(const FVector2d& A, const FVector2d& B)
	{
		return A.X * B.Y - A.Y * B.X;
	}

	double LoopSignedArea(const TArray<FVector2d>& Loop)
	{
		double Sum = 0.0;
		const int32 NumVerts = Loop.Num();
		for (int32 Index = 0; Index < NumVerts; ++Index)
		{
			Sum += Cross2(Loop[Index], Loop[(Index + 1) % NumVerts]);
		}
		return Sum * 0.5;
	}

	/** A traced curve with normalized winding (outers CCW / holes CW), promoted to doubles for clipping. */
	struct FNormalizedCurve
	{
		TArray<FVector2d> Vertices;
		FAxisAlignedBox2d Bounds;
	};

	/**
	 * Re-orient the traced curves by containment parity: a curve whose representative vertex lies inside an odd
	 * number of OTHER curves is a hole (oriented CW), otherwise an outer (oriented CCW). The marching-squares
	 * winding convention is deliberately not trusted.
	 */
	TArray<FNormalizedCurve> NormalizeCurveWindings(const TArray<FPolygon2f>& TracedCurves)
	{
		const int32 NumCurves = TracedCurves.Num();

		TArray<FAxisAlignedBox2f> CurveBounds;
		CurveBounds.Reserve(NumCurves);
		for (const FPolygon2f& Curve : TracedCurves)
		{
			CurveBounds.Add(Curve.Bounds());
		}

		TArray<FNormalizedCurve> Result;
		Result.Reserve(NumCurves);
		for (int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
		{
			const FPolygon2f& Curve = TracedCurves[CurveIndex];
			const int32 NumVerts = Curve.VertexCount();
			if (NumVerts < 3)
			{
				continue;
			}

			const FVector2f Representative = Curve[0];
			int32 ContainmentCount = 0;
			for (int32 OtherIndex = 0; OtherIndex < NumCurves; ++OtherIndex)
			{
				if (OtherIndex == CurveIndex || TracedCurves[OtherIndex].VertexCount() < 3)
				{
					continue;
				}
				if (!CurveBounds[OtherIndex].Contains(Representative))
				{
					continue;
				}
				if (TracedCurves[OtherIndex].Contains(Representative))
				{
					++ContainmentCount;
				}
			}

			const bool bIsHole = (ContainmentCount % 2) == 1;
			const bool bWantCCW = !bIsHole;
			const bool bIsCCW = Curve.SignedArea() > 0.0f;
			const bool bReverse = (bIsCCW != bWantCCW);

			FNormalizedCurve& Normalized = Result.AddDefaulted_GetRef();
			Normalized.Vertices.Reserve(NumVerts);
			for (int32 VertIndex = 0; VertIndex < NumVerts; ++VertIndex)
			{
				const FVector2f Vertex = Curve[bReverse ? (NumVerts - 1 - VertIndex) : VertIndex];
				Normalized.Vertices.Add(FVector2d(Vertex.X, Vertex.Y));
			}
			Normalized.Bounds = FAxisAlignedBox2d(Normalized.Vertices);
		}
		return Result;
	}

	/**
	 * One clip half-plane of a triangle's UV footprint. The line is derived from the LEXICOGRAPHICALLY SORTED
	 * endpoint pair so two triangles sharing the edge derive bit-identical line coefficients — their clip points
	 * along the shared edge then agree to floating-point noise regardless of vertex order, which is what lets
	 * FMergeCoincidentMeshEdges weld the seam afterwards.
	 */
	struct FClipEdge
	{
		FVector2d LinePoint = FVector2d::ZeroVector;
		FVector2d LineNormal = FVector2d::ZeroVector; // Not normalized; scale cancels in the intersection parameter.
		double InsideSign = 1.0;                      // Side of the line the triangle interior lies on.
	};

	FClipEdge MakeClipEdge(const FVector2d& EdgeA, const FVector2d& EdgeB, const FVector2d& OppositeVertex)
	{
		const bool bSwap = (EdgeB.X < EdgeA.X) || (EdgeB.X == EdgeA.X && EdgeB.Y < EdgeA.Y);
		const FVector2d SortedStart = bSwap ? EdgeB : EdgeA;
		const FVector2d SortedEnd = bSwap ? EdgeA : EdgeB;
		const FVector2d Direction = SortedEnd - SortedStart;

		FClipEdge Edge;
		Edge.LinePoint = SortedStart;
		Edge.LineNormal = FVector2d(-Direction.Y, Direction.X);
		const FVector2d ToOpposite = OppositeVertex - SortedStart;
		const double OppositeSide = Edge.LineNormal.X * ToOpposite.X + Edge.LineNormal.Y * ToOpposite.Y;
		Edge.InsideSign = (OppositeSide >= 0.0) ? 1.0 : -1.0;
		return Edge;
	}

	/** Raw (sign-convention-free) line offset of a point; identical for both triangles sharing the edge. */
	double LineOffset(const FClipEdge& Edge, const FVector2d& Point)
	{
		return Edge.LineNormal.X * (Point.X - Edge.LinePoint.X) + Edge.LineNormal.Y * (Point.Y - Edge.LinePoint.Y);
	}

	/** One Sutherland-Hodgman pass: clip a closed loop against a single half-plane. Winding is preserved. */
	void ClipLoopAgainstEdge(const TArray<FVector2d>& InLoop, const FClipEdge& Edge, TArray<FVector2d>& OutLoop)
	{
		OutLoop.Reset();
		const int32 NumVerts = InLoop.Num();
		for (int32 Index = 0; Index < NumVerts; ++Index)
		{
			const FVector2d& Current = InLoop[Index];
			const FVector2d& Next = InLoop[(Index + 1) % NumVerts];
			const double OffsetCurrent = LineOffset(Edge, Current);
			const double OffsetNext = LineOffset(Edge, Next);
			const bool bCurrentInside = (OffsetCurrent * Edge.InsideSign) >= 0.0;
			const bool bNextInside = (OffsetNext * Edge.InsideSign) >= 0.0;

			if (bCurrentInside)
			{
				OutLoop.Add(Current);
			}
			if (bCurrentInside != bNextInside)
			{
				// The interpolation parameter uses the raw offsets, so both triangles sharing this
				// edge compute the identical crossing point for the identical curve segment.
				const double T = OffsetCurrent / (OffsetCurrent - OffsetNext);
				OutLoop.Add(Current + (Next - Current) * T);
			}
		}
	}

	/** Collapse consecutive duplicate points (including last-vs-first) produced by on-line clip vertices. */
	void RemoveConsecutiveDuplicates(TArray<FVector2d>& Loop)
	{
		if (Loop.Num() < 2)
		{
			return;
		}
		TArray<FVector2d> Cleaned;
		Cleaned.Reserve(Loop.Num());
		for (const FVector2d& Point : Loop)
		{
			if (Cleaned.Num() == 0 || FVector2d::DistSquared(Cleaned.Last(), Point) > DuplicatePointToleranceSq)
			{
				Cleaned.Add(Point);
			}
		}
		while (Cleaned.Num() >= 2 && FVector2d::DistSquared(Cleaned.Last(), Cleaned[0]) <= DuplicatePointToleranceSq)
		{
			Cleaned.Pop();
		}
		Loop = MoveTemp(Cleaned);
	}

	/** Exact UV -> 3D position affine map built from a source triangle's three (UV, Position) pairs. */
	struct FTriangleUVFrame
	{
		FVector3d Origin = FVector3d::ZeroVector; // Position at UV0.
		FVector2d UV0 = FVector2d::ZeroVector;    // Original (untranslated) UV of vertex 0.
		FVector3d DPdU = FVector3d::ZeroVector;
		FVector3d DPdV = FVector3d::ZeroVector;
		double UVSignedArea2 = 0.0;               // 2 * signed UV area; sign < 0 = mirrored UVs.

		FVector3d MapUVToPosition(const FVector2d& UV) const
		{
			return Origin + DPdU * (UV.X - UV0.X) + DPdV * (UV.Y - UV0.Y);
		}
	};

	bool BuildTriangleUVFrame(const FVector3d (&Positions)[3], const FVector2d (&UVs)[3], FTriangleUVFrame& OutFrame)
	{
		const FVector2d DeltaUV1 = UVs[1] - UVs[0];
		const FVector2d DeltaUV2 = UVs[2] - UVs[0];
		const double Det = Cross2(DeltaUV1, DeltaUV2);
		if (FMath::Abs(Det) < DegenerateUVAreaTolerance)
		{
			return false;
		}

		const FVector3d DeltaP1 = Positions[1] - Positions[0];
		const FVector3d DeltaP2 = Positions[2] - Positions[0];
		const double InvDet = 1.0 / Det;

		OutFrame.Origin = Positions[0];
		OutFrame.UV0 = UVs[0];
		// [DeltaP1 DeltaP2] = [DPdU DPdV] * [[DeltaUV1.X, DeltaUV2.X], [DeltaUV1.Y, DeltaUV2.Y]] — 2x2 solve.
		OutFrame.DPdU = (DeltaP1 * DeltaUV2.Y - DeltaP2 * DeltaUV1.Y) * InvDet;
		OutFrame.DPdV = (DeltaP2 * DeltaUV1.X - DeltaP1 * DeltaUV2.X) * InvDet;
		OutFrame.UVSignedArea2 = Det;
		return true;
	}

	/** Shared handles into the output mesh's attribute layers. */
	struct FOutputMeshLayers
	{
		FDynamicMesh3* Mesh = nullptr;
		FDynamicMeshUVOverlay* UV = nullptr;
		FDynamicMeshNormalOverlay* Normals = nullptr;
		FDynamicMeshMaterialAttribute* MaterialIDs = nullptr;
	};

	/** Copy one source triangle into the output verbatim (positions, UVs, normals, MaterialID). */
	void CopyTriangleVerbatim(
		const FDynamicMesh3& SourceMesh,
		const int32 SourceTid,
		const int32 MaterialID,
		FOutputMeshLayers& Output)
	{
		const FIndex3i TriVerts = SourceMesh.GetTriangle(SourceTid);
		const int32 NewVids[3] = {
			Output.Mesh->AppendVertex(SourceMesh.GetVertex(TriVerts.A)),
			Output.Mesh->AppendVertex(SourceMesh.GetVertex(TriVerts.B)),
			Output.Mesh->AppendVertex(SourceMesh.GetVertex(TriVerts.C))};
		const int32 NewTid = Output.Mesh->AppendTriangle(FIndex3i(NewVids[0], NewVids[1], NewVids[2]));
		if (NewTid < 0)
		{
			return;
		}

		const FDynamicMeshAttributeSet* SourceAttributes = SourceMesh.Attributes();
		const FDynamicMeshUVOverlay* SourceUV = SourceAttributes ? SourceAttributes->PrimaryUV() : nullptr;
		if (SourceUV != nullptr && SourceUV->IsSetTriangle(SourceTid))
		{
			FVector2f UVA, UVB, UVC;
			SourceUV->GetTriElements(SourceTid, UVA, UVB, UVC);
			Output.UV->SetTriangle(NewTid, FIndex3i(
				Output.UV->AppendElement(UVA),
				Output.UV->AppendElement(UVB),
				Output.UV->AppendElement(UVC)));
		}

		const FDynamicMeshNormalOverlay* SourceNormals = SourceAttributes ? SourceAttributes->PrimaryNormals() : nullptr;
		if (SourceNormals != nullptr && SourceNormals->IsSetTriangle(SourceTid))
		{
			FVector3f NormalA, NormalB, NormalC;
			SourceNormals->GetTriElements(SourceTid, NormalA, NormalB, NormalC);
			Output.Normals->SetTriangle(NewTid, FIndex3i(
				Output.Normals->AppendElement(NormalA),
				Output.Normals->AppendElement(NormalB),
				Output.Normals->AppendElement(NormalC)));
		}
		else
		{
			const FVector3f FaceNormal(SourceMesh.GetTriNormal(SourceTid));
			Output.Normals->SetTriangle(NewTid, FIndex3i(
				Output.Normals->AppendElement(FaceNormal),
				Output.Normals->AppendElement(FaceNormal),
				Output.Normals->AppendElement(FaceNormal)));
		}

		Output.MaterialIDs->SetValue(NewTid, MaterialID);
	}

	/** Output-mesh ids allocated for one Delaunay vertex of a stamp (shared by every fill triangle using it). */
	struct FStampVertexIds
	{
		int32 Vid = INDEX_NONE;
		int32 UVElement = INDEX_NONE;
		int32 NormalElement = INDEX_NONE;
	};

	/**
	 * Stamp one masked source triangle: clip every overlapping traced curve against the triangle's UV footprint,
	 * triangulate all clipped loops in one constrained Delaunay with NonZeroWinding fill, and emit the fill
	 * triangles through the triangle's UV->3D affine map.
	 *
	 * @return the number of triangles emitted into the output mesh.
	 */
	int32 StampTriangle(
		const FDynamicMesh3& SourceMesh,
		const int32 SourceTid,
		const int32 MaterialID,
		const FTriangleUVFrame& Frame,
		const FVector2d (&TranslatedUVs)[3],
		const FVector2d& TileOffset,
		const TArray<FNormalizedCurve>& Curves,
		FOutputMeshLayers& Output)
	{
		FAxisAlignedBox2d TriBounds;
		TriBounds.Contain(TranslatedUVs[0]);
		TriBounds.Contain(TranslatedUVs[1]);
		TriBounds.Contain(TranslatedUVs[2]);
		TriBounds.Min -= FVector2d(TileFitTolerance, TileFitTolerance);
		TriBounds.Max += FVector2d(TileFitTolerance, TileFitTolerance);

		const FClipEdge ClipEdges[3] = {
			MakeClipEdge(TranslatedUVs[0], TranslatedUVs[1], TranslatedUVs[2]),
			MakeClipEdge(TranslatedUVs[1], TranslatedUVs[2], TranslatedUVs[0]),
			MakeClipEdge(TranslatedUVs[2], TranslatedUVs[0], TranslatedUVs[1])};

		// Clip every overlapping curve; accumulate ALL surviving loops into one vertex/edge soup so a single
		// NonZeroWinding fill resolves outer/hole containment (including clip-truncated holes) with no extra
		// bookkeeping.
		TArray<FVector2d> ClipVertices;
		TArray<FIndex2i> ClipEdgeConstraints;
		TArray<FVector2d> WorkLoopA;
		TArray<FVector2d> WorkLoopB;
		for (const FNormalizedCurve& Curve : Curves)
		{
			if (!Curve.Bounds.Intersects(TriBounds))
			{
				continue;
			}

			WorkLoopA = Curve.Vertices;
			for (int32 EdgeIndex = 0; EdgeIndex < 3 && WorkLoopA.Num() >= 3; ++EdgeIndex)
			{
				ClipLoopAgainstEdge(WorkLoopA, ClipEdges[EdgeIndex], WorkLoopB);
				Swap(WorkLoopA, WorkLoopB);
			}
			RemoveConsecutiveDuplicates(WorkLoopA);
			if (WorkLoopA.Num() < 3 || FMath::Abs(LoopSignedArea(WorkLoopA)) < MinClippedLoopArea)
			{
				continue;
			}

			const int32 StartVertex = ClipVertices.Num();
			const int32 LoopCount = WorkLoopA.Num();
			ClipVertices.Append(WorkLoopA);
			ClipEdgeConstraints.Reserve(ClipEdgeConstraints.Num() + LoopCount);
			for (int32 LoopIndex = 0; LoopIndex < LoopCount; ++LoopIndex)
			{
				ClipEdgeConstraints.Add(FIndex2i(StartVertex + LoopIndex, StartVertex + ((LoopIndex + 1) % LoopCount)));
			}
		}

		if (ClipVertices.Num() < 3)
		{
			return 0; // Triangle is fully transparent — cut away.
		}

		FDelaunay2 Delaunay;
		// Identical clip points from touching curves are legal input; remap edges onto the used instances.
		Delaunay.bAutomaticallyFixEdgesToDuplicateVertices = true;
		// Overlapping constraint segments (Sutherland-Hodgman bridge edges along the clip boundary) can defeat
		// exact validation; the winding fill below is best-effort correct for them, so don't fail the stamp.
		Delaunay.bValidateEdges = false;
		Delaunay.Triangulate(TArrayView<const FVector2d>(ClipVertices), TArrayView<const FIndex2i>(ClipEdgeConstraints));

		TArray<FIndex3i> FilledTriangles;
		Delaunay.GetFilledTriangles(FilledTriangles, ClipEdgeConstraints, FDelaunay2::EFillMode::NonZeroWinding);
		if (FilledTriangles.Num() == 0)
		{
			return 0;
		}

		const FVector3f FaceNormal(SourceMesh.GetTriNormal(SourceTid));
		const bool bMirroredUVs = Frame.UVSignedArea2 < 0.0;

		TMap<int32, FStampVertexIds> LocalToOutput;
		LocalToOutput.Reserve(ClipVertices.Num());

		int32 EmittedCount = 0;
		for (const FIndex3i& FillTri : FilledTriangles)
		{
			FIndex3i Ordered = FillTri;
			// Normalize the fill triangle to CCW in UV space ourselves rather than trusting the Delaunay
			// output convention, then flip once more when the source triangle's UVs are mirrored so the
			// emitted 3D winding always matches the source triangle's facing.
			const double FillArea2 = Cross2(
				ClipVertices[Ordered.B] - ClipVertices[Ordered.A],
				ClipVertices[Ordered.C] - ClipVertices[Ordered.A]);
			if (FillArea2 < 0.0)
			{
				Swap(Ordered.B, Ordered.C);
			}
			if (bMirroredUVs)
			{
				Swap(Ordered.B, Ordered.C);
			}

			FStampVertexIds CornerIds[3];
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 LocalIndex = Ordered[Corner];
				FStampVertexIds* Existing = LocalToOutput.Find(LocalIndex);
				if (Existing != nullptr)
				{
					CornerIds[Corner] = *Existing;
					continue;
				}

				const FVector2d OriginalUV = ClipVertices[LocalIndex] + TileOffset;
				FStampVertexIds NewIds;
				NewIds.Vid = Output.Mesh->AppendVertex(Frame.MapUVToPosition(OriginalUV));
				NewIds.UVElement = Output.UV->AppendElement(FVector2f((float)OriginalUV.X, (float)OriginalUV.Y));
				NewIds.NormalElement = Output.Normals->AppendElement(FaceNormal);
				LocalToOutput.Add(LocalIndex, NewIds);
				CornerIds[Corner] = NewIds;
			}

			const int32 NewTid = Output.Mesh->AppendTriangle(
				FIndex3i(CornerIds[0].Vid, CornerIds[1].Vid, CornerIds[2].Vid));
			if (NewTid < 0)
			{
				continue; // Degenerate/duplicate fill triangle — skip.
			}
			Output.UV->SetTriangle(NewTid, FIndex3i(CornerIds[0].UVElement, CornerIds[1].UVElement, CornerIds[2].UVElement));
			Output.Normals->SetTriangle(NewTid, FIndex3i(CornerIds[0].NormalElement, CornerIds[1].NormalElement, CornerIds[2].NormalElement));
			Output.MaterialIDs->SetValue(NewTid, MaterialID);
			++EmittedCount;
		}
		return EmittedCount;
	}
}

namespace Arc::FoliageCut
{
	bool CutMeshToMask(
		const UE::Geometry::FDynamicMesh3& SourceMesh,
		const TArray<UE::Geometry::FPolygon2f>& TracedCurves,
		const FArcCardCutParams& Params,
		UE::Geometry::FDynamicMesh3& OutMesh,
		FArcCardCutStats& OutStats)
	{
		namespace Local = ArcFoliageCardCutterCpp;
		using UE::Geometry::FDynamicMesh3;
		using UE::Geometry::FDynamicMeshAttributeSet;
		using UE::Geometry::FDynamicMeshMaterialAttribute;
		using UE::Geometry::FDynamicMeshUVOverlay;
		using UE::Geometry::FIndex3i;
		using UE::Geometry::FMergeCoincidentMeshEdges;

		OutStats = FArcCardCutStats();

		const FDynamicMeshAttributeSet* SourceAttributes = SourceMesh.Attributes();
		const FDynamicMeshUVOverlay* SourceUV = SourceAttributes ? SourceAttributes->PrimaryUV() : nullptr;
		if (SourceUV == nullptr)
		{
			UE_LOG(LogArcFoliageCardCutter, Warning, TEXT("CutMeshToMask: source mesh has no UV overlay 0; cannot stamp."));
			return false;
		}
		const FDynamicMeshMaterialAttribute* SourceMaterialIDs = SourceAttributes->GetMaterialID();

		OutMesh.Clear();
		OutMesh.EnableAttributes();
		OutMesh.Attributes()->EnableMaterialID();

		Local::FOutputMeshLayers Output;
		Output.Mesh = &OutMesh;
		Output.UV = OutMesh.Attributes()->PrimaryUV();
		Output.Normals = OutMesh.Attributes()->PrimaryNormals();
		Output.MaterialIDs = OutMesh.Attributes()->GetMaterialID();

		const TArray<Local::FNormalizedCurve> Curves = Local::NormalizeCurveWindings(TracedCurves);

		int32 TileSpanningWarnings = 0;
		for (int32 SourceTid : SourceMesh.TriangleIndicesItr())
		{
			const int32 MaterialID = SourceMaterialIDs ? SourceMaterialIDs->GetValue(SourceTid) : 0;

			if (!Params.MaskedSectionIndices.Contains(MaterialID))
			{
				if (Params.bPassThroughUnmaskedSections)
				{
					Local::CopyTriangleVerbatim(SourceMesh, SourceTid, MaterialID, Output);
					++OutStats.PassedThroughTris;
					++OutStats.EmittedTris;
				}
				continue;
			}

			// Unset or degenerate UVs: no valid affine map exists — pass the triangle through unchanged.
			bool bDegenerate = !SourceUV->IsSetTriangle(SourceTid);
			Local::FTriangleUVFrame Frame;
			FVector2d UVs[3] = {FVector2d::ZeroVector, FVector2d::ZeroVector, FVector2d::ZeroVector};
			if (!bDegenerate)
			{
				FVector2f UVA, UVB, UVC;
				SourceUV->GetTriElements(SourceTid, UVA, UVB, UVC);
				UVs[0] = FVector2d(UVA.X, UVA.Y);
				UVs[1] = FVector2d(UVB.X, UVB.Y);
				UVs[2] = FVector2d(UVC.X, UVC.Y);

				const FIndex3i TriVerts = SourceMesh.GetTriangle(SourceTid);
				const FVector3d Positions[3] = {
					SourceMesh.GetVertex(TriVerts.A),
					SourceMesh.GetVertex(TriVerts.B),
					SourceMesh.GetVertex(TriVerts.C)};
				bDegenerate = !Local::BuildTriangleUVFrame(Positions, UVs, Frame);
			}
			if (bDegenerate)
			{
				Local::CopyTriangleVerbatim(SourceMesh, SourceTid, MaterialID, Output);
				++OutStats.SkippedDegenerate;
				++OutStats.EmittedTris;
				continue;
			}

			// UV-tile handling: translate the footprint into [0,1] by the integral floor of its UV minimum.
			// The offset is integral, so it is exactly representable and both tile-mates of a shared edge in
			// the same tile translate identically. Triangles spanning a tile boundary are passed through.
			const double MinU = FMath::Min3(UVs[0].X, UVs[1].X, UVs[2].X);
			const double MinV = FMath::Min3(UVs[0].Y, UVs[1].Y, UVs[2].Y);
			const double MaxU = FMath::Max3(UVs[0].X, UVs[1].X, UVs[2].X);
			const double MaxV = FMath::Max3(UVs[0].Y, UVs[1].Y, UVs[2].Y);
			const FVector2d TileOffset(FMath::FloorToDouble(MinU), FMath::FloorToDouble(MinV));
			if ((MaxU - TileOffset.X) > (1.0 + Local::TileFitTolerance) ||
				(MaxV - TileOffset.Y) > (1.0 + Local::TileFitTolerance))
			{
				Local::CopyTriangleVerbatim(SourceMesh, SourceTid, MaterialID, Output);
				++OutStats.SkippedTileSpanning;
				++OutStats.EmittedTris;
				++TileSpanningWarnings;
				continue;
			}

			const FVector2d TranslatedUVs[3] = {UVs[0] - TileOffset, UVs[1] - TileOffset, UVs[2] - TileOffset};
			OutStats.EmittedTris += Local::StampTriangle(
				SourceMesh, SourceTid, MaterialID, Frame, TranslatedUVs, TileOffset, Curves, Output);
			++OutStats.StampedSourceTris;
		}

		if (TileSpanningWarnings > 0)
		{
			UE_LOG(LogArcFoliageCardCutter, Warning,
				TEXT("CutMeshToMask: %d triangle(s) span a UV tile boundary and were passed through unstamped."),
				TileSpanningWarnings);
		}

		if (OutMesh.TriangleCount() == 0)
		{
			UE_LOG(LogArcFoliageCardCutter, Warning, TEXT("CutMeshToMask: output mesh is empty (mask cut away all geometry)."));
			return false;
		}

		// Weld seams between per-source-triangle stamps so bent multi-quad blades form one connected island
		// instead of tearing at every source-triangle boundary. Shared-edge clip points agree to floating-point
		// noise (identical sorted clip lines + identical curve segments), so a small tolerance suffices.
		FMergeCoincidentMeshEdges Welder(&OutMesh);
		Welder.MergeVertexTolerance = Params.WeldTolerance;
		Welder.MergeSearchTolerance = 2.0 * Params.WeldTolerance;
		Welder.bWeldAttrsOnMergedEdges = true;
		Welder.SplitAttributeWelder.UVDistSqrdThreshold = 1.e-10f;  // Only merge effectively identical UVs.
		Welder.SplitAttributeWelder.NormalVecDotThreshold = 1.e-6f; // Coplanar stamps merge; real creases stay split.
		Welder.Apply();

		return true;
	}
}
