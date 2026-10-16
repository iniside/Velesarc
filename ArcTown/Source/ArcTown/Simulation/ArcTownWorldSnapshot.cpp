// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcTownWorldSnapshot.h"

#include "Simulation/ArcTownDeterministic.h"
#include "Town/ArcTownSiteSnapshot.h"

#define LOCTEXT_NAMESPACE "ArcTownWorldSnapshot"

namespace ArcTownWorldSnapshotPrivate
{
	constexpr uint64 StateChecksumKey = 0x4F6C1D97B3A5E2C7ull;

	void AddFloat(ArcTownDeterministic::FKeyedHash64& Hash, float Value)
	{
		if (Value == 0.0f)
		{
			Value = 0.0f;
		}

		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		Hash.AddUInt32(Bits);
	}

	void AddDouble(ArcTownDeterministic::FKeyedHash64& Hash, double Value)
	{
		if (Value == 0.0)
		{
			Value = 0.0;
		}

		uint64 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		Hash.AddUInt64(Bits);
	}

	void NormalizeSignedZero(float& Value)
	{
		if (Value == 0.0f)
		{
			Value = 0.0f;
		}
	}

	void NormalizeSignedZero(double& Value)
	{
		if (Value == 0.0)
		{
			Value = 0.0;
		}
	}

	bool NodeLess(const FArcTownRoadNode& Left, const FArcTownRoadNode& Right)
	{
		if (Left.LocalXY.X != Right.LocalXY.X)
		{
			return Left.LocalXY.X < Right.LocalXY.X;
		}
		if (Left.LocalXY.Y != Right.LocalXY.Y)
		{
			return Left.LocalXY.Y < Right.LocalXY.Y;
		}
		if (Left.bFinished != Right.bFinished)
		{
			return Left.bFinished < Right.bFinished;
		}
		return Left.FailedExpansions < Right.FailedExpansions;
	}

	bool EdgeLess(const FArcTownRoadEdge& Left, const FArcTownRoadEdge& Right)
	{
		if (Left.NodeA != Right.NodeA)
		{
			return Left.NodeA < Right.NodeA;
		}
		if (Left.NodeB != Right.NodeB)
		{
			return Left.NodeB < Right.NodeB;
		}
		if (Left.bBuilt != Right.bBuilt)
		{
			return Left.bBuilt < Right.bBuilt;
		}
		if (Left.Hierarchy != Right.Hierarchy)
		{
			return Left.Hierarchy < Right.Hierarchy;
		}
		if (Left.bBridge != Right.bBridge)
		{
			return Left.bBridge < Right.bBridge;
		}
		if (Left.HalfWidth != Right.HalfWidth)
		{
			return Left.HalfWidth < Right.HalfWidth;
		}
		if (Left.Traffic != Right.Traffic)
		{
			return Left.Traffic < Right.Traffic;
		}
		return Left.RoadId < Right.RoadId;
	}

	bool BuildingLess(const FArcTownBuildingRecord& Left, const FArcTownBuildingRecord& Right)
	{
		if (Left.LocalXY.X != Right.LocalXY.X)
		{
			return Left.LocalXY.X < Right.LocalXY.X;
		}
		if (Left.LocalXY.Y != Right.LocalXY.Y)
		{
			return Left.LocalXY.Y < Right.LocalXY.Y;
		}
		if (Left.CatalogIndex != Right.CatalogIndex)
		{
			return Left.CatalogIndex < Right.CatalogIndex;
		}
		if (Left.BirthYear != Right.BirthYear)
		{
			return Left.BirthYear < Right.BirthYear;
		}
		if (Left.EdgeIndex != Right.EdgeIndex)
		{
			return Left.EdgeIndex < Right.EdgeIndex;
		}
		return Left.Yaw < Right.Yaw;
	}

	bool BranchSeedLess(const FArcTownRoadBranchSeed& Left, const FArcTownRoadBranchSeed& Right)
	{
		if (Left.NodeIndex != Right.NodeIndex)
		{
			return Left.NodeIndex < Right.NodeIndex;
		}
		if (Left.HeadingRad != Right.HeadingRad)
		{
			return Left.HeadingRad < Right.HeadingRad;
		}
		return Left.bFollowsMinorField < Right.bFollowsMinorField;
	}

