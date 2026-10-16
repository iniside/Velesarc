// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcTownBootstrap.h"

#include "Simulation/ArcTownDeterministic.h"
#include "Simulation/ArcTownFieldSettings.h"
#include "Simulation/ArcTownGrowthDiagnostics.h"
#include "Simulation/ArcTownRoadExpansion.h"
#include "Simulation/ArcTownSiteFields.h"
#include "Simulation/ArcTownWorldSnapshot.h"
#include "Town/ArcTownSiteSnapshot.h"

namespace ArcTownBootstrapPrivate
{
namespace
{
	bool SetError(FText& OutError, const FString& Message)
	{
		OutError = FText::FromString(Message);
		return false;
	}

	double RatioTerm(const double Reference, const double Value)
	{
		if (!FMath::IsFinite(Reference) || Reference <= 0.0)
		{
			return 1.0;
		}
		if (!FMath::IsFinite(Value) || Value <= 0.0)
		{
			return 0.0;
		}
		return FMath::Clamp(Reference / Value, 0.0, 1.0);
	}

	int32 FindStrongestAttractor(TConstArrayView<FArcTownFrozenAttractor> Attractors)
	{
		int32 BestIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Attractors.Num(); ++Index)
		{
			const FArcTownFrozenAttractor& Candidate = Attractors[Index];
			if (!FMath::IsFinite(Candidate.Strength) || Candidate.Strength <= 0.0f)
			{
				continue;
			}

			if (BestIndex == INDEX_NONE)
			{
				BestIndex = Index;
				continue;
			}

			const FArcTownFrozenAttractor& Best = Attractors[BestIndex];
			if (Candidate.Strength > Best.Strength)
			{
				BestIndex = Index;
				continue;
			}
			if (Candidate.Strength == Best.Strength
				&& Candidate.PathName.Compare(Best.PathName, ESearchCase::CaseSensitive) < 0)
			{
				BestIndex = Index;
			}
		}
		return BestIndex;
	}

	int32 EnqueueBranchSeeds(
		const TArray<FArcTownRoadBranchSeed>& Produced,
		const int32 MaxPendingSeeds,
		FArcTownWorldSnapshot& Snapshot)
	{
		int32 QueuedCount = 0;
		for (const FArcTownRoadBranchSeed& Seed : Produced)
		{
			if (Snapshot.PendingBranchSeeds.Num() >= MaxPendingSeeds)
			{
				break;
			}

			Snapshot.PendingBranchSeeds.Add(Seed);
			++QueuedCount;
		}
		return QueuedCount;
	}

	void FinishOverloadedNodes(FArcTownWorldSnapshot& Snapshot, const int32 MaxNodeValence)
	{
		TArray<int32> Valence;
		Valence.SetNumZeroed(Snapshot.Nodes.Num());
		for (const FArcTownRoadEdge& Edge : Snapshot.Edges)
		{
			if (Valence.IsValidIndex(Edge.NodeA))
			{
				++Valence[Edge.NodeA];
			}
			if (Valence.IsValidIndex(Edge.NodeB))
			{
				++Valence[Edge.NodeB];
			}
		}

		for (int32 NodeIndex = 0; NodeIndex < Snapshot.Nodes.Num(); ++NodeIndex)
		{
			if (Valence[NodeIndex] >= MaxNodeValence)
			{
				Snapshot.Nodes[NodeIndex].bFinished = 1;
			}
		}
	}
}
}

bool ArcTownBootstrap::AreFieldsUsable(const FArcTownSiteSnapshot& Site, const FArcTownSiteFields& Fields)
{
	const int64 CellCount = static_cast<int64>(Site.GridDims.X) * static_cast<int64>(Site.GridDims.Y);
	return Fields.Dims == Site.GridDims
		&& CellCount > 0
		&& static_cast<int64>(Fields.Slope.Num()) == CellCount
		&& static_cast<int64>(Fields.DistanceToWaterCm.Num()) == CellCount
		&& static_cast<int64>(Fields.HeightAboveChannel.Num()) == CellCount
		&& static_cast<int64>(Fields.bBuildable.Num()) == CellCount;
}

