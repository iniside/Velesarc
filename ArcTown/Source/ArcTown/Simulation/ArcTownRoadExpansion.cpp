// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcTownRoadExpansion.h"

#include "Simulation/ArcTownBootstrap.h"
#include "Simulation/ArcTownDeterministic.h"
#include "Simulation/ArcTownFieldSettings.h"
#include "Simulation/ArcTownGrowthDiagnostics.h"
#include "Simulation/ArcTownSiteFields.h"
#include "Simulation/ArcTownTensorField.h"
#include "Simulation/ArcTownWorldSnapshot.h"
#include "Town/ArcTownSiteSnapshot.h"

namespace ArcTownRoadExpansionPrivate
{
namespace
{
	constexpr double ParameterEpsilon = 1.0e-6;

	constexpr double AngleQuanta = 1024.0;

	constexpr double SnapRadiusPerStep = 0.4;

	bool SetError(FText& OutError, const FString& Message)
	{
		OutError = FText::FromString(Message);
		return false;
	}

	FVector2D ToDouble(const FVector2f& Value)
	{
		return FVector2D(static_cast<double>(Value.X), static_cast<double>(Value.Y));
	}

	FVector2f ToFloat(const FVector2D& Value)
	{
		return FVector2f(static_cast<float>(Value.X), static_cast<float>(Value.Y));
	}

	FVector2f HeadingVector(const double AngleRad)
	{
		return FVector2f(static_cast<float>(FMath::Cos(AngleRad)), static_cast<float>(FMath::Sin(AngleRad)));
	}

	int64 QuantizeAngle(const double AngleRad)
	{
		return static_cast<int64>(FMath::RoundToDouble(FMath::UnwindRadians(AngleRad) * AngleQuanta));
	}

	void ComputeValence(const FArcTownWorldSnapshot& Snapshot, TArray<int32>& OutValence)
	{
		OutValence.Reset();
		OutValence.SetNumZeroed(Snapshot.Nodes.Num());
		for (const FArcTownRoadEdge& Edge : Snapshot.Edges)
		{
			if (OutValence.IsValidIndex(Edge.NodeA))
			{
				++OutValence[Edge.NodeA];
			}
			if (OutValence.IsValidIndex(Edge.NodeB))
			{
				++OutValence[Edge.NodeB];
			}
		}
	}

	bool IsIncident(const FArcTownRoadEdge& Edge, const int32 NodeIndex)
	{
		return Edge.NodeA == NodeIndex || Edge.NodeB == NodeIndex;
	}

	bool HasEdgeBetween(const FArcTownWorldSnapshot& Snapshot, const int32 FirstNode, const int32 SecondNode)
	{
		for (const FArcTownRoadEdge& Edge : Snapshot.Edges)
		{
			if ((Edge.NodeA == FirstNode && Edge.NodeB == SecondNode)
				|| (Edge.NodeA == SecondNode && Edge.NodeB == FirstNode))
			{
				return true;
			}
		}
		return false;
	}

	bool IsInsideBounds(const FBox2D& Bounds, const FVector2D& Point)
	{
		return Bounds.bIsValid
			&& Point.X >= Bounds.Min.X && Point.X <= Bounds.Max.X
			&& Point.Y >= Bounds.Min.Y && Point.Y <= Bounds.Max.Y;
	}

	bool SegmentIntersection(
		const FVector2D& RayStart,
		const FVector2D& RayEnd,
		const FVector2D& EdgeStart,
		const FVector2D& EdgeEnd,
		double& OutRayParameter)
	{
		const FVector2D RaySpan = RayEnd - RayStart;
		const FVector2D EdgeSpan = EdgeEnd - EdgeStart;
		const double Denominator = RaySpan.X * EdgeSpan.Y - RaySpan.Y * EdgeSpan.X;
		if (FMath::Abs(Denominator) <= UE_DOUBLE_SMALL_NUMBER)
		{
			return false;
		}

		const FVector2D Offset = EdgeStart - RayStart;
		const double RayParameter = (Offset.X * EdgeSpan.Y - Offset.Y * EdgeSpan.X) / Denominator;
		const double EdgeParameter = (Offset.X * RaySpan.Y - Offset.Y * RaySpan.X) / Denominator;
		if (!FMath::IsFinite(RayParameter) || !FMath::IsFinite(EdgeParameter))
		{
			return false;
		}

		if (RayParameter <= ParameterEpsilon || RayParameter > 1.0
			|| EdgeParameter <= ParameterEpsilon || EdgeParameter >= 1.0 - ParameterEpsilon)
		{
			return false;
		}

		OutRayParameter = RayParameter;
		return true;
	}

	int32 FindSnapNode(
		const FArcTownWorldSnapshot& Snapshot,
		const FVector2D& Point,
		const double SnapRadiusSquared,
		const int32 ExcludedNodeIndex)
	{
		int32 BestNode = INDEX_NONE;
		double BestDistanceSquared = 0.0;
		for (int32 NodeIndex = 0; NodeIndex < Snapshot.Nodes.Num(); ++NodeIndex)
		{
			if (NodeIndex == ExcludedNodeIndex)
			{
				continue;
			}

			const double DistanceSquared = (ToDouble(Snapshot.Nodes[NodeIndex].LocalXY) - Point).SizeSquared();
			if (DistanceSquared > SnapRadiusSquared)
			{
				continue;
			}

			if (BestNode == INDEX_NONE || DistanceSquared < BestDistanceSquared)
			{
				BestNode = NodeIndex;
				BestDistanceSquared = DistanceSquared;
			}
		}
		return BestNode;
	}

	bool CrossesRoadNetwork(
		const FArcTownWorldSnapshot& Snapshot,
		const FVector2D& Start,
		const FVector2D& End,
		const int32 ExcludedNodeIndex)
	{
		for (const FArcTownRoadEdge& Edge : Snapshot.Edges)
		{
			if (IsIncident(Edge, ExcludedNodeIndex)
				|| Edge.bBridge != 0
				|| !Snapshot.Nodes.IsValidIndex(Edge.NodeA)
				|| !Snapshot.Nodes.IsValidIndex(Edge.NodeB))
			{
				continue;
			}

			double Parameter = 0.0;
			if (SegmentIntersection(
				Start,
				End,
				ToDouble(Snapshot.Nodes[Edge.NodeA].LocalXY),
				ToDouble(Snapshot.Nodes[Edge.NodeB].LocalXY),
				Parameter))
			{
				return true;
			}
		}
		return false;
	}

	int32 AddRoadEdge(
		FArcTownWorldSnapshot& Snapshot,
		const int32 FirstNode,
		const int32 SecondNode,
		const double HalfWidthCm,
		const bool bBuilt,
		const bool bBridge,
		const int32 RoadId)
	{
		FArcTownRoadEdge Edge;
		Edge.NodeA = FirstNode;
		Edge.NodeB = SecondNode;
		Edge.RoadId = RoadId;
		Edge.bBuilt = bBuilt ? 1 : 0;
		Edge.bBridge = bBridge ? 1 : 0;
		Edge.HalfWidth = static_cast<float>(HalfWidthCm);
		return Snapshot.Edges.Add(Edge);
	}

	enum class ETerrainGateResult : uint8
	{
		Passed,
		SlopeTooSteep,
		TooCloseToWater,
		FieldsMissing
	};

