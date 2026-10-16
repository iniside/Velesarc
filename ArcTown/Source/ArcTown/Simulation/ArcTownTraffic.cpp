// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcTownTraffic.h"

#include "Simulation/ArcTownDeterministic.h"
#include "Simulation/ArcTownGrowthDiagnostics.h"
#include "Simulation/ArcTownWorldSnapshot.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTownTraffic, Log, All);

namespace ArcTownTrafficPrivate
{
namespace
{
	constexpr double TripVolume = 1.0;
	constexpr double BaseNodeMass = 1.0;
	constexpr double MaxTripsPerYear = 1000000.0;

	struct FAdjacentRoad
	{
		int32 OtherNode = INDEX_NONE;

		int32 EdgeIndex = INDEX_NONE;

		double LengthCm = 0.0;
	};

	struct FFrontierEntry
	{
		double DistanceCm = 0.0;

		int32 NodeIndex = INDEX_NONE;
	};

	struct FFrontierLess
	{
		bool operator()(const FFrontierEntry& Left, const FFrontierEntry& Right) const
		{
			if (Left.DistanceCm != Right.DistanceCm)
			{
				return Left.DistanceCm < Right.DistanceCm;
			}
			return Left.NodeIndex < Right.NodeIndex;
		}
	};

	struct FTrafficRankLess
	{
		const FArcTownWorldSnapshot* Snapshot = nullptr;

		bool operator()(const int32 LeftEdge, const int32 RightEdge) const
		{
			const float LeftTraffic = Snapshot->Edges[LeftEdge].Traffic;
			const float RightTraffic = Snapshot->Edges[RightEdge].Traffic;
			if (LeftTraffic != RightTraffic)
			{
				return LeftTraffic > RightTraffic;
			}
			return LeftEdge < RightEdge;
		}
	};

	FVector2D ToDouble(const FVector2f& Value)
	{
		return FVector2D(static_cast<double>(Value.X), static_cast<double>(Value.Y));
	}

	// Planned roads are routed over: the traffic one attracts is what decides whether it gets built.
	void BuildAdjacency(const FArcTownWorldSnapshot& Snapshot, TArray<TArray<FAdjacentRoad>>& OutAdjacency)
	{
		OutAdjacency.Reset();
		OutAdjacency.SetNum(Snapshot.Nodes.Num());

		for (int32 EdgeIndex = 0; EdgeIndex < Snapshot.Edges.Num(); ++EdgeIndex)
		{
			const FArcTownRoadEdge& Edge = Snapshot.Edges[EdgeIndex];
			if (!Snapshot.Nodes.IsValidIndex(Edge.NodeA)
				|| !Snapshot.Nodes.IsValidIndex(Edge.NodeB)
				|| Edge.NodeA == Edge.NodeB)
			{
				continue;
			}

			const double LengthCm =
				(ToDouble(Snapshot.Nodes[Edge.NodeB].LocalXY) - ToDouble(Snapshot.Nodes[Edge.NodeA].LocalXY)).Size();
			if (!FMath::IsFinite(LengthCm) || LengthCm <= 0.0)
			{
				continue;
			}

			FAdjacentRoad Forward;
			Forward.OtherNode = Edge.NodeB;
			Forward.EdgeIndex = EdgeIndex;
			Forward.LengthCm = LengthCm;
			OutAdjacency[Edge.NodeA].Add(Forward);

			FAdjacentRoad Backward;
			Backward.OtherNode = Edge.NodeA;
			Backward.EdgeIndex = EdgeIndex;
			Backward.LengthCm = LengthCm;
			OutAdjacency[Edge.NodeB].Add(Backward);
		}
	}