double ArcTownBootstrap::SiteTerm(
	const FArcTownSiteFields& Fields,
	const FArcTownFieldSettings& FieldSettings,
	const int32 CellIndex)
{
	using namespace ArcTownBootstrapPrivate;

	if (!Fields.Slope.IsValidIndex(CellIndex)
		|| !Fields.DistanceToWaterCm.IsValidIndex(CellIndex)
		|| !Fields.HeightAboveChannel.IsValidIndex(CellIndex))
	{
		return 0.0;
	}

	const double MaxBuildSlope = FMath::Tan(FMath::DegreesToRadians(FieldSettings.MaxBuildSlopeDeg));
	const double Slope = static_cast<double>(Fields.Slope[CellIndex]);
	const double SlopeTerm = MaxBuildSlope > 0.0 && FMath::IsFinite(Slope)
		? FMath::Clamp(1.0 - Slope / MaxBuildSlope, 0.0, 1.0)
		: 0.0;

	const double WaterTerm =
		RatioTerm(FieldSettings.MinWaterClearanceCm, static_cast<double>(Fields.DistanceToWaterCm[CellIndex]));
	const double ChannelTerm =
		RatioTerm(FieldSettings.MinHeightAboveChannelM, static_cast<double>(Fields.HeightAboveChannel[CellIndex]));

	return (SlopeTerm + WaterTerm + ChannelTerm) / 3.0;
}

bool ArcTownBootstrap::ChooseCore(
	const FArcTownSiteSnapshot& Site,
	const FArcTownSiteFields& Fields,
	const FArcTownFieldSettings& FieldSettings,
	TConstArrayView<FArcTownFrozenAttractor> Attractors,
	const FGuid& SimulationId,
	const int32 Seed,
	FVector2f& OutCoreTownLocalXY,
	FArcTownBootstrapReport& OutReport,
	FText& OutError)
{
	using namespace ArcTownBootstrapPrivate;

	OutCoreTownLocalXY = FVector2f::ZeroVector;

	if (!Site.IsValid())
	{
		return SetError(OutError, TEXT("ArcTown cannot choose a town core without a valid sampled site; sample the site first."));
	}

	if (!AreFieldsUsable(Site, Fields))
	{
		return SetError(OutError, TEXT("ArcTown cannot choose a town core because the site fields do not match the sampled site; build the fields again."));
	}

	const FIntPoint SiteDims = Site.GetSiteDims();
	if (SiteDims.X <= 0 || SiteDims.Y <= 0)
	{
		return SetError(OutError, TEXT("ArcTown cannot choose a town core because the sampled site has no site cells outside its margin."));
	}

	const int32 StrongestIndex = FindStrongestAttractor(Attractors);
	if (StrongestIndex != INDEX_NONE)
	{
		const FArcTownFrozenAttractor& Attractor = Attractors[StrongestIndex];
		FIntPoint Cell = FIntPoint::ZeroValue;
		if (!ArcTownSiteFrame::TownLocalXYToCell(Site, Attractor.CenterTownLocalXY, Cell))
		{
			return SetError(OutError, FString::Printf(
				TEXT("Attractor %s is the strongest one this town sees, and its centre falls outside the sampled site, so the town core cannot stand there. Move it over the town volume or lower its Strength."),
				*Attractor.PathName));
		}

		const int32 CellIndex = Site.SnapshotIndex(Cell.X, Cell.Y);
		if (!Fields.bBuildable.IsValidIndex(CellIndex) || Fields.bBuildable[CellIndex] == 0)
		{
			return SetError(OutError, FString::Printf(
				TEXT("Attractor %s is the strongest one this town sees, and the terrain under its centre is not buildable, so the town core cannot stand there. Move it onto buildable ground or lower its Strength."),
				*Attractor.PathName));
		}

		OutCoreTownLocalXY = Attractor.CenterTownLocalXY;
		OutReport.CoreTownLocalXY = OutCoreTownLocalXY;
		OutReport.CoreCell = Cell;
		OutReport.bCoreFromAttractor = true;
		OutError = FText::GetEmpty();
		return true;
	}

	int32 BestCellIndex = INDEX_NONE;
	FIntPoint BestCell = FIntPoint::ZeroValue;
	double BestTerm = 0.0;
	for (int32 SiteY = 0; SiteY < SiteDims.Y; ++SiteY)
	{
		for (int32 SiteX = 0; SiteX < SiteDims.X; ++SiteX)
		{
			const int32 CellIndex = Site.SnapshotIndex(SiteX, SiteY);
			if (!Fields.bBuildable.IsValidIndex(CellIndex) || Fields.bBuildable[CellIndex] == 0)
			{
				continue;
			}

			const double Term = SiteTerm(Fields, FieldSettings, CellIndex);
			if (BestCellIndex == INDEX_NONE || Term > BestTerm)
			{
				BestCellIndex = CellIndex;
				BestCell = FIntPoint(SiteX, SiteY);
				BestTerm = Term;
			}
		}
	}

	if (BestCellIndex == INDEX_NONE)
	{
		return SetError(OutError, TEXT("No cell under this town volume is buildable, so the town has no core to grow from. Move the volume or relax MaxBuildSlopeDeg, MinWaterClearanceCm and MinHeightAboveChannelM."));
	}

	const FVector2f CellCenter = ArcTownSiteFrame::CellToTownLocalXY(Site, BestCell.X, BestCell.Y);
	const double JitterX = ArcTownDeterministic::KeyToUnitOpen(ArcTownDeterministic::MakeKey(
		SimulationId, Seed, ArcTownDeterministic::EDeterministicDomain::CoreSiteChoice, 0, 0)) - 0.5;
	const double JitterY = ArcTownDeterministic::KeyToUnitOpen(ArcTownDeterministic::MakeKey(
		SimulationId, Seed, ArcTownDeterministic::EDeterministicDomain::CoreSiteChoice, 0, 1)) - 0.5;

	OutCoreTownLocalXY = FVector2f(
		CellCenter.X + static_cast<float>(JitterX * Site.SampleSpacing),
		CellCenter.Y + static_cast<float>(JitterY * Site.SampleSpacing));
	OutReport.CoreTownLocalXY = OutCoreTownLocalXY;
	OutReport.CoreCell = BestCell;
	OutReport.bCoreFromAttractor = false;
	OutError = FText::GetEmpty();
	return true;
}

