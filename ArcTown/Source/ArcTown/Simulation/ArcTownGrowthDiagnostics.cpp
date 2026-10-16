// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcTownGrowthDiagnostics.h"

#include "Simulation/ArcTownPlacement.h"
#include "Simulation/ArcTownRoadExpansion.h"
#include "Simulation/ArcTownSimulationInput.h"
#include "Simulation/ArcTownSiteFields.h"
#include "Simulation/ArcTownTensorField.h"
#include "Simulation/ArcTownTraffic.h"
#include "Simulation/ArcTownWorldSnapshot.h"
#include "Town/ArcTownSiteSnapshot.h"

DEFINE_LOG_CATEGORY(LogArcTownGrowth);

namespace ArcTownGrowthDiagnosticsPrivate
{
namespace
{
	const TCHAR* const UnitExpansionAttempt = TEXT("per-expansion-attempt");
	const TCHAR* const UnitYear = TEXT("per-year");
	const TCHAR* const UnitCatalogEntry = TEXT("per-catalog-entry-per-year");
	const TCHAR* const UnitBlock = TEXT("per-frontage-block-per-year");
	const TCHAR* const UnitFrontageRun = TEXT("per-frontage-run");
	const TCHAR* const UnitPlotAttempt = TEXT("per-plot-attempt");
	const TCHAR* const UnitEntryEvaluation = TEXT("per-entry-evaluation");
	const TCHAR* const UnitCandidateEvaluation = TEXT("per-candidate-evaluation");
	const TCHAR* const UnitRound = TEXT("per-round");
	const TCHAR* const UnitBridgeAttempt = TEXT("per-bridge-attempt");

	struct FRejectionReason
	{
		const TCHAR* Name = nullptr;
		const TCHAR* Unit = nullptr;
	};

	const FRejectionReason ExpansionReasons[] = {
		{TEXT("SlopeGate"), UnitExpansionAttempt},
		{TEXT("WaterClearance"), UnitExpansionAttempt},
		{TEXT("TerrainFieldsMissing"), UnitExpansionAttempt},
		{TEXT("OutsideVolumeBounds"), UnitExpansionAttempt},
		{TEXT("OutsideSite"), UnitExpansionAttempt},
		{TEXT("SnapTargetAlreadyConnected"), UnitExpansionAttempt},
		{TEXT("SplitTargetAlreadyConnected"), UnitExpansionAttempt},
		{TEXT("SplitProducedNoNode"), UnitExpansionAttempt},
		{TEXT("NoReopenableNode"), UnitYear},
		{TEXT("NoNewFrontSite"), UnitYear},
		{TEXT("BridgeSpanTooLong"), UnitBridgeAttempt},
	};
	static_assert(
		UE_ARRAY_COUNT(ExpansionReasons) == static_cast<int32>(EArcTownExpansionRejection::Count),
		"Every EArcTownExpansionRejection needs a name and a counting unit in the aggregate line.");

	const TCHAR* const RoadTraceEndNames[] = {
		TEXT("StartNodeInvalid"),
		TEXT("Snapped"),
		TEXT("Intersected"),
		TEXT("TerrainRejected"),
		TEXT("LeftVolumeBounds"),
		TEXT("LeftSite"),
		TEXT("StepLimit"),
	};
	static_assert(
		UE_ARRAY_COUNT(RoadTraceEndNames) == static_cast<int32>(EArcTownRoadTraceEnd::Count),
		"Every EArcTownRoadTraceEnd needs a name for the trace-end lines.");