	ETerrainGateResult EvaluateSlopeAndWaterGate(
		const FArcTownSiteSnapshot& Site,
		const FArcTownSiteFields& Fields,
		const FArcTownFieldSettings& FieldSettings,
		const double MaxRoadSlope,
		const FVector2f& TownLocalXY)
	{
		FIntPoint Cell = FIntPoint::ZeroValue;
		if (!ArcTownSiteFrame::TownLocalXYToCell(Site, TownLocalXY, Cell))
		{
			return ETerrainGateResult::Passed;
		}

		const int32 CellIndex = Site.SnapshotIndex(Cell.X, Cell.Y);
		if (!Fields.Slope.IsValidIndex(CellIndex) || !Fields.DistanceToWaterCm.IsValidIndex(CellIndex))
		{
			return ETerrainGateResult::FieldsMissing;
		}

		if (!(static_cast<double>(Fields.Slope[CellIndex]) <= MaxRoadSlope))
		{
			return ETerrainGateResult::SlopeTooSteep;
		}
		if (!(static_cast<double>(Fields.DistanceToWaterCm[CellIndex]) >= FieldSettings.MinWaterClearanceCm))
		{
			return ETerrainGateResult::TooCloseToWater;
		}
		return ETerrainGateResult::Passed;
	}

	EArcTownExpansionRejection ToRejection(const ETerrainGateResult Result)
	{
		switch (Result)
		{
		case ETerrainGateResult::SlopeTooSteep:
			return EArcTownExpansionRejection::SlopeGate;
		case ETerrainGateResult::TooCloseToWater:
			return EArcTownExpansionRejection::WaterClearance;
		default:
			return EArcTownExpansionRejection::TerrainFieldsMissing;
		}
	}

	struct FBridgeLanding
	{
		FVector2D Point = FVector2D::ZeroVector;
		double SpanCm = 0.0;
		int32 SnapNodeIndex = INDEX_NONE;
		int32 WaterBlockedProbes = 0;
		bool bFound = false;
	};

	FBridgeLanding FindBridgeLanding(
		const FArcTownSiteSnapshot& Site,
		const FArcTownSiteFields& Fields,
		const FArcTownFieldSettings& FieldSettings,
		const FArcTownRoadSettings& RoadSettings,
		const FArcTownWorldSnapshot& Snapshot,
		const FBox2D& VolumeBoundsTownLocalXY,
		const double MaxRoadSlope,
		const double SnapRadiusCm,
		const int32 BankNodeIndex,
		const FVector2D& BankPoint,
		const FVector2D& Direction,
		FArcTownExpansionYearReport& OutReport)
	{
		FBridgeLanding Landing;

		const double StepLengthCm = RoadSettings.RoadStepLengthCm;
		const double MaxSpanCm = FMath::IsFinite(RoadSettings.MaxBridgeSpanCm)
			? FMath::Max(RoadSettings.MaxBridgeSpanCm, 0.0)
			: 0.0;
		if (!(StepLengthCm > 0.0))
		{
			return Landing;
		}

		const double SnapRadiusSquared = SnapRadiusCm * SnapRadiusCm;
		const double StepBudget = FMath::Min(
			MaxSpanCm / StepLengthCm, static_cast<double>(FMath::Max(RoadSettings.MaxStepsPerRoad, 0)));
		const int32 MaxStepCount = FMath::FloorToInt32(StepBudget);
		for (int32 StepCount = 2; StepCount <= MaxStepCount; ++StepCount)
		{
			const double SpanCm = static_cast<double>(StepCount) * StepLengthCm;
			const FVector2D ProbePoint = BankPoint + Direction * SpanCm;
			const FVector2f ProbeLocalXY = ToFloat(ProbePoint);

			if (!IsInsideBounds(VolumeBoundsTownLocalXY, ProbePoint)
				|| !ArcTownSiteFrame::IsInsideSite(Site, ProbeLocalXY))
			{
				continue;
			}

			const ETerrainGateResult GateResult =
				EvaluateSlopeAndWaterGate(Site, Fields, FieldSettings, MaxRoadSlope, ProbeLocalXY);
			if (GateResult != ETerrainGateResult::Passed)
			{
				Landing.WaterBlockedProbes += GateResult == ETerrainGateResult::TooCloseToWater ? 1 : 0;
				continue;
			}

			if (CrossesRoadNetwork(Snapshot, BankPoint, ProbePoint, BankNodeIndex))
			{
				break;
			}

			const int32 SnapNodeIndex = FindSnapNode(Snapshot, ProbePoint, SnapRadiusSquared, BankNodeIndex);
			if (SnapNodeIndex != INDEX_NONE && HasEdgeBetween(Snapshot, BankNodeIndex, SnapNodeIndex))
			{
				continue;
			}

			Landing.Point = ProbePoint;
			Landing.SpanCm = SpanCm;
			Landing.SnapNodeIndex = SnapNodeIndex;
			Landing.bFound = true;
			return Landing;
		}

		return Landing;
	}

	// The tensor field returns an axis, ambiguous by pi; the branch nearer the previous heading is the one
	// that carries the road forward, and the far branch would fold it back on itself every step.
	double ResolveAxisAngleRad(const double AxisAngleRad, const double PreviousAngleRad)
	{
		const double Deviation = FMath::Abs(FMath::UnwindRadians(AxisAngleRad - PreviousAngleRad));
		return Deviation <= 0.5 * UE_DOUBLE_PI ? AxisAngleRad : AxisAngleRad + UE_DOUBLE_PI;
	}

	FVector2f SampleFieldDirection(
		const FArcTownTensorField& Field,
		const FVector2f NodeLocalXY,
		const FVector2f IncomingDirection,
		const bool bMinorField)
	{
		return bMinorField
			? ArcTownTensorField::SampleMinorDirection(Field, NodeLocalXY, IncomingDirection)
			: ArcTownTensorField::SampleMajorDirection(Field, NodeLocalXY, IncomingDirection);
	}

	double AxisAlignment(const FVector2f AxisDirection, const double HeadingRad)
	{
		const double AxisAngleRad =
			FMath::Atan2(static_cast<double>(AxisDirection.Y), static_cast<double>(AxisDirection.X));
		return FMath::Abs(FMath::Cos(HeadingRad - AxisAngleRad));
	}

	int32 ResolveBranchesPerRoad(const FArcTownRoadSettings& RoadSettings)
	{
		const double RoadLengthCm =
			static_cast<double>(RoadSettings.MaxStepsPerRoad) * FMath::Max(RoadSettings.RoadStepLengthCm, 0.0);
		return FMath::Max(
			FMath::FloorToInt32(RoadLengthCm / FMath::Max(RoadSettings.StreetBlockSizeCm, 1.0)), 1);
	}

	double ResolveStepAngleRad(
		const FArcTownTensorField& Field,
		const FArcTownRoadSettings& RoadSettings,
		const FArcTownWorldSnapshot& Snapshot,
		const FVector2f NodeLocalXY,
		const double PreviousAngleRad,
		const bool bFollowMinorField,
		const int32 Year,
		const int32 RoadId,
		const int32 StepIndex)
	{
		const FVector2f FieldDirection = SampleFieldDirection(
			Field, NodeLocalXY, HeadingVector(PreviousAngleRad), bFollowMinorField);
		const double AxisAngleRad =
			FMath::Atan2(static_cast<double>(FieldDirection.Y), static_cast<double>(FieldDirection.X));
		const double ResolvedAngleRad = ResolveAxisAngleRad(AxisAngleRad, PreviousAngleRad);

		const uint64 AngleKey = ArcTownDeterministic::MakeAnnualKey(
			Snapshot.SimulationId,
			Snapshot.Seed,
			ArcTownDeterministic::EDeterministicDomain::RoadExpansionAngle,
			Year,
			RoadId,
			StepIndex,
			QuantizeAngle(ResolvedAngleRad));
		const double TargetAngleRad = ResolvedAngleRad
			+ (ArcTownDeterministic::KeyToUnitOpen(AngleKey) * 2.0 - 1.0)
				* FMath::DegreesToRadians(RoadSettings.ExpansionAngleDeviationDeg);

		const double MaxTurnRad = FMath::DegreesToRadians(RoadSettings.MaxTurnPerStepDeg);
		const double DeltaRad =
			FMath::Clamp(FMath::UnwindRadians(TargetAngleRad - PreviousAngleRad), -MaxTurnRad, MaxTurnRad);
		return PreviousAngleRad + DeltaRad;
	}