	template <typename RecordType, typename LessType>
	void SortRecords(TArray<RecordType>& Records, LessType Less, TArray<int32>* OutOldToNew = nullptr)
	{
		const int32 Count = Records.Num();

		TArray<int32> Order;
		Order.SetNumUninitialized(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Order[Index] = Index;
		}

		Order.Sort(
			[&Records, &Less](const int32 LeftIndex, const int32 RightIndex)
			{
				if (Less(Records[LeftIndex], Records[RightIndex]))
				{
					return true;
				}
				if (Less(Records[RightIndex], Records[LeftIndex]))
				{
					return false;
				}
				return LeftIndex < RightIndex;
			});

		TArray<RecordType> Sorted;
		Sorted.Reserve(Count);
		if (OutOldToNew != nullptr)
		{
			OutOldToNew->SetNumUninitialized(Count);
		}
		for (int32 NewIndex = 0; NewIndex < Count; ++NewIndex)
		{
			const int32 OldIndex = Order[NewIndex];
			if (OutOldToNew != nullptr)
			{
				(*OutOldToNew)[OldIndex] = NewIndex;
			}
			Sorted.Add(Records[OldIndex]);
		}

		Records = MoveTemp(Sorted);
	}

	int32 Remap(const TArray<int32>& OldToNew, const int32 OldIndex)
	{
		return OldToNew.IsValidIndex(OldIndex) ? OldToNew[OldIndex] : INDEX_NONE;
	}

	template <typename RecordType, typename LessType>
	bool IsOrdered(const TArray<RecordType>& Records, LessType Less)
	{
		for (int32 Index = 1; Index < Records.Num(); ++Index)
		{
			if (Less(Records[Index], Records[Index - 1]))
			{
				return false;
			}
		}
		return true;
	}

	uint64 ComputeChecksum(const FArcTownWorldSnapshot& Snapshot)
	{
		ArcTownDeterministic::FKeyedHash64 Hash(StateChecksumKey);
		Hash.AddUInt32(Snapshot.RuleVersion);
		Hash.AddGuid(Snapshot.SimulationId);
		Hash.AddInt32(Snapshot.Seed);
		Hash.AddUInt64(Snapshot.SiteSnapshotHash);
		Hash.AddUInt64(Snapshot.SettingsHash);
		Hash.AddInt32(Snapshot.Year);
		Hash.AddInt32(Snapshot.NextRoadId);

		Hash.AddInt32(Snapshot.Nodes.Num());
		for (const FArcTownRoadNode& Node : Snapshot.Nodes)
		{
			AddFloat(Hash, Node.LocalXY.X);
			AddFloat(Hash, Node.LocalXY.Y);
			Hash.AddUInt8(Node.bFinished);
			Hash.AddInt32(Node.FailedExpansions);
		}

		Hash.AddInt32(Snapshot.Edges.Num());
		for (const FArcTownRoadEdge& Edge : Snapshot.Edges)
		{
			Hash.AddInt32(Edge.NodeA);
			Hash.AddInt32(Edge.NodeB);
			Hash.AddInt32(Edge.RoadId);
			Hash.AddUInt8(Edge.bBuilt);
			Hash.AddUInt8(Edge.Hierarchy);
			Hash.AddUInt8(Edge.bBridge);
			AddFloat(Hash, Edge.HalfWidth);
			AddFloat(Hash, Edge.Traffic);
		}

		Hash.AddInt32(Snapshot.Buildings.Num());
		for (const FArcTownBuildingRecord& Building : Snapshot.Buildings)
		{
			Hash.AddInt32(Building.CatalogIndex);
			AddFloat(Hash, Building.LocalXY.X);
			AddFloat(Hash, Building.LocalXY.Y);
			AddFloat(Hash, Building.Yaw);
			Hash.AddInt32(Building.BirthYear);
			Hash.AddInt32(Building.EdgeIndex);
		}

		Hash.AddInt32(Snapshot.PendingBranchSeeds.Num());
		for (const FArcTownRoadBranchSeed& Seed : Snapshot.PendingBranchSeeds)
		{
			Hash.AddInt32(Seed.NodeIndex);
			AddDouble(Hash, Seed.HeadingRad);
			Hash.AddUInt8(Seed.bFollowsMinorField);
		}

		return Hash.Finalize();
	}

	float SquaredDistanceToSegment(const FVector2f& Point, const FVector2f& Start, const FVector2f& End)
	{
		const FVector2f Span = End - Start;
		const float LengthSquared = Span.X * Span.X + Span.Y * Span.Y;
		if (LengthSquared <= 0.0f)
		{
			return (Point - Start).SizeSquared();
		}

		const FVector2f ToPoint = Point - Start;
		const float Parameter = FMath::Clamp((ToPoint.X * Span.X + ToPoint.Y * Span.Y) / LengthSquared, 0.0f, 1.0f);
		return (Point - (Start + Span * Parameter)).SizeSquared();
	}

