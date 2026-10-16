// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcTownGrowthEngine.h"

#include "Layout/ArcTownRoadArrangement.h"
#include "Morphology/ArcTownBlocks.h"
#include "Simulation/ArcTownDeterministic.h"
#include "Simulation/ArcTownGrowthDiagnostics.h"
#include "Simulation/ArcTownSiteFields.h"

#define LOCTEXT_NAMESPACE "ArcTownGrowthEngine"

namespace ArcTownGrowthEnginePrivate
{
namespace
{
	constexpr uint64 SettingsHashKey = 0x7B1E4C6DA92F3085ull;

	constexpr double QuantaPerCentimetre = 1000.0;

	constexpr double MaxQuantizedMagnitude = 9.0e15;

	constexpr double ArrangementSnapToleranceInSamples = 0.1;

	bool SetError(FText& OutError, FText&& Message)
	{
		OutError = MoveTemp(Message);
		return false;
	}

	void AddQuantized(ArcTownDeterministic::FKeyedHash64& Hash, const double Value)
	{
		const double Scaled = FMath::IsFinite(Value) ? Value * QuantaPerCentimetre : 0.0;
		const double Clamped = FMath::Clamp(Scaled, -MaxQuantizedMagnitude, MaxQuantizedMagnitude);
		Hash.AddInt64(static_cast<int64>(FMath::RoundToDouble(Clamped)));
	}

	void AddDoubleBits(ArcTownDeterministic::FKeyedHash64& Hash, double Value)
	{
		if (Value == 0.0)
		{
			Value = 0.0;
		}

		uint64 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		Hash.AddUInt64(Bits);
	}

	void AddFloatBits(ArcTownDeterministic::FKeyedHash64& Hash, float Value)
	{
		if (Value == 0.0f)
		{
			Value = 0.0f;
		}

		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		Hash.AddUInt32(Bits);
	}

	void AddString(ArcTownDeterministic::FKeyedHash64& Hash, const FString& Value)
	{
		Hash.AddInt32(Value.Len());
		for (const TCHAR Character : Value)
		{
			Hash.AddUInt32(static_cast<uint32>(Character));
		}
	}

	void AddTags(ArcTownDeterministic::FKeyedHash64& Hash, const FGameplayTagContainer& Tags)
	{
		TArray<FString> Names;
		Names.Reserve(Tags.Num());
		for (const FGameplayTag& Tag : Tags)
		{
			Names.Add(Tag.GetTagName().ToString());
		}
		Names.Sort(
			[](const FString& Left, const FString& Right)
			{
				return Left.Compare(Right, ESearchCase::CaseSensitive) < 0;
			});

		Hash.AddInt32(Names.Num());
		for (const FString& Name : Names)
		{
			AddString(Hash, Name);
		}
	}

	void AddBox(ArcTownDeterministic::FKeyedHash64& Hash, const FBox& Box)
	{
		Hash.AddUInt8(Box.IsValid != 0 ? 1 : 0);
		AddQuantized(Hash, Box.Min.X);
		AddQuantized(Hash, Box.Min.Y);
		AddQuantized(Hash, Box.Min.Z);
		AddQuantized(Hash, Box.Max.X);
		AddQuantized(Hash, Box.Max.Y);
		AddQuantized(Hash, Box.Max.Z);
	}

	void AddCatalog(ArcTownDeterministic::FKeyedHash64& Hash, TConstArrayView<FArcTownFrozenBuildingEntry> Catalog)
	{
		Hash.AddInt32(Catalog.Num());
		for (const FArcTownFrozenBuildingEntry& Entry : Catalog)
		{
			AddString(Hash, Entry.VisualPath.ToString());
			AddDoubleBits(Hash, Entry.FootprintXY.X);
			AddDoubleBits(Hash, Entry.FootprintXY.Y);
			AddDoubleBits(Hash, Entry.PlotSizeMinXY.X);
			AddDoubleBits(Hash, Entry.PlotSizeMinXY.Y);
			AddDoubleBits(Hash, Entry.PlotSizeMaxXY.X);
			AddDoubleBits(Hash, Entry.PlotSizeMaxXY.Y);
			AddDoubleBits(Hash, Entry.FloorHeightAbovePivot);
			AddDoubleBits(Hash, Entry.MeshLocalDoorForwardXY.X);
			AddDoubleBits(Hash, Entry.MeshLocalDoorForwardXY.Y);
			AddTags(Hash, Entry.BuildingTags);
			AddDoubleBits(Hash, Entry.InfluenceRadiusCm);
			AddTags(Hash, Entry.PreferredDistricts);
			AddTags(Hash, Entry.PreferredBuildings);
			AddTags(Hash, Entry.DenyBuildings);
			AddDoubleBits(Hash, Entry.DenyMinDistanceCm);
			AddFloatBits(Hash, Entry.SelectionWeight);
			Hash.AddInt32(Entry.EarliestYear);
			Hash.AddUInt8(Entry.MinHierarchy);
			Hash.AddUInt8(Entry.MaxHierarchy);
		}
	}