	void ComputeShortestPaths(
		const TArray<TArray<FAdjacentRoad>>& Adjacency,
		const int32 OriginNode,
		TArray<double>& OutDistanceCm,
		TArray<int32>& OutPredecessorEdge,
		TArray<bool>& InOutSettled,
		TArray<FFrontierEntry>& InOutFrontier)
	{
		const int32 NodeCount = Adjacency.Num();
		OutDistanceCm.Reset();
		OutDistanceCm.Init(TNumericLimits<double>::Max(), NodeCount);
		OutPredecessorEdge.Reset();
		OutPredecessorEdge.Init(INDEX_NONE, NodeCount);
		InOutSettled.Reset();
		InOutSettled.Init(false, NodeCount);
		InOutFrontier.Reset();

		if (!OutDistanceCm.IsValidIndex(OriginNode))
		{
			return;
		}

		const FFrontierLess Less;
		OutDistanceCm[OriginNode] = 0.0;

		FFrontierEntry Start;
		Start.DistanceCm = 0.0;
		Start.NodeIndex = OriginNode;
		InOutFrontier.HeapPush(Start, Less);

		while (InOutFrontier.Num() > 0)
		{
			FFrontierEntry Current;
			InOutFrontier.HeapPop(Current, Less, EAllowShrinking::No);
			if (InOutSettled[Current.NodeIndex])
			{
				continue;
			}
			InOutSettled[Current.NodeIndex] = true;

			const double CurrentDistanceCm = OutDistanceCm[Current.NodeIndex];
			for (const FAdjacentRoad& Adjacent : Adjacency[Current.NodeIndex])
			{
				if (InOutSettled[Adjacent.OtherNode])
				{
					continue;
				}

				const double CandidateCm = CurrentDistanceCm + Adjacent.LengthCm;
				const double BestCm = OutDistanceCm[Adjacent.OtherNode];
				const bool bImproves = CandidateCm < BestCm
					|| (CandidateCm == BestCm && Adjacent.EdgeIndex < OutPredecessorEdge[Adjacent.OtherNode]);
				if (!bImproves)
				{
					continue;
				}

				OutDistanceCm[Adjacent.OtherNode] = CandidateCm;
				OutPredecessorEdge[Adjacent.OtherNode] = Adjacent.EdgeIndex;

				FFrontierEntry Relaxed;
				Relaxed.DistanceCm = CandidateCm;
				Relaxed.NodeIndex = Adjacent.OtherNode;
				InOutFrontier.HeapPush(Relaxed, Less);
			}
		}
	}

	int32 FindAnchorNode(const FArcTownWorldSnapshot& Snapshot, const FArcTownBuildingRecord& Building)
	{
		const FVector2D BuildingPoint = ToDouble(Building.LocalXY);
		if (Snapshot.Edges.IsValidIndex(Building.EdgeIndex))
		{
			const FArcTownRoadEdge& Edge = Snapshot.Edges[Building.EdgeIndex];
			if (Snapshot.Nodes.IsValidIndex(Edge.NodeA) && Snapshot.Nodes.IsValidIndex(Edge.NodeB))
			{
				const double ToNodeA = (ToDouble(Snapshot.Nodes[Edge.NodeA].LocalXY) - BuildingPoint).SizeSquared();
				const double ToNodeB = (ToDouble(Snapshot.Nodes[Edge.NodeB].LocalXY) - BuildingPoint).SizeSquared();
				return ToNodeB < ToNodeA ? Edge.NodeB : Edge.NodeA;
			}
		}

		int32 NearestNode = INDEX_NONE;
		double NearestDistanceSquared = 0.0;
		for (int32 NodeIndex = 0; NodeIndex < Snapshot.Nodes.Num(); ++NodeIndex)
		{
			const double DistanceSquared = (ToDouble(Snapshot.Nodes[NodeIndex].LocalXY) - BuildingPoint).SizeSquared();
			if (NearestNode == INDEX_NONE || DistanceSquared < NearestDistanceSquared)
			{
				NearestNode = NodeIndex;
				NearestDistanceSquared = DistanceSquared;
			}
		}
		return NearestNode;
	}

	void ComputeNodeMass(const FArcTownWorldSnapshot& Snapshot, TArray<double>& OutMass)
	{
		OutMass.Reset();
		OutMass.Init(BaseNodeMass, Snapshot.Nodes.Num());

		for (const FArcTownBuildingRecord& Building : Snapshot.Buildings)
		{
			const int32 AnchorNode = FindAnchorNode(Snapshot, Building);
			if (OutMass.IsValidIndex(AnchorNode))
			{
				OutMass[AnchorNode] += 1.0;
			}
		}
	}