	double ResolveBranchSpacingCm(
		const FArcTownRoadSettings& RoadSettings,
		const FArcTownWorldSnapshot& Snapshot,
		const int32 Year,
		const int32 RoadId,
		const int32 BranchOrdinal,
		const double StartHeadingRad)
	{
		const uint64 SpacingKey = ArcTownDeterministic::MakeAnnualKey(
			Snapshot.SimulationId,
			Snapshot.Seed,
			ArcTownDeterministic::EDeterministicDomain::RoadBranchSpacing,
			Year,
			RoadId,
			BranchOrdinal,
			QuantizeAngle(StartHeadingRad));
		const double JitterCm = (ArcTownDeterministic::KeyToUnitOpen(SpacingKey) * 2.0 - 1.0)
			* FMath::Max(RoadSettings.BranchSpacingJitterCm, 0.0);
		return FMath::Max(RoadSettings.StreetBlockSizeCm + JitterCm, RoadSettings.RoadStepLengthCm);
	}

	void EnqueueBranchSeeds(
		const TArray<FArcTownRoadBranchSeed>& Produced,
		const int32 MaxPendingSeeds,
		const int32 ConsumedSeedCount,
		FArcTownWorldSnapshot& Snapshot,
		FArcTownExpansionYearReport& OutReport)
	{
		OutReport.BranchesSeeded += Produced.Num();
		for (const FArcTownRoadBranchSeed& Seed : Produced)
		{
			if (Snapshot.PendingBranchSeeds.Num() - ConsumedSeedCount >= MaxPendingSeeds)
			{
				++OutReport.BranchSeedsDropped;
				continue;
			}

			Snapshot.PendingBranchSeeds.Add(Seed);
			++OutReport.BranchSeedsQueued;
		}
	}

	struct FNodeSpatialGrid
	{
		FIntPoint ToCell(const FVector2D& Point) const
		{
			return FIntPoint(
				static_cast<int32>(FMath::FloorToDouble(Point.X / CellSizeCm)),
				static_cast<int32>(FMath::FloorToDouble(Point.Y / CellSizeCm)));
		}

		void Reset(const double InCellSizeCm)
		{
			CellSizeCm = FMath::Max(InCellSizeCm, 1.0);
			Cells.Reset();
			Points.Reset();
		}

		void SyncTo(const FArcTownWorldSnapshot& Snapshot)
		{
			check(Points.Num() <= Snapshot.Nodes.Num());
			for (int32 NodeIndex = Points.Num(); NodeIndex < Snapshot.Nodes.Num(); ++NodeIndex)
			{
				const FVector2D Point = ToDouble(Snapshot.Nodes[NodeIndex].LocalXY);
				Points.Add(Point);
				Cells.FindOrAdd(ToCell(Point)).Add(NodeIndex);
			}
		}

		int32 CountNodesWithin(const FVector2D& Point, const double RadiusCm, const int32 ExcludedNodeIndex) const
		{
			const FIntPoint MinCell = ToCell(Point - FVector2D(RadiusCm, RadiusCm));
			const FIntPoint MaxCell = ToCell(Point + FVector2D(RadiusCm, RadiusCm));
			const double RadiusSquared = RadiusCm * RadiusCm;
			int32 Count = 0;
			for (int32 CellY = MinCell.Y; CellY <= MaxCell.Y; ++CellY)
			{
				for (int32 CellX = MinCell.X; CellX <= MaxCell.X; ++CellX)
				{
					const TArray<int32>* const Bucket = Cells.Find(FIntPoint(CellX, CellY));
					if (Bucket == nullptr)
					{
						continue;
					}

					for (const int32 NodeIndex : *Bucket)
					{
						if (NodeIndex != ExcludedNodeIndex
							&& (Points[NodeIndex] - Point).SizeSquared() <= RadiusSquared)
						{
							++Count;
						}
					}
				}
			}
			return Count;
		}

		bool HasNodeWithin(const FVector2D& Point, const double RadiusCm) const
		{
			const FIntPoint MinCell = ToCell(Point - FVector2D(RadiusCm, RadiusCm));
			const FIntPoint MaxCell = ToCell(Point + FVector2D(RadiusCm, RadiusCm));
			const double RadiusSquared = RadiusCm * RadiusCm;
			for (int32 CellY = MinCell.Y; CellY <= MaxCell.Y; ++CellY)
			{
				for (int32 CellX = MinCell.X; CellX <= MaxCell.X; ++CellX)
				{
					const TArray<int32>* const Bucket = Cells.Find(FIntPoint(CellX, CellY));
					if (Bucket == nullptr)
					{
						continue;
					}

					for (const int32 NodeIndex : *Bucket)
					{
						if ((Points[NodeIndex] - Point).SizeSquared() <= RadiusSquared)
						{
							return true;
						}
					}
				}
			}
			return false;
		}

		double CellSizeCm = 1.0;
		TMap<FIntPoint, TArray<int32>> Cells;
		TArray<FVector2D> Points;
	};

	int32 ReopenFinishedNodes(
		const FArcTownSiteSnapshot& Site,
		const FArcTownSiteFields& Fields,
		const FArcTownFieldSettings& FieldSettings,
		const FArcTownRoadSettings& RoadSettings,
		const FBox2D& VolumeBoundsTownLocalXY,
		const FVector2D& CorePoint,
		const double MaxRoadSlope,
		const double ProbeLengthCm,
		const double SnapRadiusCm,
		const int32 Year,
		const TArray<int32>& Valence,
		const FNodeSpatialGrid& Grid,
		FArcTownWorldSnapshot& Snapshot)
	{
		const int32 ProbeCount = FMath::Clamp(
			FMath::CeilToInt32(360.0 / FMath::Max(2.0 * RoadSettings.ExpansionAngleDeviationDeg, 1.0)), 4, 32);
		const double ProbeStepRad = 2.0 * UE_DOUBLE_PI / static_cast<double>(ProbeCount);

		int32 ReopenedCount = 0;
		for (int32 NodeIndex = 0; NodeIndex < Snapshot.Nodes.Num(); ++NodeIndex)
		{
			if (Snapshot.Nodes[NodeIndex].bFinished == 0
				|| !Valence.IsValidIndex(NodeIndex)
				|| Valence[NodeIndex] >= RoadSettings.MaxNodeValence)
			{
				continue;
			}

			const FVector2D NodePoint = ToDouble(Snapshot.Nodes[NodeIndex].LocalXY);
			const uint64 ProbeKey = ArcTownDeterministic::MakeAnnualKey(
				Snapshot.SimulationId,
				Snapshot.Seed,
				ArcTownDeterministic::EDeterministicDomain::NodeReopenProbe,
				Year,
				NodeIndex,
				0,
				static_cast<int64>(FMath::RoundToDouble((NodePoint - CorePoint).Size())));
			const double ProbeOffsetRad = ArcTownDeterministic::KeyToUnitOpen(ProbeKey) * ProbeStepRad;

			for (int32 ProbeIndex = 0; ProbeIndex < ProbeCount; ++ProbeIndex)
			{
				const double AngleRad = ProbeOffsetRad + static_cast<double>(ProbeIndex) * ProbeStepRad;
				const FVector2D ProbePoint =
					NodePoint + FVector2D(FMath::Cos(AngleRad), FMath::Sin(AngleRad)) * ProbeLengthCm;
				const FVector2f ProbeLocalXY = ToFloat(ProbePoint);

				if (!IsInsideBounds(VolumeBoundsTownLocalXY, ProbePoint)
					|| !ArcTownSiteFrame::IsInsideSite(Site, ProbeLocalXY)
					|| EvaluateSlopeAndWaterGate(Site, Fields, FieldSettings, MaxRoadSlope, ProbeLocalXY)
						!= ETerrainGateResult::Passed
					|| Grid.HasNodeWithin(ProbePoint, SnapRadiusCm))
				{
					continue;
				}

				Snapshot.Nodes[NodeIndex].bFinished = 0;
				Snapshot.Nodes[NodeIndex].FailedExpansions = 0;
				++ReopenedCount;
				break;
			}
		}

		return ReopenedCount;
	}