	void AddAttractors(ArcTownDeterministic::FKeyedHash64& Hash, TConstArrayView<FArcTownFrozenAttractor> Attractors)
	{
		Hash.AddInt32(Attractors.Num());
		for (const FArcTownFrozenAttractor& Attractor : Attractors)
		{
			AddString(Hash, Attractor.PathName);
			AddQuantized(Hash, static_cast<double>(Attractor.CenterTownLocalXY.X));
			AddQuantized(Hash, static_cast<double>(Attractor.CenterTownLocalXY.Y));
			AddFloatBits(Hash, Attractor.Strength);
			AddDoubleBits(Hash, Attractor.InfluenceRadiusCm);
			AddTags(Hash, Attractor.DistrictTags);
		}
	}

	void AddGrowthSettings(ArcTownDeterministic::FKeyedHash64& Hash, const FArcTownGrowthSettings& Settings)
	{
		AddDoubleBits(Hash, Settings.Fields.ElevationQuantumM);
		AddDoubleBits(Hash, Settings.Fields.MaxBuildSlopeDeg);
		AddDoubleBits(Hash, Settings.Fields.MinWaterClearanceCm);
		AddDoubleBits(Hash, Settings.Fields.MinHeightAboveChannelM);

		AddDoubleBits(Hash, Settings.Bootstrap.SeedRoadHalfWidthCm);

		AddDoubleBits(Hash, Settings.TensorField.RadialWeight);
		AddDoubleBits(Hash, Settings.TensorField.GridWeight);
		AddDoubleBits(Hash, Settings.TensorField.BoundaryWeight);
		AddDoubleBits(Hash, Settings.TensorField.TerrainHalfWeightSlopeDeg);
		AddDoubleBits(Hash, Settings.TensorField.GridAngleDeg);
		AddDoubleBits(Hash, Settings.TensorField.CoreFalloffCm);

		Hash.AddInt32(Settings.Roads.MaxFailedExpansions);
		AddDoubleBits(Hash, Settings.Roads.MaxRoadSlopeDeg);
		AddDoubleBits(Hash, Settings.Roads.StreetBlockSizeCm);
		AddDoubleBits(Hash, Settings.Roads.RoadStepLengthCm);
		AddDoubleBits(Hash, Settings.Roads.RoadSnapRadiusCm);
		Hash.AddInt32(Settings.Roads.MaxNodeValence);
		AddDoubleBits(Hash, Settings.Roads.MaxTurnPerStepDeg);
		AddDoubleBits(Hash, Settings.Roads.ExpansionAngleDeviationDeg);
		AddDoubleBits(Hash, Settings.Roads.BranchSpacingJitterCm);
		Hash.AddInt32(Settings.Roads.MaxStepsPerRoad);
		Hash.AddInt32(Settings.Roads.RoadsPerYear);
		AddDoubleBits(Hash, Settings.Roads.DensityHalfSaturationNodes);
		AddDoubleBits(Hash, Settings.Roads.RoadHalfWidthCm);
		AddDoubleBits(Hash, Settings.Roads.MaxBridgeSpanCm);

		AddDoubleBits(Hash, Settings.Traffic.TripsPerBuildingPerYear);
		Hash.AddInt32(Settings.Traffic.MinTripsPerYear);
		AddDoubleBits(Hash, Settings.Traffic.HalfAttractionDistanceCm);
		AddDoubleBits(Hash, Settings.Traffic.PromotionThresholdPerYear);
		AddDoubleBits(Hash, Settings.Traffic.HierarchyPromotionTopPercent);
		AddDoubleBits(Hash, Settings.Traffic.RoadLaneWidthCm);
		Hash.AddInt32(Settings.Traffic.HierarchyLaneCounts.Num());
		for (const double LaneCount : Settings.Traffic.HierarchyLaneCounts)
		{
			AddDoubleBits(Hash, LaneCount);
		}
		Hash.AddInt32(Settings.Traffic.HierarchyPromotionTripsPerYear.Num());
		for (const double PromotionTrips : Settings.Traffic.HierarchyPromotionTripsPerYear)
		{
			AddDoubleBits(Hash, PromotionTrips);
		}
		AddDoubleBits(Hash, Settings.Traffic.TrafficDecayPerYear);

		AddDoubleBits(Hash, Settings.Placement.BuildingsPerYear);
		AddDoubleBits(Hash, Settings.Placement.BuildingsPerStandingBuildingPerYear);
		AddDoubleBits(Hash, Settings.Placement.DistrictWeight);
		AddDoubleBits(Hash, Settings.Placement.BuildingWeight);
		AddDoubleBits(Hash, Settings.Placement.DenyWeight);
		AddDoubleBits(Hash, Settings.Placement.SiteWeight);

		Hash.AddUInt8(static_cast<uint8>(Settings.Morphology.Period));
		AddDoubleBits(Hash, Settings.Morphology.MinFrontageRunCm);
		AddDoubleBits(Hash, Settings.Morphology.SetbackCm);
		AddDoubleBits(Hash, Settings.Morphology.YawJitterDeg);
		AddDoubleBits(Hash, Settings.Morphology.DefaultPlotGapCm);
	}