	const FRejectionReason PlacementReasons[] = {
		{TEXT("CatalogEntryEarliestYear"), UnitCatalogEntry},
		{TEXT("CatalogEntryZeroWeight"), UnitCatalogEntry},
		{TEXT("CatalogEntryUnusableFootprint"), UnitCatalogEntry},
		{TEXT("NoEligibleCatalogEntry"), UnitYear},
		{TEXT("ZeroBuildingsRolled"), UnitYear},
		{TEXT("RunBelowMinFrontage"), UnitFrontageRun},
		{TEXT("BlockBoundaryUnusable"), UnitBlock},
		{TEXT("BlockSideDegenerate"), UnitBlock},
		{TEXT("BlockRunsCollapsed"), UnitBlock},
		{TEXT("RunHierarchyInadmissible"), UnitFrontageRun},
		{TEXT("PlotHierarchyInadmissible"), UnitPlotAttempt},
		{TEXT("PlotOutsideVolumeBounds"), UnitPlotAttempt},
		{TEXT("PlotOutsideSite"), UnitPlotAttempt},
		{TEXT("PlotNotBuildable"), UnitPlotAttempt},
		{TEXT("PlotWithoutUsableEntry"), UnitPlotAttempt},
		{TEXT("PlotWithoutPlotSize"), UnitPlotAttempt},
		{TEXT("PlotClippedAway"), UnitPlotAttempt},
		{TEXT("PlotOverlapsPlot"), UnitPlotAttempt},
		{TEXT("NoFreePlot"), UnitYear},
		{TEXT("RoadArrangementFailed"), UnitYear},
		{TEXT("EntryDeniedByMinDistance"), UnitEntryEvaluation},
		{TEXT("EntryScoreNotFinite"), UnitEntryEvaluation},
		{TEXT("BuildingLargerThanPlot"), UnitCandidateEvaluation},
		{TEXT("FrontageOverlapsRoadFronted"), UnitCandidateEvaluation},
		{TEXT("FrontageOverlapsRoadBuiltEdge"), UnitCandidateEvaluation},
		{TEXT("FrontageOverlapsRoadPlannedEdge"), UnitCandidateEvaluation},
		{TEXT("PlacedOutsideVolumeBounds"), UnitCandidateEvaluation},
		{TEXT("PlacedOutsideSite"), UnitCandidateEvaluation},
		{TEXT("PlacedNotBuildable"), UnitCandidateEvaluation},
		{TEXT("PlacedDeniedByMinDistance"), UnitCandidateEvaluation},
		{TEXT("RoundPlacedNothing"), UnitRound},
	};
	static_assert(
		UE_ARRAY_COUNT(PlacementReasons) == static_cast<int32>(EArcTownPlacementRejection::Count),
		"Every EArcTownPlacementRejection needs a name and a counting unit in the aggregate line.");

	bool IsSameUnit(const TCHAR* const Left, const TCHAR* const Right)
	{
		return FCString::Strcmp(Left, Right) == 0;
	}

	FString FormatTally(const int32* const Counts, const FRejectionReason* const Reasons, const int32 ReasonCount)
	{
		FString Line;
		for (int32 GroupIndex = 0; GroupIndex < ReasonCount; ++GroupIndex)
		{
			const TCHAR* const Unit = Reasons[GroupIndex].Unit;

			bool bAlreadyPrinted = false;
			for (int32 EarlierIndex = 0; EarlierIndex < GroupIndex; ++EarlierIndex)
			{
				if (IsSameUnit(Reasons[EarlierIndex].Unit, Unit))
				{
					bAlreadyPrinted = true;
					break;
				}
			}
			if (bAlreadyPrinted)
			{
				continue;
			}

			int32 UnitTotal = 0;
			for (int32 ReasonIndex = GroupIndex; ReasonIndex < ReasonCount; ++ReasonIndex)
			{
				if (IsSameUnit(Reasons[ReasonIndex].Unit, Unit))
				{
					UnitTotal += Counts[ReasonIndex];
				}
			}
			if (UnitTotal <= 0)
			{
				continue;
			}

			if (!Line.IsEmpty())
			{
				Line += TEXT("; ");
			}
			Line += FString::Printf(TEXT("%s (%d tallied):"), Unit, UnitTotal);

			bool bFirstInUnit = true;
			for (int32 ReasonIndex = GroupIndex; ReasonIndex < ReasonCount; ++ReasonIndex)
			{
				if (Counts[ReasonIndex] == 0 || !IsSameUnit(Reasons[ReasonIndex].Unit, Unit))
				{
					continue;
				}

				Line += FString::Printf(
					TEXT("%s %s=%d (%.1f%%)"),
					bFirstInUnit ? TEXT("") : TEXT(","),
					Reasons[ReasonIndex].Name,
					Counts[ReasonIndex],
					100.0 * static_cast<double>(Counts[ReasonIndex]) / static_cast<double>(UnitTotal));
				bFirstInUnit = false;
			}
		}

		return Line.IsEmpty() ? FString(TEXT("none")) : Line;
	}

