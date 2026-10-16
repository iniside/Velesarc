// Copyright Lukasz Baran. All Rights Reserved.

#include "Morphology/ArcTownBlocks.h"

#include "Layout/ArcTownRoadArrangement.h"
#include "Layout/ArcTownRoadArrangementGraph.h"
#include "Simulation/ArcTownTraffic.h"

namespace ArcTownBlocksPrivate
{
namespace
{
	constexpr double ParallelCrossThreshold = 1.0e-4;

	constexpr double CoincidentToleranceCm = 0.1;

	FVector2f ToFloat(const FVector2d& Value)
	{
		return FVector2f(static_cast<float>(Value.X), static_cast<float>(Value.Y));
	}

	FVector2d ToDouble(const FVector2f& Value)
	{
		return FVector2d(static_cast<double>(Value.X), static_cast<double>(Value.Y));
	}

	double Cross(const FVector2d& A, const FVector2d& B)
	{
		return A.X * B.Y - A.Y * B.X;
	}

	double SignedArea(TConstArrayView<FVector2d> Polygon)
	{
		double TwiceArea = 0.0;
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			const int32 NextIndex = Index + 1 == Polygon.Num() ? 0 : Index + 1;
			const FVector2d& Start = Polygon[Index];
			const FVector2d& End = Polygon[NextIndex];
			TwiceArea += Start.X * End.Y - End.X * Start.Y;
		}

		return 0.5 * TwiceArea;
	}

	bool SegmentsProperlyCross(
		const FVector2d& P0,
		const FVector2d& P1,
		const FVector2d& Q0,
		const FVector2d& Q1)
	{
		const double D0 = Cross(Q1 - Q0, P0 - Q0);
		const double D1 = Cross(Q1 - Q0, P1 - Q0);
		const double D2 = Cross(P1 - P0, Q0 - P0);
		const double D3 = Cross(P1 - P0, Q1 - P0);

		const bool bStraddlesQ = (D0 > 0.0 && D1 < 0.0) || (D0 < 0.0 && D1 > 0.0);
		const bool bStraddlesP = (D2 > 0.0 && D3 < 0.0) || (D2 < 0.0 && D3 > 0.0);
		return bStraddlesQ && bStraddlesP;
	}

	// A vertex snapped onto a segment the arrangement then failed to split comes back collinear and
	// overlapping, never straddling, so the crossing test alone accepts it.
	bool SegmentsOverlapCollinear(
		const FVector2d& P0,
		const FVector2d& P1,
		const FVector2d& Q0,
		const FVector2d& Q1)
	{
		const FVector2d PDelta = P1 - P0;
		const FVector2d QDelta = Q1 - Q0;
		const double PLengthSquared = PDelta.SquaredLength();
		const double QLengthSquared = QDelta.SquaredLength();
		if (PLengthSquared <= 0.0 || QLengthSquared <= 0.0)
		{
			return false;
		}

		const double QLength = FMath::Sqrt(QLengthSquared);
		if (FMath::Abs(Cross(QDelta, P0 - Q0)) > CoincidentToleranceCm * QLength
			|| FMath::Abs(Cross(QDelta, P1 - Q0)) > CoincidentToleranceCm * QLength)
		{
			return false;
		}

		const double PLength = FMath::Sqrt(PLengthSquared);
		const double FirstAlong = (Q0 - P0).Dot(PDelta) / PLengthSquared;
		const double SecondAlong = (Q1 - P0).Dot(PDelta) / PLengthSquared;
		const double Low = FMath::Max(0.0, FMath::Min(FirstAlong, SecondAlong));
		const double High = FMath::Min(1.0, FMath::Max(FirstAlong, SecondAlong));
		return (High - Low) * PLength > CoincidentToleranceCm;
	}