	bool MatchesResumeStamps(const FArcTownSimulationInput& Input, FText& OutError)
	{
		const FArcTownWorldSnapshot& Source = Input.SourceSnapshot;

		if (Source.RuleVersion != Input.RuleVersion)
		{
			return SetError(OutError, FText::Format(
				LOCTEXT("ResumeRuleVersion", "This town's simulation was produced by rule version {0}, and the current rules are version {1}. Reset Simulation to start over."),
				FText::AsNumber(static_cast<int32>(Source.RuleVersion)),
				FText::AsNumber(static_cast<int32>(Input.RuleVersion))));
		}

		if (Source.Seed != Input.Seed)
		{
			return SetError(OutError, FText::Format(
				LOCTEXT("ResumeSeed", "This town's simulation state was grown from seed {0}, and the town's seed is now {1}. Reset Simulation to start over."),
				FText::AsNumber(Source.Seed),
				FText::AsNumber(Input.Seed)));
		}

		if (Source.SimulationId != Input.SimulationId)
		{
			return SetError(OutError, LOCTEXT("ResumeSimulationId", "This town's simulation state belongs to a different town. Reset Simulation to start over."));
		}

		if (Source.SiteSnapshotHash != Input.SiteSnapshotHash)
		{
			return SetError(OutError, LOCTEXT("ResumeSiteHash", "This town's sampled terrain changed since its simulation state was grown. Reset Simulation to start over."));
		}

		if (Source.SettingsHash != Input.SettingsHash)
		{
			return SetError(OutError, LOCTEXT("ResumeSettingsHash", "This town's growth settings, building catalog, attractors or bounds changed since its simulation state was grown. Reset Simulation to start over."));
		}

		return true;
	}

	FBox2D MakeBounds2D(const FBox& Bounds)
	{
		return FBox2D(FVector2D(Bounds.Min.X, Bounds.Min.Y), FVector2D(Bounds.Max.X, Bounds.Max.Y));
	}
}
}

uint64 ArcTownGrowthEngine::ComputeSettingsHash(
	const FArcTownGrowthSettings& Settings,
	const FBox& VolumeBoundsTownLocal,
	TConstArrayView<FArcTownFrozenBuildingEntry> Catalog,
	TConstArrayView<FArcTownFrozenAttractor> SortedAttractors)
{
	using namespace ArcTownGrowthEnginePrivate;

	ArcTownDeterministic::FKeyedHash64 Hash(SettingsHashKey);
	AddBox(Hash, VolumeBoundsTownLocal);
	AddCatalog(Hash, Catalog);
	AddAttractors(Hash, SortedAttractors);
	AddGrowthSettings(Hash, Settings);
	return Hash.Finalize();
}