	bool SeedNewFront(
		const FArcTownTensorField& Field,
		const FArcTownSiteSnapshot& Site,
		const FArcTownSiteFields& Fields,
		const FArcTownFieldSettings& FieldSettings,
		const FArcTownRoadSettings& RoadSettings,
		const FBox2D& VolumeBoundsTownLocalXY,
		const FVector2D& CorePoint,
		const double BlockSizeCm,
		const int32 Year,
		const FNodeSpatialGrid& Grid,
		const int32 MaxPendingSeeds,
		const int32 ConsumedSeedCount,
		FArcTownWorldSnapshot& Snapshot,
		FArcTownExpansionYearReport& OutReport)
	{
		const FIntPoint SiteDims = Site.GetSiteDims();
		// Two candidates closer than one block cannot both carry a front, so the site is sampled at that lattice.
		const int32 StrideCells = FMath::Max(
			1, FMath::RoundToInt32(BlockSizeCm / FMath::Max(Site.SampleSpacing, 1.0)));

		FVector2f BestLocalXY = FVector2f::ZeroVector;
		double BestScore = 0.0;
		bool bFoundSite = false;
		for (int32 SiteY = 0; SiteY < SiteDims.Y; SiteY += StrideCells)
		{
			for (int32 SiteX = 0; SiteX < SiteDims.X; SiteX += StrideCells)
			{
				const int32 CellIndex = Site.SnapshotIndex(SiteX, SiteY);
				if (!Fields.bBuildable.IsValidIndex(CellIndex) || Fields.bBuildable[CellIndex] == 0)
				{
					continue;
				}

				const FVector2f CellLocalXY = ArcTownSiteFrame::CellToTownLocalXY(Site, SiteX, SiteY);
				const FVector2D CellPoint = ToDouble(CellLocalXY);
				if (!IsInsideBounds(VolumeBoundsTownLocalXY, CellPoint) || Grid.HasNodeWithin(CellPoint, BlockSizeCm))
				{
					continue;
				}

				const double SiteTerm = ArcTownBootstrap::SiteTerm(Fields, FieldSettings, CellIndex);
				if (!(SiteTerm > 0.0))
				{
					continue;
				}

				const uint64 SiteKey = ArcTownDeterministic::MakeAnnualKey(
					Snapshot.SimulationId,
					Snapshot.Seed,
					ArcTownDeterministic::EDeterministicDomain::NewFrontSite,
					Year,
					CellIndex,
					0,
					static_cast<int64>(FMath::RoundToDouble(SiteTerm * 1024.0)));
				const double Score = SiteTerm * ArcTownDeterministic::KeyToUnitOpen(SiteKey);
				if (!bFoundSite || Score > BestScore)
				{
					bFoundSite = true;
					BestScore = Score;
					BestLocalXY = CellLocalXY;
				}
			}
		}

		if (!bFoundSite)
		{
			return false;
		}

		FArcTownRoadNode FrontNode;
		FrontNode.LocalXY = BestLocalXY;
		const int32 FrontNodeIndex = Snapshot.Nodes.Add(FrontNode);

		FArcTownRoadTraceParams Params;
		Params.Field = &Field;
		Params.FieldSettings = &FieldSettings;
		Params.RoadSettings = &RoadSettings;
		Params.VolumeBoundsTownLocalXY = VolumeBoundsTownLocalXY;
		Params.Year = Year;
		Params.StartNodeIndex = FrontNodeIndex;
		Params.StartHeadingRad =
			ArcTownRoadExpansion::ResolveStartHeadingRad(Snapshot, FrontNodeIndex, CorePoint, Year);
		Params.bFollowMinorField =
			ArcTownRoadExpansion::ResolveFollowMinorField(Field, BestLocalXY, Params.StartHeadingRad);
		Params.HalfWidthCm = RoadSettings.RoadHalfWidthCm;

		FArcTownRoadTraceResult Result;
		ArcTownRoadExpansion::TraceRoad(Params, Snapshot, OutReport, Result);
		++OutReport.RoadsTraced;
		++OutReport.RoadEnds[static_cast<int32>(Result.End)];

		if (Result.EdgesAdded == 0)
		{
			Snapshot.Nodes.SetNum(FrontNodeIndex);
			return false;
		}

		EnqueueBranchSeeds(Result.BranchSeeds, MaxPendingSeeds, ConsumedSeedCount, Snapshot, OutReport);
		return true;
	}

	void FinishOverloadedNodes(
		const FArcTownRoadSettings& RoadSettings,
		const TArray<int32>& Valence,
		FArcTownWorldSnapshot& Snapshot,
		FArcTownExpansionYearReport& OutReport)
	{
		for (int32 NodeIndex = 0; NodeIndex < Snapshot.Nodes.Num(); ++NodeIndex)
		{
			if (Valence.IsValidIndex(NodeIndex)
				&& Valence[NodeIndex] >= RoadSettings.MaxNodeValence
				&& Snapshot.Nodes[NodeIndex].bFinished == 0)
			{
				++OutReport.NodesFinishedByValence;
				Snapshot.Nodes[NodeIndex].bFinished = 1;
			}
		}
	}

	bool TryRelieveSaturation(
		const FArcTownTensorField& Field,
		const FArcTownSiteSnapshot& Site,
		const FArcTownSiteFields& Fields,
		const FArcTownFieldSettings& FieldSettings,
		const FArcTownRoadSettings& RoadSettings,
		const FBox2D& VolumeBoundsTownLocalXY,
		const FVector2D& CorePoint,
		const double MaxRoadSlope,
		const double BlockSizeCm,
		const double StepLengthCm,
		const double SnapRadiusCm,
		const int32 Year,
		const int32 MaxPendingSeeds,
		const int32 ConsumedSeedCount,
		TArray<int32>& Valence,
		const FNodeSpatialGrid& Grid,
		FArcTownWorldSnapshot& Snapshot,
		FArcTownExpansionYearReport& OutReport)
	{
		const int32 ReopenedCount = ReopenFinishedNodes(
			Site,
			Fields,
			FieldSettings,
			RoadSettings,
			VolumeBoundsTownLocalXY,
			CorePoint,
			MaxRoadSlope,
			StepLengthCm,
			SnapRadiusCm,
			Year,
			Valence,
			Grid,
			Snapshot);
		if (ReopenedCount > 0)
		{
			OutReport.NodesReopened += ReopenedCount;
			return true;
		}
		ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownExpansionRejection::NoReopenableNode);

