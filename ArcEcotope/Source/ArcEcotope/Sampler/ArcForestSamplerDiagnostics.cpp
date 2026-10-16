// Copyright Lukasz Baran. All Rights Reserved.

#include "Sampler/ArcForestSamplerDiagnostics.h"

#include "Determinism/ArcForestDeterministic.h"
#include "Sampler/ArcForestSamplerKeys.h"

DEFINE_LOG_CATEGORY(LogArcForestSampler);

namespace ArcForestSamplerDiagnosticsPrivate
{
	constexpr int32 MinimumPairSampleStems = 200;
	constexpr int32 PairRingCount = 6;
	constexpr double PairRangeCrownRadii = 4.0;
	constexpr int32 ReferenceReplicateCount = 3;
	constexpr int32 MaximumOverlapReports = 8;
	constexpr int64 MaximumGridCells = 1ll << 22;
	constexpr int32 MaximumGridCellsPerAxis = 2048;

	enum class EReferenceKeySlot : int64
	{
		UsableCell = 0,
		OffsetX = 1,
		OffsetY = 2,
	};

	class FPairCountingGrid
	{
	public:
		void Build(
			const TArray<FVector2D>& Points,
			const FVector2D& InOriginCm,
			const FVector2D& ExtentCm,
			const double MinimumCellSizeCm)
		{
			OriginCm = InOriginCm;
			const double ExtentXCm = FMath::Max(ExtentCm.X, 1.0);
			const double ExtentYCm = FMath::Max(ExtentCm.Y, 1.0);
			const int64 TargetCellCount =
				FMath::Clamp<int64>(static_cast<int64>(Points.Num()) * 4, 64, MaximumGridCells);
			const double NominalCellSizeCm = FMath::Max(
				FMath::Max(MinimumCellSizeCm, 1.0),
				FMath::Sqrt(ExtentXCm * ExtentYCm / static_cast<double>(TargetCellCount)));
			CellsX = FMath::Clamp(FMath::CeilToInt32(ExtentXCm / NominalCellSizeCm), 1, MaximumGridCellsPerAxis);
			CellsY = FMath::Clamp(FMath::CeilToInt32(ExtentYCm / NominalCellSizeCm), 1, MaximumGridCellsPerAxis);
			CellSizeXCm = FMath::Max(ExtentXCm / static_cast<double>(CellsX), 1.0);
			CellSizeYCm = FMath::Max(ExtentYCm / static_cast<double>(CellsY), 1.0);

			const int32 CellCount = CellsX * CellsY;
			CellStart.Init(0, CellCount + 1);
			TArray<int32> CellOfPoint;
			CellOfPoint.SetNumUninitialized(Points.Num());
			for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
			{
				const int32 CellIndex = FindCellIndex(Points[PointIndex]);
				CellOfPoint[PointIndex] = CellIndex;
				++CellStart[CellIndex + 1];
			}
			for (int32 CellIndex = 0; CellIndex < CellCount; ++CellIndex)
			{
				CellStart[CellIndex + 1] += CellStart[CellIndex];
			}
			TArray<int32> Cursor = CellStart;
			Items.SetNumUninitialized(Points.Num());
			for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
			{
				Items[Cursor[CellOfPoint[PointIndex]]++] = PointIndex;
			}
		}

		void GatherNeighbours(const FVector2D& Centre, const double RadiusCm, TArray<int32>& OutIndices) const
		{
			OutIndices.Reset();
			const int32 SpanX = FMath::Max(0, FMath::CeilToInt32(RadiusCm / CellSizeXCm));
			const int32 SpanY = FMath::Max(0, FMath::CeilToInt32(RadiusCm / CellSizeYCm));
			const int32 CentreX = FindAxisCell(Centre.X - OriginCm.X, CellSizeXCm, CellsX);
			const int32 CentreY = FindAxisCell(Centre.Y - OriginCm.Y, CellSizeYCm, CellsY);
			for (int32 CellY = FMath::Max(0, CentreY - SpanY); CellY <= FMath::Min(CellsY - 1, CentreY + SpanY); ++CellY)
			{
				for (int32 CellX = FMath::Max(0, CentreX - SpanX); CellX <= FMath::Min(CellsX - 1, CentreX + SpanX); ++CellX)
				{
					const int32 CellIndex = CellX + CellY * CellsX;
					for (int32 Slot = CellStart[CellIndex]; Slot < CellStart[CellIndex + 1]; ++Slot)
					{
						OutIndices.Add(Items[Slot]);
					}
				}
			}
		}