	bool IsFinite(const FVector2f& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
	}

	bool IsIndistinguishablePosition(const FVector2f& Left, const FVector2f& Right)
	{
		const float Magnitude = FMath::Max(
			FMath::Max(FMath::Abs(Left.X), FMath::Abs(Left.Y)),
			FMath::Max(FMath::Abs(Right.X), FMath::Abs(Right.Y)));
		const float Tolerance = Magnitude * FLT_EPSILON;
		return FMath::Abs(Left.X - Right.X) <= Tolerance && FMath::Abs(Left.Y - Right.Y) <= Tolerance;
	}
}

int32 FArcTownWorldSnapshot::SplitEdgeAtPoint(const int32 EdgeIndex, const FVector2f LocalXY)
{
	using namespace ArcTownWorldSnapshotPrivate;

	if (!Edges.IsValidIndex(EdgeIndex) || !IsFinite(LocalXY))
	{
		return INDEX_NONE;
	}

	const FArcTownRoadEdge Original = Edges[EdgeIndex];
	if (!Nodes.IsValidIndex(Original.NodeA) || !Nodes.IsValidIndex(Original.NodeB))
	{
		return INDEX_NONE;
	}

	const FVector2f Start = Nodes[Original.NodeA].LocalXY;
	const FVector2f End = Nodes[Original.NodeB].LocalXY;
	if (!IsFinite(Start) || !IsFinite(End))
	{
		return INDEX_NONE;
	}

	const FVector2f Span = End - Start;
	const float LengthSquared = Span.X * Span.X + Span.Y * Span.Y;
	if (LengthSquared <= 0.0f)
	{
		return Original.NodeA;
	}

	const FVector2f ToPoint = LocalXY - Start;
	const float Parameter = (ToPoint.X * Span.X + ToPoint.Y * Span.Y) / LengthSquared;
	if (!FMath::IsFinite(Parameter))
	{
		return INDEX_NONE;
	}
	if (Parameter <= 0.0f)
	{
		return Original.NodeA;
	}
	if (Parameter >= 1.0f)
	{
		return Original.NodeB;
	}

	const FVector2f SplitPosition = Start + Span * Parameter;
	if (IsIndistinguishablePosition(SplitPosition, Start))
	{
		return Original.NodeA;
	}
	if (IsIndistinguishablePosition(SplitPosition, End))
	{
		return Original.NodeB;
	}

	FArcTownRoadNode SplitNode;
	SplitNode.LocalXY = SplitPosition;
	const int32 SplitNodeIndex = Nodes.Add(SplitNode);

	Edges[EdgeIndex].NodeB = SplitNodeIndex;

	FArcTownRoadEdge SecondHalf = Original;
	SecondHalf.NodeA = SplitNodeIndex;
	SecondHalf.NodeB = Original.NodeB;
	const int32 SecondHalfIndex = Edges.Add(SecondHalf);

	const FVector2f SplitLocation = Nodes[SplitNodeIndex].LocalXY;
	for (FArcTownBuildingRecord& Building : Buildings)
	{
		if (Building.EdgeIndex != EdgeIndex || !IsFinite(Building.LocalXY))
		{
			continue;
		}

		const float FirstHalfDistance = SquaredDistanceToSegment(Building.LocalXY, Start, SplitLocation);
		const float SecondHalfDistance = SquaredDistanceToSegment(Building.LocalXY, SplitLocation, End);
		if (SecondHalfDistance < FirstHalfDistance)
		{
			Building.EdgeIndex = SecondHalfIndex;
		}
	}

	return SplitNodeIndex;
}