bool ArcTownBootstrap::Seed(
	const FArcTownSiteSnapshot& Site,
	const FArcTownSiteFields& Fields,
	const FArcTownFieldSettings& FieldSettings,
	const FArcTownBootstrapSettings& BootstrapSettings,
	const FArcTownRoadSettings& RoadSettings,
	const FArcTownTensorFieldSettings& TensorSettings,
	TConstArrayView<FArcTownFrozenAttractor> Attractors,
	const FBox2D& VolumeBoundsTownLocalXY,
	FArcTownWorldSnapshot& Snapshot,
	FVector2f& OutCoreTownLocalXY,
	FArcTownBootstrapReport& OutReport,
	FText& OutError)
{
	using namespace ArcTownBootstrapPrivate;

	OutCoreTownLocalXY = FVector2f::ZeroVector;

	if (Snapshot.Nodes.Num() != 0 || Snapshot.Edges.Num() != 0)
	{
		return SetError(OutError, TEXT("ArcTown seeds a town only into an empty simulation state; reset the simulation before seeding it again."));
	}

	if (!FMath::IsFinite(RoadSettings.RoadStepLengthCm) || RoadSettings.RoadStepLengthCm <= 0.0
		|| RoadSettings.MaxStepsPerRoad < 1
		|| !FMath::IsFinite(BootstrapSettings.SeedRoadHalfWidthCm)
		|| BootstrapSettings.SeedRoadHalfWidthCm <= 0.0)
	{
		return SetError(OutError, TEXT("ArcTown needs a positive RoadStepLengthCm, at least one step in MaxStepsPerRoad, and a positive SeedRoadHalfWidthCm to seed a town."));
	}

	if (!ArcTownRoadExpansion::IsMaxRoadSlopeDegUsable(RoadSettings))
	{
		return SetError(OutError, ArcTownRoadExpansion::MaxRoadSlopeDegRequirementText);
	}

	if (!ChooseCore(
		Site,
		Fields,
		FieldSettings,
		Attractors,
		Snapshot.SimulationId,
		Snapshot.Seed,
		OutCoreTownLocalXY,
		OutReport,
		OutError))
	{
		return false;
	}

	FArcTownTensorField Field;
	Field.Site = &Site;
	Field.Fields = &Fields;
	Field.Attractors = Attractors;
	Field.Settings = TensorSettings;
	Field.CoreTownLocalXY = OutCoreTownLocalXY;

	FArcTownRoadNode CoreNode;
	CoreNode.LocalXY = OutCoreTownLocalXY;
	const int32 CoreNodeIndex = Snapshot.Nodes.Add(CoreNode);

	FArcTownRoadTraceParams Params;
	Params.Field = &Field;
	Params.FieldSettings = &FieldSettings;
	Params.RoadSettings = &RoadSettings;
	Params.VolumeBoundsTownLocalXY = VolumeBoundsTownLocalXY;
	Params.Year = 0;
	Params.StartNodeIndex = CoreNodeIndex;
	Params.StartHeadingRad = ArcTownRoadExpansion::ResolveStartHeadingRad(
		Snapshot,
		CoreNodeIndex,
		FVector2D(static_cast<double>(OutCoreTownLocalXY.X), static_cast<double>(OutCoreTownLocalXY.Y)),
		0);
	Params.bFollowMinorField =
		ArcTownRoadExpansion::ResolveFollowMinorField(Field, OutCoreTownLocalXY, Params.StartHeadingRad);
	Params.HalfWidthCm = BootstrapSettings.SeedRoadHalfWidthCm;
	Params.bBuilt = true;

	FArcTownExpansionYearReport TraceReport;
	FArcTownRoadTraceResult Forward;
	ArcTownRoadExpansion::TraceRoad(Params, Snapshot, TraceReport, Forward);

	Params.StartHeadingRad += UE_DOUBLE_PI;
	Params.RoadId = Forward.EdgesAdded > 0 ? Forward.RoadId : INDEX_NONE;

	FArcTownRoadTraceResult Backward;
	ArcTownRoadExpansion::TraceRoad(Params, Snapshot, TraceReport, Backward);

	const int32 GenesisEdges = Forward.EdgesAdded + Backward.EdgesAdded;
	if (GenesisEdges == 0)
	{
		Snapshot.Nodes.Reset();
		Snapshot.Edges.Reset();
		Snapshot.PendingBranchSeeds.Reset();
		Snapshot.NextRoadId = 0;
		return SetError(OutError, FString::Printf(
			TEXT("The town core at (%.0f, %.0f) cm has no room for a genesis road that stays inside the sampled site on road-worthy ground: the trace through the core ended %s one way and %s the other, and its gates tallied %s. Enlarge the town volume, shorten RoadStepLengthCm, or relax MaxRoadSlopeDeg and MinWaterClearanceCm."),
			OutCoreTownLocalXY.X,
			OutCoreTownLocalXY.Y,
			ArcTownGrowthDiagnostics::RoadTraceEndName(Forward.End),
			ArcTownGrowthDiagnostics::RoadTraceEndName(Backward.End),
			*ArcTownGrowthDiagnostics::FormatExpansionRejections(TraceReport)));
	}

	const int32 GenesisBranchesSeeded = Forward.BranchSeeds.Num() + Backward.BranchSeeds.Num();
	const int32 MaxPendingSeeds = ArcTownRoadExpansion::ResolveMaxPendingBranchSeeds(RoadSettings, Snapshot);
	int32 GenesisBranchSeedsQueued = EnqueueBranchSeeds(Forward.BranchSeeds, MaxPendingSeeds, Snapshot);
	GenesisBranchSeedsQueued += EnqueueBranchSeeds(Backward.BranchSeeds, MaxPendingSeeds, Snapshot);

	FinishOverloadedNodes(Snapshot, RoadSettings.MaxNodeValence);
	Snapshot.CanonicalizeAndRefreshChecksum();

	OutReport.GenesisRoadEdges = GenesisEdges;
	OutReport.GenesisBranchesSeeded = GenesisBranchesSeeded;
	OutReport.GenesisBranchSeedsQueued = GenesisBranchSeedsQueued;
	OutReport.bSeeded = true;
	OutError = FText::GetEmpty();
	return true;
}