	private:
		static int32 FindAxisCell(const double Offset, const double CellSizeCm, const int32 CellCount)
		{
			return FMath::Clamp(FMath::FloorToInt32(Offset / CellSizeCm), 0, CellCount - 1);
		}

		int32 FindCellIndex(const FVector2D& Position) const
		{
			return FindAxisCell(Position.X - OriginCm.X, CellSizeXCm, CellsX)
				+ FindAxisCell(Position.Y - OriginCm.Y, CellSizeYCm, CellsY) * CellsX;
		}

		FVector2D OriginCm = FVector2D::ZeroVector;
		double CellSizeXCm = 1.0;
		double CellSizeYCm = 1.0;
		int32 CellsX = 1;
		int32 CellsY = 1;
		TArray<int32> CellStart;
		TArray<int32> Items;
	};

	void AccumulateRingPairs(
		const TArray<FVector2D>& Points,
		const FVector2D& OriginCm,
		const FVector2D& ExtentCm,
		const double MaximumRadiusCm,
		const double RingWidthCm,
		TArray<int64>& OutRingPairs)
	{
		if (Points.Num() < 2 || OutRingPairs.IsEmpty())
		{
			return;
		}
		FPairCountingGrid Grid;
		Grid.Build(Points, OriginCm, ExtentCm, MaximumRadiusCm);
		TArray<int32> Neighbours;
		for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
		{
			Grid.GatherNeighbours(Points[PointIndex], MaximumRadiusCm, Neighbours);
			for (const int32 OtherIndex : Neighbours)
			{
				if (OtherIndex <= PointIndex)
				{
					continue;
				}
				const double DistanceCm = FVector2D::Distance(Points[OtherIndex], Points[PointIndex]);
				if (DistanceCm >= MaximumRadiusCm)
				{
					continue;
				}
				const int32 Ring = FMath::Clamp(
					FMath::FloorToInt32(DistanceCm / RingWidthCm), 0, OutRingPairs.Num() - 1);
				++OutRingPairs[Ring];
			}
		}
	}

	double FindMedianCrownRadiusCm(
		const TArrayView<const UE::ArcForest::Sampler::FArcForestSamplerDiagnosticsStem>& Stems)
	{
		TArray<double> Radii;
		Radii.Reserve(Stems.Num());
		for (const UE::ArcForest::Sampler::FArcForestSamplerDiagnosticsStem& Stem : Stems)
		{
			Radii.Add(Stem.CrownRadiusCm);
		}
		if (Radii.IsEmpty())
		{
			return 0.0;
		}
		Radii.Sort();
		return Radii[Radii.Num() / 2];
	}

	FString DescribeRate(const int64 Count, const int64 Total)
	{
		if (Total <= 0)
		{
			return FString::Printf(TEXT("%lld"), Count);
		}
		return FString::Printf(
			TEXT("%lld(%.3f)"), Count, static_cast<double>(Count) / static_cast<double>(Total));
	}

	double DrawReferenceUnit(
		const UE::ArcForest::Sampler::FArcForestSamplerDiagnosticsContext& Context,
		const int32 RecordCellIndex,
		const int64 StemOrdinal,
		const EReferenceKeySlot Slot)
	{
		return UE::ArcForest::KeyToUnitOpen(UE::ArcForest::Sampler::MakeSamplerKey(
			Context.SimulationId,
			Context.Seed,
			UE::ArcForest::Sampler::SamplerDiagnosticsReference,
			FGuid(),
			RecordCellIndex,
			StemOrdinal + static_cast<int64>(Slot)));
	}
}

