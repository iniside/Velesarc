// Copyright Lukasz Baran. All Rights Reserved.

#include "Layout/ArcTownLayout.h"

#include "Curve/DynamicGraph2.h"
#include "Layout/ArcTownRoadArrangement.h"
#include "Layout/ArcTownRoadArrangementGraph.h"
#include "Simulation/ArcTownWorldSnapshot.h"
#include "Town/ArcTownSiteSnapshot.h"

namespace ArcTownLayoutPrivate
{
namespace
{
	constexpr double SnapToleranceInSamples = 0.1;

	constexpr int32 ChaikinIterations = 2;

	using ArcTownRoadArrangement::FSourceEdge;
	using ArcTownRoadArrangement::FSegmentWidth;
	using ArcTownRoadArrangement::FindWidthNear;

	FVector2f ToFloat(const FVector2d& Value)
	{
		return FVector2f(static_cast<float>(Value.X), static_cast<float>(Value.Y));
	}

	int32 NextChainEdge(const UE::Geometry::FDynamicGraph2d& Graph, const int32 VID, const int32 IncomingEID)
	{
		for (const int32 EID : Graph.VtxEdgesItr(VID))
		{
			if (EID != IncomingEID)
			{
				return EID;
			}
		}

		return UE::Geometry::FDynamicGraph2d::InvalidID;
	}

	void TraceChain(
		const UE::Geometry::FDynamicGraph2d& Graph,
		const int32 StartVID,
		const int32 StartEID,
		TBitArray<>& VisitedEdges,
		TArray<int32>& OutChain)
	{
		OutChain.Reset();
		OutChain.Add(StartVID);

		int32 CurrentVID = StartVID;
		int32 CurrentEID = StartEID;
		while (CurrentEID != UE::Geometry::FDynamicGraph2d::InvalidID && !VisitedEdges[CurrentEID])
		{
			VisitedEdges[CurrentEID] = true;

			const UE::Geometry::FIndex2i EdgeVerts = Graph.GetEdgeV(CurrentEID);
			const int32 NextVID = EdgeVerts.A == CurrentVID ? EdgeVerts.B : EdgeVerts.A;
			OutChain.Add(NextVID);

			if (NextVID == StartVID || Graph.GetVtxEdgeCount(NextVID) != 2)
			{
				break;
			}

			CurrentVID = NextVID;
			CurrentEID = NextChainEdge(Graph, NextVID, CurrentEID);
		}
	}

	// Chaikin runs after planarization: it moves points, so smoothing first would drag the
	// intersections off the source edges the widths are matched against.
	void SmoothChaikin(TArray<FVector2f>& Points, TArray<float>& HalfWidth, const bool bClosed)
	{
		if (Points.Num() < 3)
		{
			return;
		}

		TArray<FVector2f> SmoothedPoints;
		TArray<float> SmoothedWidth;
		for (int32 Iteration = 0; Iteration < ChaikinIterations; ++Iteration)
		{
			SmoothedPoints.Reset(2 * Points.Num());
			SmoothedWidth.Reset(2 * HalfWidth.Num());

			if (!bClosed)
			{
				SmoothedPoints.Add(Points[0]);
				SmoothedWidth.Add(HalfWidth[0]);
			}

			const int32 SegmentCount = bClosed ? Points.Num() : Points.Num() - 1;
			for (int32 Index = 0; Index < SegmentCount; ++Index)
			{
				const int32 NextIndex = Index + 1 == Points.Num() ? 0 : Index + 1;
				const FVector2f& Start = Points[Index];
				const FVector2f& End = Points[NextIndex];
				SmoothedPoints.Add(Start * 0.75f + End * 0.25f);
				SmoothedPoints.Add(Start * 0.25f + End * 0.75f);

				const float StartWidth = HalfWidth[Index];
				const float EndWidth = HalfWidth[NextIndex];
				SmoothedWidth.Add(StartWidth * 0.75f + EndWidth * 0.25f);
				SmoothedWidth.Add(StartWidth * 0.25f + EndWidth * 0.75f);
			}

			if (!bClosed)
			{
				SmoothedPoints.Add(Points.Last());
				SmoothedWidth.Add(HalfWidth.Last());
			}

			Points = SmoothedPoints;
			HalfWidth = SmoothedWidth;
		}
	}

	void EmitChain(
		const UE::Geometry::FDynamicGraph2d& Graph,
		TConstArrayView<FSourceEdge> SourceEdges,
		const double SnapTolerance,
		TConstArrayView<int32> Chain,
		TArray<FArcTownGeneratedPath>& OutPaths,
		TMap<int32, uint8>& OutNodeHierarchy)
	{
		if (Chain.Num() < 2)
		{
			return;
		}

		FArcTownGeneratedPath Path;
		Path.bClosed = Chain.Num() >= 3 && Chain[0] == Chain.Last();
		if (!Path.bClosed)
		{
			Path.StartNodeId = Chain[0];
			Path.EndNodeId = Chain.Last();
		}

		const int32 NumPoints = Path.bClosed ? Chain.Num() - 1 : Chain.Num();
		Path.Points.Reserve(NumPoints);
		Path.HalfWidth.SetNumZeroed(NumPoints);

		for (int32 Index = 0; Index < NumPoints; ++Index)
		{
			Path.Points.Add(ToFloat(Graph.GetVertex(Chain[Index])));
		}

		const int32 SegmentCount = Path.bClosed ? NumPoints : NumPoints - 1;
		for (int32 Index = 0; Index < SegmentCount; ++Index)
		{
			const int32 NextIndex = Index + 1 == NumPoints ? 0 : Index + 1;
			const FVector2d Start = Graph.GetVertex(Chain[Index]);
			const FVector2d End = Graph.GetVertex(Chain[NextIndex]);
			const FSegmentWidth Width = FindWidthNear(SourceEdges, 0.5 * (Start + End), SnapTolerance);

			Path.HalfWidth[Index] = FMath::Max(Path.HalfWidth[Index], Width.HalfWidth);
			Path.HalfWidth[NextIndex] = FMath::Max(Path.HalfWidth[NextIndex], Width.HalfWidth);
			Path.Hierarchy = FMath::Max(Path.Hierarchy, Width.Hierarchy);
		}

		uint8& StartHierarchy = OutNodeHierarchy.FindOrAdd(Chain[0]);
		StartHierarchy = FMath::Max(StartHierarchy, Path.Hierarchy);
		uint8& EndHierarchy = OutNodeHierarchy.FindOrAdd(Chain.Last());
		EndHierarchy = FMath::Max(EndHierarchy, Path.Hierarchy);

		SmoothChaikin(Path.Points, Path.HalfWidth, Path.bClosed);
		OutPaths.Add(MoveTemp(Path));
	}