	int32 CountBuiltEdges(const FArcTownWorldSnapshot& Snapshot)
	{
		int32 BuiltEdges = 0;
		for (const FArcTownRoadEdge& Edge : Snapshot.Edges)
		{
			if (Edge.bBuilt != 0)
			{
				++BuiltEdges;
			}
		}
		return BuiltEdges;
	}

	double ToMetres(const double Cm)
	{
		return Cm / 100.0;
	}

	FString TagsToString(const FGameplayTagContainer& Tags)
	{
		return Tags.IsEmpty() ? FString(TEXT("none")) : Tags.ToStringSimple();
	}

	constexpr int32 MaxCatalogEntriesLogged = 64;

	FString FormatWidthLadder(const FArcTownTrafficSettings& TrafficSettings)
	{
		FString Ladder;
		for (int32 Hierarchy = 0; Hierarchy <= static_cast<int32>(ArcTownTraffic::MaxRoadHierarchy); ++Hierarchy)
		{
			const double CarriagewayCm =
				2.0 * ArcTownTraffic::HalfWidthCmForHierarchy(TrafficSettings, static_cast<uint8>(Hierarchy));

			if (!Ladder.IsEmpty())
			{
				Ladder += TEXT(", ");
			}
			Ladder += FString::Printf(
				TEXT("h%d=%.0f cm (%.2f m)"), Hierarchy, CarriagewayCm, ToMetres(CarriagewayCm));
		}
		return Ladder;
	}
}
}

void ArcTownGrowthDiagnostics::CountBuildableSiteCells(
	const FArcTownSiteSnapshot& Site,
	const FArcTownSiteFields& Fields,
	int32& OutBuildableCells,
	int32& OutTotalCells)
{
	OutBuildableCells = 0;
	OutTotalCells = 0;

	const FIntPoint SiteDims = Site.GetSiteDims();
	for (int32 SiteY = 0; SiteY < SiteDims.Y; ++SiteY)
	{
		for (int32 SiteX = 0; SiteX < SiteDims.X; ++SiteX)
		{
			++OutTotalCells;

			const int32 CellIndex = Site.SnapshotIndex(SiteX, SiteY);
			if (Fields.bBuildable.IsValidIndex(CellIndex) && Fields.bBuildable[CellIndex] != 0)
			{
				++OutBuildableCells;
			}
		}
	}
}

void ArcTownGrowthDiagnostics::Accumulate(
	FArcTownGrowthRunReport& RunReport,
	const FArcTownExpansionYearReport& YearReport)
{
	RunReport.Expansion.AttemptBudget += YearReport.AttemptBudget;
	RunReport.Expansion.AttemptsRun += YearReport.AttemptsRun;
	RunReport.Expansion.NodesConsidered += YearReport.NodesConsidered;
	RunReport.Expansion.ExpansionsAttempted += YearReport.ExpansionsAttempted;
	RunReport.Expansion.EdgesAdded += YearReport.EdgesAdded;
	RunReport.Expansion.BridgesBuilt += YearReport.BridgesBuilt;
	RunReport.Expansion.NodesFinishedByValence += YearReport.NodesFinishedByValence;
	RunReport.Expansion.NodesFinishedByFailedExpansions += YearReport.NodesFinishedByFailedExpansions;
	RunReport.Expansion.StoppedWithoutEligibleNode += YearReport.StoppedWithoutEligibleNode;
	RunReport.Expansion.NodesReopened += YearReport.NodesReopened;
	RunReport.Expansion.NewFrontsSeeded += YearReport.NewFrontsSeeded;
	RunReport.Expansion.YearsSaturated += YearReport.YearsSaturated;
	RunReport.Expansion.RoadsTraced += YearReport.RoadsTraced;
	RunReport.Expansion.BranchesSeeded += YearReport.BranchesSeeded;
	RunReport.Expansion.BranchSeedsQueued += YearReport.BranchSeedsQueued;
	RunReport.Expansion.BranchSeedsDropped += YearReport.BranchSeedsDropped;
	RunReport.Expansion.BranchSeedsSkipped += YearReport.BranchSeedsSkipped;
	RunReport.Expansion.BranchRoadsTraced += YearReport.BranchRoadsTraced;
	RunReport.Expansion.PendingBranchSeeds = YearReport.PendingBranchSeeds;

	for (int32 ReasonIndex = 0; ReasonIndex < static_cast<int32>(EArcTownExpansionRejection::Count); ++ReasonIndex)
	{
		RunReport.Expansion.Rejections[ReasonIndex] += YearReport.Rejections[ReasonIndex];
	}

	for (int32 EndIndex = 0; EndIndex < static_cast<int32>(EArcTownRoadTraceEnd::Count); ++EndIndex)
	{
		RunReport.Expansion.RoadEnds[EndIndex] += YearReport.RoadEnds[EndIndex];
	}
}