	void ComputeNodeTraffic(const FArcTownWorldSnapshot& Snapshot, TArray<double>& OutTraffic)
	{
		OutTraffic.Reset();
		OutTraffic.Init(0.0, Snapshot.Nodes.Num());

		for (const FArcTownRoadEdge& Edge : Snapshot.Edges)
		{
			if (!FMath::IsFinite(Edge.Traffic))
			{
				continue;
			}

			const double EdgeTraffic = static_cast<double>(Edge.Traffic);
			if (OutTraffic.IsValidIndex(Edge.NodeA))
			{
				OutTraffic[Edge.NodeA] += EdgeTraffic;
			}
			if (OutTraffic.IsValidIndex(Edge.NodeB))
			{
				OutTraffic[Edge.NodeB] += EdgeTraffic;
			}
		}
	}

	int64 QuantizeTraffic(const double Traffic)
	{
		const double Scaled = FMath::Clamp(Traffic * 1024.0, 0.0, 9.0e15);
		return static_cast<int64>(Scaled);
	}

	double RaceValue(const uint64 Key, const double Weight)
	{
		const double Draw = ArcTownDeterministic::KeyToUnitOpen(Key);
		return -FMath::Loge(Draw) / Weight;
	}

	bool TryGetPromotionTripsPerYear(
		const FArcTownTrafficSettings& TrafficSettings,
		const uint8 TargetHierarchy,
		double& OutTripsPerYear)
	{
		const int32 ClassIndex = static_cast<int32>(TargetHierarchy);
		if (!TrafficSettings.HierarchyPromotionTripsPerYear.IsValidIndex(ClassIndex))
		{
			return false;
		}

		const double AuthoredTrips = TrafficSettings.HierarchyPromotionTripsPerYear[ClassIndex];
		if (!FMath::IsFinite(AuthoredTrips))
		{
			return false;
		}

		OutTripsPerYear = FMath::Max(AuthoredTrips, 0.0);
		return true;
	}

	double TripsAtPercentile(const TArray<double>& AscendingTrips, const double Fraction)
	{
		if (AscendingTrips.IsEmpty())
		{
			return 0.0;
		}

		const int32 Rank = FMath::Clamp(
			FMath::CeilToInt32(Fraction * static_cast<double>(AscendingTrips.Num())) - 1,
			0,
			AscendingTrips.Num() - 1);
		return AscendingTrips[Rank];
	}

	FString FormatHierarchyCounts(const FArcTownWorldSnapshot& Snapshot)
	{
		const int32 ClassCount = static_cast<int32>(ArcTownTraffic::MaxRoadHierarchy) + 1;
		TArray<int32> Counts;
		Counts.Init(0, ClassCount);
		for (const FArcTownRoadEdge& Edge : Snapshot.Edges)
		{
			++Counts[FMath::Clamp(static_cast<int32>(Edge.Hierarchy), 0, ClassCount - 1)];
		}

		FString Text;
		for (int32 ClassIndex = 0; ClassIndex < ClassCount; ++ClassIndex)
		{
			Text += FString::Printf(
				TEXT("%s%d:%d"), ClassIndex == 0 ? TEXT("") : TEXT(", "), ClassIndex, Counts[ClassIndex]);
		}
		return Text;
	}

	void LogTrafficDistribution(
		const int32 Year,
		const FArcTownWorldSnapshot& Snapshot,
		const int32 QualifiedEdges,
		const int32 WidenedEdges)
	{
		if (!UE_LOG_ACTIVE(LogArcTownGrowth, Log))
		{
			return;
		}

		TArray<double> TripsPerYear;
		TripsPerYear.Reserve(Snapshot.Edges.Num());
		for (const FArcTownRoadEdge& Edge : Snapshot.Edges)
		{
			if (FMath::IsFinite(Edge.Traffic) && Edge.Traffic > 0.0f)
			{
				TripsPerYear.Add(static_cast<double>(Edge.Traffic));
			}
		}
		TripsPerYear.Sort();

		UE_LOGF(
			LogArcTownGrowth,
			Log,"ArcTown year %d traffic distribution: %d of %d edge(s) carrying trips; trips/year median %.1f, p75 %.1f, p90 %.1f, p99 %.1f, max %.1f; %d edge(s) cleared their next class threshold and %d widened; hierarchy classes %ls.",
			Year,
			TripsPerYear.Num(),
			Snapshot.Edges.Num(),
			TripsAtPercentile(TripsPerYear, 0.5),
			TripsAtPercentile(TripsPerYear, 0.75),
			TripsAtPercentile(TripsPerYear, 0.9),
			TripsAtPercentile(TripsPerYear, 0.99),
			TripsPerYear.IsEmpty() ? 0.0 : TripsPerYear.Last(),
			QualifiedEdges,
			WidenedEdges,
			*FormatHierarchyCounts(Snapshot));
	}

