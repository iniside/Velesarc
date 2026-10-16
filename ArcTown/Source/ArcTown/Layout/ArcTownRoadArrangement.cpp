// Copyright Lukasz Baran. All Rights Reserved.

#include "Layout/ArcTownRoadArrangement.h"

#include "Arrangement2d.h"
#include "BoxTypes.h"
#include "Layout/ArcTownRoadArrangementGraph.h"
#include "Simulation/ArcTownWorldSnapshot.h"

namespace ArcTownRoadArrangement
{
struct FGraphStorage
{
	explicit FGraphStorage(const UE::Geometry::FAxisAlignedBox2d& Bounds)
		: Arrangement(Bounds)
	{
	}

	UE::Geometry::FArrangement2d Arrangement;
};

namespace ArcTownRoadArrangementPrivate
{
namespace
{
	FVector2d ToDouble(const FVector2f& Value)
	{
		return FVector2d(static_cast<double>(Value.X), static_cast<double>(Value.Y));
	}

	double DistanceSquaredToEdge(const FSourceEdge& Source, const FVector2d& Point)
	{
		const FVector2d Delta = Source.End - Source.Start;
		const double LengthSquared = Delta.SquaredLength();
		if (LengthSquared <= 0.0)
		{
			return UE::Geometry::DistanceSquared(Source.Start, Point);
		}

		const double Alpha = FMath::Clamp((Point - Source.Start).Dot(Delta) / LengthSquared, 0.0, 1.0);
		return UE::Geometry::DistanceSquared(Source.Start + Alpha * Delta, Point);
	}

	double NearestDistanceSquared(TConstArrayView<FSourceEdge> SourceEdges, const FVector2d& Point)
	{
		double BestDistanceSquared = TNumericLimits<double>::Max();
		for (const FSourceEdge& Source : SourceEdges)
		{
			BestDistanceSquared = FMath::Min(BestDistanceSquared, DistanceSquaredToEdge(Source, Point));
		}

		return BestDistanceSquared;
	}
}
}

void GatherSourceEdges(const FArcTownWorldSnapshot& Snapshot, const double MinLengthCm, TArray<FSourceEdge>& OutEdges)
{
	using namespace ArcTownRoadArrangementPrivate;

	OutEdges.Reset();
	OutEdges.Reserve(Snapshot.Edges.Num());

	for (int32 EdgeIndex = 0; EdgeIndex < Snapshot.Edges.Num(); ++EdgeIndex)
	{
		const FArcTownRoadEdge& Edge = Snapshot.Edges[EdgeIndex];

		// A bridge span carries no road bed, so it must reach neither the path stamps nor their raster.
		if (Edge.bBuilt == 0 || Edge.bBridge != 0)
		{
			continue;
		}

		const FVector2d A = ToDouble(Snapshot.Nodes[Edge.NodeA].LocalXY);
		const FVector2d B = ToDouble(Snapshot.Nodes[Edge.NodeB].LocalXY);
		if (UE::Geometry::DistanceSquared(A, B) <= MinLengthCm * MinLengthCm)
		{
			continue;
		}

		FSourceEdge& Source = OutEdges.AddDefaulted_GetRef();
		Source.Start = A;
		Source.End = B;
		Source.HalfWidth = Edge.HalfWidth;
		Source.Hierarchy = Edge.Hierarchy;
		Source.SnapshotEdgeIndex = EdgeIndex;
	}
}

// Width comes from the nearest source edge, never from the arrangement's GID: split_segment_at_t
// reaches SplitEdgeWithExistingVertex, which rewrites the Group of edges it did not create.
FSegmentWidth FindWidthNear(TConstArrayView<FSourceEdge> SourceEdges, const FVector2d& Point, const double SnapTolerance)
{
	using namespace ArcTownRoadArrangementPrivate;

	const double BestDistanceSquared = NearestDistanceSquared(SourceEdges, Point);

	FSegmentWidth Result;
	const double BandDistanceSquared = BestDistanceSquared + SnapTolerance * SnapTolerance;
	for (const FSourceEdge& Source : SourceEdges)
	{
		if (DistanceSquaredToEdge(Source, Point) <= BandDistanceSquared)
		{
			Result.HalfWidth = FMath::Max(Result.HalfWidth, Source.HalfWidth);
			Result.Hierarchy = FMath::Max(Result.Hierarchy, Source.Hierarchy);
		}
	}

	return Result;
}

int32 FindNearestSourceEdge(TConstArrayView<FSourceEdge> SourceEdges, const FVector2d& Point, const double SnapTolerance)
{
	using namespace ArcTownRoadArrangementPrivate;

	if (SourceEdges.IsEmpty())
	{
		return INDEX_NONE;
	}

	const double BestDistanceSquared = NearestDistanceSquared(SourceEdges, Point);
	const double BandDistanceSquared = BestDistanceSquared + SnapTolerance * SnapTolerance;

	int32 BestIndex = INDEX_NONE;
	int32 BestSnapshotEdgeIndex = MAX_int32;
	for (int32 Index = 0; Index < SourceEdges.Num(); ++Index)
	{
		const FSourceEdge& Source = SourceEdges[Index];
		if (DistanceSquaredToEdge(Source, Point) > BandDistanceSquared)
		{
			continue;
		}

		if (Source.SnapshotEdgeIndex < BestSnapshotEdgeIndex)
		{
			BestSnapshotEdgeIndex = Source.SnapshotEdgeIndex;
			BestIndex = Index;
		}
	}

	return BestIndex;
}

bool Build(const FArcTownWorldSnapshot& Snapshot, const double SnapToleranceCm, FArrangement& OutArrangement)
{
	OutArrangement.Storage.Reset();
	OutArrangement.SnapToleranceCm = SnapToleranceCm;

	GatherSourceEdges(Snapshot, SnapToleranceCm, OutArrangement.SourceEdges);
	if (OutArrangement.SourceEdges.IsEmpty())
	{
		return false;
	}

	UE::Geometry::FAxisAlignedBox2d Bounds;
	for (const FSourceEdge& Source : OutArrangement.SourceEdges)
	{
		Bounds.Contain(Source.Start);
		Bounds.Contain(Source.End);
	}

	OutArrangement.Storage = MakePimpl<FGraphStorage>(Bounds);

	// Every Insert scans all graph edges and all isolated vertices - linear per insert, quadratic per
	// build - so this cost tracks road count, not the growth simulation upstream of it.
	UE::Geometry::FArrangement2d& Arrangement = OutArrangement.Storage->Arrangement;
	Arrangement.VertexSnapTol = SnapToleranceCm;
	for (int32 SourceIndex = 0; SourceIndex < OutArrangement.SourceEdges.Num(); ++SourceIndex)
	{
		const FSourceEdge& Source = OutArrangement.SourceEdges[SourceIndex];
		Arrangement.Insert(Source.Start, Source.End, SourceIndex);
	}

	return true;
}

const UE::Geometry::FDynamicGraph2d& GetGraph(const FArrangement& Arrangement)
{
	check(Arrangement.Storage.IsValid());
	return Arrangement.Storage->Arrangement.Graph;
}
}