void ArcTownGrowthDiagnostics::Accumulate(
	FArcTownGrowthRunReport& RunReport,
	const FArcTownPlacementYearReport& YearReport)
{
	RunReport.Placement.BuildingsRolled += YearReport.BuildingsRolled;
	RunReport.Placement.FrontagePolygons += YearReport.FrontagePolygons;
	RunReport.Placement.PlotsTiled += YearReport.PlotsTiled;
	RunReport.Placement.CandidatesEvaluated += YearReport.CandidatesEvaluated;
	RunReport.Placement.BuildingsPlaced += YearReport.BuildingsPlaced;
	RunReport.Placement.BlockRejections.OuterFaces += YearReport.BlockRejections.OuterFaces;
	RunReport.Placement.BlockRejections.TooFewSides += YearReport.BlockRejections.TooFewSides;
	RunReport.Placement.BlockRejections.NonSimpleBoundary += YearReport.BlockRejections.NonSimpleBoundary;
	RunReport.Placement.BlockRejections.AreaBelowThreshold += YearReport.BlockRejections.AreaBelowThreshold;
	RunReport.Placement.BlockRejections.FrontageCollapsed += YearReport.BlockRejections.FrontageCollapsed;
	RunReport.Placement.BlockRejections.StripBelowThreshold += YearReport.BlockRejections.StripBelowThreshold;
	RunReport.Placement.BlockRejections.StripDepthUnusable += YearReport.BlockRejections.StripDepthUnusable;

	for (int32 ReasonIndex = 0; ReasonIndex < static_cast<int32>(EArcTownPlacementRejection::Count); ++ReasonIndex)
	{
		RunReport.Placement.Rejections[ReasonIndex] += YearReport.Rejections[ReasonIndex];
	}
}

void ArcTownGrowthDiagnostics::Accumulate(
	FArcTownGrowthRunReport& RunReport,
	const FArcTownTrafficYearReport& YearReport)
{
	RunReport.Traffic.TripsRequested += YearReport.TripsRequested;
	RunReport.Traffic.TripsRouted += YearReport.TripsRouted;
	RunReport.Traffic.TripsWithoutReachableTarget += YearReport.TripsWithoutReachableTarget;
	RunReport.Traffic.RoadsPromoted += YearReport.RoadsPromoted;
	RunReport.Traffic.RoadsWidened += YearReport.RoadsWidened;
}

const TCHAR* ArcTownGrowthDiagnostics::RoadTraceEndName(const EArcTownRoadTraceEnd End)
{
	using namespace ArcTownGrowthDiagnosticsPrivate;

	const int32 EndIndex = static_cast<int32>(End);
	return EndIndex >= 0 && EndIndex < static_cast<int32>(EArcTownRoadTraceEnd::Count)
		? RoadTraceEndNames[EndIndex]
		: TEXT("Unknown");
}

FString ArcTownGrowthDiagnostics::FormatExpansionRejections(const FArcTownExpansionYearReport& Report)
{
	using namespace ArcTownGrowthDiagnosticsPrivate;

	return FormatTally(
		Report.Rejections, ExpansionReasons, static_cast<int32>(EArcTownExpansionRejection::Count));
}

FString ArcTownGrowthDiagnostics::FormatPlacementRejections(const FArcTownPlacementYearReport& Report)
{
	using namespace ArcTownGrowthDiagnosticsPrivate;

	return FormatTally(
		Report.Rejections, PlacementReasons, static_cast<int32>(EArcTownPlacementRejection::Count));
}

