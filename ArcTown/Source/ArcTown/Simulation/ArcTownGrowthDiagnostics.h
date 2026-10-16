// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Morphology/ArcTownBlocks.h"
#include "Simulation/ArcTownTraffic.h"

struct FArcTownSimulationInput;
struct FArcTownSiteFields;
struct FArcTownSiteSnapshot;
struct FArcTownWorldSnapshot;

ARCTOWN_API DECLARE_LOG_CATEGORY_EXTERN(LogArcTownGrowth, Log, All);

enum class EArcTownExpansionRejection : uint8
{
	SlopeGate,
	WaterClearance,
	TerrainFieldsMissing,
	OutsideVolumeBounds,
	OutsideSite,
	SnapTargetAlreadyConnected,
	SplitTargetAlreadyConnected,
	SplitProducedNoNode,
	NoReopenableNode,
	NoNewFrontSite,
	BridgeSpanTooLong,
	Count
};

enum class EArcTownRoadTraceEnd : uint8
{
	StartNodeInvalid,
	Snapped,
	Intersected,
	TerrainRejected,
	LeftVolumeBounds,
	LeftSite,
	StepLimit,
	Count
};

struct FArcTownExpansionYearReport
{
	int32 AttemptBudget = 0;

	int32 AttemptsRun = 0;

	int32 NodesConsidered = 0;

	int32 ExpansionsAttempted = 0;

	int32 EdgesAdded = 0;

	int32 BridgesBuilt = 0;

	int32 NodesFinishedByValence = 0;

	int32 NodesFinishedByFailedExpansions = 0;

	int32 StoppedWithoutEligibleNode = 0;

	int32 NodesReopened = 0;

	int32 NewFrontsSeeded = 0;

	int32 YearsSaturated = 0;

	int32 RoadsTraced = 0;

	int32 BranchesSeeded = 0;

	int32 BranchSeedsQueued = 0;

	int32 BranchSeedsDropped = 0;

	int32 BranchSeedsSkipped = 0;

	int32 BranchRoadsTraced = 0;

	int32 PendingBranchSeeds = 0;

	int32 Rejections[static_cast<int32>(EArcTownExpansionRejection::Count)] = {};

	int32 RoadEnds[static_cast<int32>(EArcTownRoadTraceEnd::Count)] = {};
};

enum class EArcTownPlacementRejection : uint8
{
	CatalogEntryEarliestYear,
	CatalogEntryZeroWeight,
	CatalogEntryUnusableFootprint,
	NoEligibleCatalogEntry,
	ZeroBuildingsRolled,
	RunBelowMinFrontage,
	BlockBoundaryUnusable,
	BlockSideDegenerate,
	BlockRunsCollapsed,
	RunHierarchyInadmissible,
	PlotHierarchyInadmissible,
	PlotOutsideVolumeBounds,
	PlotOutsideSite,
	PlotNotBuildable,
	PlotWithoutUsableEntry,
	PlotWithoutPlotSize,
	PlotClippedAway,
	PlotOverlapsPlot,
	NoFreePlot,
	RoadArrangementFailed,
	EntryDeniedByMinDistance,
	EntryScoreNotFinite,
	BuildingLargerThanPlot,
	FrontageOverlapsRoadFronted,
	FrontageOverlapsRoadBuiltEdge,
	FrontageOverlapsRoadPlannedEdge,
	PlacedOutsideVolumeBounds,
	PlacedOutsideSite,
	PlacedNotBuildable,
	PlacedDeniedByMinDistance,
	RoundPlacedNothing,
	Count
};

struct FArcTownPlacementYearReport
{
	int32 BuildingsRolled = 0;

	int32 FrontagePolygons = 0;

	int32 PlotsTiled = 0;

	int32 CandidatesEvaluated = 0;

	int32 BuildingsPlaced = 0;

	FArcTownBlockRejections BlockRejections;

	int32 Rejections[static_cast<int32>(EArcTownPlacementRejection::Count)] = {};
};

struct FArcTownBootstrapReport
{
	FVector2f CoreTownLocalXY = FVector2f::ZeroVector;

	FIntPoint CoreCell = FIntPoint::ZeroValue;

	bool bCoreFromAttractor = false;

	bool bSeeded = false;

	int32 GenesisRoadEdges = 0;

	int32 GenesisBranchesSeeded = 0;

	int32 GenesisBranchSeedsQueued = 0;

	int32 BuildableSiteCells = 0;

	int32 TotalSiteCells = 0;
};

struct FArcTownGrowthRunReport
{
	int32 YearsSimulated = 0;

	FArcTownExpansionYearReport Expansion;

	FArcTownPlacementYearReport Placement;

	FArcTownTrafficYearReport Traffic;
};

namespace ArcTownGrowthDiagnostics
{
	inline void Tally(FArcTownExpansionYearReport& Report, const EArcTownExpansionRejection Reason)
	{
		++Report.Rejections[static_cast<int32>(Reason)];
	}

	inline void Tally(FArcTownPlacementYearReport& Report, const EArcTownPlacementRejection Reason)
	{
		++Report.Rejections[static_cast<int32>(Reason)];
	}

	ARCTOWN_API void CountBuildableSiteCells(
		const FArcTownSiteSnapshot& Site,
		const FArcTownSiteFields& Fields,
		int32& OutBuildableCells,
		int32& OutTotalCells);

	ARCTOWN_API void Accumulate(FArcTownGrowthRunReport& RunReport, const FArcTownExpansionYearReport& YearReport);

	ARCTOWN_API void Accumulate(FArcTownGrowthRunReport& RunReport, const FArcTownPlacementYearReport& YearReport);

	ARCTOWN_API void Accumulate(FArcTownGrowthRunReport& RunReport, const FArcTownTrafficYearReport& YearReport);

	ARCTOWN_API const TCHAR* RoadTraceEndName(EArcTownRoadTraceEnd End);

	ARCTOWN_API FString FormatExpansionRejections(const FArcTownExpansionYearReport& Report);

	ARCTOWN_API FString FormatPlacementRejections(const FArcTownPlacementYearReport& Report);

	ARCTOWN_API void LogBootstrap(const FArcTownBootstrapReport& Report);

	/** Once-per-run dump of the frozen inputs and their resolved derived values; a log-only reader-facing block. */
	ARCTOWN_API void LogRunInputs(const FArcTownSimulationInput& Input, const FArcTownBootstrapReport& Bootstrap);

	ARCTOWN_API void LogYear(
		int32 Year,
		const FArcTownWorldSnapshot& Snapshot,
		const FArcTownExpansionYearReport& Expansion,
		const FArcTownPlacementYearReport& Placement,
		const FArcTownTrafficYearReport& Traffic);

	ARCTOWN_API void LogSaturated(int32 Year, const FArcTownWorldSnapshot& Snapshot);

	ARCTOWN_API void LogRunSummary(const FArcTownGrowthRunReport& RunReport, const FArcTownWorldSnapshot& Snapshot);
}