	void ExtractPaths(
		const UE::Geometry::FDynamicGraph2d& Graph,
		TConstArrayView<FSourceEdge> SourceEdges,
		const double SnapTolerance,
		TArray<FArcTownGeneratedPath>& OutPaths,
		TArray<FArcTownGeneratedJunction>& OutJunctions)
	{
		TBitArray<> VisitedEdges(false, Graph.MaxEdgeID());
		TArray<int32> IncidentEdges;
		TArray<int32> Chain;
		TMap<int32, uint8> NodeHierarchy;

		for (const int32 VID : Graph.VertexIndices())
		{
			const int32 EdgeCount = Graph.GetVtxEdgeCount(VID);
			if (EdgeCount == 2 || EdgeCount == 0)
			{
				continue;
			}

			// Valence is the graph's alone: a dropped path or a loop back to the vertex undercounts its arms.
			FArcTownGeneratedJunction& Junction = OutJunctions.AddDefaulted_GetRef();
			Junction.NodeId = VID;
			Junction.LocalXY = ToFloat(Graph.GetVertex(VID));
			Junction.Valence = static_cast<uint8>(FMath::Min(EdgeCount, static_cast<int32>(MAX_uint8)));

			IncidentEdges.Reset();
			Graph.SortedVtxEdges(VID, IncidentEdges);
			for (const int32 EID : IncidentEdges)
			{
				if (VisitedEdges[EID])
				{
					continue;
				}

				TraceChain(Graph, VID, EID, VisitedEdges, Chain);
				EmitChain(Graph, SourceEdges, SnapTolerance, Chain, OutPaths, NodeHierarchy);
			}
		}

		for (const int32 EID : Graph.EdgeIndices())
		{
			if (VisitedEdges[EID])
			{
				continue;
			}

			TraceChain(Graph, Graph.GetEdgeV(EID).A, EID, VisitedEdges, Chain);
			EmitChain(Graph, SourceEdges, SnapTolerance, Chain, OutPaths, NodeHierarchy);
		}

		for (FArcTownGeneratedJunction& Junction : OutJunctions)
		{
			if (const uint8* const Hierarchy = NodeHierarchy.Find(Junction.NodeId))
			{
				Junction.Hierarchy = *Hierarchy;
			}
		}
	}

	void PlaceBuildings(const FArcTownWorldSnapshot& Snapshot, TArray<FArcTownPlacedBuilding>& OutPlaced)
	{
		OutPlaced.Reserve(Snapshot.Buildings.Num());
		for (const FArcTownBuildingRecord& Record : Snapshot.Buildings)
		{
			FArcTownPlacedBuilding& Placed = OutPlaced.AddDefaulted_GetRef();
			Placed.CatalogIndex = Record.CatalogIndex;
			Placed.LocalXform = FTransform(
				FRotator(0.0, FMath::RadiansToDegrees(static_cast<double>(Record.Yaw)), 0.0),
				FVector(static_cast<double>(Record.LocalXY.X), static_cast<double>(Record.LocalXY.Y), 0.0));
		}
	}
}
}

bool ArcTownLayout::BuildBuildingsFromSnapshot(
	const FArcTownWorldSnapshot& Snapshot,
	const FArcTownSiteSnapshot& Site,
	TArray<FArcTownPlacedBuilding>& OutPlaced,
	FText& OutError)
{
	using namespace ArcTownLayoutPrivate;

	OutPlaced.Reset();
	OutError = FText::GetEmpty();

	if (!Snapshot.Validate(Site, OutError))
	{
		return false;
	}

	PlaceBuildings(Snapshot, OutPlaced);
	return true;
}

bool ArcTownLayout::BuildPathsFromSnapshot(
	const FArcTownWorldSnapshot& Snapshot,
	const FArcTownSiteSnapshot& Site,
	TArray<FArcTownGeneratedPath>& OutPaths,
	TArray<FArcTownGeneratedJunction>& OutJunctions,
	FText& OutError)
{
	using namespace ArcTownLayoutPrivate;

	OutPaths.Reset();
	OutJunctions.Reset();
	OutError = FText::GetEmpty();

	if (!Snapshot.Validate(Site, OutError))
	{
		return false;
	}

	const double SnapToleranceCm = SnapToleranceInSamples * Site.SampleSpacing;

	ArcTownRoadArrangement::FArrangement Arrangement;
	if (!ArcTownRoadArrangement::Build(Snapshot, SnapToleranceCm, Arrangement))
	{
		return true;
	}

	ExtractPaths(
		ArcTownRoadArrangement::GetGraph(Arrangement), Arrangement.SourceEdges, SnapToleranceCm, OutPaths, OutJunctions);
	return true;
}