void ArcTownGrowthDiagnostics::LogBootstrap(const FArcTownBootstrapReport& Report)
{
	const TCHAR* const CoreSource =
		Report.bCoreFromAttractor ? TEXT("the strongest attractor") : TEXT("the site-term argmax");
	const double BuildablePercent = Report.TotalSiteCells > 0
		? 100.0 * static_cast<double>(Report.BuildableSiteCells) / static_cast<double>(Report.TotalSiteCells)
		: 0.0;

	UE_CLOGF(
		Report.bSeeded,
		LogArcTownGrowth,
		Log,"ArcTown seeded: core cell (%d, %d) at town-local (%.0f, %.0f) cm from %ls; genesis road of %d edge(s) laying %d branch seed(s) of which %d queued; %d of %d site cell(s) buildable (%.1f%%).",
		Report.CoreCell.X,
		Report.CoreCell.Y,
		Report.CoreTownLocalXY.X,
		Report.CoreTownLocalXY.Y,
		CoreSource,
		Report.GenesisRoadEdges,
		Report.GenesisBranchesSeeded,
		Report.GenesisBranchSeedsQueued,
		Report.BuildableSiteCells,
		Report.TotalSiteCells,
		BuildablePercent);

	UE_CLOGF(
		!Report.bSeeded,
		LogArcTownGrowth,
		Log,"ArcTown resumed: core cell (%d, %d) at town-local (%.0f, %.0f) cm from %ls; %d of %d site cell(s) buildable (%.1f%%).",
		Report.CoreCell.X,
		Report.CoreCell.Y,
		Report.CoreTownLocalXY.X,
		Report.CoreTownLocalXY.Y,
		CoreSource,
		Report.BuildableSiteCells,
		Report.TotalSiteCells,
		BuildablePercent);
}