		if (SeedNewFront(
			Field,
			Site,
			Fields,
			FieldSettings,
			RoadSettings,
			VolumeBoundsTownLocalXY,
			CorePoint,
			BlockSizeCm,
			Year,
			Grid,
			MaxPendingSeeds,
			ConsumedSeedCount,
			Snapshot,
			OutReport))
		{
			++OutReport.NewFrontsSeeded;
			ComputeValence(Snapshot, Valence);
			FinishOverloadedNodes(RoadSettings, Valence, Snapshot, OutReport);
			return true;
		}
		ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownExpansionRejection::NoNewFrontSite);

		return false;
	}
}
}

const TCHAR* const ArcTownRoadExpansion::MaxRoadSlopeDegRequirementText =
	TEXT("ArcTown refuses ground steeper than MaxRoadSlopeDeg, so it must be an angle of 0 to 89 degrees.");

bool ArcTownRoadExpansion::IsMaxRoadSlopeDegUsable(const FArcTownRoadSettings& RoadSettings)
{
	return FMath::IsFinite(RoadSettings.MaxRoadSlopeDeg)
		&& RoadSettings.MaxRoadSlopeDeg >= 0.0
		&& RoadSettings.MaxRoadSlopeDeg <= 89.0;
}

double ArcTownRoadExpansion::ResolveSnapRadiusCm(const FArcTownRoadSettings& RoadSettings)
{
	using namespace ArcTownRoadExpansionPrivate;

	return FMath::IsFinite(RoadSettings.RoadSnapRadiusCm) && RoadSettings.RoadSnapRadiusCm > 0.0
		? RoadSettings.RoadSnapRadiusCm
		: RoadSettings.RoadStepLengthCm * SnapRadiusPerStep;
}

bool ArcTownRoadExpansion::ResolveFollowMinorField(
	const FArcTownTensorField& Field,
	const FVector2f NodeLocalXY,
	const double StartHeadingRad)
{
	using namespace ArcTownRoadExpansionPrivate;

	if (!FMath::IsFinite(StartHeadingRad))
	{
		return false;
	}

	const FVector2f Heading = HeadingVector(StartHeadingRad);
	const double MajorAlignment =
		AxisAlignment(SampleFieldDirection(Field, NodeLocalXY, Heading, false), StartHeadingRad);
	const double MinorAlignment =
		AxisAlignment(SampleFieldDirection(Field, NodeLocalXY, Heading, true), StartHeadingRad);
	return MinorAlignment > MajorAlignment;
}

int32 ArcTownRoadExpansion::ResolveMaxPendingBranchSeeds(
	const FArcTownRoadSettings& RoadSettings,
	const FArcTownWorldSnapshot& Snapshot)
{
	using namespace ArcTownRoadExpansionPrivate;

	const double NetworkLengthCm =
		static_cast<double>(Snapshot.Edges.Num()) * FMath::Max(RoadSettings.RoadStepLengthCm, 0.0);
	const int32 BlockSites =
		FMath::FloorToInt32(NetworkLengthCm / FMath::Max(RoadSettings.StreetBlockSizeCm, 1.0));
	return FMath::Max(BlockSites, ResolveBranchesPerRoad(RoadSettings));
}

double ArcTownRoadExpansion::ResolveStartHeadingRad(
	const FArcTownWorldSnapshot& Snapshot,
	const int32 NodeIndex,
	const FVector2D& CoreTownLocalXY,
	const int32 Year)
{
	using namespace ArcTownRoadExpansionPrivate;

	if (!Snapshot.Nodes.IsValidIndex(NodeIndex))
	{
		return 0.0;
	}

	const FVector2D NodePoint = ToDouble(Snapshot.Nodes[NodeIndex].LocalXY);

	TArray<double> IncidentAngles;
	for (int32 EdgeIndex = 0; EdgeIndex < Snapshot.Edges.Num(); ++EdgeIndex)
	{
		const FArcTownRoadEdge& Edge = Snapshot.Edges[EdgeIndex];
		if (!IsIncident(Edge, NodeIndex))
		{
			continue;
		}

		const int32 OtherIndex = Edge.NodeA == NodeIndex ? Edge.NodeB : Edge.NodeA;
		if (!Snapshot.Nodes.IsValidIndex(OtherIndex))
		{
			continue;
		}

		const FVector2D Leaving = ToDouble(Snapshot.Nodes[OtherIndex].LocalXY) - NodePoint;
		if (Leaving.SizeSquared() > UE_DOUBLE_SMALL_NUMBER)
		{
			IncidentAngles.Add(FMath::UnwindRadians(FMath::Atan2(Leaving.Y, Leaving.X)));
		}
	}

	if (IncidentAngles.Num() > 0)
	{
		IncidentAngles.Sort();

		int32 WidestGapIndex = 0;
		double WidestGapRad = 0.0;
		for (int32 AngleIndex = 0; AngleIndex < IncidentAngles.Num(); ++AngleIndex)
		{
			const int32 NextIndex = (AngleIndex + 1) % IncidentAngles.Num();
			const double GapRad = AngleIndex + 1 < IncidentAngles.Num()
				? IncidentAngles[NextIndex] - IncidentAngles[AngleIndex]
				: IncidentAngles[NextIndex] + 2.0 * UE_DOUBLE_PI - IncidentAngles[AngleIndex];
			if (AngleIndex == 0 || GapRad > WidestGapRad)
			{
				WidestGapIndex = AngleIndex;
				WidestGapRad = GapRad;
			}
		}

		return FMath::UnwindRadians(IncidentAngles[WidestGapIndex] + 0.5 * WidestGapRad);
	}

	const FVector2D Outward = NodePoint - CoreTownLocalXY;
	if (Outward.SizeSquared() > UE_DOUBLE_SMALL_NUMBER)
	{
		return FMath::Atan2(Outward.Y, Outward.X);
	}

	const uint64 HeadingKey = ArcTownDeterministic::MakeAnnualKey(
		Snapshot.SimulationId,
		Snapshot.Seed,
		ArcTownDeterministic::EDeterministicDomain::SeedRoadDirection,
		Year,
		NodeIndex,
		0,
		0);
	return ArcTownDeterministic::KeyToUnitOpen(HeadingKey) * 2.0 * UE_DOUBLE_PI - UE_DOUBLE_PI;
}