	bool IsSimplePolygon(TConstArrayView<FVector2d> Polygon)
	{
		for (int32 First = 0; First < Polygon.Num(); ++First)
		{
			for (int32 Second = First + 1; Second < Polygon.Num(); ++Second)
			{
				if ((Polygon[First] - Polygon[Second]).SizeSquared()
					<= CoincidentToleranceCm * CoincidentToleranceCm)
				{
					return false;
				}
			}
		}

		for (int32 First = 0; First < Polygon.Num(); ++First)
		{
			const int32 FirstEnd = First + 1 == Polygon.Num() ? 0 : First + 1;
			for (int32 Second = First + 2; Second < Polygon.Num(); ++Second)
			{
				const int32 SecondEnd = Second + 1 == Polygon.Num() ? 0 : Second + 1;
				if (SecondEnd == First)
				{
					continue;
				}

				if (SegmentsProperlyCross(
						Polygon[First],
						Polygon[FirstEnd],
						Polygon[Second],
						Polygon[SecondEnd])
					|| SegmentsOverlapCollinear(
						Polygon[First],
						Polygon[FirstEnd],
						Polygon[Second],
						Polygon[SecondEnd]))
				{
					return false;
				}
			}
		}

		for (int32 First = 0; First < Polygon.Num(); ++First)
		{
			const int32 Second = First + 1 == Polygon.Num() ? 0 : First + 1;
			const int32 SecondEnd = Second + 1 == Polygon.Num() ? 0 : Second + 1;
			if (SegmentsOverlapCollinear(
					Polygon[First],
					Polygon[Second],
					Polygon[Second],
					Polygon[SecondEnd]))
			{
				return false;
			}
		}

		return true;
	}

	bool IsLexLess(const FVector2f& A, const FVector2f& B)
	{
		if (A.Y != B.Y)
		{
			return A.Y < B.Y;
		}

		return A.X < B.X;
	}

	bool IsFrontageLess(const FArcTownBlock& A, const FArcTownBlock& B)
	{
		const int32 CommonCount = FMath::Min(A.Boundary.Num(), B.Boundary.Num());
		for (int32 Index = 0; Index < CommonCount; ++Index)
		{
			if (A.Boundary[Index] != B.Boundary[Index])
			{
				return IsLexLess(A.Boundary[Index], B.Boundary[Index]);
			}
		}

		if (A.Boundary.Num() != B.Boundary.Num())
		{
			return A.Boundary.Num() < B.Boundary.Num();
		}

		return A.AreaCm2 < B.AreaCm2;
	}

	template <typename ElementType>
	void RotateToFront(TArray<ElementType>& Values, const int32 FirstIndex)
	{
		if (FirstIndex <= 0)
		{
			return;
		}

		TArray<ElementType> Rotated;
		Rotated.Reserve(Values.Num());
		for (int32 Offset = 0; Offset < Values.Num(); ++Offset)
		{
			Rotated.Add(Values[(FirstIndex + Offset) % Values.Num()]);
		}

		Values = MoveTemp(Rotated);
	}

	void Canonicalize(FArcTownBlock& Block)
	{
		int32 FirstVertex = 0;
		for (int32 Index = 1; Index < Block.Boundary.Num(); ++Index)
		{
			if (IsLexLess(Block.Boundary[Index], Block.Boundary[FirstVertex]))
			{
				FirstVertex = Index;
			}
		}

		RotateToFront(Block.Boundary, FirstVertex);
		RotateToFront(Block.BoundaryEdgeIndex, FirstVertex);
		RotateToFront(Block.BoundaryHierarchy, FirstVertex);
		RotateToFront(Block.BoundaryHalfWidth, FirstVertex);
	}