void ArcTownGrowthDiagnostics::LogRunInputs(
	const FArcTownSimulationInput& Input,
	const FArcTownBootstrapReport& Bootstrap)
{
	using namespace ArcTownGrowthDiagnosticsPrivate;

	if (!UE_LOG_ACTIVE(LogArcTownGrowth, Log))
	{
		return;
	}

	const FVector TerrainScale = Input.Site.TerrainToWorld.GetScale3D();
	const double SiteExtentXCm =
		(static_cast<double>(Input.Site.GridDims.X) - 2.0 * Input.Site.MarginCells - 1.0) * Input.Site.SampleSpacing;
	const double SiteExtentYCm =
		(static_cast<double>(Input.Site.GridDims.Y) - 2.0 * Input.Site.MarginCells - 1.0) * Input.Site.SampleSpacing;
	const FVector VolumeExtentCm = Input.VolumeBoundsTownLocal.GetSize();

	UE_LOGF(
		LogArcTownGrowth,
		Log,"ArcTown run inputs: terrain scale (%.4f, %.4f, %.4f); site grid %dx%d cell(s) at %.1f cm (%.2f m) spacing with %d margin cell(s), giving a %.0f cm (%.2f m) x %.0f cm (%.2f m) site extent; town volume local bounds %.0f cm (%.2f m) x %.0f cm (%.2f m) x %.0f cm (%.2f m).",
		TerrainScale.X,
		TerrainScale.Y,
		TerrainScale.Z,
		Input.Site.GridDims.X,
		Input.Site.GridDims.Y,
		Input.Site.SampleSpacing,
		ToMetres(Input.Site.SampleSpacing),
		Input.Site.MarginCells,
		SiteExtentXCm,
		ToMetres(SiteExtentXCm),
		SiteExtentYCm,
		ToMetres(SiteExtentYCm),
		VolumeExtentCm.X,
		ToMetres(VolumeExtentCm.X),
		VolumeExtentCm.Y,
		ToMetres(VolumeExtentCm.Y),
		VolumeExtentCm.Z,
		ToMetres(VolumeExtentCm.Z));

	const double SnapRadiusCm = ArcTownRoadExpansion::ResolveSnapRadiusCm(Input.Settings.Roads);
	const bool bSnapRadiusAuthored =
		FMath::IsFinite(Input.Settings.Roads.RoadSnapRadiusCm) && Input.Settings.Roads.RoadSnapRadiusCm > 0.0;

	const double FreshEdgeMaxHalfWidthCm =
		ArcTownTraffic::MaxHalfWidthCm(Input.Settings.Traffic, Input.Settings.Roads.RoadHalfWidthCm);
	const double FreshEdgeWallDistanceCm =
		FreshEdgeMaxHalfWidthCm + ArcTownPlacement::PeriodSetbackCm(Input.Settings.Morphology);

	UE_LOGF(
		LogArcTownGrowth,
		Log,"ArcTown run inputs: StreetBlockSizeCm %.0f cm (%.2f m); RoadStepLengthCm %.0f cm (%.2f m); RoadSnapRadiusCm %.0f cm (%.2f m) [%ls]; MaxTurnPerStepDeg %.1f deg; ExpansionAngleDeviationDeg %.1f deg; TerrainHalfWeightSlopeDeg %.1f deg; BoundaryWeight %.2f; DensityHalfSaturationNodes %.1f node(s); HierarchyPromotionTopPercent %.1f%%; MaxStepsPerRoad %d; MaxNodeValence %d; fresh-edge max half-width %.0f cm (%.2f m) putting a fresh-edge building's street-facing wall %.0f cm (%.2f m) from the road axis; MaxRoadSlopeDeg %.1f deg; MinWaterClearanceCm %.0f cm (%.2f m); %d of %d site cell(s) buildable.",
		Input.Settings.Roads.StreetBlockSizeCm,
		ToMetres(Input.Settings.Roads.StreetBlockSizeCm),
		Input.Settings.Roads.RoadStepLengthCm,
		ToMetres(Input.Settings.Roads.RoadStepLengthCm),
		SnapRadiusCm,
		ToMetres(SnapRadiusCm),
		bSnapRadiusAuthored ? TEXT("authored") : TEXT("derived"),
		Input.Settings.Roads.MaxTurnPerStepDeg,
		Input.Settings.Roads.ExpansionAngleDeviationDeg,
		Input.Settings.TensorField.TerrainHalfWeightSlopeDeg,
		Input.Settings.TensorField.BoundaryWeight,
		Input.Settings.Roads.DensityHalfSaturationNodes,
		Input.Settings.Traffic.HierarchyPromotionTopPercent,
		Input.Settings.Roads.MaxStepsPerRoad,
		Input.Settings.Roads.MaxNodeValence,
		FreshEdgeMaxHalfWidthCm,
		ToMetres(FreshEdgeMaxHalfWidthCm),
		FreshEdgeWallDistanceCm,
		ToMetres(FreshEdgeWallDistanceCm),
		Input.Settings.Roads.MaxRoadSlopeDeg,
		Input.Settings.Fields.MinWaterClearanceCm,
		ToMetres(Input.Settings.Fields.MinWaterClearanceCm),
		Bootstrap.BuildableSiteCells,
		Bootstrap.TotalSiteCells);

	UE_LOGF(
		LogArcTownGrowth,
		Log,"ArcTown run inputs: road width ladder at RoadLaneWidthCm %.0f cm (%.2f m) per lane: %ls.",
		Input.Settings.Traffic.RoadLaneWidthCm,
		ToMetres(Input.Settings.Traffic.RoadLaneWidthCm),
		*FormatWidthLadder(Input.Settings.Traffic));

	const int32 CatalogCount = Input.Catalog.Num();
	UE_LOGF(
		LogArcTownGrowth,
		Log,"ArcTown catalog: %d entr%ls.",
		CatalogCount,
		CatalogCount == 1 ? TEXT("y") : TEXT("ies"));

	const int32 CatalogLinesToLog = FMath::Min(CatalogCount, MaxCatalogEntriesLogged);
	for (int32 CatalogIndex = 0; CatalogIndex < CatalogLinesToLog; ++CatalogIndex)
	{
		const FArcTownFrozenBuildingEntry& Entry = Input.Catalog[CatalogIndex];
		UE_LOGF(
			LogArcTownGrowth,
			Log,"ArcTown catalog[%d]: %ls; footprint %.0fx%.0f cm (%.2fx%.2f m); floor height %.0f cm (%.2f m) above pivot; influence radius %.0f cm (%.2f m); deny min distance %.0f cm (%.2f m); selection weight %.2f; earliest year %d; building tags %ls; preferred districts %ls; preferred buildings %ls; deny buildings %ls.",
			CatalogIndex,
			*Entry.VisualPath.ToString(),
			Entry.FootprintXY.X,
			Entry.FootprintXY.Y,
			ToMetres(Entry.FootprintXY.X),
			ToMetres(Entry.FootprintXY.Y),
			Entry.FloorHeightAbovePivot,
			ToMetres(Entry.FloorHeightAbovePivot),
			Entry.InfluenceRadiusCm,
			ToMetres(Entry.InfluenceRadiusCm),
			Entry.DenyMinDistanceCm,
			ToMetres(Entry.DenyMinDistanceCm),
			Entry.SelectionWeight,
			Entry.EarliestYear,
			*TagsToString(Entry.BuildingTags),
			*TagsToString(Entry.PreferredDistricts),
			*TagsToString(Entry.PreferredBuildings),
			*TagsToString(Entry.DenyBuildings));
	}

	UE_CLOGF(
		CatalogCount > CatalogLinesToLog,
		LogArcTownGrowth,
		Log,"ArcTown catalog: %d further entr%ls elided.",
		CatalogCount - CatalogLinesToLog,
		(CatalogCount - CatalogLinesToLog) == 1 ? TEXT("y") : TEXT("ies"));

	const int32 AttractorCount = Input.Attractors.Num();
	UE_CLOGF(AttractorCount == 0, LogArcTownGrowth, Log,"ArcTown attractors: none configured.");
	UE_CLOGF(AttractorCount > 0, LogArcTownGrowth, Log,"ArcTown attractors: %d configured.", AttractorCount);

	for (const FArcTownFrozenAttractor& Attractor : Input.Attractors)
	{
		UE_LOGF(
			LogArcTownGrowth,
			Log,"ArcTown attractor %ls: centre (%.2f, %.2f) m town-local; strength %.2f; influence radius %.0f cm (%.2f m); district tags %ls.",
			*Attractor.PathName,
			ToMetres(Attractor.CenterTownLocalXY.X),
			ToMetres(Attractor.CenterTownLocalXY.Y),
			Attractor.Strength,
			Attractor.InfluenceRadiusCm,
			ToMetres(Attractor.InfluenceRadiusCm),
			*TagsToString(Attractor.DistrictTags));
	}
}