void ArcTownRoadExpansion::TraceRoad(
	const FArcTownRoadTraceParams& Params,
	FArcTownWorldSnapshot& Snapshot,
	FArcTownExpansionYearReport& OutReport,
	FArcTownRoadTraceResult& OutResult)
{
	using namespace ArcTownRoadExpansionPrivate;

	OutResult = FArcTownRoadTraceResult();

	if (Params.Field == nullptr
		|| Params.Field->Site == nullptr
		|| Params.Field->Fields == nullptr
		|| Params.FieldSettings == nullptr
		|| Params.RoadSettings == nullptr
		|| !Snapshot.Nodes.IsValidIndex(Params.StartNodeIndex)
		|| !FMath::IsFinite(Params.StartHeadingRad)
		|| !IsMaxRoadSlopeDegUsable(*Params.RoadSettings))
	{
		OutResult.End = EArcTownRoadTraceEnd::StartNodeInvalid;
		return;
	}

	const FArcTownSiteSnapshot& Site = *Params.Field->Site;
	const FArcTownSiteFields& Fields = *Params.Field->Fields;
	const FArcTownFieldSettings& FieldSettings = *Params.FieldSettings;
	const FArcTownRoadSettings& RoadSettings = *Params.RoadSettings;

	const double MaxRoadSlope = FMath::Tan(FMath::DegreesToRadians(RoadSettings.MaxRoadSlopeDeg));
	const double StepLengthCm = RoadSettings.RoadStepLengthCm;
	const double SnapRadiusCm = ResolveSnapRadiusCm(RoadSettings);
	const double SnapRadiusSquared = SnapRadiusCm * SnapRadiusCm;

	const int32 RoadId = Params.RoadId != INDEX_NONE ? Params.RoadId : Snapshot.NextRoadId;

	OutResult.RoadId = RoadId;
	OutResult.EndNodeIndex = Params.StartNodeIndex;
	OutResult.End = EArcTownRoadTraceEnd::StepLimit;

	int32 CurrentNode = Params.StartNodeIndex;
	double PreviousAngleRad = Params.StartHeadingRad;
	int32 BranchOrdinal = 0;
	double DistanceSinceBranchCm = 0.0;
	double NextBranchAtCm = ResolveBranchSpacingCm(
		RoadSettings, Snapshot, Params.Year, RoadId, BranchOrdinal, Params.StartHeadingRad);

	for (int32 StepIndex = 0; StepIndex < RoadSettings.MaxStepsPerRoad; ++StepIndex)
	{
		const FVector2f NodeLocalXY = Snapshot.Nodes[CurrentNode].LocalXY;
		const FVector2D NodePoint = ToDouble(NodeLocalXY);
		const double AngleRad = ResolveStepAngleRad(
			*Params.Field,
			RoadSettings,
			Snapshot,
			NodeLocalXY,
			PreviousAngleRad,
			Params.bFollowMinorField,
			Params.Year,
			RoadId,
			StepIndex);
		const FVector2D Direction(FMath::Cos(AngleRad), FMath::Sin(AngleRad));
		const FVector2D EndPoint = NodePoint + Direction * StepLengthCm;

		int32 TargetNode = INDEX_NONE;
		bool bEnded = false;
		bool bCreatedFreeNode = false;
		bool bBridged = false;
		double AdvancedLengthCm = StepLengthCm;
		EArcTownRoadTraceEnd EndReason = EArcTownRoadTraceEnd::StepLimit;

		int32 HitEdge = INDEX_NONE;
		double HitParameter = 0.0;
		for (int32 EdgeIndex = 0; EdgeIndex < Snapshot.Edges.Num(); ++EdgeIndex)
		{
			const FArcTownRoadEdge& Edge = Snapshot.Edges[EdgeIndex];
			if (IsIncident(Edge, CurrentNode)
				|| Edge.bBridge != 0
				|| !Snapshot.Nodes.IsValidIndex(Edge.NodeA)
				|| !Snapshot.Nodes.IsValidIndex(Edge.NodeB))
			{
				continue;
			}

			double Parameter = 0.0;
			if (!SegmentIntersection(
				NodePoint,
				EndPoint,
				ToDouble(Snapshot.Nodes[Edge.NodeA].LocalXY),
				ToDouble(Snapshot.Nodes[Edge.NodeB].LocalXY),
				Parameter))
			{
				continue;
			}

			if (HitEdge == INDEX_NONE || Parameter < HitParameter)
			{
				HitEdge = EdgeIndex;
				HitParameter = Parameter;
			}
		}

		if (HitEdge != INDEX_NONE)
		{
			bEnded = true;
			EndReason = EArcTownRoadTraceEnd::Intersected;

			const FVector2f SplitLocalXY = ToFloat(NodePoint + Direction * (StepLengthCm * HitParameter));
			const int32 NodesBeforeSplit = Snapshot.Nodes.Num();
			const int32 JunctionIndex = Snapshot.SplitEdgeAtPoint(HitEdge, SplitLocalXY);
			const bool bCreatedJunction = Snapshot.Nodes.Num() > NodesBeforeSplit;
			if (!Snapshot.Nodes.IsValidIndex(JunctionIndex))
			{
				ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownExpansionRejection::SplitProducedNoNode);
			}
			else if (!bCreatedJunction
				&& (JunctionIndex == CurrentNode || HasEdgeBetween(Snapshot, CurrentNode, JunctionIndex)))
			{
				ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownExpansionRejection::SplitTargetAlreadyConnected);
			}
			else
			{
				TargetNode = JunctionIndex;
			}
		}

		if (!bEnded)
		{
			TargetNode = FindSnapNode(Snapshot, EndPoint, SnapRadiusSquared, CurrentNode);
			if (TargetNode != INDEX_NONE)
			{
				bEnded = true;
				EndReason = EArcTownRoadTraceEnd::Snapped;
				if (HasEdgeBetween(Snapshot, CurrentNode, TargetNode))
				{
					ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownExpansionRejection::SnapTargetAlreadyConnected);
					TargetNode = INDEX_NONE;
				}
			}
		}

		if (!bEnded)
		{
			const FVector2f EndLocalXY = ToFloat(EndPoint);
			const ETerrainGateResult GateResult =
				EvaluateSlopeAndWaterGate(Site, Fields, FieldSettings, MaxRoadSlope, EndLocalXY);
			const bool bInsideBounds = IsInsideBounds(Params.VolumeBoundsTownLocalXY, EndPoint);
			const bool bInsideSite = ArcTownSiteFrame::IsInsideSite(Site, EndLocalXY);

			// Water is an obstacle a road crosses, and slope is one it cannot: only the water gate opens a span.
			const bool bBridgeable = GateResult == ETerrainGateResult::TooCloseToWater
				&& EvaluateSlopeAndWaterGate(Site, Fields, FieldSettings, MaxRoadSlope, NodeLocalXY)
					!= ETerrainGateResult::SlopeTooSteep;
			const FBridgeLanding Landing = bBridgeable
				? FindBridgeLanding(
					Site,
					Fields,
					FieldSettings,
					RoadSettings,
					Snapshot,
					Params.VolumeBoundsTownLocalXY,
					MaxRoadSlope,
					SnapRadiusCm,
					CurrentNode,
					NodePoint,
					Direction,
					OutReport)
				: FBridgeLanding();

			if (Landing.bFound)
			{
				if (Landing.SnapNodeIndex != INDEX_NONE)
				{
					TargetNode = Landing.SnapNodeIndex;
					bEnded = true;
					EndReason = EArcTownRoadTraceEnd::Snapped;
				}
				else
				{
					FArcTownRoadNode LandingNode;
					LandingNode.LocalXY = ToFloat(Landing.Point);
					TargetNode = Snapshot.Nodes.Add(LandingNode);
					bCreatedFreeNode = true;
				}
				bBridged = true;
				AdvancedLengthCm = Landing.SpanCm;
				++OutReport.BridgesBuilt;
			}
			else if (GateResult != ETerrainGateResult::Passed)
			{
				ArcTownGrowthDiagnostics::Tally(OutReport, ToRejection(GateResult));
				if (Landing.WaterBlockedProbes > 0)
				{
					ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownExpansionRejection::BridgeSpanTooLong);
				}
				bEnded = true;
				EndReason = EArcTownRoadTraceEnd::TerrainRejected;
			}
			else if (!bInsideBounds || !bInsideSite)
			{
				ArcTownGrowthDiagnostics::Tally(
					OutReport,
					!bInsideBounds
						? EArcTownExpansionRejection::OutsideVolumeBounds
						: EArcTownExpansionRejection::OutsideSite);
				Snapshot.Nodes[CurrentNode].bFinished = 1;
				bEnded = true;
				EndReason = !bInsideBounds
					? EArcTownRoadTraceEnd::LeftVolumeBounds
					: EArcTownRoadTraceEnd::LeftSite;
			}
			else
			{
				FArcTownRoadNode NewNode;
				NewNode.LocalXY = EndLocalXY;
				TargetNode = Snapshot.Nodes.Add(NewNode);
				bCreatedFreeNode = true;
			}
		}

		if (TargetNode != INDEX_NONE)
		{
			if (Params.RoadId == INDEX_NONE && OutResult.EdgesAdded == 0)
			{
				++Snapshot.NextRoadId;
			}

			AddRoadEdge(Snapshot, CurrentNode, TargetNode, Params.HalfWidthCm, Params.bBuilt, bBridged, RoadId);
			++OutResult.EdgesAdded;
			++OutReport.EdgesAdded;

			CurrentNode = TargetNode;
			OutResult.EndNodeIndex = TargetNode;
			PreviousAngleRad = AngleRad;
		}

		if (bEnded)
		{
			OutResult.End = EndReason;
			return;
		}

		DistanceSinceBranchCm += AdvancedLengthCm;
		if (bCreatedFreeNode && DistanceSinceBranchCm >= NextBranchAtCm)
		{
			const FVector2f BranchDirection = SampleFieldDirection(
				*Params.Field,
				Snapshot.Nodes[CurrentNode].LocalXY,
				HeadingVector(AngleRad),
				!Params.bFollowMinorField);
			const uint64 SideKey = ArcTownDeterministic::MakeAnnualKey(
				Snapshot.SimulationId,
				Snapshot.Seed,
				ArcTownDeterministic::EDeterministicDomain::RoadBranchSide,
				Params.Year,
				RoadId,
				BranchOrdinal,
				QuantizeAngle(AngleRad));
			const double Side = ArcTownDeterministic::KeyToUnitOpen(SideKey) < 0.5 ? -1.0 : 1.0;

			FArcTownRoadBranchSeed& Seed = OutResult.BranchSeeds.AddDefaulted_GetRef();
			Seed.NodeIndex = CurrentNode;
			Seed.HeadingRad = FMath::Atan2(
				Side * static_cast<double>(BranchDirection.Y), Side * static_cast<double>(BranchDirection.X));
			Seed.bFollowsMinorField = Params.bFollowMinorField ? 0 : 1;

			++BranchOrdinal;
			DistanceSinceBranchCm = 0.0;
			NextBranchAtCm = ResolveBranchSpacingCm(
				RoadSettings, Snapshot, Params.Year, RoadId, BranchOrdinal, Params.StartHeadingRad);
		}
	}

	OutResult.End = EArcTownRoadTraceEnd::StepLimit;
}