	void WalkFaces(
		const ArcTownRoadArrangement::FArrangement& Arrangement,
		const double MinAreaCm2,
		TArray<FArcTownBlock>& OutBlocks,
		FArcTownBlockRejections& OutRejections,
		TBitArray<>& OutCoveredHalfEdges)
	{
		OutBlocks.Reset();
		OutCoveredHalfEdges.Reset();

		const UE::Geometry::FDynamicGraph2d& Graph = ArcTownRoadArrangement::GetGraph(Arrangement);
		const int32 MaxVertexID = Graph.MaxVertexID();
		const int32 MaxEdgeID = Graph.MaxEdgeID();
		if (MaxVertexID <= 0 || MaxEdgeID <= 0)
		{
			return;
		}

		OutCoveredHalfEdges.Init(false, 2 * MaxEdgeID);

		TArray<FVector2d> Positions;
		Positions.SetNum(MaxVertexID);
		TArray<TArray<int32>> VertexRing;
		VertexRing.SetNum(MaxVertexID);
		for (int32 VID = 0; VID < MaxVertexID; ++VID)
		{
			if (!Graph.IsVertex(VID))
			{
				continue;
			}

			Positions[VID] = Graph.GetVertex(VID);
			Graph.SortedVtxEdges(VID, VertexRing[VID]);
		}

		TArray<int32> EdgeStart;
		TArray<int32> EdgeEnd;
		EdgeStart.Init(INDEX_NONE, MaxEdgeID);
		EdgeEnd.Init(INDEX_NONE, MaxEdgeID);
		TBitArray<> EdgeAlive(false, MaxEdgeID);
		for (int32 EID = 0; EID < MaxEdgeID; ++EID)
		{
			if (!Graph.IsEdge(EID))
			{
				continue;
			}

			const UE::Geometry::FIndex2i EdgeVerts = Graph.GetEdgeV(EID);
			EdgeStart[EID] = EdgeVerts.A;
			EdgeEnd[EID] = EdgeVerts.B;
			EdgeAlive[EID] = true;
		}

		TArray<int32> SpurQueue;
		for (int32 VID = 0; VID < MaxVertexID; ++VID)
		{
			if (VertexRing[VID].Num() == 1)
			{
				SpurQueue.Add(VID);
			}
		}

		while (!SpurQueue.IsEmpty())
		{
			const int32 VID = SpurQueue.Pop(EAllowShrinking::No);
			if (VertexRing[VID].Num() != 1)
			{
				continue;
			}

			const int32 EID = VertexRing[VID][0];
			const int32 OtherVID = EdgeStart[EID] == VID ? EdgeEnd[EID] : EdgeStart[EID];

			EdgeAlive[EID] = false;
			VertexRing[VID].Reset();
			VertexRing[OtherVID].RemoveSingle(EID);
			if (VertexRing[OtherVID].Num() == 1)
			{
				SpurQueue.Add(OtherVID);
			}
		}

		TBitArray<> VisitedHalfEdges(false, 2 * MaxEdgeID);
		TArray<int32> Face;
		TArray<int32> FaceHalfEdges;
		TArray<FVector2d> FacePoints;

		for (int32 StartEID = 0; StartEID < MaxEdgeID; ++StartEID)
		{
			if (!EdgeAlive[StartEID])
			{
				continue;
			}

			for (int32 StartDirection = 0; StartDirection < 2; ++StartDirection)
			{
				const int32 StartHalfEdge = 2 * StartEID + StartDirection;
				if (VisitedHalfEdges[StartHalfEdge])
				{
					continue;
				}

				Face.Reset();
				FaceHalfEdges.Reset();
				bool bBroken = false;
				int32 HalfEdge = StartHalfEdge;
				do
				{
					VisitedHalfEdges[HalfEdge] = true;

					const int32 EID = HalfEdge / 2;
					const bool bForward = (HalfEdge & 1) == 0;
					const int32 FromVID = bForward ? EdgeStart[EID] : EdgeEnd[EID];
					const int32 ToVID = bForward ? EdgeEnd[EID] : EdgeStart[EID];
					Face.Add(FromVID);
					FaceHalfEdges.Add(HalfEdge);

					const TArray<int32>& Ring = VertexRing[ToVID];
					const int32 RingPosition = Ring.Find(EID);
					checkf(
						RingPosition != INDEX_NONE,
						TEXT("Edge %d is alive but missing from the ring of vertex %d: spur trimming clears "
							 "EdgeAlive and both endpoint rings in the same step, so every alive edge stays in "
							 "the ring of the vertex it enters."),
						EID,
						ToVID);

					if (RingPosition == INDEX_NONE)
					{
						bBroken = true;
						break;
					}

					const int32 PreviousPosition = (RingPosition + Ring.Num() - 1) % Ring.Num();
					const int32 NextEID = Ring[PreviousPosition];
					const int32 NextDirection = EdgeStart[NextEID] == ToVID ? 0 : 1;
					HalfEdge = 2 * NextEID + NextDirection;
				}
				while (HalfEdge != StartHalfEdge);

				if (bBroken)
				{
					for (const int32 AbortedHalfEdge : FaceHalfEdges)
					{
						VisitedHalfEdges[AbortedHalfEdge] = false;
					}

					continue;
				}

				if (Face.Num() < 3)
				{
					++OutRejections.TooFewSides;
					continue;
				}

				FacePoints.Reset();
				FacePoints.Reserve(Face.Num());
				for (const int32 VID : Face)
				{
					FacePoints.Add(Positions[VID]);
				}

				// Stepping back one entry in the CCW edge ring traces bounded faces counter-clockwise, so the
				// one face per connected component that comes out clockwise is that component's outer wall.
				const double AreaCm2 = SignedArea(FacePoints);
				if (AreaCm2 <= 0.0)
				{
					++OutRejections.OuterFaces;
					continue;
				}

				for (const int32 CoveredHalfEdge : FaceHalfEdges)
				{
					OutCoveredHalfEdges[CoveredHalfEdge] = true;
				}

				if (AreaCm2 < MinAreaCm2)
				{
					++OutRejections.AreaBelowThreshold;
					continue;
				}

				if (!IsSimplePolygon(FacePoints))
				{
					++OutRejections.NonSimpleBoundary;
					continue;
				}

				FArcTownBlock& Block = OutBlocks.AddDefaulted_GetRef();
				Block.AreaCm2 = AreaCm2;
				Block.Boundary.Reserve(FacePoints.Num());
				Block.BoundaryEdgeIndex.Reserve(FacePoints.Num());
				Block.BoundaryHierarchy.Reserve(FacePoints.Num());
				Block.BoundaryHalfWidth.Reserve(FacePoints.Num());

				for (int32 Index = 0; Index < FacePoints.Num(); ++Index)
				{
					const int32 NextIndex = Index + 1 == FacePoints.Num() ? 0 : Index + 1;
					const FVector2d& Start = FacePoints[Index];
					const FVector2d& End = FacePoints[NextIndex];
					Block.Boundary.Add(ToFloat(Start));

					// Side attribution goes through the nearest source edge, never the arrangement GID:
					// split_segment_at_t rewrites the Group of edges it did not create.
					const int32 SourceIndex = ArcTownRoadArrangement::FindNearestSourceEdge(
						Arrangement.SourceEdges,
						0.5 * (Start + End),
						Arrangement.SnapToleranceCm);

					if (SourceIndex == INDEX_NONE)
					{
						Block.BoundaryEdgeIndex.Add(INDEX_NONE);
						Block.BoundaryHierarchy.Add(0);
						Block.BoundaryHalfWidth.Add(0.0f);
						continue;
					}

					const ArcTownRoadArrangement::FSourceEdge& Source = Arrangement.SourceEdges[SourceIndex];
					Block.BoundaryEdgeIndex.Add(Source.SnapshotEdgeIndex);
					Block.BoundaryHierarchy.Add(Source.Hierarchy);
					Block.BoundaryHalfWidth.Add(Source.HalfWidth);
				}

				Canonicalize(Block);
			}
		}

		OutBlocks.Sort(
			[](const FArcTownBlock& A, const FArcTownBlock& B) -> bool
			{
				return IsFrontageLess(A, B);
			});
	}