void UE::ArcForest::Sampler::ReportSamplerDiagnostics(const FArcForestSamplerDiagnosticsContext& Context)
{
	using namespace ArcForestSamplerDiagnosticsPrivate;

	const int32 StemCount = Context.Stems.Num();
	const int32 BandCount = Context.DescendingBands.Num();
	const double BaseCellAreaCm2 = Context.BaseCellSizeCm * Context.BaseCellSizeCm;
	const double UsableAreaCm2 = static_cast<double>(Context.UsableBaseCells.Num()) * BaseCellAreaCm2;

	TArray<int32> RealizedByEntry;
	RealizedByEntry.Init(0, BandCount);
	double CrownAreaCm2 = 0.0;
	int32 UnclassifiedStemCount = 0;
	int32 PassCount = 0;
	for (const FArcForestSamplerDiagnosticsStem& Stem : Context.Stems)
	{
		if (RealizedByEntry.IsValidIndex(Stem.EntryOrdinal))
		{
			++RealizedByEntry[Stem.EntryOrdinal];
		}
		else
		{
			++UnclassifiedStemCount;
		}
		CrownAreaCm2 += UE_DOUBLE_PI * FMath::Square(Stem.CrownRadiusCm);
		PassCount = FMath::Max(PassCount, Stem.PassIndex + 1);
	}

	UE_LOGF(
		LogArcForestSampler,
		Log,"ArcForest sampler diagnostics | stems=%d bands=%d usable=%.2f ha unevenAgedness=%.2f",
		StemCount,
		BandCount,
		UsableAreaCm2 / 1.0e8,
		Context.UnevenAgedness);

	double ExpectedTotal = 0.0;
	int64 RealizedTotal = 0;
	for (int32 BandOrdinal = 0; BandOrdinal < BandCount; ++BandOrdinal)
	{
		const FArcForestSamplerDiagnosticsBand& Band = Context.DescendingBands[BandOrdinal];
		const double Expected = Band.ExpectedStems;
		const int32 Realized = RealizedByEntry[BandOrdinal];
		ExpectedTotal += Expected;
		RealizedTotal += Realized;
		const FString RatioText = Expected > 0.0
			? FString::Printf(TEXT("%.3f"), static_cast<double>(Realized) / Expected)
			: FString(TEXT("n/a"));
		UE_LOGF(
			LogArcForestSampler,
			Log,"  band %2d species=%ls authored=%d [%.0f..%.0f cm] expected=%.1f realized=%d ratio=%ls",
			BandOrdinal,
			*Band.SpeciesId.ToString().Left(8),
			Band.BandIndex,
			Band.MinHeightCm,
			Band.MaxHeightCm,
			Expected,
			Realized,
			*RatioText);
	}

	UE_CLOGF(
		BandCount > 0,
		LogArcForestSampler,
		Log,"  size classes: expected=%.1f realized=%lld ratio=%.3f, summed over the record cells that produced a target.",
		ExpectedTotal,
		RealizedTotal,
		ExpectedTotal > 0.0 ? static_cast<double>(RealizedTotal) / ExpectedTotal : 0.0);
	UE_CLOGF(
		UnclassifiedStemCount > 0,
		LogArcForestSampler,
		Warning,"  %d stem(s) carry no band ordinal, so the realized counts above miss them.",
		UnclassifiedStemCount);

	FString OverlapReport;
	int32 OverlapCount = 0;
	for (int32 LeftOrdinal = 0; LeftOrdinal < BandCount; ++LeftOrdinal)
	{
		for (int32 RightOrdinal = LeftOrdinal + 1; RightOrdinal < BandCount; ++RightOrdinal)
		{
			const FArcForestSamplerDiagnosticsBand& Left = Context.DescendingBands[LeftOrdinal];
			const FArcForestSamplerDiagnosticsBand& Right = Context.DescendingBands[RightOrdinal];
			if (Left.SpeciesId != Right.SpeciesId
				|| Left.MinHeightCm >= Right.MaxHeightCm
				|| Right.MinHeightCm >= Left.MaxHeightCm)
			{
				continue;
			}
			++OverlapCount;
			if (OverlapCount <= MaximumOverlapReports)
			{
				OverlapReport += FString::Printf(TEXT(" %d/%d"), LeftOrdinal, RightOrdinal);
			}
		}
	}
	UE_CLOGF(
		OverlapCount > 0,
		LogArcForestSampler,
		Warning,"  %d band ordinal pair(s) of one species share a height range (%ls), so those rows are not disjoint size classes and their targets both cover the shared range.",
		OverlapCount,
		*OverlapReport);

	const bool bOccupancyAvailable = UsableAreaCm2 > 0.0 && Context.MaximumCrownOccupancy > 0.0;
	const double RealizedOccupancy = UsableAreaCm2 > 0.0 ? CrownAreaCm2 / UsableAreaCm2 : 0.0;
	UE_CLOGF(
		bOccupancyAvailable,
		LogArcForestSampler,
		Log,"  crown occupancy realized=%.3f ceiling=%.3f ratio=%.3f over the %.2f ha of usable ground inside emitting record cells.",
		RealizedOccupancy,
		Context.MaximumCrownOccupancy,
		Context.MaximumCrownOccupancy > 0.0 ? RealizedOccupancy / Context.MaximumCrownOccupancy : 0.0,
		UsableAreaCm2 / 1.0e8);
	UE_CLOGF(
		!bOccupancyAvailable,
		LogArcForestSampler,
		Warning,"  crown occupancy unavailable: %.0f cm2 of usable ground against a ceiling of %.3f.",
		UsableAreaCm2,
		Context.MaximumCrownOccupancy);

	const FArcForestSamplerDiagnosticsRejections& Rejections = Context.Rejections;
	const int64 GeneratedCandidates = Rejections.GeneratedCandidates;
	const int64 AccountedCandidates = Rejections.OffUsableSurface
		+ Rejections.MissingSurfaceZ
		+ Rejections.NoBandWithHabitat
		+ Rejections.NoBandDrawn
		+ Rejections.AcceptanceRejected
		+ Rejections.Inhibited
		+ static_cast<int64>(StemCount);
	UE_LOGF(
		LogArcForestSampler,
		Log,"  candidates=%lld -> placed=%ls offSurface=%ls noSurfaceZ=%ls noHabitatBand=%ls noBandDrawn=%ls acceptance=%ls inhibition=%ls",
		GeneratedCandidates,
		*DescribeRate(static_cast<int64>(StemCount), GeneratedCandidates),
		*DescribeRate(Rejections.OffUsableSurface, GeneratedCandidates),
		*DescribeRate(Rejections.MissingSurfaceZ, GeneratedCandidates),
		*DescribeRate(Rejections.NoBandWithHabitat, GeneratedCandidates),
		*DescribeRate(Rejections.NoBandDrawn, GeneratedCandidates),
		*DescribeRate(Rejections.AcceptanceRejected, GeneratedCandidates),
		*DescribeRate(Rejections.Inhibited, GeneratedCandidates));
	UE_CLOGF(
		AccountedCandidates != GeneratedCandidates,
		LogArcForestSampler,
		Warning,"  the candidate split accounts for %lld of %lld draws, so the rates above are not a partition.",
		AccountedCandidates,
		GeneratedCandidates);
	UE_CLOGF(
		Rejections.SkippedRecordCells > 0,
		LogArcForestSampler,
		Warning,"  %lld record cell visit(s) produced no candidate at all because the cell rectangle could not be resolved.",
		Rejections.SkippedRecordCells);

	const double MedianCrownRadiusCm = FindMedianCrownRadiusCm(Context.Stems);
	const double WindowXCm = FMath::Max(Context.BaseDimensions.X * Context.BaseCellSizeCm, 1.0);
	const double WindowYCm = FMath::Max(Context.BaseDimensions.Y * Context.BaseCellSizeCm, 1.0);
	const FVector2D WindowExtentCm(WindowXCm, WindowYCm);
	const double MaximumPairRadiusCm = PairRangeCrownRadii * MedianCrownRadiusCm;
	const bool bPairEstimateAvailable = StemCount >= MinimumPairSampleStems
		&& MedianCrownRadiusCm > 0.0
		&& PassCount > 0
		&& Context.BaseDimensions.X > 0
		&& !Context.UsableBaseCells.IsEmpty()
		&& MaximumPairRadiusCm < FMath::Min(WindowXCm, WindowYCm);

	UE_CLOGF(
		StemCount < MinimumPairSampleStems,
		LogArcForestSampler,
		Log,"  g(r): too few samples (%d stems, %d needed).",
		StemCount,
		MinimumPairSampleStems);
	UE_CLOGF(
		StemCount >= MinimumPairSampleStems && !bPairEstimateAvailable,
		LogArcForestSampler,
		Warning,"  g(r) unavailable: a median crown radius of %.1f cm over %d pass(es) and %d usable cell(s) does not fit six rings inside a %.0fx%.0f cm window.",
		MedianCrownRadiusCm,
		PassCount,
		Context.UsableBaseCells.Num(),
		WindowXCm,
		WindowYCm);

	if (!bPairEstimateAvailable)
	{
		return;
	}

	TMap<int32, int32> EmittingCellByRecordCell;
	EmittingCellByRecordCell.Reserve(Context.EmittingCells.Num());
	for (int32 CellIndex = 0; CellIndex < Context.EmittingCells.Num(); ++CellIndex)
	{
		EmittingCellByRecordCell.Add(Context.EmittingCells[CellIndex].RecordCellIndex, CellIndex);
	}

	TArray<TArray<int32>> StemsByPass;
	StemsByPass.SetNum(PassCount);
	for (int32 StemIndex = 0; StemIndex < StemCount; ++StemIndex)
	{
		const int32 PassIndex = Context.Stems[StemIndex].PassIndex;
		if (StemsByPass.IsValidIndex(PassIndex))
		{
			StemsByPass[PassIndex].Add(StemIndex);
		}
	}

	const double RingWidthCm = MaximumPairRadiusCm / static_cast<double>(PairRingCount);
	TArray<int64> ObservedRingPairs;
	ObservedRingPairs.Init(0, PairRingCount);
	TArray<FVector2D> Points;
	// Stage B resolves survivors only inside one band pass, so a cross-pass pair carries no inhibition at all and would pull the estimate toward the reference.
	for (const TArray<int32>& PassStems : StemsByPass)
	{
		Points.Reset();
		Points.Reserve(PassStems.Num());
		for (const int32 StemIndex : PassStems)
		{
			Points.Add(Context.Stems[StemIndex].PositionCm);
		}
		AccumulateRingPairs(
			Points, Context.BaseOriginCm, WindowExtentCm, MaximumPairRadiusCm, RingWidthCm, ObservedRingPairs);
	}

	// The reference is redrawn from this run's own stems - same per-pass counts, same record cells, same usable ground - because a Poisson reference would fold the fragmented usable region, the deterministic per-cell candidate counts and the stage A crown-scale thinning into the number and read them as inhibition.
	TArray<double> ReferenceSums;
	ReferenceSums.Init(0.0, PairRingCount);
	TArray<double> ReferenceMinimums;
	ReferenceMinimums.Init(TNumericLimits<double>::Max(), PairRingCount);
	TArray<double> ReferenceMaximums;
	ReferenceMaximums.Init(0.0, PairRingCount);
	TArray<int64> ReplicateRingPairs;
	bool bReferenceComplete = true;
	for (int32 Replicate = 0; Replicate < ReferenceReplicateCount && bReferenceComplete; ++Replicate)
	{
		ReplicateRingPairs.Init(0, PairRingCount);
		for (const TArray<int32>& PassStems : StemsByPass)
		{
			Points.Reset();
			Points.Reserve(PassStems.Num());
			for (const int32 StemIndex : PassStems)
			{
				const FArcForestSamplerDiagnosticsStem& Stem = Context.Stems[StemIndex];
				const int32* EmittingIndex = EmittingCellByRecordCell.Find(Stem.RecordCellIndex);
				if (EmittingIndex == nullptr || Context.EmittingCells[*EmittingIndex].UsableCount <= 0)
				{
					bReferenceComplete = false;
					break;
				}
				const FArcForestSamplerDiagnosticsCell& Cell = Context.EmittingCells[*EmittingIndex];
				const int64 StemOrdinal =
					(static_cast<int64>(Replicate) * static_cast<int64>(StemCount) + StemIndex) * 4ll;
				const int32 UsableOffset = FMath::Clamp(
					FMath::FloorToInt32(
						DrawReferenceUnit(Context, Stem.RecordCellIndex, StemOrdinal, EReferenceKeySlot::UsableCell)
						* static_cast<double>(Cell.UsableCount)),
					0,
					Cell.UsableCount - 1);
				const int32 BaseCellIndex = Context.UsableBaseCells[Cell.UsableStart + UsableOffset];
				const int32 BaseX = BaseCellIndex % Context.BaseDimensions.X;
				const int32 BaseY = BaseCellIndex / Context.BaseDimensions.X;
				Points.Emplace(
					Context.BaseOriginCm.X + (static_cast<double>(BaseX)
						+ DrawReferenceUnit(Context, Stem.RecordCellIndex, StemOrdinal, EReferenceKeySlot::OffsetX))
						* Context.BaseCellSizeCm,
					Context.BaseOriginCm.Y + (static_cast<double>(BaseY)
						+ DrawReferenceUnit(Context, Stem.RecordCellIndex, StemOrdinal, EReferenceKeySlot::OffsetY))
						* Context.BaseCellSizeCm);
			}
			if (!bReferenceComplete)
			{
				break;
			}
			AccumulateRingPairs(
				Points, Context.BaseOriginCm, WindowExtentCm, MaximumPairRadiusCm, RingWidthCm, ReplicateRingPairs);
		}
		if (!bReferenceComplete)
		{
			break;
		}
		for (int32 Ring = 0; Ring < PairRingCount; ++Ring)
		{
			const double ReplicatePairs = static_cast<double>(ReplicateRingPairs[Ring]);
			ReferenceSums[Ring] += ReplicatePairs;
			ReferenceMinimums[Ring] = FMath::Min(ReferenceMinimums[Ring], ReplicatePairs);
			ReferenceMaximums[Ring] = FMath::Max(ReferenceMaximums[Ring], ReplicatePairs);
		}
	}

	if (!bReferenceComplete)
	{
		UE_LOGF(
			LogArcForestSampler,
			Warning,"  g(r) unavailable: a placed stem sits in a record cell that reported no usable ground, so the randomized reference cannot be drawn.");
		return;
	}

	FString RingReport;
	for (int32 Ring = 0; Ring < PairRingCount; ++Ring)
	{
		const double InnerCm = Ring * RingWidthCm;
		const double OuterCm = InnerCm + RingWidthCm;
		const double MeanReferencePairs = ReferenceSums[Ring] / static_cast<double>(ReferenceReplicateCount);
		const double Observed = static_cast<double>(ObservedRingPairs[Ring]);
		if (MeanReferencePairs <= 0.0)
		{
			RingReport += FString::Printf(
				TEXT(" [%.1f-%.1f m]=n/a(obs=%lld ref=0)"),
				InnerCm / 100.0,
				OuterCm / 100.0,
				ObservedRingPairs[Ring]);
			continue;
		}
		const FString BandUpperText = ReferenceMinimums[Ring] > 0.0
			? FString::Printf(TEXT("%.2f"), Observed / ReferenceMinimums[Ring])
			: FString(TEXT("inf"));
		RingReport += FString::Printf(
			TEXT(" [%.1f-%.1f m]=%.2f(obs=%lld ref=%.0f band %.2f-%s)"),
			InnerCm / 100.0,
			OuterCm / 100.0,
			Observed / MeanReferencePairs,
			ObservedRingPairs[Ring],
			MeanReferencePairs,
			Observed / ReferenceMaximums[Ring],
			*BandUpperText);
	}

	UE_LOGF(
		LogArcForestSampler,
		Log,"  g(r) within-pass out to %.1f m (4x the %.2f m median crown radius), observed pairs over %d randomized redraws of the same stems:%ls",
		MaximumPairRadiusCm / 100.0,
		MedianCrownRadiusCm / 100.0,
		ReferenceReplicateCount,
		*RingReport);
	UE_LOGF(
		LogArcForestSampler,
		Log,"  g(r) reads 1 where the placed stems sit like the randomized redraw of the same stems; below 1 means fewer pairs at that separation than the redraw produced, which this estimator cannot split between stage B inhibition and the stage A light term. The printed band is the spread across the redraws.");
}