bool ArcTownRoadExpansion::ExpandRoads(
	const FArcTownTensorField& Field,
	const FArcTownFieldSettings& FieldSettings,
	const FArcTownRoadSettings& RoadSettings,
	const FBox2D& VolumeBoundsTownLocalXY,
	const int32 Year,
	FArcTownWorldSnapshot& Snapshot,
	FArcTownExpansionYearReport& OutReport,
	FText& OutError)
{
	using namespace ArcTownRoadExpansionPrivate;

	OutReport = FArcTownExpansionYearReport();

	if (Field.Site == nullptr || Field.Fields == nullptr)
	{
		return SetError(OutError, TEXT("ArcTown cannot expand roads without a sampled site and its terrain fields."));
	}

	const FArcTownSiteSnapshot& Site = *Field.Site;
	const FArcTownSiteFields& Fields = *Field.Fields;
	if (!Site.IsValid())
	{
		return SetError(OutError, TEXT("ArcTown cannot expand roads because the sampled site is not valid; sample the site again."));
	}

	if (!ArcTownBootstrap::AreFieldsUsable(Site, Fields))
	{
		return SetError(OutError, TEXT("ArcTown cannot expand roads because the terrain fields do not match the sampled site; build the fields again."));
	}

	if (!ArcTownSiteFrame::HasUsableExtentXY(VolumeBoundsTownLocalXY))
	{
		return SetError(OutError, TEXT("ArcTown cannot expand roads without the town volume bounds in the town-local frame."));
	}

	if (Snapshot.Nodes.Num() == 0)
	{
		return SetError(OutError, TEXT("ArcTown cannot expand roads before the town is seeded; seed it first."));
	}

	if (!FMath::IsFinite(RoadSettings.StreetBlockSizeCm) || RoadSettings.StreetBlockSizeCm <= 0.0)
	{
		return SetError(OutError, TEXT("ArcTown grows streets at the spacing StreetBlockSizeCm states, so it must be a positive distance in centimetres."));
	}

	if (!FMath::IsFinite(RoadSettings.RoadStepLengthCm) || RoadSettings.RoadStepLengthCm <= 0.0)
	{
		return SetError(OutError, TEXT("ArcTown traces a road one RoadStepLengthCm at a time, so it must be a positive distance in centimetres."));
	}

	if (RoadSettings.MaxStepsPerRoad < 1)
	{
		return SetError(OutError, TEXT("ArcTown traces at least one step per road, so MaxStepsPerRoad must be at least 1."));
	}

	if (!FMath::IsFinite(RoadSettings.MaxBridgeSpanCm) || RoadSettings.MaxBridgeSpanCm < 0.0)
	{
		return SetError(OutError, TEXT("ArcTown crosses no water wider than MaxBridgeSpanCm, so it must be a distance in centimetres of zero or more."));
	}

	if (RoadSettings.RoadsPerYear < 1)
	{
		return SetError(OutError, TEXT("ArcTown traces at least one road a year, so RoadsPerYear must be at least 1."));
	}

	if (RoadSettings.MaxNodeValence < 3)
	{
		return SetError(OutError, TEXT("ArcTown needs MaxNodeValence of at least 3, or a junction can carry no street beyond the two that meet there."));
	}

	if (!FMath::IsFinite(RoadSettings.RoadHalfWidthCm) || RoadSettings.RoadHalfWidthCm <= 0.0)
	{
		return SetError(OutError, TEXT("ArcTown lays every fresh road RoadHalfWidthCm to each side of its centreline, so it must be a positive distance in centimetres."));
	}

	if (!IsMaxRoadSlopeDegUsable(RoadSettings))
	{
		return SetError(OutError, MaxRoadSlopeDegRequirementText);
	}

	const double MaxRoadSlope = FMath::Tan(FMath::DegreesToRadians(RoadSettings.MaxRoadSlopeDeg));
	const double BlockSizeCm = RoadSettings.StreetBlockSizeCm;
	const double StepLengthCm = RoadSettings.RoadStepLengthCm;
	const double SnapRadiusCm = ResolveSnapRadiusCm(RoadSettings);
	const FVector2D CorePoint = ToDouble(Field.CoreTownLocalXY);

	TArray<int32> Valence;

	int32 BranchCursor = 0;
	TArray<FArcTownRoadBranchSeed> RetainedSeeds;

	const int32 AttemptBudget = RoadSettings.RoadsPerYear;
	const int32 MaxPendingSeeds = ResolveMaxPendingBranchSeeds(RoadSettings, Snapshot);
	const int32 ScoredAttemptPeriod = ResolveBranchesPerRoad(RoadSettings) + 1;
	const double DensityHalfSaturationNodes = FMath::Max(RoadSettings.DensityHalfSaturationNodes, 1.0);
	OutReport.AttemptBudget = AttemptBudget;

	FNodeSpatialGrid Grid;
	Grid.Reset(BlockSizeCm);

	bool bReliefTried = false;
	for (int32 Attempt = 0; Attempt < AttemptBudget; ++Attempt)
	{
		++OutReport.AttemptsRun;

		ComputeValence(Snapshot, Valence);
		Grid.SyncTo(Snapshot);

		int32 StartNode = INDEX_NONE;
		double StartHeadingRad = 0.0;
		bool bFollowMinorField = false;
		bool bStartedFromSeed = false;
		const bool bScoredAttempt = (FMath::Max(Year, 0) + Attempt) % ScoredAttemptPeriod == 0;
		while (!bScoredAttempt && BranchCursor < Snapshot.PendingBranchSeeds.Num())
		{
			const FArcTownRoadBranchSeed Seed = Snapshot.PendingBranchSeeds[BranchCursor];
			++BranchCursor;

			const bool bSeedSiteUsable = Snapshot.Nodes.IsValidIndex(Seed.NodeIndex)
				&& Valence.IsValidIndex(Seed.NodeIndex)
				&& Valence[Seed.NodeIndex] < RoadSettings.MaxNodeValence
				&& FMath::IsFinite(Seed.HeadingRad);
			if (bSeedSiteUsable && Snapshot.Nodes[Seed.NodeIndex].bFinished == 0)
			{
				StartNode = Seed.NodeIndex;
				StartHeadingRad = Seed.HeadingRad;
				bFollowMinorField = Seed.bFollowsMinorField != 0;
				bStartedFromSeed = true;
				break;
			}

			if (bSeedSiteUsable)
			{
				RetainedSeeds.Add(Seed);
				continue;
			}

			++OutReport.BranchSeedsSkipped;
		}

		if (StartNode == INDEX_NONE)
		{
			double BestScore = 0.0;
			for (int32 NodeIndex = 0; NodeIndex < Snapshot.Nodes.Num(); ++NodeIndex)
			{
				if (Snapshot.Nodes[NodeIndex].bFinished != 0 || Valence[NodeIndex] >= RoadSettings.MaxNodeValence)
				{
					continue;
				}

				++OutReport.NodesConsidered;

				const FVector2D NodePoint = ToDouble(Snapshot.Nodes[NodeIndex].LocalXY);
				const int32 LocalNodeCount = Grid.CountNodesWithin(NodePoint, BlockSizeCm, NodeIndex);
				const uint64 PickKey = ArcTownDeterministic::MakeAnnualKey(
					Snapshot.SimulationId,
					Snapshot.Seed,
					ArcTownDeterministic::EDeterministicDomain::RoadExpansionPick,
					Year,
					NodeIndex,
					Attempt,
					static_cast<int64>(LocalNodeCount));
				const double Draw = ArcTownDeterministic::KeyToUnitOpen(PickKey);
				const double Score =
					Draw / (1.0 + static_cast<double>(LocalNodeCount) / DensityHalfSaturationNodes);

				if (StartNode == INDEX_NONE || Score > BestScore)
				{
					StartNode = NodeIndex;
					BestScore = Score;
				}
			}

			if (StartNode == INDEX_NONE)
			{
				if (bReliefTried)
				{
					++OutReport.StoppedWithoutEligibleNode;
					break;
				}
				bReliefTried = true;

				if (TryRelieveSaturation(
					Field,
					Site,
					Fields,
					FieldSettings,
					RoadSettings,
					VolumeBoundsTownLocalXY,
					CorePoint,
					MaxRoadSlope,
					BlockSizeCm,
					StepLengthCm,
					SnapRadiusCm,
					Year,
					MaxPendingSeeds,
					BranchCursor - RetainedSeeds.Num(),
					Valence,
					Grid,
					Snapshot,
					OutReport))
				{
					continue;
				}

				++OutReport.YearsSaturated;
				++OutReport.StoppedWithoutEligibleNode;
				break;
			}

			StartHeadingRad = ResolveStartHeadingRad(Snapshot, StartNode, CorePoint, Year);
			bFollowMinorField =
				ResolveFollowMinorField(Field, Snapshot.Nodes[StartNode].LocalXY, StartHeadingRad);
		}

		++OutReport.ExpansionsAttempted;

		FArcTownRoadTraceParams Params;
		Params.Field = &Field;
		Params.FieldSettings = &FieldSettings;
		Params.RoadSettings = &RoadSettings;
		Params.VolumeBoundsTownLocalXY = VolumeBoundsTownLocalXY;
		Params.Year = Year;
		Params.StartNodeIndex = StartNode;
		Params.StartHeadingRad = StartHeadingRad;
		Params.bFollowMinorField = bFollowMinorField;
		Params.HalfWidthCm = RoadSettings.RoadHalfWidthCm;

		FArcTownRoadTraceResult Result;
		TraceRoad(Params, Snapshot, OutReport, Result);
		++OutReport.RoadsTraced;
		++OutReport.RoadEnds[static_cast<int32>(Result.End)];
		if (bStartedFromSeed)
		{
			++OutReport.BranchRoadsTraced;
		}

		EnqueueBranchSeeds(
			Result.BranchSeeds, MaxPendingSeeds, BranchCursor - RetainedSeeds.Num(), Snapshot, OutReport);

		if (Result.EdgesAdded == 0)
		{
			if (++Snapshot.Nodes[StartNode].FailedExpansions >= RoadSettings.MaxFailedExpansions
				&& Snapshot.Nodes[StartNode].bFinished == 0)
			{
				++OutReport.NodesFinishedByFailedExpansions;
				Snapshot.Nodes[StartNode].bFinished = 1;
			}
			continue;
		}

		Snapshot.Nodes[StartNode].FailedExpansions = 0;
		ComputeValence(Snapshot, Valence);
		FinishOverloadedNodes(RoadSettings, Valence, Snapshot, OutReport);
	}

	if (OutReport.EdgesAdded == 0 && !bReliefTried)
	{
		ComputeValence(Snapshot, Valence);
		Grid.SyncTo(Snapshot);

		if (!TryRelieveSaturation(
			Field,
			Site,
			Fields,
			FieldSettings,
			RoadSettings,
			VolumeBoundsTownLocalXY,
			CorePoint,
			MaxRoadSlope,
			BlockSizeCm,
			StepLengthCm,
			SnapRadiusCm,
			Year,
			MaxPendingSeeds,
			BranchCursor - RetainedSeeds.Num(),
			Valence,
			Grid,
			Snapshot,
			OutReport))
		{
			++OutReport.YearsSaturated;
		}
	}

	if (BranchCursor > 0)
	{
		Snapshot.PendingBranchSeeds.RemoveAt(0, BranchCursor, EAllowShrinking::No);
	}
	Snapshot.PendingBranchSeeds.Append(RetainedSeeds);
	OutReport.PendingBranchSeeds = Snapshot.PendingBranchSeeds.Num();

	OutError = FText::GetEmpty();
	return true;
}