	bool TryInsetBlock(
		const FArcTownBlock& Block,
		const FArcTownTrafficSettings& TrafficSettings,
		const double MinAreaCm2,
		FArcTownBlock& OutFrontage)
	{
		const int32 SideCount = Block.Boundary.Num();
		if (SideCount < 3
			|| Block.BoundaryEdgeIndex.Num() != SideCount
			|| Block.BoundaryHierarchy.Num() != SideCount
			|| Block.BoundaryHalfWidth.Num() != SideCount)
		{
			return false;
		}

		TArray<FVector2d> Vertices;
		TArray<FVector2d> Directions;
		TArray<FVector2d> InwardNormals;
		TArray<double> SideLengths;
		TArray<double> Insets;
		Vertices.Reserve(SideCount);
		Directions.Reserve(SideCount);
		InwardNormals.Reserve(SideCount);
		SideLengths.Reserve(SideCount);
		Insets.Reserve(SideCount);

		for (const FVector2f& Vertex : Block.Boundary)
		{
			Vertices.Add(ToDouble(Vertex));
		}

		for (int32 Side = 0; Side < SideCount; ++Side)
		{
			const int32 NextSide = Side + 1 == SideCount ? 0 : Side + 1;
			const FVector2d Span = Vertices[NextSide] - Vertices[Side];
			const double LengthCm = Span.Size();
			if (!FMath::IsFinite(LengthCm) || LengthCm <= CoincidentToleranceCm)
			{
				return false;
			}

			const FVector2d Direction = Span / LengthCm;
			Directions.Add(Direction);
			InwardNormals.Add(FVector2d(-Direction.Y, Direction.X));
			SideLengths.Add(LengthCm);

			// The inset takes the half width promotion can still widen this street to, the same one the
			// carriageway reservation takes; today's width would leave every plot on the widened road bed.
			Insets.Add(
				Block.BoundaryEdgeIndex[Side] == INDEX_NONE
					? 0.0
					: ArcTownTraffic::MaxHalfWidthCm(
						TrafficSettings, static_cast<double>(Block.BoundaryHalfWidth[Side])));
		}

		TArray<FVector2d> Corners;
		TArray<int32> CornerSides;
		Corners.Reserve(2 * SideCount);
		CornerSides.Reserve(2 * SideCount);

		for (int32 Side = 0; Side < SideCount; ++Side)
		{
			const int32 PreviousSide = (Side + SideCount - 1) % SideCount;
			const FVector2d& Corner = Vertices[Side];
			const FVector2d PreviousEnd = Corner + InwardNormals[PreviousSide] * Insets[PreviousSide];
			const FVector2d SideStart = Corner + InwardNormals[Side] * Insets[Side];

			bool bBevel = true;
			const double Turn = Cross(Directions[PreviousSide], Directions[Side]);
			if (FMath::Abs(Turn) > ParallelCrossThreshold)
			{
				const FVector2d Offset = SideStart - PreviousEnd;
				const double AlongPrevious = Cross(Offset, Directions[Side]) / Turn;
				const double AlongSide = Cross(Offset, Directions[PreviousSide]) / Turn;
				if (FMath::IsFinite(AlongPrevious)
					&& FMath::IsFinite(AlongSide)
					&& FMath::Abs(AlongPrevious) <= SideLengths[PreviousSide]
					&& FMath::Abs(AlongSide) <= SideLengths[Side])
				{
					Corners.Add(PreviousEnd + Directions[PreviousSide] * AlongPrevious);
					CornerSides.Add(Side);
					bBevel = false;
				}
			}

			if (bBevel)
			{
				// Opposed inward normals put each bevel point across the other street's axis, so the corner
				// only bevels while the two sides still face the same way.
				if (InwardNormals[PreviousSide].Dot(InwardNormals[Side]) < 0.0)
				{
					return false;
				}

				if ((SideStart - PreviousEnd).Size() > CoincidentToleranceCm)
				{
					Corners.Add(PreviousEnd);
					CornerSides.Add(INDEX_NONE);
				}

				Corners.Add(SideStart);
				CornerSides.Add(Side);
			}
		}

		TArray<FVector2d> Polygon;
		TArray<int32> PolygonSides;
		Polygon.Reserve(Corners.Num());
		PolygonSides.Reserve(Corners.Num());
		for (int32 Index = 0; Index < Corners.Num(); ++Index)
		{
			if (!Polygon.IsEmpty() && (Corners[Index] - Polygon.Last()).Size() <= CoincidentToleranceCm)
			{
				PolygonSides.Last() = CornerSides[Index];
				continue;
			}

			Polygon.Add(Corners[Index]);
			PolygonSides.Add(CornerSides[Index]);
		}

		while (Polygon.Num() >= 2 && (Polygon.Last() - Polygon[0]).Size() <= CoincidentToleranceCm)
		{
			Polygon.Pop(EAllowShrinking::No);
			PolygonSides.Pop(EAllowShrinking::No);
		}

		if (Polygon.Num() < 3)
		{
			return false;
		}

		const double AreaCm2 = SignedArea(Polygon);
		if (AreaCm2 <= 0.0 || AreaCm2 < MinAreaCm2)
		{
			return false;
		}

		if (!IsSimplePolygon(Polygon))
		{
			return false;
		}

		OutFrontage = FArcTownBlock();
		OutFrontage.AreaCm2 = AreaCm2;
		OutFrontage.Boundary.Reserve(Polygon.Num());
		OutFrontage.BoundaryEdgeIndex.Reserve(Polygon.Num());
		OutFrontage.BoundaryHierarchy.Reserve(Polygon.Num());
		OutFrontage.BoundaryHalfWidth.Reserve(Polygon.Num());
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			const int32 SourceSide = PolygonSides[Index];
			OutFrontage.Boundary.Add(ToFloat(Polygon[Index]));
			OutFrontage.BoundaryEdgeIndex.Add(
				SourceSide == INDEX_NONE ? INDEX_NONE : Block.BoundaryEdgeIndex[SourceSide]);
			OutFrontage.BoundaryHierarchy.Add(SourceSide == INDEX_NONE ? 0 : Block.BoundaryHierarchy[SourceSide]);
			OutFrontage.BoundaryHalfWidth.Add(
				SourceSide == INDEX_NONE ? 0.0f : Block.BoundaryHalfWidth[SourceSide]);
		}