void ArcTownGrowthEngine::SortAttractors(TArray<FArcTownFrozenAttractor>& Attractors)
{
	Attractors.Sort(
		[](const FArcTownFrozenAttractor& Left, const FArcTownFrozenAttractor& Right)
		{
			return Left.PathName.Compare(Right.PathName, ESearchCase::CaseSensitive) < 0;
		});
}

bool ArcTownGrowthEngine::Run(
	const FArcTownSimulationInput& Input,
	TFunctionRef<bool(int32 CompletedYears)> OnYearBegin,
	FArcTownWorldSnapshot& OutSnapshot,
	FArcTownTrafficYearReport& OutFinalYearTraffic,
	FText& OutError)
{
	using namespace ArcTownGrowthEnginePrivate;

	OutSnapshot = FArcTownWorldSnapshot();
	OutFinalYearTraffic = FArcTownTrafficYearReport();
	OutError = FText::GetEmpty();

	if (Input.RuleVersion != ArcTownDeterministic::SimulationRuleVersion)
	{
		return SetError(OutError, LOCTEXT("InputRuleVersion", "This simulation run was frozen against a different rule version. Reset Simulation to start over."));
	}

	if (!Input.SimulationId.IsValid())
	{
		return SetError(OutError, LOCTEXT("InputSimulationId", "This simulation run carries no simulation id. Reset Simulation to start over."));
	}

	if (Input.YearsToSimulate <= 0)
	{
		return SetError(OutError, LOCTEXT("InputYears", "A simulation run needs at least one year to simulate."));
	}

	if (!Input.Site.IsValid())
	{
		return SetError(OutError, LOCTEXT("InputSite", "This town has no usable sampled terrain. Sample Site, then simulate again."));
	}

	if (!ArcTownSiteFrame::HasUsableExtentXY(Input.VolumeBoundsTownLocal))
	{
		return SetError(OutError, LOCTEXT("InputBounds", "This town's volume bounds are empty, so roads have nowhere to grow."));
	}

	FArcTownSiteFields Fields;
	if (!ArcTownSiteFields::BuildSiteFields(Input.Site, Input.Settings.Fields, Fields, OutError))
	{
		return false;
	}

	FArcTownBootstrapReport BootstrapReport;
	if (UE_LOG_ACTIVE(LogArcTownGrowth, Log))
	{
		ArcTownGrowthDiagnostics::CountBuildableSiteCells(
			Input.Site, Fields, BootstrapReport.BuildableSiteCells, BootstrapReport.TotalSiteCells);
	}

	FArcTownWorldSnapshot Snapshot;
	FVector2f CoreTownLocalXY = FVector2f::ZeroVector;
	const FBox2D BoundsTownLocalXY = MakeBounds2D(Input.VolumeBoundsTownLocal);

	// A default-constructed state carries RuleVersion 0, which Validate would report as stale rules.
	if (Input.SourceSnapshot.SimulationId.IsValid())
	{
		if (!MatchesResumeStamps(Input, OutError))
		{
			return false;
		}

		if (!Input.SourceSnapshot.Validate(Input.Site, OutError))
		{
			return false;
		}

		Snapshot = Input.SourceSnapshot;

		if (!ArcTownBootstrap::ChooseCore(
			Input.Site,
			Fields,
			Input.Settings.Fields,
			Input.Attractors,
			Snapshot.SimulationId,
			Snapshot.Seed,
			CoreTownLocalXY,
			BootstrapReport,
			OutError))
		{
			return false;
		}
	}
	else
	{
		Snapshot.RuleVersion = Input.RuleVersion;
		Snapshot.SimulationId = Input.SimulationId;
		Snapshot.Seed = Input.Seed;
		Snapshot.SiteSnapshotHash = Input.SiteSnapshotHash;
		Snapshot.SettingsHash = Input.SettingsHash;
		Snapshot.Year = 0;

		if (!ArcTownBootstrap::Seed(
			Input.Site,
			Fields,
			Input.Settings.Fields,
			Input.Settings.Bootstrap,
			Input.Settings.Roads,
			Input.Settings.TensorField,
			Input.Attractors,
			BoundsTownLocalXY,
			Snapshot,
			CoreTownLocalXY,
			BootstrapReport,
			OutError))
		{
			return false;
		}
	}

	ArcTownGrowthDiagnostics::LogBootstrap(BootstrapReport);
	ArcTownGrowthDiagnostics::LogRunInputs(Input, BootstrapReport);

	FArcTownTensorField Field;
	Field.Site = &Input.Site;
	Field.Fields = &Fields;
	Field.Attractors = Input.Attractors;
	Field.Settings = Input.Settings.TensorField;
	Field.CoreTownLocalXY = CoreTownLocalXY;

	const int32 StartYear = Snapshot.Year;

	FArcTownGrowthRunReport RunReport;
	bool bSaturationReported = false;

	for (int32 YearOffset = 0; YearOffset < Input.YearsToSimulate; ++YearOffset)
	{
		if (!OnYearBegin(YearOffset))
		{
			return SetError(OutError, LOCTEXT("RunCancelled", "The simulation run was cancelled."));
		}

		const int32 Year = StartYear + YearOffset + 1;

		// Placement follows promotion so a building fronts a street built this year; decay stays last.
		FArcTownExpansionYearReport ExpansionReport;
		if (!ArcTownRoadExpansion::ExpandRoads(
			Field,
			Input.Settings.Fields,
			Input.Settings.Roads,
			BoundsTownLocalXY,
			Year,
			Snapshot,
			ExpansionReport,
			OutError))
		{
			return false;
		}

		FArcTownTrafficYearReport TrafficReport;
		ArcTownTraffic::AccumulateTraffic(Input.Settings.Traffic, Year, Snapshot, TrafficReport);
		ArcTownTraffic::PromoteRoads(Input.Settings.Traffic, Year, Snapshot, TrafficReport);
		OutFinalYearTraffic = TrafficReport;

		const double MaxPlotDepthCm = ArcTownPlacement::MaxPlotDepthCm(
			Input.Settings.Morphology, Input.Catalog, Year);

		TArray<FArcTownBlock> Frontage;
		FArcTownBlockRejections BlockRejections;
		ArcTownRoadArrangement::FArrangement Arrangement;
		const double ArrangementSnapToleranceCm =
			ArrangementSnapToleranceInSamples * Input.Site.SampleSpacing;
		const bool bArrangementBuilt =
			ArcTownRoadArrangement::Build(Snapshot, ArrangementSnapToleranceCm, Arrangement);
		if (bArrangementBuilt)
		{
			ArcTownBlocks::BuildFrontage(
				Arrangement,
				Input.Settings.Traffic,
				MaxPlotDepthCm,
				FMath::Max(Input.Settings.Morphology.MinFrontageRunCm, 0.0) * MaxPlotDepthCm,
				Frontage,
				BlockRejections);
		}

		FArcTownPlacementYearReport PlacementReport;
		if (!ArcTownPlacement::PlaceBuildings(
			Input.Site,
			Fields,
			Input.Settings.Fields,
			Input.Settings.Placement,
			Input.Settings.Morphology,
			Input.Settings.Traffic,
			Input.Catalog,
			Input.Attractors,
			Frontage,
			ArrangementSnapToleranceCm,
			BoundsTownLocalXY,
			Year,
			Snapshot,
			PlacementReport,
			OutError))
		{
			return false;
		}

		PlacementReport.FrontagePolygons = Frontage.Num();
		PlacementReport.BlockRejections = BlockRejections;
		if (!bArrangementBuilt)
		{
			ArcTownGrowthDiagnostics::Tally(
				PlacementReport, EArcTownPlacementRejection::RoadArrangementFailed);
		}

		ArcTownTraffic::DecayTraffic(Input.Settings.Traffic, Snapshot);

		Snapshot.Year = Year;
		Snapshot.CanonicalizeAndRefreshChecksum();

		if (ExpansionReport.YearsSaturated > 0 && !bSaturationReported)
		{
			bSaturationReported = true;
			ArcTownGrowthDiagnostics::LogSaturated(Year, Snapshot);
		}

		++RunReport.YearsSimulated;
		ArcTownGrowthDiagnostics::Accumulate(RunReport, ExpansionReport);
		ArcTownGrowthDiagnostics::Accumulate(RunReport, PlacementReport);
		ArcTownGrowthDiagnostics::Accumulate(RunReport, TrafficReport);
		ArcTownGrowthDiagnostics::LogYear(Year, Snapshot, ExpansionReport, PlacementReport, TrafficReport);
	}

	ArcTownGrowthDiagnostics::LogRunSummary(RunReport, Snapshot);

	OutSnapshot = MoveTemp(Snapshot);
	return true;
}

#undef LOCTEXT_NAMESPACE