void ArcTownGrowthDiagnostics::LogYear(
	const int32 Year,
	const FArcTownWorldSnapshot& Snapshot,
	const FArcTownExpansionYearReport& Expansion,
	const FArcTownPlacementYearReport& Placement,
	const FArcTownTrafficYearReport& Traffic)
{
	using namespace ArcTownGrowthDiagnosticsPrivate;

	const int32 BuiltEdges = CountBuiltEdges(Snapshot);

	UE_LOGF(
		LogArcTownGrowth,
		Verbose,"ArcTown year %d: %d node(s), %d edge(s) (%d built, %d planned), %d building(s), %d of %d trip(s) routed, %d road(s) promoted.",
		Year,
		Snapshot.Nodes.Num(),
		Snapshot.Edges.Num(),
		BuiltEdges,
		Snapshot.Edges.Num() - BuiltEdges,
		Snapshot.Buildings.Num(),
		Traffic.TripsRouted,
		Traffic.TripsRequested,
		Traffic.RoadsPromoted);

	UE_LOGF(
		LogArcTownGrowth,
		VeryVerbose,"ArcTown year %d expansion: %d of %d budgeted attempt(s) run, %d expansion(s) attempted over %d node evaluation(s), %d road(s) traced of which %d from a branch seed, %d edge(s) added of which %d bridge(s) over water; branch seeds %d laid, %d queued, %d dropped over the pending cap, %d skipped as ineligible, %d still pending; rejections %ls; %d node(s) finished by valence, %d by MaxFailedExpansions, %d node(s) reopened, %d new front(s) seeded.",
		Year,
		Expansion.AttemptsRun,
		Expansion.AttemptBudget,
		Expansion.ExpansionsAttempted,
		Expansion.NodesConsidered,
		Expansion.RoadsTraced,
		Expansion.BranchRoadsTraced,
		Expansion.EdgesAdded,
		Expansion.BridgesBuilt,
		Expansion.BranchesSeeded,
		Expansion.BranchSeedsQueued,
		Expansion.BranchSeedsDropped,
		Expansion.BranchSeedsSkipped,
		Expansion.PendingBranchSeeds,
		*FormatExpansionRejections(Expansion),
		Expansion.NodesFinishedByValence,
		Expansion.NodesFinishedByFailedExpansions,
		Expansion.NodesReopened,
		Expansion.NewFrontsSeeded);

	UE_LOGF(
		LogArcTownGrowth,
		VeryVerbose,"ArcTown year %d placement: %d building(s) rolled, %d placed, %d frontage polygon(s) of which %d block face(s) rejected, %d plot(s) tiled, %d candidate evaluation(s); rejections %ls.",
		Year,
		Placement.BuildingsRolled,
		Placement.BuildingsPlaced,
		Placement.FrontagePolygons,
		Placement.BlockRejections.Total(),
		Placement.PlotsTiled,
		Placement.CandidatesEvaluated,
		*FormatPlacementRejections(Placement));
}