		Canonicalize(OutFrontage);
		return true;
	}

	bool TryMakeStrip(
		const ArcTownRoadArrangement::FArrangement& Arrangement,
		const FArcTownTrafficSettings& TrafficSettings,
		const FVector2d& From,
		const FVector2d& To,
		const double MaxPlotDepthCm,
		const double MinRunLengthCm,
		FArcTownBlock& OutStrip,
		bool& bOutBelowThreshold)
	{
		bOutBelowThreshold = false;

		const FVector2d Span = To - From;
		const double LengthCm = Span.Size();
		if (!FMath::IsFinite(LengthCm) || LengthCm <= CoincidentToleranceCm)
		{
			return false;
		}

		const int32 SourceIndex = ArcTownRoadArrangement::FindNearestSourceEdge(
			Arrangement.SourceEdges, 0.5 * (From + To), Arrangement.SnapToleranceCm);
		if (SourceIndex == INDEX_NONE)
		{
			return false;
		}

		if (LengthCm < MinRunLengthCm)
		{
			bOutBelowThreshold = true;
			return false;
		}

		const ArcTownRoadArrangement::FSourceEdge& Source = Arrangement.SourceEdges[SourceIndex];
		const double OffsetCm =
			ArcTownTraffic::MaxHalfWidthCm(TrafficSettings, static_cast<double>(Source.HalfWidth));

		const FVector2d Direction = Span / LengthCm;
		const FVector2d LeftNormal(-Direction.Y, Direction.X);
		const FVector2d NearOffset = LeftNormal * OffsetCm;
		const FVector2d FarOffset = LeftNormal * (OffsetCm + MaxPlotDepthCm);

		OutStrip = FArcTownBlock();
		OutStrip.AreaCm2 = LengthCm * MaxPlotDepthCm;
		OutStrip.Boundary = {
			ToFloat(From + NearOffset),
			ToFloat(To + NearOffset),
			ToFloat(To + FarOffset),
			ToFloat(From + FarOffset)};
		OutStrip.BoundaryEdgeIndex = {Source.SnapshotEdgeIndex, INDEX_NONE, INDEX_NONE, INDEX_NONE};
		OutStrip.BoundaryHierarchy = {Source.Hierarchy, 0, 0, 0};
		OutStrip.BoundaryHalfWidth = {Source.HalfWidth, 0.0f, 0.0f, 0.0f};

		Canonicalize(OutStrip);
		return true;
	}
}
}