	int32 ComputeTripCount(const FArcTownTrafficSettings& TrafficSettings, const int32 BuildingCount, const int32 Year)
	{
		const double PerBuilding =
			FMath::IsFinite(TrafficSettings.TripsPerBuildingPerYear) ? FMath::Max(TrafficSettings.TripsPerBuildingPerYear, 0.0) : 0.0;
		const double DerivedTrips = FMath::FloorToDouble(PerBuilding * static_cast<double>(BuildingCount));
		if (DerivedTrips > MaxTripsPerYear)
		{
			UE_LOGF(
				LogArcTownTraffic,
				Warning,"ArcTown year %d derived trip count %.0f exceeds the %.0f-trip cap; capping trips for this year.",
				Year,
				DerivedTrips,
				MaxTripsPerYear);
		}

		const int32 MinTrips = FMath::Max(TrafficSettings.MinTripsPerYear, 0);
		const double UncappedTrips = FMath::Max(static_cast<double>(MinTrips), DerivedTrips);
		return static_cast<int32>(FMath::Clamp(UncappedTrips, 0.0, MaxTripsPerYear));
	}
}
}

double ArcTownTraffic::HalfWidthCmForHierarchy(const FArcTownTrafficSettings& TrafficSettings, const uint8 Hierarchy)
{
	const double LaneWidthCm =
		FMath::IsFinite(TrafficSettings.RoadLaneWidthCm) ? FMath::Max(TrafficSettings.RoadLaneWidthCm, 0.0) : 0.0;
	const int32 LaneIndex = FMath::Min(
		static_cast<int32>(Hierarchy), TrafficSettings.HierarchyLaneCounts.Num() - 1);
	if (!TrafficSettings.HierarchyLaneCounts.IsValidIndex(LaneIndex))
	{
		return 0.5 * LaneWidthCm;
	}

	const double AuthoredLaneCount = TrafficSettings.HierarchyLaneCounts[LaneIndex];
	const double LaneCount = FMath::IsFinite(AuthoredLaneCount) ? FMath::Max(AuthoredLaneCount, 0.0) : 0.0;
	return 0.5 * LaneCount * LaneWidthCm;
}

double ArcTownTraffic::MaxHalfWidthCm(const FArcTownTrafficSettings& TrafficSettings, const double CurrentHalfWidthCm)
{
	const double CurrentCm = FMath::IsFinite(CurrentHalfWidthCm) ? FMath::Max(CurrentHalfWidthCm, 0.0) : 0.0;
	return FMath::Max(CurrentCm, HalfWidthCmForHierarchy(TrafficSettings, MaxRoadHierarchy));
}