void ArcTownGrowthDiagnostics::LogSaturated(const int32 Year, const FArcTownWorldSnapshot& Snapshot)
{
	UE_LOGF(
		LogArcTownGrowth,
		Log,"ArcTown growth saturated in year %d at %d node(s), %d edge(s), %d building(s): no finished junction has road-worthy free space beside it, and no cell inside the volume bounds is a street block clear of the network. Enlarge the town volume, shrink StreetBlockSizeCm, or relax MaxRoadSlopeDeg, MinWaterClearanceCm and MaxBuildSlopeDeg.",
		Year,
		Snapshot.Nodes.Num(),
		Snapshot.Edges.Num(),
		Snapshot.Buildings.Num());
}

void ArcTownGrowthDiagnostics::LogRunSummary(
	const FArcTownGrowthRunReport& RunReport,
	const FArcTownWorldSnapshot& Snapshot)
{
	using namespace ArcTownGrowthDiagnosticsPrivate;

	const int32 BuiltEdges = CountBuiltEdges(Snapshot);

	UE_LOGF(
		LogArcTownGrowth,
		Log,
		"ArcTown simulated %d year(s) to %d node(s), %d edge(s) (%d built, %d planned), %d building(s). "
		"Road expansion: %d of %d budgeted attempt(s) run, %d expansion(s) attempted over %d node evaluation(s), %d road(s) traced of which %d from a branch seed, %d edge(s) added of which %d bridge(s) over water; branch seeds %d laid, %d queued, %d dropped over the pending cap, %d skipped as ineligible, %d still pending; %d node(s) finished by valence, %d by MaxFailedExpansions, %d node(s) reopened, %d new front(s) seeded, %d year(s) stopped with no eligible node of which %d saturated; rejections %ls. "
		"Placement: %d building(s) rolled, %d placed, %d frontage polygon(s) of which %d block face(s) rejected, %d plot(s) tiled, %d candidate evaluation(s); rejections %ls. "
		"Traffic: %d of %d trip(s) routed, %d without a reachable target, %d road(s) promoted, %d widened.",
		RunReport.YearsSimulated,
		Snapshot.Nodes.Num(),
		Snapshot.Edges.Num(),
		BuiltEdges,
		Snapshot.Edges.Num() - BuiltEdges,
		Snapshot.Buildings.Num(),
		RunReport.Expansion.AttemptsRun,
		RunReport.Expansion.AttemptBudget,
		RunReport.Expansion.ExpansionsAttempted,
		RunReport.Expansion.NodesConsidered,
		RunReport.Expansion.RoadsTraced,
		RunReport.Expansion.BranchRoadsTraced,
		RunReport.Expansion.EdgesAdded,
		RunReport.Expansion.BridgesBuilt,
		RunReport.Expansion.BranchesSeeded,
		RunReport.Expansion.BranchSeedsQueued,
		RunReport.Expansion.BranchSeedsDropped,
		RunReport.Expansion.BranchSeedsSkipped,
		RunReport.Expansion.PendingBranchSeeds,
		RunReport.Expansion.NodesFinishedByValence,
		RunReport.Expansion.NodesFinishedByFailedExpansions,
		RunReport.Expansion.NodesReopened,
		RunReport.Expansion.NewFrontsSeeded,
		RunReport.Expansion.StoppedWithoutEligibleNode,
		RunReport.Expansion.YearsSaturated,
		*FormatExpansionRejections(RunReport.Expansion),
		RunReport.Placement.BuildingsRolled,
		RunReport.Placement.BuildingsPlaced,
		RunReport.Placement.FrontagePolygons,
		RunReport.Placement.BlockRejections.Total(),
		RunReport.Placement.PlotsTiled,
		RunReport.Placement.CandidatesEvaluated,
		*FormatPlacementRejections(RunReport.Placement),
		RunReport.Traffic.TripsRouted,
		RunReport.Traffic.TripsRequested,
		RunReport.Traffic.TripsWithoutReachableTarget,
		RunReport.Traffic.RoadsPromoted,
		RunReport.Traffic.RoadsWidened);
}