void ArcTownBlocks::ExtractBlocks(
	const ArcTownRoadArrangement::FArrangement& Arrangement,
	const double MinAreaCm2,
	TArray<FArcTownBlock>& OutBlocks,
	FArcTownBlockRejections& OutRejections)
{
	using namespace ArcTownBlocksPrivate;

	OutBlocks.Reset();
	OutRejections = FArcTownBlockRejections();

	if (!Arrangement.IsValid())
	{
		return;
	}

	TBitArray<> CoveredHalfEdges;
	WalkFaces(Arrangement, MinAreaCm2, OutBlocks, OutRejections, CoveredHalfEdges);
}

bool ArcTownBlocks::BuildFrontagePolygon(
	const FArcTownBlock& Block,
	const FArcTownTrafficSettings& TrafficSettings,
	const double MinAreaCm2,
	FArcTownBlock& OutFrontage)
{
	using namespace ArcTownBlocksPrivate;

	return TryInsetBlock(Block, TrafficSettings, MinAreaCm2, OutFrontage);
}

void ArcTownBlocks::BuildFrontage(
	const ArcTownRoadArrangement::FArrangement& Arrangement,
	const FArcTownTrafficSettings& TrafficSettings,
	const double MaxPlotDepthCm,
	const double MinAreaCm2,
	TArray<FArcTownBlock>& OutFrontage,
	FArcTownBlockRejections& OutRejections)
{
	using namespace ArcTownBlocksPrivate;

	OutFrontage.Reset();
	OutRejections = FArcTownBlockRejections();

	if (!Arrangement.IsValid())
	{
		return;
	}

	TArray<FArcTownBlock> Blocks;
	TBitArray<> CoveredHalfEdges;
	WalkFaces(Arrangement, MinAreaCm2, Blocks, OutRejections, CoveredHalfEdges);

	for (const FArcTownBlock& Block : Blocks)
	{
		FArcTownBlock Frontage;
		if (TryInsetBlock(Block, TrafficSettings, MinAreaCm2, Frontage))
		{
			OutFrontage.Add(MoveTemp(Frontage));
			continue;
		}

		++OutRejections.FrontageCollapsed;
	}

	if (!CoveredHalfEdges.IsEmpty())
	{
		const bool bDepthUsable = FMath::IsFinite(MaxPlotDepthCm) && MaxPlotDepthCm > 0.0;
		const double MinRunLengthCm = bDepthUsable ? MinAreaCm2 / MaxPlotDepthCm : 0.0;
		const UE::Geometry::FDynamicGraph2d& Graph = ArcTownRoadArrangement::GetGraph(Arrangement);
		const int32 MaxEdgeID = Graph.MaxEdgeID();
		for (int32 EID = 0; EID < MaxEdgeID; ++EID)
		{
			if (!Graph.IsEdge(EID))
			{
				continue;
			}

			const UE::Geometry::FIndex2i EdgeVerts = Graph.GetEdgeV(EID);
			const FVector2d VertexA = Graph.GetVertex(EdgeVerts.A);
			const FVector2d VertexB = Graph.GetVertex(EdgeVerts.B);

			// Gated per half-edge: a ring road with a block on one side still has its other side open,
			// and a per-edge gate would leave that open side with no frontage at all.
			for (int32 Direction = 0; Direction < 2; ++Direction)
			{
				const int32 HalfEdge = 2 * EID + Direction;
				if (!CoveredHalfEdges.IsValidIndex(HalfEdge) || CoveredHalfEdges[HalfEdge])
				{
					continue;
				}

				if (!bDepthUsable)
				{
					++OutRejections.StripDepthUnusable;
					continue;
				}

				FArcTownBlock Strip;
				bool bBelowThreshold = false;
				if (TryMakeStrip(
						Arrangement,
						TrafficSettings,
						Direction == 0 ? VertexA : VertexB,
						Direction == 0 ? VertexB : VertexA,
						MaxPlotDepthCm,
						MinRunLengthCm,
						Strip,
						bBelowThreshold))
				{
					OutFrontage.Add(MoveTemp(Strip));
					continue;
				}

				if (bBelowThreshold)
				{
					++OutRejections.StripBelowThreshold;
				}
			}
		}
	}

	OutFrontage.Sort(
		[](const FArcTownBlock& A, const FArcTownBlock& B) -> bool
		{
			return IsFrontageLess(A, B);
		});
}