void FArcTownWorldSnapshot::CanonicalizeAndRefreshChecksum()
{
	using namespace ArcTownWorldSnapshotPrivate;

	TArray<int32> NodeOldToNew;
	SortRecords(Nodes, &NodeLess, &NodeOldToNew);

	for (FArcTownRoadEdge& Edge : Edges)
	{
		Edge.NodeA = Remap(NodeOldToNew, Edge.NodeA);
		Edge.NodeB = Remap(NodeOldToNew, Edge.NodeB);
	}

	for (FArcTownRoadBranchSeed& BranchSeed : PendingBranchSeeds)
	{
		BranchSeed.NodeIndex = Remap(NodeOldToNew, BranchSeed.NodeIndex);
	}
	PendingBranchSeeds.RemoveAll(
		[](const FArcTownRoadBranchSeed& BranchSeed)
		{
			return BranchSeed.NodeIndex == INDEX_NONE || !FMath::IsFinite(BranchSeed.HeadingRad);
		});

	SortRecords(PendingBranchSeeds, &BranchSeedLess);

	TArray<int32> EdgeOldToNew;
	SortRecords(Edges, &EdgeLess, &EdgeOldToNew);

	for (FArcTownBuildingRecord& Building : Buildings)
	{
		Building.EdgeIndex = Remap(EdgeOldToNew, Building.EdgeIndex);
	}

	SortRecords(Buildings, &BuildingLess);

	for (FArcTownRoadNode& Node : Nodes)
	{
		NormalizeSignedZero(Node.LocalXY.X);
		NormalizeSignedZero(Node.LocalXY.Y);
	}
	for (FArcTownRoadEdge& Edge : Edges)
	{
		NormalizeSignedZero(Edge.HalfWidth);
		NormalizeSignedZero(Edge.Traffic);
	}
	for (FArcTownBuildingRecord& Building : Buildings)
	{
		NormalizeSignedZero(Building.LocalXY.X);
		NormalizeSignedZero(Building.LocalXY.Y);
		NormalizeSignedZero(Building.Yaw);
	}
	for (FArcTownRoadBranchSeed& BranchSeed : PendingBranchSeeds)
	{
		NormalizeSignedZero(BranchSeed.HeadingRad);
	}

	StateChecksum = ComputeChecksum(*this);
}