void ArcTownTraffic::AccumulateTraffic(
	const FArcTownTrafficSettings& TrafficSettings,
	const int32 Year,
	FArcTownWorldSnapshot& Snapshot,
	FArcTownTrafficYearReport& OutReport)
{
	using namespace ArcTownTrafficPrivate;

	if (Snapshot.Nodes.Num() == 0 || Snapshot.Edges.Num() == 0)
	{
		return;
	}

	const int32 TripCount = ComputeTripCount(TrafficSettings, Snapshot.Buildings.Num(), Year);
	if (TripCount <= 0)
	{
		return;
	}

	TArray<TArray<FAdjacentRoad>> Adjacency;
	BuildAdjacency(Snapshot, Adjacency);

	TArray<double> NodeMass;
	ComputeNodeMass(Snapshot, NodeMass);

	TArray<double> NodeTraffic;
	ComputeNodeTraffic(Snapshot, NodeTraffic);

	TArray<double> AccumulatedTraffic;
	AccumulatedTraffic.Reserve(Snapshot.Edges.Num());
	for (const FArcTownRoadEdge& Edge : Snapshot.Edges)
	{
		AccumulatedTraffic.Add(FMath::IsFinite(Edge.Traffic) ? FMath::Max(static_cast<double>(Edge.Traffic), 0.0) : 0.0);
	}

	const double HalfAttractionCm = FMath::Max(TrafficSettings.HalfAttractionDistanceCm, 1.0);
	const int32 NodeCount = Snapshot.Nodes.Num();

	TArray<double> DistanceCm;
	TArray<int32> PredecessorEdge;
	TArray<bool> Settled;
	TArray<FFrontierEntry> Frontier;

	OutReport.TripsRequested += TripCount;

	for (int32 TripIndex = 0; TripIndex < TripCount; ++TripIndex)
	{
		int32 OriginNode = INDEX_NONE;
		double BestOriginRace = 0.0;
		for (int32 NodeIndex = 0; NodeIndex < NodeCount; ++NodeIndex)
		{
			if (Adjacency[NodeIndex].Num() == 0)
			{
				continue;
			}

			const uint64 OriginKey = ArcTownDeterministic::MakeAnnualKey(
				Snapshot.SimulationId,
				Snapshot.Seed,
				ArcTownDeterministic::EDeterministicDomain::TrafficTripOrigin,
				Year,
				NodeIndex,
				TripIndex,
				QuantizeTraffic(NodeTraffic[NodeIndex]));
			const double Race = RaceValue(OriginKey, NodeMass[NodeIndex]);
			if (OriginNode == INDEX_NONE || Race < BestOriginRace)
			{
				OriginNode = NodeIndex;
				BestOriginRace = Race;
			}
		}

		if (OriginNode == INDEX_NONE)
		{
			const int32 RemainingTrips = TripCount - TripIndex;
			OutReport.TripsWithoutReachableTarget += RemainingTrips;
			UE_LOGF(
				LogArcTownTraffic,
				Warning,"ArcTown year %d abandoned %d of %d requested trips: no road node has adjacency to originate a trip from.",
				Year,
				RemainingTrips,
				TripCount);
			break;
		}

		ComputeShortestPaths(Adjacency, OriginNode, DistanceCm, PredecessorEdge, Settled, Frontier);

		int32 TargetNode = INDEX_NONE;
		double BestTargetRace = 0.0;
		for (int32 NodeIndex = 0; NodeIndex < NodeCount; ++NodeIndex)
		{
			if (NodeIndex == OriginNode || !Settled[NodeIndex])
			{
				continue;
			}

			const double Normalized = DistanceCm[NodeIndex] / HalfAttractionCm;
			const double Weight = NodeMass[NodeIndex] / (1.0 + Normalized * Normalized);
			if (!FMath::IsFinite(Weight) || Weight <= 0.0)
			{
				continue;
			}

			const uint64 TargetKey = ArcTownDeterministic::MakeAnnualKey(
				Snapshot.SimulationId,
				Snapshot.Seed,
				ArcTownDeterministic::EDeterministicDomain::TrafficTripTarget,
				Year,
				NodeIndex,
				TripIndex,
				QuantizeTraffic(NodeTraffic[NodeIndex]));
			const double Race = RaceValue(TargetKey, Weight);
			if (TargetNode == INDEX_NONE || Race < BestTargetRace)
			{
				TargetNode = NodeIndex;
				BestTargetRace = Race;
			}
		}

		if (TargetNode == INDEX_NONE)
		{
			++OutReport.TripsWithoutReachableTarget;
			UE_LOGF(
				LogArcTownTraffic,
				Warning,"ArcTown year %d trip %d starting at road node %d reached no destination; the road network is disconnected there.",
				Year,
				TripIndex,
				OriginNode);
			continue;
		}

		int32 WalkNode = TargetNode;
		while (WalkNode != OriginNode)
		{
			const int32 EdgeIndex = PredecessorEdge[WalkNode];
			if (!AccumulatedTraffic.IsValidIndex(EdgeIndex))
			{
				break;
			}

			AccumulatedTraffic[EdgeIndex] += TripVolume;

			const FArcTownRoadEdge& Edge = Snapshot.Edges[EdgeIndex];
			WalkNode = Edge.NodeA == WalkNode ? Edge.NodeB : Edge.NodeA;
		}

		++OutReport.TripsRouted;
	}

	for (int32 EdgeIndex = 0; EdgeIndex < Snapshot.Edges.Num(); ++EdgeIndex)
	{
		Snapshot.Edges[EdgeIndex].Traffic = static_cast<float>(AccumulatedTraffic[EdgeIndex]);
	}
}