bool FArcTownWorldSnapshot::Validate(const FArcTownSiteSnapshot& Site, FText& OutError) const
{
	using namespace ArcTownWorldSnapshotPrivate;

	if (!Site.IsValid())
	{
		OutError = LOCTEXT("UnusableSite", "This town's sampled terrain is missing or unusable. Sample Site, then simulate again.");
		return false;
	}

	if (RuleVersion != ArcTownDeterministic::SimulationRuleVersion)
	{
		OutError = FText::Format(
			LOCTEXT("InvalidRuleVersion", "This town's simulation was produced by rule version {0}, and the current rules are version {1}. Reset Simulation to start over."),
			FText::AsNumber(static_cast<int32>(RuleVersion)),
			FText::AsNumber(static_cast<int32>(ArcTownDeterministic::SimulationRuleVersion)));
		return false;
	}

	if (!SimulationId.IsValid())
	{
		OutError = LOCTEXT("InvalidSimulationId", "This town's simulation state carries no simulation id. Reset Simulation to start over.");
		return false;
	}

	if (Year < 0)
	{
		OutError = FText::Format(
			LOCTEXT("InvalidYear", "This town's simulation year is {0}. Reset Simulation to start over."),
			FText::AsNumber(Year));
		return false;
	}

	double LocalExtentBoundCm = 0.0;
	if (Nodes.Num() > 0 || Buildings.Num() > 0)
	{
		const FIntPoint SiteDims = Site.GetSiteDims();
		const double SpanX = static_cast<double>(SiteDims.X - 1) * Site.SampleSpacing;
		const double SpanY = static_cast<double>(SiteDims.Y - 1) * Site.SampleSpacing;
		LocalExtentBoundCm = FMath::Max(SpanX, SpanY);
		if (!FMath::IsFinite(LocalExtentBoundCm) || LocalExtentBoundCm <= 0.0)
		{
			OutError = LOCTEXT("DegenerateSite", "This town's sampled terrain has no extent. Sample Site, then simulate again.");
			return false;
		}
	}

	for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
	{
		const FArcTownRoadNode& Node = Nodes[NodeIndex];
		if (!IsFinite(Node.LocalXY)
			|| FMath::Abs(static_cast<double>(Node.LocalXY.X)) > LocalExtentBoundCm
			|| FMath::Abs(static_cast<double>(Node.LocalXY.Y)) > LocalExtentBoundCm)
		{
			OutError = FText::Format(
				LOCTEXT("NodeOutsideSite", "Road node {0} sits at ({1}, {2}) cm, outside the sampled terrain. Reset Simulation to start over."),
				FText::AsNumber(NodeIndex),
				FText::AsNumber(Node.LocalXY.X),
				FText::AsNumber(Node.LocalXY.Y));
			return false;
		}

		if (Node.FailedExpansions < 0)
		{
			OutError = FText::Format(
				LOCTEXT("NodeFailedExpansions", "Road node {0} carries a failed-expansion count of {1}, which is not a count of attempts. Reset Simulation to start over."),
				FText::AsNumber(NodeIndex),
				FText::AsNumber(Node.FailedExpansions));
			return false;
		}
	}

	for (int32 EdgeIndex = 0; EdgeIndex < Edges.Num(); ++EdgeIndex)
	{
		const FArcTownRoadEdge& Edge = Edges[EdgeIndex];
		if (!Nodes.IsValidIndex(Edge.NodeA) || !Nodes.IsValidIndex(Edge.NodeB) || Edge.NodeA == Edge.NodeB)
		{
			OutError = FText::Format(
				LOCTEXT("EdgeEndpoints", "Road {0} connects nodes {1} and {2}, which is not a pair of distinct road nodes. Reset Simulation to start over."),
				FText::AsNumber(EdgeIndex),
				FText::AsNumber(Edge.NodeA),
				FText::AsNumber(Edge.NodeB));
			return false;
		}

		if (!FMath::IsFinite(Edge.HalfWidth) || Edge.HalfWidth <= 0.0f || !FMath::IsFinite(Edge.Traffic) || Edge.Traffic < 0.0f)
		{
			OutError = FText::Format(
				LOCTEXT("EdgeAttributes", "Road {0} has half width {1} cm and traffic {2}. Reset Simulation to start over."),
				FText::AsNumber(EdgeIndex),
				FText::AsNumber(Edge.HalfWidth),
				FText::AsNumber(Edge.Traffic));
			return false;
		}
	}

	for (int32 BuildingIndex = 0; BuildingIndex < Buildings.Num(); ++BuildingIndex)
	{
		const FArcTownBuildingRecord& Building = Buildings[BuildingIndex];
		if (Building.CatalogIndex < 0)
		{
			OutError = FText::Format(
				LOCTEXT("BuildingCatalogIndex", "Building {0} references catalog entry {1}. Reset Simulation to start over."),
				FText::AsNumber(BuildingIndex),
				FText::AsNumber(Building.CatalogIndex));
			return false;
		}

		if (Building.EdgeIndex != INDEX_NONE && !Edges.IsValidIndex(Building.EdgeIndex))
		{
			OutError = FText::Format(
				LOCTEXT("BuildingEdgeIndex", "Building {0} fronts road {1}, which does not exist. Reset Simulation to start over."),
				FText::AsNumber(BuildingIndex),
				FText::AsNumber(Building.EdgeIndex));
			return false;
		}

		if (!IsFinite(Building.LocalXY)
			|| !FMath::IsFinite(Building.Yaw)
			|| FMath::Abs(static_cast<double>(Building.LocalXY.X)) > LocalExtentBoundCm
			|| FMath::Abs(static_cast<double>(Building.LocalXY.Y)) > LocalExtentBoundCm)
		{
			OutError = FText::Format(
				LOCTEXT("BuildingOutsideSite", "Building {0} sits at ({1}, {2}) cm, outside the sampled terrain. Reset Simulation to start over."),
				FText::AsNumber(BuildingIndex),
				FText::AsNumber(Building.LocalXY.X),
				FText::AsNumber(Building.LocalXY.Y));
			return false;
		}
	}

	for (int32 SeedIndex = 0; SeedIndex < PendingBranchSeeds.Num(); ++SeedIndex)
	{
		const FArcTownRoadBranchSeed& BranchSeed = PendingBranchSeeds[SeedIndex];
		if (!Nodes.IsValidIndex(BranchSeed.NodeIndex) || !FMath::IsFinite(BranchSeed.HeadingRad))
		{
			OutError = FText::Format(
				LOCTEXT("BranchSeedInvalid", "Pending branch seed {0} leaves node {1} on heading {2}, which is not a road node and a finite heading. Reset Simulation to start over."),
				FText::AsNumber(SeedIndex),
				FText::AsNumber(BranchSeed.NodeIndex),
				FText::AsNumber(BranchSeed.HeadingRad));
			return false;
		}
	}

	if (!IsOrdered(Nodes, &NodeLess)
		|| !IsOrdered(Edges, &EdgeLess)
		|| !IsOrdered(Buildings, &BuildingLess)
		|| !IsOrdered(PendingBranchSeeds, &BranchSeedLess))
	{
		OutError = LOCTEXT("NotCanonical", "This town's simulation state is not in canonical order. Reset Simulation to start over.");
		return false;
	}

	if (StateChecksum != ComputeChecksum(*this))
	{
		OutError = LOCTEXT("ChecksumMismatch", "This town's simulation state does not match its checksum. Reset Simulation to start over.");
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