void ArcTownTraffic::PromoteRoads(
	const FArcTownTrafficSettings& TrafficSettings,
	const int32 Year,
	FArcTownWorldSnapshot& Snapshot,
	FArcTownTrafficYearReport& OutReport)
{
	using namespace ArcTownTrafficPrivate;

	TArray<int32> RankedEdges;
	RankedEdges.Reserve(Snapshot.Edges.Num());

	for (int32 EdgeIndex = 0; EdgeIndex < Snapshot.Edges.Num(); ++EdgeIndex)
	{
		FArcTownRoadEdge& Edge = Snapshot.Edges[EdgeIndex];
		if (!FMath::IsFinite(Edge.Traffic))
		{
			continue;
		}

		if (Edge.bBuilt == 0 && static_cast<double>(Edge.Traffic) > TrafficSettings.PromotionThresholdPerYear)
		{
			Edge.bBuilt = 1;
			++OutReport.RoadsPromoted;
		}

		if (Edge.Hierarchy >= MaxRoadHierarchy)
		{
			continue;
		}

		const uint8 TargetHierarchy = static_cast<uint8>(Edge.Hierarchy + 1);
		double TargetTripsPerYear = 0.0;
		if (!TryGetPromotionTripsPerYear(TrafficSettings, TargetHierarchy, TargetTripsPerYear))
		{
			continue;
		}

		if (static_cast<double>(Edge.Traffic) > TargetTripsPerYear)
		{
			RankedEdges.Add(EdgeIndex);
		}
	}

	const double TopPercent = FMath::IsFinite(TrafficSettings.HierarchyPromotionTopPercent)
		? TrafficSettings.HierarchyPromotionTopPercent
		: 0.0;
	const double TopFraction = FMath::Clamp(TopPercent, 0.0, 100.0) * 0.01;

	int32 WidenedEdges = 0;
	if (!RankedEdges.IsEmpty() && TopFraction > 0.0)
	{
		FTrafficRankLess Less;
		Less.Snapshot = &Snapshot;
		RankedEdges.Sort(Less);

		const int32 CutCount = FMath::Clamp(
			FMath::FloorToInt32(static_cast<double>(RankedEdges.Num()) * TopFraction), 1, RankedEdges.Num());

		for (int32 RankIndex = 0; RankIndex < CutCount; ++RankIndex)
		{
			FArcTownRoadEdge& Edge = Snapshot.Edges[RankedEdges[RankIndex]];
			Edge.Hierarchy = static_cast<uint8>(Edge.Hierarchy + 1);
			Edge.HalfWidth = static_cast<float>(FMath::Max(
				static_cast<double>(Edge.HalfWidth), HalfWidthCmForHierarchy(TrafficSettings, Edge.Hierarchy)));
			++WidenedEdges;
		}

		OutReport.RoadsWidened += WidenedEdges;
	}

	LogTrafficDistribution(Year, Snapshot, RankedEdges.Num(), WidenedEdges);
}

// Runs after promotion so a road this year's trips just filled can be built before its traffic is faded.
void ArcTownTraffic::DecayTraffic(const FArcTownTrafficSettings& TrafficSettings, FArcTownWorldSnapshot& Snapshot)
{
	const double Decay =
		FMath::IsFinite(TrafficSettings.TrafficDecayPerYear) ? FMath::Clamp(TrafficSettings.TrafficDecayPerYear, 0.0, 1.0) : 0.0;
	const double Retained = 1.0 - Decay;

	for (FArcTownRoadEdge& Edge : Snapshot.Edges)
	{
		const double Faded = FMath::IsFinite(Edge.Traffic) ? static_cast<double>(Edge.Traffic) * Retained : 0.0;
		Edge.Traffic = static_cast<float>(FMath::Max(Faded, 0.0));
	}
}
