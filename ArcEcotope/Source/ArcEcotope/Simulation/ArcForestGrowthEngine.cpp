// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcForestGrowthEngine.h"

#include "ArcEcotopeModule.h"
#include "Algo/Accumulate.h"
#include "Async/ParallelFor.h"
#include "Determinism/ArcForestDeterministic.h"
#include "Diagnostics/ArcForestDiagnostics.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "Math/RotationMatrix.h"
#include "Misc/ScopeExit.h"
#include "Simulation/ArcEcotopeBudgetSettings.h"
#include "Simulation/ArcEcotopeChunkLayout.h"
#include "Simulation/ArcForestCrownGeometry.h"
#include "Simulation/ArcForestDisturbanceNoise.h"
#include "Simulation/ArcForestDisturbanceSettings.h"
#include "Simulation/ArcForestHabitatFactorStore.h"
#include "Simulation/ArcForestHabitatResponse.h"
#include "Simulation/ArcEcotopeSiteChunkReader.h"
#include "Species/ArcEcotopeSpeciesModel.h"
#include "Templates/TypeHash.h"

namespace ArcForestGrowthEnginePrivate
{
	using UE::ArcForest::Crown::CalculateCrownDepthCm;
	using UE::ArcForest::Crown::CalculateCrownStratumWeights;
	using UE::ArcForest::Crown::DefaultCanopyStratumCount;
	using UE::ArcForest::Crown::FStratifiedOccupancy;
	using UE::ArcForest::CalculateDisturbanceHeightFactor;
	using UE::ArcForest::CalculateDisturbanceSeverity;
	using UE::ArcEcotope::Deadwood::CalculateDecayedFraction;
	using UE::ArcEcotope::Deadwood::CalculateGeometricEventYear;
	using UE::ArcEcotope::Deadwood::CalculateRetireYear;
	using UE::ArcForest::Habitat::EvaluateResponse;
	using UE::ArcForest::Habitat::GetHabitatResponses;

	constexpr float MinimumPlantHeightCm = 30.0f;
	constexpr double MaximumOccupancy = 1000000.0;
	constexpr double ProbabilityQuantum = 1000000.0;
	constexpr int32 ChunkLayoutRecordHaloCells = 9;
	constexpr int32 CapacityEstimateRecordHaloCells = ChunkLayoutRecordHaloCells;
	/** Hard per-year bound independent of authored fecundity. */
	constexpr int64 MaximumRecruitmentAttemptsPerYear = 1024ll * 1024ll;
	constexpr int32 MaximumHabitatFactorTaskSlots = 46;
	constexpr double SquareCentimetersPerHectare = 100000000.0;
	/** Annual logistic rate a patch fills its cell's capacity at, selected by GrowthSpeed: about 61 years at the floor, 15 at the ceiling. */
	constexpr double MinimumCoverLogisticRate = 0.15;
	constexpr double MaximumCoverLogisticRate = 0.60;
	constexpr float CohortHeightBinSizeCm = 10.0f;
	/** Per-axis bound on the record cells one crown footprint may cover, so cost does not follow crown radius. */
	constexpr int32 MaximumFootprintSpanCells = 9;
	/** The same bound for the diagnostic base-grid spread, wide enough for a record-cell-wide source rect plus its kernel. */
	constexpr int32 MaximumDiagnosticFootprintSpanCells = 64;
	constexpr int32 HeightCensusBucketCount = 10;
	/** Leaves room for allocator granularity while the explicit array capacities remain inside the query budget. */
	constexpr int64 HabitatQueryAllocatorGuardBytes = 256ll * 1024ll;
	/** Parallel habitat batches need at least two useful 4096-query shards. */
	constexpr int32 MinimumQueriesForParallelHabitatBatch = 8 * 1024;
	constexpr int32 MinimumQueriesPerParallelHabitatShard = 4 * 1024;
	/** An absolute stand-calibration height, independent of any species maximum. */
	constexpr double HeightCensusThresholdCm = 1500.0;
	/** Auto-mode below this size stays serial; an explicit task-slot CVar still overrides it for benchmarks. */
	constexpr int32 MinimumRecordsForParallelGrowth = 1024;
	constexpr int64 SiteCompatibilityScheduleBudgetBytes = 64ll * 1024ll * 1024ll;

	const FArcEcotopeSiteMetadata& GetSiteMetadata(const FArcForestRunInput& Input)
	{
		check(Input.SiteSource.IsValid());
		return Input.SiteSource->Metadata;
	}

	/**
	 * Caps the number of planned contiguous growth batches. This is deliberately a task count, not a
	 * worker-thread count: the task graph owns worker scheduling. Zero derives one planned task per
	 * logical processor, and positive values support 1/2/4/8-task benchmark sweeps.
	 */
	TAutoConsoleVariable<int32> CVarArcForestSimulationParallelGrowthTaskSlots(
		TEXT("arcecotope.Simulation.ParallelGrowthTaskSlots"),
		0,
		TEXT("Maximum planned ArcForest growth task slots (not worker threads); 0 selects one per logical processor."),
		ECVF_Default);

	/** Dedicated cache budget for the immutable site fields read once before annual simulation begins. */
	TAutoConsoleVariable<int32> CVarArcForestGrowthSiteReaderBudgetMiB(
		TEXT("arcecotope.SiteChunkReader.GrowthBudgetMiB"),
		64,
		TEXT("Cache budget in MiB for ArcForest annual soil/validity preprocessing; clamped to at least 1 MiB."),
		ECVF_Default);

	/** Bounds the transient full-width row slab used to preserve global dense ordering. */
	TAutoConsoleVariable<int32> CVarArcForestGrowthSiteScratchBudgetMiB(
		TEXT("arcecotope.SiteChunkReader.GrowthScratchBudgetMiB"),
		64,
		TEXT("Scratch budget in MiB for ordered ArcForest annual site preprocessing; clamped to at least 1 MiB."),
		ECVF_Default);

	/** Fixed resident budget for the transient static-factor chunk cache. */
	TAutoConsoleVariable<int32> CVarArcForestHabitatFactorStoreBudgetMiB(
		TEXT("arcecotope.Simulation.HabitatFactorStoreBudgetMiB"),
		128,
		TEXT("Resident MiB budget for the ArcForest transient habitat factor chunk cache; clamped to at least 1 MiB."),
		ECVF_Default);

	/** Explicit pin bound; the active evaluator derives a smaller batch automatically when memory requires it. */
	TAutoConsoleVariable<int32> CVarArcForestHabitatFactorStoreMaxActiveLeases(
		TEXT("arcecotope.Simulation.HabitatFactorStoreMaxActiveLeases"),
		46,
		TEXT("Maximum simultaneous ArcForest habitat factor chunk leases; clamped to [1,46]."),
		ECVF_Default);

	/** Bounds transient caller/helper arrays for stable-ordinal habitat query windows. */
	TAutoConsoleVariable<int32> CVarArcForestHabitatQueryScratchBudgetMiB(
		TEXT("arcecotope.Simulation.HabitatQueryScratchBudgetMiB"),
		128,
		TEXT("Transient caller/helper scratch budget in MiB for grouped annual habitat queries; clamped to at least 1 MiB."),
		ECVF_Default);

	/** Independent run-local budget for immutable regional habitat route seeds. */
	TAutoConsoleVariable<int32> CVarArcForestRegionalRouteSeedBudgetMiB(
		TEXT("arcecotope.Simulation.RegionalRouteSeedBudgetMiB"),
		128,
		TEXT("Run-local MiB budget for immutable ArcForest regional habitat route seeds; clamped to at least 1 MiB."),
		ECVF_Default);

	/** Shared retained budget for the lazy regional record index and static envelope table. */
	TAutoConsoleVariable<int32> CVarArcForestRegionalRecruitmentRetainedBudgetMiB(
		TEXT("arcecotope.Simulation.RegionalRecruitmentRetainedBudgetMiB"),
		768,
		TEXT("Run-local retained MiB budget shared by the ArcForest regional recruitment record index and static envelope table; clamped to at least 1 MiB."),
		ECVF_Default);

	/** Independent year-local budget for sparse record-envelope rejection state. */
	TAutoConsoleVariable<int32> CVarArcForestRegionalRecruitmentSamplerScratchBudgetMiB(
		TEXT("arcecotope.Simulation.RegionalRecruitmentSamplerScratchBudgetMiB"),
		256,
		TEXT("Transient MiB budget for ArcForest record-envelope regional recruitment proposals, pending attempts, and grouped habitat queries; clamped to at least 1 MiB."),
		ECVF_Default);

	bool TryAddCapacityValue(const int64 Left, const int64 Right, int64& OutValue)
	{
		if (Left < 0 || Right < 0 || Left > MAX_int64 - Right)
		{
			return false;
		}
		OutValue = Left + Right;
		return true;
	}

	bool TryMultiplyCapacityValue(const int64 Left, const int64 Right, int64& OutValue)
	{
		if (Left < 0 || Right < 0 || (Left > 0 && Right > MAX_int64 / Left))
		{
			return false;
		}
		OutValue = Left * Right;
		return true;
	}

	bool TryAccumulateCapacityBytes(
		const int64 ElementCount,
		const SIZE_T ElementSize,
		int64& InOutBytes)
	{
		int64 Bytes = 0;
		return TryMultiplyCapacityValue(ElementCount, static_cast<int64>(ElementSize), Bytes)
			&& TryAddCapacityValue(InOutBytes, Bytes, InOutBytes);
	}

	bool TryAccumulateAllocatedBytes(const SIZE_T AllocatedBytes, int64& InOutBytes)
	{
		return AllocatedBytes <= static_cast<SIZE_T>(MAX_int64)
			&& TryAddCapacityValue(InOutBytes, static_cast<int64>(AllocatedBytes), InOutBytes);
	}

	double ToMebibytes(const int64 Bytes)
	{
		return static_cast<double>(Bytes) / (1024.0 * 1024.0);
	}

	enum class ERecruitmentSource : uint8
	{
		Local,
		Regional,
	};

	struct FRecruitmentAttemptGroup
	{
		FGuid SpeciesId;
		int32 SpeciesIndex = INDEX_NONE;
		int32 SourceCellIndex = INDEX_NONE;
		ERecruitmentSource Source = ERecruitmentSource::Local;
		int64 DesiredAttempts = 0;
		int64 AllocatedAttempts = 0;
		double AllocationFraction = 0.0;
		double AllocationScore = 0.0;
		uint64 GroupIdentity = 0;
	};

	struct FRecruitmentCandidate
	{
		FGuid SpeciesId;
		int32 SpeciesIndex = INDEX_NONE;
		int32 TargetCellIndex = INDEX_NONE;
		int64 StemCount = 0;
		uint64 AdmissionPriority = 0;
	};

	struct FValidBaseCellTable;

	struct FRegionalHabitatRouteSeed
	{
		int32 ChunkIndex = INDEX_NONE;
		int32 CoreLocalIndex = INDEX_NONE;
	};
	static_assert(sizeof(FRegionalHabitatRouteSeed) == 8);

	struct FRegionalHabitatRouteSeedTable
	{
		TArray<FRegionalHabitatRouteSeed> Seeds;
		const FArcEcotopeSiteMetadata* SiteIdentity = nullptr;
		const FArcEcotopeChunkLayout* LayoutIdentity = nullptr;
		const FValidBaseCellTable* ValidBaseCellsIdentity = nullptr;

		void Empty()
		{
			Seeds.Empty();
			SiteIdentity = nullptr;
			LayoutIdentity = nullptr;
			ValidBaseCellsIdentity = nullptr;
		}
	};

	struct FRegionalHabitatRouteSeedBuildResult
	{
		bool bAdmissionAttempted = false;
		bool bAdmitted = false;
		bool bCancelled = false;
		FString AdmissionReason = TEXT("not-attempted");
		int64 ProjectedBytes = 0;
		int64 ActualBytes = 0;
		int32 CompletedSeedCount = 0;
		double BuildMilliseconds = 0.0;
	};

	struct FRegionalTargetRunTelemetry
	{
		struct FLazySpeciesTotals
		{
			FGuid SpeciesId;
			int32 SpeciesIndex = INDEX_NONE;
			int64 ProposalCount = 0;
			int64 CompletedProposalCount = 0;
			int64 AcceptedAttemptCount = 0;
			int64 UnresolvedAttemptCount = 0;
			int32 ZeroMassYearCount = 0;
			int32 MaximumRetryOrdinal = INDEX_NONE;
		};

		FRegionalHabitatRouteSeedBuildResult RegionalRouteSeedBuild;
		int32 RegionalRouteSeedAdmissions = 0;
		int32 RegionalRouteSeedFallbacks = 0;
		int32 LazySamplerAttemptedYearCount = 0;
		int32 LazySamplerCompletedYearCount = 0;
		int32 LazySamplerSuccessYearCount = 0;
		int32 LazySamplerExhaustionYearCount = 0;
		int32 LazySamplerEnvelopeViolationYearCount = 0;
		int64 LazyAllocatedNonZeroMassAttempts = 0;
		int64 LazyProposalCount = 0;
		int64 LazyHabitatQueryCount = 0;
		int64 LazyQueryWindowCount = 0;
		int64 LazyAcceptedAttemptCount = 0;
		int64 LazyUnresolvedAttemptCount = 0;
		int64 LazyZeroMassGroupCount = 0;
		int64 LazyEnvelopeViolationCount = 0;
		int64 LazyCompletedRoundCount = 0;
		int32 LazyMaximumRetryOrdinal = INDEX_NONE;
		int64 LazyPeakProjectedScratchBytes = 0;
		int64 LazyPeakActualScratchBytes = 0;
		double LazySamplerWallMilliseconds = 0.0;
		TArray<FLazySpeciesTotals> LazySpeciesTotals;
	};

	bool GuidLess(const FGuid& Left, const FGuid& Right)
	{
		if (Left.A != Right.A)
		{
			return Left.A < Right.A;
		}
		if (Left.B != Right.B)
		{
			return Left.B < Right.B;
		}
		if (Left.C != Right.C)
		{
			return Left.C < Right.C;
		}
		return Left.D < Right.D;
	}

	bool IsValidResponse(
		const FArcForestFrozenResponseRange& Response,
		const float AllowedMinimum,
		const float AllowedMaximum)
	{
		return FMath::IsFinite(Response.Minimum)
			&& FMath::IsFinite(Response.Optimum)
			&& FMath::IsFinite(Response.Maximum)
			&& Response.Minimum >= AllowedMinimum
			&& Response.Maximum <= AllowedMaximum
			&& Response.Minimum <= Response.Optimum
			&& Response.Optimum <= Response.Maximum;
	}

	int32 GetRecordCellIndexForBaseCell(const FArcEcotopeSiteMetadata& Site, const int32 BaseCellIndex)
	{
		return Site.MapCellIndexBetweenLevels(BaseCellIndex, 0, Site.RecordGridLevel);
	}

	struct FValidBaseCellEntry
	{
		int32 BaseCellIndex = INDEX_NONE;
		int32 RecordCellIndex = INDEX_NONE;
		double CellAreaSquareCentimeters = 0.0;
	};

	/**
	 * The base cells carrying a valid surface, with the site geometry that is immutable for a whole
	 * run. Entries remain in ascending base-cell order so record-envelope recruitment, grouped habitat
	 * routing and deterministic local lookup share one stable base-cell identity.
	 */
	struct FValidBaseCellTable
	{
		TArray<FValidBaseCellEntry> Entries;
		TArray<float> ModificationCutByValidEntry;
		TArray<float> BareRockByValidEntry;
		/** Trees read the buffered channel field; ground cover and GPU foliage keep the raw ChannelExclusion. */
		TArray<float> ChannelBufferByValidEntry;
		TArray<float> ChannelExclusionByValidEntry;
		TArray<float> EstablishmentWearByValidEntry;
		TArray<float> SubstrateMobilityByValidEntry;
		TArray<float> SubstrateMobilityByBaseCell;
		TArray<float> ChannelBufferByBaseCell;
		TArray<float> LocalSurfaceZCmByValidEntry;
		/** Base-cell-indexed immutable validity used by every annual point lookup. */
		TArray<uint8> BaseValidityMask;
		double TotalAreaSquareCentimeters = 0.0;
	};

	struct FRegionalRecruitmentRecordIndex
	{
		/** CSR ranges for record cells; size is the authoritative runtime record-cell count plus one. */
		TArray<int32> RecordOffsets;
		/** Stable valid-entry ordinals grouped by record cell. */
		TArray<int32> ValidEntryIndicesByRecord;
		/** Dense base-cell-to-valid-entry mapping; invalid surface cells remain INDEX_NONE. */
		TArray<int32> ValidEntryIndexByBaseCell;
		const FArcEcotopeSiteMetadata* SiteIdentity = nullptr;
		const FValidBaseCellTable* ValidBaseCellsIdentity = nullptr;

		void Empty()
		{
			RecordOffsets.Empty();
			ValidEntryIndicesByRecord.Empty();
			ValidEntryIndexByBaseCell.Empty();
			SiteIdentity = nullptr;
			ValidBaseCellsIdentity = nullptr;
		}
	};

	struct FRegionalRecruitmentRecordIndexBuildResult
	{
		bool bAdmissionAttempted = false;
		bool bAdmitted = false;
		bool bCancelled = false;
		FString AdmissionReason;
		int64 BudgetBytes = 0;
		int64 ProjectedBytes = 0;
		int64 ActualBytes = 0;
		int64 PeakScratchBytes = 0;
		int32 IndexedEntryCount = 0;
		int32 RecordCellCount = 0;
		double BuildMilliseconds = 0.0;
	};

	struct FRegionalRecruitmentEnvelopeTable
	{
		/** Species-indexed compact row lookup; non-regional species remain INDEX_NONE. */
		TArray<int32> EnvelopeRowBySpecies;
		/** Inverse row mapping used by the one-pass factor traversal. */
		TArray<int32> SpeciesIndexByEnvelopeRow;
		/** Row-major maximum Area * UpperHabitatQuantum01 * ModificationCutFactor. */
		TArray<double> MaxStaticCellWeight;
		const FArcEcotopeSiteMetadata* SiteIdentity = nullptr;
		const FArcEcotopeChunkLayout* LayoutIdentity = nullptr;
		const FValidBaseCellTable* ValidBaseCellsIdentity = nullptr;
		const FRegionalRecruitmentRecordIndex* RecordIndexIdentity = nullptr;
		const FArcForestHabitatFactorStore* FactorStoreIdentity = nullptr;

		void Empty()
		{
			EnvelopeRowBySpecies.Empty();
			SpeciesIndexByEnvelopeRow.Empty();
			MaxStaticCellWeight.Empty();
			SiteIdentity = nullptr;
			LayoutIdentity = nullptr;
			ValidBaseCellsIdentity = nullptr;
			RecordIndexIdentity = nullptr;
			FactorStoreIdentity = nullptr;
		}
	};

	struct FRegionalRecruitmentEnvelopeBuildResult
	{
		bool bAdmissionAttempted = false;
		bool bAdmitted = false;
		bool bCancelled = false;
		FString AdmissionReason;
		int64 BudgetBytes = 0;
		int64 ProjectedBytes = 0;
		int64 ActualBytes = 0;
		int64 CombinedProjectedBytes = 0;
		int64 CombinedActualBytes = 0;
		int32 IndexedSpeciesCount = 0;
		int32 RecordCellCount = 0;
		int64 ScannedFactorSamples = 0;
		int32 ScannedChunkCount = 0;
		double BuildMilliseconds = 0.0;
	};

	bool BuildRegionalHabitatRouteSeeds(
		const FArcEcotopeSiteMetadata& Site,
		const FArcEcotopeChunkLayout& Layout,
		const FValidBaseCellTable& ValidBaseCells,
		const FArcForestCancellationToken& Cancellation,
		FRegionalHabitatRouteSeedTable& OutTable,
		FRegionalHabitatRouteSeedBuildResult& OutResult,
		FString& OutError)
	{
		const double BuildStartSeconds = FPlatformTime::Seconds();
		OutTable.Empty();
		OutResult = FRegionalHabitatRouteSeedBuildResult();
		OutResult.bAdmissionAttempted = true;
		OutError.Reset();
		ON_SCOPE_EXIT
		{
			OutResult.BuildMilliseconds = UE::ArcForest::GetElapsedMilliseconds(BuildStartSeconds);
		};

		const int64 ValidEntryCount = static_cast<int64>(ValidBaseCells.Entries.Num());
		if (ValidEntryCount < 0 || ValidEntryCount > MAX_int32)
		{
			OutResult.AdmissionReason = TEXT("array-limit");
			return true;
		}
		if (!TryMultiplyCapacityValue(
				ValidEntryCount,
				static_cast<int64>(sizeof(FRegionalHabitatRouteSeed)),
				OutResult.ProjectedBytes))
		{
			OutResult.AdmissionReason = TEXT("projected-size-overflow");
			return true;
		}

		const int64 BudgetBytes = static_cast<int64>(FMath::Max(
			CVarArcForestRegionalRouteSeedBudgetMiB.GetValueOnAnyThread(), 1)) * 1024ll * 1024ll;
		if (OutResult.ProjectedBytes > BudgetBytes)
		{
			OutResult.AdmissionReason = TEXT("projected-budget");
			return true;
		}

		OutTable.Seeds.SetNumUninitialized(static_cast<int32>(ValidEntryCount));
		OutResult.ActualBytes = static_cast<int64>(OutTable.Seeds.GetAllocatedSize());
		if (OutResult.ActualBytes > BudgetBytes)
		{
			OutResult.AdmissionReason = TEXT("actual-capacity-budget");
			OutTable.Empty();
			return true;
		}

		const FIntPoint ChunkDimensions = Layout.GetChunkGridDimensions();
		const int32 SiteCellCount = Site.GetCellCount();
		const int32 RecordCellCount = Site.GetLevelCellCount(Site.RecordGridLevel);
		for (int32 EntryIndex = 0; EntryIndex < ValidBaseCells.Entries.Num(); ++EntryIndex)
		{
			if ((EntryIndex & 4095) == 0 && Cancellation.IsCancellationRequested())
			{
				OutResult.bCancelled = true;
				OutResult.AdmissionReason = TEXT("cancelled");
				OutTable.Empty();
				return false;
			}

			const FValidBaseCellEntry& Entry = ValidBaseCells.Entries[EntryIndex];
			if (Entry.BaseCellIndex < 0
				|| Entry.BaseCellIndex >= SiteCellCount
				|| Site.GridDimensions.X <= 0
				|| Site.GridDimensions.Y <= 0)
			{
				OutResult.AdmissionReason = TEXT("mapping-corruption");
				OutError = FString::Printf(
					TEXT("ArcForest regional habitat route seed %d referenced invalid base cell %d."),
					EntryIndex,
					Entry.BaseCellIndex);
				OutTable.Empty();
				return false;
			}

			const FIntPoint BaseCell(
				Entry.BaseCellIndex % Site.GridDimensions.X,
				Entry.BaseCellIndex / Site.GridDimensions.X);
			FArcEcotopeChunkId ChunkId;
			if (!Layout.FindChunkForBaseCell(BaseCell, ChunkId)
				|| ChunkDimensions.X <= 0
				|| ChunkDimensions.Y <= 0
				|| ChunkId.X < 0
				|| ChunkId.X >= ChunkDimensions.X
				|| ChunkId.Y < 0
				|| ChunkId.Y >= ChunkDimensions.Y)
			{
				OutResult.AdmissionReason = TEXT("mapping-corruption");
				OutError = FString::Printf(
					TEXT("ArcForest regional habitat route seed %d could not resolve base cell %d to a chunk."),
					EntryIndex,
					Entry.BaseCellIndex);
				OutTable.Empty();
				return false;
			}

			const int64 ChunkIndex64 = static_cast<int64>(ChunkId.X)
				+ static_cast<int64>(ChunkId.Y) * ChunkDimensions.X;
			FArcEcotopeChunkRect Chunk;
			if (ChunkIndex64 < 0
				|| ChunkIndex64 > MAX_int32
				|| !Layout.GetChunk(static_cast<int32>(ChunkIndex64), Chunk)
				|| Chunk.Id.X != ChunkId.X
				|| Chunk.Id.Y != ChunkId.Y)
			{
				OutResult.AdmissionReason = TEXT("mapping-corruption");
				OutError = FString::Printf(
					TEXT("ArcForest regional habitat route seed %d resolved an invalid chunk descriptor."),
					EntryIndex);
				OutTable.Empty();
				return false;
			}

			const int32 CoreWidth = Chunk.BaseCoreRect.Width();
			const int32 CoreHeight = Chunk.BaseCoreRect.Height();
			const int64 CoreCellCount = static_cast<int64>(CoreWidth) * CoreHeight;
			const int64 CoreLocalIndex = static_cast<int64>(BaseCell.X - Chunk.BaseCoreRect.Min.X)
				+ static_cast<int64>(BaseCell.Y - Chunk.BaseCoreRect.Min.Y) * CoreWidth;
			const int32 ExpectedRecordCellIndex = GetRecordCellIndexForBaseCell(
				Site, Entry.BaseCellIndex);
			if (CoreWidth <= 0
				|| CoreHeight <= 0
				|| CoreCellCount <= 0
				|| CoreCellCount > MAX_int32
				|| BaseCell.X < Chunk.BaseCoreRect.Min.X
				|| BaseCell.X >= Chunk.BaseCoreRect.Max.X
				|| BaseCell.Y < Chunk.BaseCoreRect.Min.Y
				|| BaseCell.Y >= Chunk.BaseCoreRect.Max.Y
				|| CoreLocalIndex < 0
				|| CoreLocalIndex >= CoreCellCount
				|| ExpectedRecordCellIndex < 0
				|| ExpectedRecordCellIndex >= RecordCellCount
				|| Entry.RecordCellIndex != ExpectedRecordCellIndex)
			{
				OutResult.AdmissionReason = TEXT("mapping-corruption");
				OutError = FString::Printf(
					TEXT("ArcForest regional habitat route seed %d found inconsistent geometry for base cell %d."),
					EntryIndex,
					Entry.BaseCellIndex);
				OutTable.Empty();
				return false;
			}

			FRegionalHabitatRouteSeed& Seed = OutTable.Seeds[EntryIndex];
			Seed.ChunkIndex = static_cast<int32>(ChunkIndex64);
			Seed.CoreLocalIndex = static_cast<int32>(CoreLocalIndex);
			++OutResult.CompletedSeedCount;
		}

		if (Cancellation.IsCancellationRequested())
		{
			OutResult.bCancelled = true;
			OutResult.AdmissionReason = TEXT("cancelled");
			OutTable.Empty();
			return false;
		}

		OutTable.SiteIdentity = &Site;
		OutTable.LayoutIdentity = &Layout;
		OutTable.ValidBaseCellsIdentity = &ValidBaseCells;
		OutResult.bAdmitted = true;
		OutResult.AdmissionReason = TEXT("admitted");
		return true;
	}

	bool BuildRegionalRecruitmentRecordIndex(
		const FArcEcotopeSiteMetadata& Site,
		const FValidBaseCellTable& ValidBaseCells,
		const int64 BudgetBytes,
		const FArcForestCancellationToken& Cancellation,
		FRegionalRecruitmentRecordIndex& OutIndex,
		FRegionalRecruitmentRecordIndexBuildResult& OutResult,
		FString& OutError)
	{
		const double BuildStartSeconds = FPlatformTime::Seconds();
		OutIndex.Empty();
		OutResult = FRegionalRecruitmentRecordIndexBuildResult();
		OutResult.bAdmissionAttempted = true;
		OutResult.BudgetBytes = BudgetBytes;
		OutResult.AdmissionReason = TEXT("building");
		OutError.Reset();
		ON_SCOPE_EXIT
		{
			OutResult.BuildMilliseconds = UE::ArcForest::GetElapsedMilliseconds(BuildStartSeconds);
		};

		const int32 BaseCellCount = Site.GetCellCount();
		const FIntPoint RecordDimensions = Site.GetLevelDimensions(Site.RecordGridLevel);
		const int32 RecordCellCount = Site.GetLevelCellCount(Site.RecordGridLevel);
		const int32 ValidEntryCount = ValidBaseCells.Entries.Num();
		const int64 RuntimeRecordCellCount = static_cast<int64>(RecordDimensions.X) * RecordDimensions.Y;
		OutResult.RecordCellCount = RecordCellCount;
		if (BudgetBytes <= 0
			|| BaseCellCount <= 0
			|| RecordDimensions.X <= 0
			|| RecordDimensions.Y <= 0
			|| RuntimeRecordCellCount <= 0
			|| RuntimeRecordCellCount > MAX_int32
			|| RuntimeRecordCellCount != RecordCellCount
			|| RecordCellCount == MAX_int32
			|| ValidEntryCount < 0
			|| ValidBaseCells.ModificationCutByValidEntry.Num() != ValidEntryCount
			|| ValidBaseCells.BareRockByValidEntry.Num() != ValidEntryCount
			|| ValidBaseCells.ChannelBufferByValidEntry.Num() != ValidEntryCount
			|| ValidBaseCells.ChannelExclusionByValidEntry.Num() != ValidEntryCount
			|| ValidBaseCells.EstablishmentWearByValidEntry.Num() != ValidEntryCount
			|| ValidBaseCells.SubstrateMobilityByValidEntry.Num() != ValidEntryCount
			|| ValidBaseCells.LocalSurfaceZCmByValidEntry.Num() != ValidEntryCount
			|| ValidBaseCells.SubstrateMobilityByBaseCell.Num() != BaseCellCount
			|| ValidBaseCells.ChannelBufferByBaseCell.Num() != BaseCellCount
			|| ValidBaseCells.BaseValidityMask.Num() != BaseCellCount)
		{
			OutResult.AdmissionReason = TEXT("input-shape");
			OutError = TEXT("ArcForest regional recruitment record index received inconsistent runtime site or valid-entry dimensions.");
			return false;
		}
		if (Cancellation.IsCancellationRequested())
		{
			OutResult.bCancelled = true;
			OutResult.AdmissionReason = TEXT("cancelled");
			return false;
		}

		const int64 OffsetCount = static_cast<int64>(RecordCellCount) + 1;
		if (!TryAccumulateCapacityBytes(OffsetCount, sizeof(int32), OutResult.ProjectedBytes)
			|| !TryAccumulateCapacityBytes(ValidEntryCount, sizeof(int32), OutResult.ProjectedBytes)
			|| !TryAccumulateCapacityBytes(BaseCellCount, sizeof(int32), OutResult.ProjectedBytes))
		{
			OutResult.AdmissionReason = TEXT("projected-size-overflow");
			return true;
		}
		if (OutResult.ProjectedBytes > BudgetBytes)
		{
			OutResult.AdmissionReason = TEXT("projected-budget");
			return true;
		}
		const auto RefreshActualBytes = [&OutIndex, &OutResult]()
		{
			int64 ActualBytes = 0;
			if (!TryAccumulateAllocatedBytes(OutIndex.RecordOffsets.GetAllocatedSize(), ActualBytes)
				|| !TryAccumulateAllocatedBytes(
					OutIndex.ValidEntryIndicesByRecord.GetAllocatedSize(), ActualBytes)
				|| !TryAccumulateAllocatedBytes(
					OutIndex.ValidEntryIndexByBaseCell.GetAllocatedSize(), ActualBytes))
			{
				return false;
			}
			OutResult.ActualBytes = ActualBytes;
			return true;
		};

		OutIndex.RecordOffsets.SetNumZeroed(static_cast<int32>(OffsetCount));
		OutIndex.ValidEntryIndexByBaseCell.Init(INDEX_NONE, BaseCellCount);
		if (!RefreshActualBytes())
		{
			OutResult.AdmissionReason = TEXT("actual-size-overflow");
			OutIndex.Empty();
			return true;
		}
		if (OutResult.ActualBytes > BudgetBytes)
		{
			OutResult.AdmissionReason = TEXT("actual-capacity-budget");
			OutIndex.Empty();
			return true;
		}
		for (int32 EntryIndex = 0; EntryIndex < ValidEntryCount; ++EntryIndex)
		{
			if ((EntryIndex & 4095) == 0 && Cancellation.IsCancellationRequested())
			{
				OutResult.bCancelled = true;
				OutResult.AdmissionReason = TEXT("cancelled");
				OutIndex.Empty();
				return false;
			}
			const FValidBaseCellEntry& Entry = ValidBaseCells.Entries[EntryIndex];
			const int32 ExpectedRecordCellIndex = Entry.BaseCellIndex >= 0
				&& Entry.BaseCellIndex < BaseCellCount
				? GetRecordCellIndexForBaseCell(Site, Entry.BaseCellIndex)
				: INDEX_NONE;
			if (Entry.BaseCellIndex < 0
				|| Entry.BaseCellIndex >= BaseCellCount
				|| Entry.RecordCellIndex < 0
				|| Entry.RecordCellIndex >= RecordCellCount
				|| Entry.RecordCellIndex != ExpectedRecordCellIndex
				|| !FMath::IsFinite(Entry.CellAreaSquareCentimeters)
				|| Entry.CellAreaSquareCentimeters <= 0.0
				|| OutIndex.ValidEntryIndexByBaseCell[Entry.BaseCellIndex] != INDEX_NONE
				|| OutIndex.RecordOffsets[Entry.RecordCellIndex + 1] == MAX_int32)
			{
				OutResult.AdmissionReason = TEXT("mapping-corruption");
				OutError = FString::Printf(
					TEXT("ArcForest regional recruitment record index found invalid or duplicate valid entry %d (base=%d record=%d)."),
					EntryIndex,
					Entry.BaseCellIndex,
					Entry.RecordCellIndex);
				OutIndex.Empty();
				return false;
			}
			OutIndex.ValidEntryIndexByBaseCell[Entry.BaseCellIndex] = EntryIndex;
			++OutIndex.RecordOffsets[Entry.RecordCellIndex + 1];
			OutResult.IndexedEntryCount = EntryIndex + 1;
		}

		int64 RunningOffset = 0;
		for (int32 OffsetIndex = 1; OffsetIndex <= RecordCellCount; ++OffsetIndex)
		{
			if ((OffsetIndex & 4095) == 0 && Cancellation.IsCancellationRequested())
			{
				OutResult.bCancelled = true;
				OutResult.AdmissionReason = TEXT("cancelled");
				OutIndex.Empty();
				return false;
			}
			RunningOffset += OutIndex.RecordOffsets[OffsetIndex];
			if (RunningOffset < 0 || RunningOffset > MAX_int32)
			{
				OutResult.AdmissionReason = TEXT("prefix-overflow");
				OutIndex.Empty();
				return true;
			}
			OutIndex.RecordOffsets[OffsetIndex] = static_cast<int32>(RunningOffset);
		}
		if (RunningOffset != ValidEntryCount)
		{
			OutResult.AdmissionReason = TEXT("prefix-corruption");
			OutError = TEXT("ArcForest regional recruitment record index prefix lost valid entries.");
			OutIndex.Empty();
			return false;
		}

		OutIndex.ValidEntryIndicesByRecord.SetNumUninitialized(ValidEntryCount);
		if (!RefreshActualBytes())
		{
			OutResult.AdmissionReason = TEXT("actual-size-overflow");
			OutIndex.Empty();
			return true;
		}
		if (OutResult.ActualBytes > BudgetBytes)
		{
			OutResult.AdmissionReason = TEXT("actual-capacity-budget");
			OutIndex.Empty();
			return true;
		}
		TArray<int32> Cursors;
		Cursors.SetNumUninitialized(RecordCellCount);
		OutResult.PeakScratchBytes = static_cast<int64>(Cursors.GetAllocatedSize());
		for (int32 RecordCellIndex = 0; RecordCellIndex < RecordCellCount; ++RecordCellIndex)
		{
			if ((RecordCellIndex & 4095) == 0 && Cancellation.IsCancellationRequested())
			{
				OutResult.bCancelled = true;
				OutResult.AdmissionReason = TEXT("cancelled");
				OutIndex.Empty();
				return false;
			}
			Cursors[RecordCellIndex] = OutIndex.RecordOffsets[RecordCellIndex];
		}
		for (int32 EntryIndex = 0; EntryIndex < ValidEntryCount; ++EntryIndex)
		{
			if ((EntryIndex & 4095) == 0 && Cancellation.IsCancellationRequested())
			{
				OutResult.bCancelled = true;
				OutResult.AdmissionReason = TEXT("cancelled");
				OutIndex.Empty();
				return false;
			}
			const int32 RecordCellIndex = ValidBaseCells.Entries[EntryIndex].RecordCellIndex;
			if (!Cursors.IsValidIndex(RecordCellIndex)
				|| Cursors[RecordCellIndex] < OutIndex.RecordOffsets[RecordCellIndex]
				|| Cursors[RecordCellIndex] >= OutIndex.RecordOffsets[RecordCellIndex + 1])
			{
				OutResult.AdmissionReason = TEXT("scatter-corruption");
				OutError = TEXT("ArcForest regional recruitment record index stable scatter exceeded a record range.");
				OutIndex.Empty();
				return false;
			}
			OutIndex.ValidEntryIndicesByRecord[Cursors[RecordCellIndex]++] = EntryIndex;
		}
		for (int32 RecordCellIndex = 0; RecordCellIndex < RecordCellCount; ++RecordCellIndex)
		{
			if ((RecordCellIndex & 4095) == 0 && Cancellation.IsCancellationRequested())
			{
				OutResult.bCancelled = true;
				OutResult.AdmissionReason = TEXT("cancelled");
				OutIndex.Empty();
				return false;
			}
			if (Cursors[RecordCellIndex] != OutIndex.RecordOffsets[RecordCellIndex + 1])
			{
				OutResult.AdmissionReason = TEXT("scatter-corruption");
				OutError = TEXT("ArcForest regional recruitment record index did not fill every record range exactly.");
				OutIndex.Empty();
				return false;
			}
		}

		if (Cancellation.IsCancellationRequested())
		{
			OutResult.bCancelled = true;
			OutResult.AdmissionReason = TEXT("cancelled");
			OutIndex.Empty();
			return false;
		}

		OutIndex.SiteIdentity = &Site;
		OutIndex.ValidBaseCellsIdentity = &ValidBaseCells;
		OutResult.bAdmitted = true;
		OutResult.AdmissionReason = TEXT("admitted");
		return true;
	}

	/** DecayModerator outlives the soil pools: every surviving deadwood record still rots at its own cell's rate. */
	struct FArcEcotopeDecayGrid
	{
		TArray<double> DecayModerator;
		TArray<double> CellAreaHectares;
	};

	constexpr uint32 SiteFieldBit(const EArcEcotopeSiteField Field)
	{
		return 1u << static_cast<uint32>(Field);
	}

	bool ValidateAcquiredChunkIdentity(
		const FArcEcotopeChunkLayout& Layout,
		const FArcEcotopeChunkId& Id,
		const FArcEcotopeSiteChunkView& View,
		FArcEcotopeChunkRect& OutChunk,
		FString& OutError)
	{
		if (!Layout.GetChunk(Id, OutChunk)
			|| !(View.Id == Id)
			|| View.BaseCoreRect.Min != OutChunk.BaseCoreRect.Min
			|| View.BaseCoreRect.Max != OutChunk.BaseCoreRect.Max)
		{
			OutError = FString::Printf(
				TEXT("ArcForest annual site reader returned a chunk that does not match layout id (%d,%d)."),
				Id.X,
				Id.Y);
			return false;
		}
		return true;
	}

	bool BuildDecayGrid(
		const FArcEcotopeSiteMetadata& Site,
		const FArcEcotopeChunkLayout& ReaderLayout,
		const FArcEcotopeSiteChunkReader& SiteReader,
		const FArcForestCancellationToken& Cancellation,
		FArcEcotopeDecayGrid& OutGrid,
		bool& bOutCancelled,
		FString& OutError)
	{
		bOutCancelled = false;
		const int32 RecordCellCount = Site.GetLevelCellCount(Site.RecordGridLevel);
		OutGrid.DecayModerator.SetNumUninitialized(RecordCellCount);
		OutGrid.CellAreaHectares.SetNumUninitialized(RecordCellCount);
		TArray<uint8> Written;
		Written.Init(0, RecordCellCount);

		const FIntPoint BaseDimensions = Site.GridDimensions;
		const FIntPoint RecordDimensions = Site.GetLevelDimensions(Site.RecordGridLevel);
		const int32 ChildSpan = static_cast<int32>(1) << Site.RecordGridLevel;
		const uint32 FieldMask = SiteFieldBit(EArcEcotopeSiteField::ValidSurface)
			| SiteFieldBit(EArcEcotopeSiteField::Fertility);
		const FIntPoint ChunkDimensions = ReaderLayout.GetChunkGridDimensions();
		for (int32 ChunkY = 0; ChunkY < ChunkDimensions.Y; ++ChunkY)
		{
			if (Cancellation.IsCancellationRequested())
			{
				bOutCancelled = true;
				return false;
			}
			for (int32 ChunkX = 0; ChunkX < ChunkDimensions.X; ++ChunkX)
			{
				if (Cancellation.IsCancellationRequested())
				{
					bOutCancelled = true;
					return false;
				}
				const FArcEcotopeChunkId Id{ChunkX, ChunkY};
				FArcEcotopeSiteChunkLease Lease;
				const bool bAcquired = SiteReader.AcquireFields(Id, 0, FieldMask, Lease, OutError);
				if (Cancellation.IsCancellationRequested())
				{
					bOutCancelled = true;
					return false;
				}
				if (!bAcquired)
				{
					OutError = FString::Printf(
						TEXT("ArcForest decay grid could not acquire chunk (%d,%d): %s"),
						ChunkX,
						ChunkY,
						*OutError);
					return false;
				}
				const FArcEcotopeSiteChunkView& View = Lease.GetView();
				FArcEcotopeChunkRect Chunk;
				if (!ValidateAcquiredChunkIdentity(ReaderLayout, Id, View, Chunk, OutError))
				{
					return false;
				}
				if (!View.ValidSurface.IsValid() || !View.Fertility.IsValid())
				{
					OutError = TEXT("ArcForest decay grid acquired invalid validity or fertility views.");
					return false;
				}

				for (int32 RecordY = Chunk.RecordCoreRect.Min.Y; RecordY < Chunk.RecordCoreRect.Max.Y; ++RecordY)
				{
					for (int32 RecordX = Chunk.RecordCoreRect.Min.X; RecordX < Chunk.RecordCoreRect.Max.X; ++RecordX)
					{
						const int32 RecordCellIndex = RecordX + RecordY * RecordDimensions.X;
						if (!Written.IsValidIndex(RecordCellIndex) || Written[RecordCellIndex] != 0)
						{
							OutError = TEXT("ArcForest decay grid encountered overlapping or invalid record-cell ownership.");
							return false;
						}

						const int32 BaseMinX = RecordX * ChildSpan;
						const int32 BaseMinY = RecordY * ChildSpan;
						const int32 BaseMaxX = FMath::Min(BaseMinX + ChildSpan, BaseDimensions.X);
						const int32 BaseMaxY = FMath::Min(BaseMinY + ChildSpan, BaseDimensions.Y);
						const FVector RecordCenter = Site.GetLevelCellCenterLocal(
							RecordCellIndex, Site.RecordGridLevel);
						int32 CenterBaseCellIndex = INDEX_NONE;
						FIntPoint CenterCoordinates = FIntPoint::ZeroValue;
						const bool bHasCenter = Site.TryGetCellIndexForLocalPosition(
							FVector2D(RecordCenter.X, RecordCenter.Y), CenterBaseCellIndex)
							&& Site.TryGetCellCoordinates(CenterBaseCellIndex, CenterCoordinates);
						int32 RepresentativeX = bHasCenter ? CenterCoordinates.X : BaseMinX;
						int32 RepresentativeY = bHasCenter ? CenterCoordinates.Y : BaseMinY;
						const uint8* CenterValidity = bHasCenter
							? View.ValidSurface.TryGet(RepresentativeX, RepresentativeY)
							: nullptr;
						bool bHasSoil = CenterValidity != nullptr && *CenterValidity != 0;
						if (!bHasSoil)
						{
							for (int32 BaseY = BaseMinY; BaseY < BaseMaxY && !bHasSoil; ++BaseY)
							{
								for (int32 BaseX = BaseMinX; BaseX < BaseMaxX; ++BaseX)
								{
									const uint8* Validity = View.ValidSurface.TryGet(BaseX, BaseY);
									if (Validity != nullptr && *Validity != 0)
									{
										RepresentativeX = BaseX;
										RepresentativeY = BaseY;
										bHasSoil = true;
										break;
									}
								}
							}
						}

						const float* Fertility = bHasSoil
							? View.Fertility.TryGet(
								RepresentativeX >> View.Fertility.Level,
								RepresentativeY >> View.Fertility.Level)
							: nullptr;
						if (bHasSoil && Fertility == nullptr)
						{
							OutError = TEXT("ArcForest decay grid could not sample representative fertility.");
							return false;
						}
						const double Capacity = Fertility != nullptr ? static_cast<double>(*Fertility) : 0.0;
						const double AreaHectares = bHasSoil
							? Site.GetLevelCellAreaSquareCentimeters(RecordCellIndex, Site.RecordGridLevel)
								/ SquareCentimetersPerHectare
							: 0.0;
						if (!FMath::IsFinite(Capacity) || Capacity < 0.0
							|| !FMath::IsFinite(AreaHectares) || AreaHectares < 0.0)
						{
							OutError = TEXT("ArcForest decay grid found an invalid baked capacity or cell area.");
							return false;
						}
						OutGrid.DecayModerator[RecordCellIndex] =
							UE::ArcEcotope::Deadwood::CalculateDecayModerator(Capacity);
						OutGrid.CellAreaHectares[RecordCellIndex] = AreaHectares;
						Written[RecordCellIndex] = 1;
					}
				}
			}
			if (Cancellation.IsCancellationRequested())
			{
				bOutCancelled = true;
				return false;
			}
		}
		if (Written.Contains(0))
		{
			OutError = TEXT("ArcForest decay grid did not cover every record cell.");
			return false;
		}
		return true;
	}

	bool BuildValidBaseCellTable(
		const FArcEcotopeSiteMetadata& Site,
		const FArcEcotopeChunkLayout& ReaderLayout,
		const FArcEcotopeSiteChunkReader& SiteReader,
		const FArcForestCancellationToken& Cancellation,
		FValidBaseCellTable& OutTable,
		bool& bOutCancelled,
		FString& OutError)
	{
		bOutCancelled = false;
		OutTable.Entries.Reset();
		OutTable.ModificationCutByValidEntry.Reset();
		OutTable.BareRockByValidEntry.Reset();
		OutTable.ChannelBufferByValidEntry.Reset();
		OutTable.ChannelExclusionByValidEntry.Reset();
		OutTable.EstablishmentWearByValidEntry.Reset();
		OutTable.SubstrateMobilityByValidEntry.Reset();
		OutTable.LocalSurfaceZCmByValidEntry.Reset();
		OutTable.TotalAreaSquareCentimeters = 0.0;
		const int32 CellCount = Site.GetCellCount();
		OutTable.BaseValidityMask.SetNumUninitialized(CellCount);
		// Zero-filled rather than SetNumUninitialized: the fill below runs only for valid cells, and a
		// record cell whose min-corner child is invalid still reads this array from the mortality pass.
		OutTable.SubstrateMobilityByBaseCell.Init(0.0f, CellCount);
		OutTable.ChannelBufferByBaseCell.Init(0.0f, CellCount);
		OutTable.Entries.Reserve(CellCount);
		OutTable.ModificationCutByValidEntry.Reserve(CellCount);
		OutTable.BareRockByValidEntry.Reserve(CellCount);
		OutTable.ChannelBufferByValidEntry.Reserve(CellCount);
		OutTable.ChannelExclusionByValidEntry.Reserve(CellCount);
		OutTable.EstablishmentWearByValidEntry.Reserve(CellCount);
		OutTable.SubstrateMobilityByValidEntry.Reserve(CellCount);
		OutTable.LocalSurfaceZCmByValidEntry.Reserve(CellCount);

		const uint32 FieldMask = SiteFieldBit(EArcEcotopeSiteField::LocalSurfaceZ)
			| SiteFieldBit(EArcEcotopeSiteField::ValidSurface)
			| SiteFieldBit(EArcEcotopeSiteField::ModificationCut)
			| SiteFieldBit(EArcEcotopeSiteField::BareRock)
			| SiteFieldBit(EArcEcotopeSiteField::ChannelBuffer)
			| SiteFieldBit(EArcEcotopeSiteField::ChannelExclusion)
			| SiteFieldBit(EArcEcotopeSiteField::EstablishmentWear)
			| SiteFieldBit(EArcEcotopeSiteField::SubstrateMobility);
		const FIntPoint ChunkDimensions = ReaderLayout.GetChunkGridDimensions();
		const int32 RecordCellCount = Site.GetLevelCellCount(Site.RecordGridLevel);
		int32 ExpectedY = 0;
		for (int32 ChunkY = 0; ChunkY < ChunkDimensions.Y; ++ChunkY)
		{
			if (Cancellation.IsCancellationRequested())
			{
				bOutCancelled = true;
				return false;
			}
			FArcEcotopeChunkRect FirstChunk;
			if (!ReaderLayout.GetChunk(FArcEcotopeChunkId{0, ChunkY}, FirstChunk))
			{
				OutError = TEXT("ArcForest valid base cell table found an invalid chunk row.");
				return false;
			}
			const int32 SlabMinY = FirstChunk.BaseCoreRect.Min.Y;
			const int32 SlabMaxY = FirstChunk.BaseCoreRect.Max.Y;
			if (SlabMinY != ExpectedY || SlabMaxY <= SlabMinY)
			{
				OutError = TEXT("ArcForest valid base cell table found a non-contiguous vertical reader slab.");
				return false;
			}
			struct FValidSiteSample
			{
				float ModificationCut = 0.0f;
				float BareRock = 0.0f;
				float ChannelBuffer = 0.0f;
				float ChannelExclusion = 0.0f;
				float EstablishmentWear = 0.0f;
				float SubstrateMobility = 0.0f;
				float LocalSurfaceZCm = 0.0f;
				uint8 Validity = 0;
				bool bCovered = false;
			};
			const int64 SlabHeight = static_cast<int64>(SlabMaxY) - SlabMinY;
			const int64 SlabSampleCount = static_cast<int64>(Site.GridDimensions.X) * SlabHeight;
			const int64 ScratchBudgetBytes = static_cast<int64>(FMath::Max(
				CVarArcForestGrowthSiteScratchBudgetMiB.GetValueOnAnyThread(), 1)) * 1024ll * 1024ll;
			if (SlabSampleCount <= 0 || SlabSampleCount > MAX_int32
				|| SlabSampleCount > ScratchBudgetBytes / static_cast<int64>(sizeof(FValidSiteSample)))
			{
				OutError = FString::Printf(
					TEXT("ArcForest valid base cell table row slab requires %.2f MiB, exceeding the %lld MiB scratch budget or array limits."),
					static_cast<double>(SlabSampleCount) * sizeof(FValidSiteSample) / (1024.0 * 1024.0),
					ScratchBudgetBytes / (1024ll * 1024ll));
				return false;
			}
			TArray<FValidSiteSample> SlabSamples;
			SlabSamples.SetNum(static_cast<int32>(SlabSampleCount));
			int32 ExpectedX = 0;
			for (int32 ChunkX = 0; ChunkX < ChunkDimensions.X; ++ChunkX)
			{
				if (Cancellation.IsCancellationRequested())
				{
					bOutCancelled = true;
					return false;
				}
				const FArcEcotopeChunkId Id{ChunkX, ChunkY};
				FArcEcotopeSiteChunkLease Lease;
				const bool bAcquired = SiteReader.AcquireFields(Id, 0, FieldMask, Lease, OutError);
				if (Cancellation.IsCancellationRequested())
				{
					bOutCancelled = true;
					return false;
				}
				if (!bAcquired)
				{
					OutError = FString::Printf(
						TEXT("ArcForest valid base cell table could not acquire chunk (%d,%d): %s"),
						ChunkX,
						ChunkY,
						*OutError);
					return false;
				}
				const FArcEcotopeSiteChunkView& View = Lease.GetView();
				FArcEcotopeChunkRect Chunk;
				if (!ValidateAcquiredChunkIdentity(ReaderLayout, Id, View, Chunk, OutError)
					|| !View.LocalSurfaceZ.IsValid()
					|| !View.ValidSurface.IsValid()
					|| !View.ModificationCut.IsValid()
					|| !View.BareRock.IsValid()
					|| !View.ChannelBuffer.IsValid()
					|| !View.ChannelExclusion.IsValid()
					|| !View.EstablishmentWear.IsValid()
					|| !View.SubstrateMobility.IsValid())
				{
					if (OutError.IsEmpty())
					{
						OutError = TEXT("ArcForest valid base cell table acquired invalid field views.");
					}
					return false;
				}
				if (Chunk.BaseCoreRect.Min.Y != SlabMinY || Chunk.BaseCoreRect.Max.Y != SlabMaxY
					|| Chunk.BaseCoreRect.Min.X != ExpectedX)
				{
					OutError = TEXT("ArcForest valid base cell table found a non-contiguous horizontal reader slab.");
					return false;
				}
				for (int32 BaseY = SlabMinY; BaseY < SlabMaxY; ++BaseY)
				{
					for (int32 BaseX = Chunk.BaseCoreRect.Min.X; BaseX < Chunk.BaseCoreRect.Max.X; ++BaseX)
					{
						const uint8* Validity = View.ValidSurface.TryGet(BaseX, BaseY);
						if (Validity == nullptr)
						{
							OutError = TEXT("ArcForest valid base cell table could not sample base validity.");
							return false;
						}
						FValidSiteSample& Sample =
							SlabSamples[BaseX + (BaseY - SlabMinY) * Site.GridDimensions.X];
						Sample.Validity = *Validity;
						Sample.bCovered = true;
						if (*Validity != 0)
						{
							const float* LocalSurfaceZ = View.LocalSurfaceZ.TryGet(BaseX, BaseY);
							const float* ModificationCut = View.ModificationCut.TryGet(
								BaseX >> View.ModificationCut.Level,
								BaseY >> View.ModificationCut.Level);
							const float* BareRock = View.BareRock.TryGet(
								BaseX >> View.BareRock.Level,
								BaseY >> View.BareRock.Level);
							const float* ChannelBuffer = View.ChannelBuffer.TryGet(
								BaseX >> View.ChannelBuffer.Level,
								BaseY >> View.ChannelBuffer.Level);
							const float* ChannelExclusion = View.ChannelExclusion.TryGet(
								BaseX >> View.ChannelExclusion.Level,
								BaseY >> View.ChannelExclusion.Level);
							const float* EstablishmentWear = View.EstablishmentWear.TryGet(
								BaseX >> View.EstablishmentWear.Level,
								BaseY >> View.EstablishmentWear.Level);
							const float* SubstrateMobility = View.SubstrateMobility.TryGet(
								BaseX >> View.SubstrateMobility.Level,
								BaseY >> View.SubstrateMobility.Level);
							if (LocalSurfaceZ == nullptr
								|| ModificationCut == nullptr
								|| BareRock == nullptr
								|| ChannelBuffer == nullptr
								|| ChannelExclusion == nullptr
								|| EstablishmentWear == nullptr
								|| SubstrateMobility == nullptr)
							{
								OutError = TEXT("ArcForest valid base cell table could not sample local surface Z, modification cut or a terrain exclusion.");
								return false;
							}
							Sample.LocalSurfaceZCm = *LocalSurfaceZ;
							Sample.ModificationCut = *ModificationCut;
							Sample.BareRock = *BareRock;
							Sample.ChannelBuffer = *ChannelBuffer;
							Sample.ChannelExclusion = *ChannelExclusion;
							Sample.EstablishmentWear = *EstablishmentWear;
							Sample.SubstrateMobility = *SubstrateMobility;
						}
					}
				}
				ExpectedX = Chunk.BaseCoreRect.Max.X;
			}
			if (ExpectedX != Site.GridDimensions.X)
			{
				OutError = TEXT("ArcForest valid base cell table horizontal slab did not cover the full base-grid width.");
				return false;
			}
			for (int32 BaseY = SlabMinY; BaseY < SlabMaxY; ++BaseY)
			{
				for (int32 BaseX = 0; BaseX < Site.GridDimensions.X; ++BaseX)
				{
					const int32 CellIndex = BaseX + BaseY * Site.GridDimensions.X;
					const FValidSiteSample& Sample =
						SlabSamples[BaseX + (BaseY - SlabMinY) * Site.GridDimensions.X];
					if (!OutTable.BaseValidityMask.IsValidIndex(CellIndex) || !Sample.bCovered)
					{
						OutError = TEXT("ArcForest valid base cell table reader slab has a coverage gap.");
						return false;
					}
					OutTable.BaseValidityMask[CellIndex] = Sample.Validity;
					if (Sample.Validity == 0)
					{
						continue;
					}
					FValidBaseCellEntry Entry;
					Entry.BaseCellIndex = CellIndex;
					Entry.RecordCellIndex = GetRecordCellIndexForBaseCell(Site, CellIndex);
					if (Entry.RecordCellIndex < 0 || Entry.RecordCellIndex >= RecordCellCount)
					{
						OutError = FString::Printf(
							TEXT("ArcForest valid base cell table mapped base cell %d to invalid record cell %d; the record grid contains %d cells."),
							CellIndex,
							Entry.RecordCellIndex,
							RecordCellCount);
						return false;
					}
					Entry.CellAreaSquareCentimeters = Site.GetLevelCellAreaSquareCentimeters(CellIndex, 0);
					OutTable.Entries.Add(Entry);
					OutTable.ModificationCutByValidEntry.Add(Sample.ModificationCut);
					OutTable.BareRockByValidEntry.Add(Sample.BareRock);
					OutTable.ChannelBufferByValidEntry.Add(Sample.ChannelBuffer);
					OutTable.ChannelExclusionByValidEntry.Add(Sample.ChannelExclusion);
					OutTable.EstablishmentWearByValidEntry.Add(Sample.EstablishmentWear);
					OutTable.SubstrateMobilityByValidEntry.Add(Sample.SubstrateMobility);
					OutTable.SubstrateMobilityByBaseCell[CellIndex] = Sample.SubstrateMobility;
					OutTable.ChannelBufferByBaseCell[CellIndex] = Sample.ChannelBuffer;
					OutTable.LocalSurfaceZCmByValidEntry.Add(Sample.LocalSurfaceZCm);
					OutTable.TotalAreaSquareCentimeters += Entry.CellAreaSquareCentimeters;
				}
			}
			ExpectedY = SlabMaxY;
			if (Cancellation.IsCancellationRequested())
			{
				bOutCancelled = true;
				return false;
			}
		}
		if (ExpectedY != Site.GridDimensions.Y)
		{
			OutError = TEXT("ArcForest valid base cell table slabs did not cover the full base-grid height.");
			return false;
		}
		checkSlow(OutTable.ModificationCutByValidEntry.Num() == OutTable.Entries.Num());
		checkSlow(OutTable.BareRockByValidEntry.Num() == OutTable.Entries.Num());
		checkSlow(OutTable.ChannelBufferByValidEntry.Num() == OutTable.Entries.Num());
		checkSlow(OutTable.ChannelExclusionByValidEntry.Num() == OutTable.Entries.Num());
		checkSlow(OutTable.EstablishmentWearByValidEntry.Num() == OutTable.Entries.Num());
		checkSlow(OutTable.SubstrateMobilityByValidEntry.Num() == OutTable.Entries.Num());
		checkSlow(OutTable.SubstrateMobilityByBaseCell.Num() == CellCount);
		checkSlow(OutTable.ChannelBufferByBaseCell.Num() == CellCount);
		checkSlow(OutTable.LocalSurfaceZCmByValidEntry.Num() == OutTable.Entries.Num());
		return true;
	}

	bool TryGetValidBaseCellsForRecordCell(
		const FArcEcotopeSiteMetadata& Site,
		const TConstArrayView<uint8> BaseValidityMask,
		const int32 RecordCellIndex,
		TArray<int32>& OutBaseCellIndices)
	{
		OutBaseCellIndices.Reset();

		const FIntPoint BaseDimensions = Site.GridDimensions;
		const FIntPoint RecordDimensions = Site.GetLevelDimensions(Site.RecordGridLevel);
		const int32 BaseCellCount = Site.GetCellCount();
		const int32 RecordCellCount = Site.GetLevelCellCount(Site.RecordGridLevel);
		if (BaseDimensions.X <= 0 || BaseDimensions.Y <= 0
			|| RecordDimensions.X <= 0 || RecordDimensions.Y <= 0
			|| BaseCellCount <= 0 || RecordCellCount <= 0
			|| BaseValidityMask.Num() != BaseCellCount
			|| RecordCellIndex < 0 || RecordCellIndex >= RecordCellCount)
		{
			return false;
		}

		const int32 RecordX = RecordCellIndex % RecordDimensions.X;
		const int32 RecordY = RecordCellIndex / RecordDimensions.X;
		const int32 ChildSpan = static_cast<int32>(1) << Site.RecordGridLevel;
		const int32 BaseMinX = RecordX * ChildSpan;
		const int32 BaseMinY = RecordY * ChildSpan;
		const int32 BaseMaxX = FMath::Min(BaseMinX + ChildSpan, BaseDimensions.X);
		const int32 BaseMaxY = FMath::Min(BaseMinY + ChildSpan, BaseDimensions.Y);
		for (int32 BaseY = BaseMinY; BaseY < BaseMaxY; ++BaseY)
		{
			for (int32 BaseX = BaseMinX; BaseX < BaseMaxX; ++BaseX)
			{
				const int32 BaseCellIndex = BaseX + BaseY * BaseDimensions.X;
				if (BaseValidityMask[BaseCellIndex] != 0)
				{
					OutBaseCellIndices.Add(BaseCellIndex);
				}
			}
		}
		return true;
	}

	/**
	 * An empty result here is a record cell lying wholly inside the channel hard zone and skips the
	 * cohort, while an empty gather from TryGetValidBaseCellsForRecordCell is corruption and still errors.
	 */
	bool TryFilterTreePlantableBaseCells(
		const FValidBaseCellTable& ValidBaseCells,
		TArray<int32>& InOutBaseCellIndices,
		FString& OutError)
	{
		int32 SurvivorCount = 0;
		for (int32 Index = 0; Index < InOutBaseCellIndices.Num(); ++Index)
		{
			const int32 BaseCellIndex = InOutBaseCellIndices[Index];
			if (!ValidBaseCells.ChannelBufferByBaseCell.IsValidIndex(BaseCellIndex))
			{
				OutError = TEXT("ArcForest promotion found a plantable base cell outside the channel buffer table.");
				return false;
			}
			if (ValidBaseCells.ChannelBufferByBaseCell[BaseCellIndex] < 1.0f)
			{
				InOutBaseCellIndices[SurvivorCount] = BaseCellIndex;
				++SurvivorCount;
			}
		}
		InOutBaseCellIndices.SetNum(SurvivorCount, EAllowShrinking::No);
		return true;
	}

	int32 FindValidBaseCellEntryIndex(
		const FValidBaseCellTable& ValidBaseCells,
		const int32 BaseCellIndex)
	{
		int32 MinIndex = 0;
		int32 MaxIndex = ValidBaseCells.Entries.Num();
		while (MinIndex < MaxIndex)
		{
			const int32 MiddleIndex = MinIndex + (MaxIndex - MinIndex) / 2;
			if (ValidBaseCells.Entries[MiddleIndex].BaseCellIndex < BaseCellIndex)
			{
				MinIndex = MiddleIndex + 1;
			}
			else
			{
				MaxIndex = MiddleIndex;
			}
		}
		return ValidBaseCells.Entries.IsValidIndex(MinIndex)
			&& ValidBaseCells.Entries[MinIndex].BaseCellIndex == BaseCellIndex
			? MinIndex
			: INDEX_NONE;
	}

	/** Static authored response state shared by factored habitat evaluation. */
	struct FHabitatScoringTables
	{
		/** Aliases Species; that array must outlive these tables and must not be resized or reordered after the fill. */
		TArray<UE::ArcForest::Habitat::FHabitatResponseTable> SpeciesResponses;
	};

	/** Immutable scoring terms shared by factored annual habitat evaluation. */
	struct FHabitatScoringState
	{
		FHabitatScoringTables Scoring;
		int32 BaseCellCount = 0;
	};

	struct FFactoredHabitatRunTelemetry
	{
		int64 BuiltSamples = 0;
		int64 HardZeroSamples = 0;
		int64 HardZeroSlopeSamples = 0;
		int64 HardZeroSunLoadSamples = 0;
		int64 HardZeroWindSamples = 0;
		int64 HardZeroFertilitySamples = 0;
		int64 HardZeroTemperatureSamples = 0;
		int64 HardZeroMoistureSamples = 0;
		int64 ProductBytes = 0;
		int64 HardZeroBytes = 0;
		double BuildWallMilliseconds = 0.0;
		double EvaluationWallMilliseconds = 0.0;
		int64 EvaluationBatchCount = 0;
		int64 ForwardTraversalYears = 0;
		int64 ReverseTraversalYears = 0;
		int64 ForwardCacheHitCount = 0;
		int64 ReverseCacheHitCount = 0;
		int64 ForwardPayloadLoadCount = 0;
		int64 ReversePayloadLoadCount = 0;
		int64 ProductionOnlyYears = 0;
		int64 FactorPrimaryYears = 0;
		int32 PeakChunksPerBatch = 0;
	};

	struct FCoverHabitatProviderRunTelemetry
	{
		int64 RecordCells = 0;
		int64 SpeciesCells = 0;
		int64 EvaluationBatches = 0;
		int64 ReusedChunkLeases = 0;
		int32 PeakChunksPerBatch = 0;
		double WallMilliseconds = 0.0;
	};

	void BuildHabitatScoringTables(
		const TArray<FArcForestFrozenSpeciesSetup>& Species,
		FHabitatScoringTables& OutTables)
	{
		OutTables.SpeciesResponses.SetNum(Species.Num());
		for (int32 SpeciesIndex = 0; SpeciesIndex < Species.Num(); ++SpeciesIndex)
		{
			GetHabitatResponses(Species[SpeciesIndex], OutTables.SpeciesResponses[SpeciesIndex].Responses);
		}
	}

	bool BuildHabitatScoringState(
		const FArcEcotopeSiteMetadata& Site,
		const TArray<FArcForestFrozenSpeciesSetup>& Species,
		FHabitatScoringState& OutTables,
		FString& OutError)
	{
		OutTables = FHabitatScoringState();
		OutTables.BaseCellCount = Site.GetCellCount();
		if (OutTables.BaseCellCount <= 0 || Species.IsEmpty())
		{
			OutError = TEXT("ArcForest habitat scoring tables require a non-empty site and species set.");
			return false;
		}
		BuildHabitatScoringTables(Species, OutTables.Scoring);
		return true;
	}

	int64 QuantizeProbability(const double Probability)
	{
		return FMath::Clamp<int64>(
			FMath::RoundToInt64(FMath::Clamp(Probability, 0.0, 1.0) * ProbabilityQuantum),
			0,
			static_cast<int64>(ProbabilityQuantum));
	}

	double DequantizeProbability(const int64 QuantizedProbability)
	{
		return static_cast<double>(QuantizedProbability) / ProbabilityQuantum;
	}

	bool BuildRegionalRecruitmentEnvelopeTable(
		const FArcEcotopeSiteMetadata& Site,
		const FArcEcotopeChunkLayout& Layout,
		const FValidBaseCellTable& ValidBaseCells,
		const FRegionalRecruitmentRecordIndex& RecordIndex,
		const TConstArrayView<FArcForestFrozenSpeciesSetup> Species,
		const FHabitatScoringTables& Scoring,
		const FArcForestHabitatFactorStore& FactorStore,
		const int64 ExistingRetainedBytes,
		const int64 BudgetBytes,
		const FArcForestCancellationToken& Cancellation,
		FRegionalRecruitmentEnvelopeTable& OutTable,
		FRegionalRecruitmentEnvelopeBuildResult& OutResult,
		FString& OutError)
	{
		const double BuildStartSeconds = FPlatformTime::Seconds();
		OutTable.Empty();
		OutResult = FRegionalRecruitmentEnvelopeBuildResult();
		OutResult.bAdmissionAttempted = true;
		OutResult.BudgetBytes = BudgetBytes;
		OutResult.AdmissionReason = TEXT("building");
		OutError.Reset();
		ON_SCOPE_EXIT
		{
			OutResult.BuildMilliseconds = UE::ArcForest::GetElapsedMilliseconds(BuildStartSeconds);
		};

		const int32 BaseCellCount = Site.GetCellCount();
		const FIntPoint RecordDimensions = Site.GetLevelDimensions(Site.RecordGridLevel);
		const int32 RecordCellCount = Site.GetLevelCellCount(Site.RecordGridLevel);
		const int64 RuntimeRecordCellCount = static_cast<int64>(RecordDimensions.X) * RecordDimensions.Y;
		const int32 ValidEntryCount = ValidBaseCells.Entries.Num();
		OutResult.RecordCellCount = RecordCellCount;
		if (BudgetBytes <= 0
			|| ExistingRetainedBytes < 0
			|| ExistingRetainedBytes > BudgetBytes
			|| BaseCellCount <= 0
			|| RecordDimensions.X <= 0
			|| RecordDimensions.Y <= 0
			|| RuntimeRecordCellCount <= 0
			|| RuntimeRecordCellCount > MAX_int32
			|| RuntimeRecordCellCount != RecordCellCount
			|| RecordCellCount == MAX_int32
			|| Species.Num() != Scoring.SpeciesResponses.Num()
			|| ValidBaseCells.ModificationCutByValidEntry.Num() != ValidEntryCount
			|| ValidBaseCells.BareRockByValidEntry.Num() != ValidEntryCount
			|| ValidBaseCells.ChannelBufferByValidEntry.Num() != ValidEntryCount
			|| ValidBaseCells.ChannelExclusionByValidEntry.Num() != ValidEntryCount
			|| ValidBaseCells.EstablishmentWearByValidEntry.Num() != ValidEntryCount
			|| ValidBaseCells.SubstrateMobilityByValidEntry.Num() != ValidEntryCount
			|| RecordIndex.SiteIdentity != &Site
			|| RecordIndex.ValidBaseCellsIdentity != &ValidBaseCells
			|| RecordIndex.RecordOffsets.Num() != RecordCellCount + 1
			|| RecordIndex.RecordOffsets[0] != 0
			|| RecordIndex.RecordOffsets.Last() != ValidEntryCount
			|| RecordIndex.ValidEntryIndicesByRecord.Num() != ValidEntryCount
			|| RecordIndex.ValidEntryIndexByBaseCell.Num() != BaseCellCount
			|| !FactorStore.IsBuilt()
			|| Layout.GetBaseDimensions() != Site.GridDimensions
			|| Layout.GetRecordDimensions() != RecordDimensions)
		{
			OutResult.AdmissionReason = TEXT("input-shape");
			OutError = TEXT("ArcForest regional recruitment envelope received inconsistent site, factor-store, scoring, or record-index state.");
			return false;
		}
		if (Cancellation.IsCancellationRequested())
		{
			OutResult.bCancelled = true;
			OutResult.AdmissionReason = TEXT("cancelled");
			return false;
		}

		int32 PotentialRegionalSpeciesCount = 0;
		for (const FArcForestFrozenSpeciesSetup& SpeciesSetup : Species)
		{
			if (SpeciesSetup.RegionalArrivalPerHectarePerYear > 0.0f)
			{
				if (PotentialRegionalSpeciesCount == MAX_int32)
				{
					OutResult.AdmissionReason = TEXT("species-count-overflow");
					return true;
				}
				++PotentialRegionalSpeciesCount;
			}
		}
		OutResult.IndexedSpeciesCount = PotentialRegionalSpeciesCount;

		int64 EnvelopeValueCount = 0;
		if (!TryMultiplyCapacityValue(
				PotentialRegionalSpeciesCount, RecordCellCount, EnvelopeValueCount)
			|| EnvelopeValueCount > MAX_int32
			|| !TryAccumulateCapacityBytes(
				Species.Num(), sizeof(int32), OutResult.ProjectedBytes)
			|| !TryAccumulateCapacityBytes(
				PotentialRegionalSpeciesCount, sizeof(int32), OutResult.ProjectedBytes)
			|| !TryAccumulateCapacityBytes(
				EnvelopeValueCount, sizeof(double), OutResult.ProjectedBytes)
			|| !TryAddCapacityValue(
				ExistingRetainedBytes, OutResult.ProjectedBytes, OutResult.CombinedProjectedBytes))
		{
			OutResult.AdmissionReason = TEXT("projected-size-overflow");
			return true;
		}
		if (OutResult.CombinedProjectedBytes > BudgetBytes)
		{
			OutResult.AdmissionReason = TEXT("projected-budget");
			return true;
		}

		OutTable.EnvelopeRowBySpecies.Init(INDEX_NONE, Species.Num());
		OutTable.SpeciesIndexByEnvelopeRow.Reserve(PotentialRegionalSpeciesCount);
		for (int32 SpeciesIndex = 0; SpeciesIndex < Species.Num(); ++SpeciesIndex)
		{
			if (Species[SpeciesIndex].RegionalArrivalPerHectarePerYear <= 0.0f)
			{
				continue;
			}
			const int32 EnvelopeRow = OutTable.SpeciesIndexByEnvelopeRow.Add(SpeciesIndex);
			OutTable.EnvelopeRowBySpecies[SpeciesIndex] = EnvelopeRow;
		}
		if (OutTable.SpeciesIndexByEnvelopeRow.Num() != PotentialRegionalSpeciesCount)
		{
			OutResult.AdmissionReason = TEXT("species-map-corruption");
			OutError = TEXT("ArcForest regional recruitment envelope compact species mapping changed during construction.");
			OutTable.Empty();
			return false;
		}
		OutTable.MaxStaticCellWeight.Init(0.0, static_cast<int32>(EnvelopeValueCount));
		if (!TryAccumulateAllocatedBytes(
				OutTable.EnvelopeRowBySpecies.GetAllocatedSize(), OutResult.ActualBytes)
			|| !TryAccumulateAllocatedBytes(
				OutTable.SpeciesIndexByEnvelopeRow.GetAllocatedSize(), OutResult.ActualBytes)
			|| !TryAccumulateAllocatedBytes(
				OutTable.MaxStaticCellWeight.GetAllocatedSize(), OutResult.ActualBytes)
			|| !TryAddCapacityValue(
				ExistingRetainedBytes, OutResult.ActualBytes, OutResult.CombinedActualBytes))
		{
			OutResult.AdmissionReason = TEXT("actual-size-overflow");
			OutTable.Empty();
			return true;
		}
		if (OutResult.CombinedActualBytes > BudgetBytes)
		{
			OutResult.AdmissionReason = TEXT("actual-capacity-budget");
			OutTable.Empty();
			return true;
		}
		if (PotentialRegionalSpeciesCount == 0)
		{
			OutTable.SiteIdentity = &Site;
			OutTable.LayoutIdentity = &Layout;
			OutTable.ValidBaseCellsIdentity = &ValidBaseCells;
			OutTable.RecordIndexIdentity = &RecordIndex;
			OutTable.FactorStoreIdentity = &FactorStore;
			OutResult.bAdmitted = true;
			OutResult.AdmissionReason = TEXT("admitted-no-regional-species");
			return true;
		}

		int64 WorkCounter = 0;
		for (int32 ChunkIndex = 0; ChunkIndex < Layout.GetChunkCount(); ++ChunkIndex)
		{
			if (Cancellation.IsCancellationRequested())
			{
				OutResult.bCancelled = true;
				OutResult.AdmissionReason = TEXT("cancelled");
				OutTable.Empty();
				return false;
			}
			FArcEcotopeChunkRect ExpectedChunk;
			if (!Layout.GetChunk(ChunkIndex, ExpectedChunk))
			{
				OutResult.AdmissionReason = TEXT("layout-corruption");
				OutError = TEXT("ArcForest regional recruitment envelope encountered an invalid layout chunk.");
				OutTable.Empty();
				return false;
			}

			{
				FArcForestHabitatFactorLease Lease;
				if (!FactorStore.AcquireChunk(ExpectedChunk.Id, Lease, OutError))
				{
					OutResult.bCancelled = Cancellation.IsCancellationRequested();
					OutResult.AdmissionReason = OutResult.bCancelled
						? TEXT("cancelled")
						: TEXT("factor-acquire-failed");
					OutTable.Empty();
					return false;
				}
				ON_SCOPE_EXIT
				{
					FactorStore.Release(Lease);
				};
				const FArcForestHabitatFactorChunk& Chunk = Lease.GetChunk();
				const int32 CoreWidth = Chunk.CoreRect.Width();
				const int32 CoreHeight = Chunk.CoreRect.Height();
				const int64 CoreCellCount64 = static_cast<int64>(CoreWidth) * CoreHeight;
				int64 ProductCount = 0;
				if (Chunk.Id != ExpectedChunk.Id
					|| Chunk.CoreRect != ExpectedChunk.BaseCoreRect
					|| CoreWidth <= 0
					|| CoreHeight <= 0
					|| CoreCellCount64 <= 0
					|| CoreCellCount64 > MAX_int32
					|| Chunk.GetCoreCellCount() != CoreCellCount64
					|| Chunk.SpeciesCount != Species.Num()
					|| !TryMultiplyCapacityValue(CoreCellCount64, Species.Num(), ProductCount)
					|| ProductCount != Chunk.StaticProducts.Num()
					|| ProductCount != Chunk.StaticHardZeros.Num())
				{
					OutResult.AdmissionReason = TEXT("factor-chunk-corruption");
					OutError = TEXT("ArcForest regional recruitment envelope acquired an inconsistent factor chunk.");
					OutTable.Empty();
					return false;
				}

				const int32 CoreCellCount = static_cast<int32>(CoreCellCount64);
				for (int32 BaseY = Chunk.CoreRect.Min.Y; BaseY < Chunk.CoreRect.Max.Y; ++BaseY)
				{
					for (int32 BaseX = Chunk.CoreRect.Min.X; BaseX < Chunk.CoreRect.Max.X; ++BaseX)
					{
						if ((++WorkCounter & 4095) == 0 && Cancellation.IsCancellationRequested())
						{
							OutResult.bCancelled = true;
							OutResult.AdmissionReason = TEXT("cancelled");
							OutTable.Empty();
							return false;
						}
						const int32 LocalIndex = BaseX - Chunk.CoreRect.Min.X
							+ (BaseY - Chunk.CoreRect.Min.Y) * CoreWidth;
						const int64 BaseCellIndex64 = static_cast<int64>(BaseX)
							+ static_cast<int64>(BaseY) * Site.GridDimensions.X;
						if (LocalIndex < 0
							|| LocalIndex >= CoreCellCount
							|| BaseCellIndex64 < 0
							|| BaseCellIndex64 >= BaseCellCount)
						{
							OutResult.AdmissionReason = TEXT("factor-cell-corruption");
							OutError = TEXT("ArcForest regional recruitment envelope factor traversal left the runtime base grid.");
							OutTable.Empty();
							return false;
						}
						const int32 BaseCellIndex = static_cast<int32>(BaseCellIndex64);
						const int32 ValidEntryIndex =
							RecordIndex.ValidEntryIndexByBaseCell[BaseCellIndex];
						if (ValidEntryIndex == INDEX_NONE)
						{
							continue;
						}
						if (!ValidBaseCells.Entries.IsValidIndex(ValidEntryIndex)
							|| ValidBaseCells.Entries[ValidEntryIndex].BaseCellIndex != BaseCellIndex
							|| !ValidBaseCells.ModificationCutByValidEntry.IsValidIndex(ValidEntryIndex)
							|| !ValidBaseCells.BareRockByValidEntry.IsValidIndex(ValidEntryIndex)
							|| !ValidBaseCells.ChannelBufferByValidEntry.IsValidIndex(ValidEntryIndex)
							|| !ValidBaseCells.EstablishmentWearByValidEntry.IsValidIndex(ValidEntryIndex)
							|| !ValidBaseCells.SubstrateMobilityByValidEntry.IsValidIndex(ValidEntryIndex))
						{
							OutResult.AdmissionReason = TEXT("valid-entry-corruption");
							OutError = TEXT("ArcForest regional recruitment envelope dense valid-entry mapping was inconsistent.");
							OutTable.Empty();
							return false;
						}

						const FValidBaseCellEntry& Entry = ValidBaseCells.Entries[ValidEntryIndex];
						const double Cut = static_cast<double>(
							ValidBaseCells.ModificationCutByValidEntry[ValidEntryIndex]);
						const double BareRock = static_cast<double>(
							ValidBaseCells.BareRockByValidEntry[ValidEntryIndex]);
						const double ChannelBuffer = static_cast<double>(
							ValidBaseCells.ChannelBufferByValidEntry[ValidEntryIndex]);
						if (Entry.RecordCellIndex < 0
							|| Entry.RecordCellIndex >= RecordCellCount
							|| !FMath::IsFinite(Entry.CellAreaSquareCentimeters)
							|| Entry.CellAreaSquareCentimeters <= 0.0
							|| !FMath::IsFinite(Cut)
							|| !FMath::IsFinite(BareRock)
							|| !FMath::IsFinite(ChannelBuffer))
						{
							OutResult.AdmissionReason = TEXT("valid-entry-corruption");
							OutError = TEXT("ArcForest regional recruitment envelope found invalid area, suppression factor, or record mapping.");
							OutTable.Empty();
							return false;
						}

						for (int32 EnvelopeRow = 0;
							EnvelopeRow < PotentialRegionalSpeciesCount;
							++EnvelopeRow)
						{
							if (OutResult.ScannedFactorSamples == MAX_int64)
							{
								OutResult.AdmissionReason = TEXT("sample-count-overflow");
								OutTable.Empty();
								return true;
							}
							++OutResult.ScannedFactorSamples;
							const int32 SpeciesIndex = OutTable.SpeciesIndexByEnvelopeRow[EnvelopeRow];
							const int64 ProductIndex = static_cast<int64>(SpeciesIndex) * CoreCellCount
								+ LocalIndex;
							if (ProductIndex < 0
								|| ProductIndex > MAX_int32
								|| !Chunk.StaticProducts.IsValidIndex(static_cast<int32>(ProductIndex))
								|| !Chunk.StaticHardZeros.IsValidIndex(static_cast<int32>(ProductIndex)))
							{
								OutResult.AdmissionReason = TEXT("factor-sample-corruption");
								OutError = TEXT("ArcForest regional recruitment envelope factor sample was out of range.");
								OutTable.Empty();
								return false;
							}

							const double StaticProduct = static_cast<double>(
								Chunk.StaticProducts[static_cast<int32>(ProductIndex)]);
							if (!FMath::IsFinite(StaticProduct) || StaticProduct < 0.0)
							{
								OutResult.AdmissionReason = TEXT("factor-sample-corruption");
								OutError = TEXT("ArcForest regional recruitment envelope found an invalid static habitat product.");
								OutTable.Empty();
								return false;
							}
							const double UpperHabitat =
								Chunk.StaticHardZeros[static_cast<int32>(ProductIndex)] ? 0.0 : StaticProduct;
							if (!FMath::IsFinite(UpperHabitat) || UpperHabitat < 0.0)
							{
								OutResult.AdmissionReason = TEXT("upper-bound-corruption");
								OutError = TEXT("ArcForest regional recruitment envelope produced an invalid habitat upper bound.");
								OutTable.Empty();
								return false;
							}
							const int64 UpperHabitatQuantum = UpperHabitat > 0.0
								? FMath::Max<int64>(QuantizeProbability(UpperHabitat), 1)
								: 0;
							const double UpperHabitatQuantum01 =
								DequantizeProbability(UpperHabitatQuantum);
							// Every factor here must also enter the acceptance term at the same entry, or
							// AcceptanceRatio exceeds one and the run hard-errors; acceptance may carry more.
							const double CutFactor = UE::ArcForest::Habitat::MakeExclusionFactor(
									Cut, Species[SpeciesIndex].ExclusionSensitivity)
								* UE::ArcForest::Habitat::MakeExclusionFactor(
									BareRock, Species[SpeciesIndex].ExclusionSensitivity)
								* UE::ArcForest::Habitat::MakeExclusionFactor(
									ChannelBuffer, Species[SpeciesIndex].ExclusionSensitivity);
							const double StaticCellWeight = Entry.CellAreaSquareCentimeters
								* UpperHabitatQuantum01 * CutFactor;
							const int64 EnvelopeIndex = static_cast<int64>(EnvelopeRow) * RecordCellCount
								+ Entry.RecordCellIndex;
							if (!FMath::IsFinite(CutFactor)
								|| CutFactor < 0.0
								|| CutFactor > 1.0
								|| !FMath::IsFinite(StaticCellWeight)
								|| StaticCellWeight < 0.0
								|| EnvelopeIndex < 0
								|| EnvelopeIndex >= OutTable.MaxStaticCellWeight.Num())
							{
								OutResult.AdmissionReason = TEXT("upper-bound-corruption");
								OutError = TEXT("ArcForest regional recruitment envelope produced an invalid weighted bound.");
								OutTable.Empty();
								return false;
							}
							double& Envelope =
								OutTable.MaxStaticCellWeight[static_cast<int32>(EnvelopeIndex)];
							Envelope = FMath::Max(Envelope, StaticCellWeight);
						}
					}
				}
			}
			++OutResult.ScannedChunkCount;
		}

		if (Cancellation.IsCancellationRequested())
		{
			OutResult.bCancelled = true;
			OutResult.AdmissionReason = TEXT("cancelled");
			OutTable.Empty();
			return false;
		}
		OutTable.SiteIdentity = &Site;
		OutTable.LayoutIdentity = &Layout;
		OutTable.ValidBaseCellsIdentity = &ValidBaseCells;
		OutTable.RecordIndexIdentity = &RecordIndex;
		OutTable.FactorStoreIdentity = &FactorStore;
		OutResult.bAdmitted = true;
		OutResult.AdmissionReason = TEXT("admitted");
		return true;
	}

	/** Year-scoped facade over immutable annual inputs and run-scoped factor chunks. */
	class FAnnualFactoredHabitatProvider
	{
	public:
		class FChunkLease
		{
		public:
			FChunkLease() = default;
			FChunkLease(FChunkLease&&) = default;
			FChunkLease& operator=(FChunkLease&&) = default;
			FChunkLease(const FChunkLease&) = delete;
			FChunkLease& operator=(const FChunkLease&) = delete;

			bool IsValid() const { return FactorLease.IsValid(); }
			int32 GetCoreCellCount() const
			{
				return Chunk != nullptr ? Chunk->GetCoreCellCount() : 0;
			}

		private:
			friend class FAnnualFactoredHabitatProvider;
			FArcForestHabitatFactorLease FactorLease;
			const FAnnualFactoredHabitatProvider* Owner = nullptr;
			const FArcForestHabitatFactorChunk* Chunk = nullptr;
		};

		FAnnualFactoredHabitatProvider(
			const FArcEcotopeSiteMetadata& InSite,
			const FHabitatScoringTables& InScoring,
			const FArcEcotopeChunkLayout& InLayout,
			const FArcForestHabitatFactorStore& InStore,
			const FArcForestCancellationToken& InCancellation)
			: Site(InSite)
			, Scoring(InScoring)
			, Layout(InLayout)
			, Store(InStore)
			, Cancellation(InCancellation)
		{
		}

		int32 GetMaximumBatchChunkCount() const { return Store.GetMaximumBatchChunkCount(); }
		const FArcForestHabitatFactorChunk& GetChunk(const FChunkLease& Lease) const
		{
			check(Lease.Owner == this && Lease.Chunk != nullptr && Lease.FactorLease.IsValid());
			return *Lease.Chunk;
		}

		bool AcquireChunk(const FArcEcotopeChunkId& Id, FChunkLease& OutLease, FString& OutError) const
		{
			OutLease = FChunkLease();
			if (Cancellation.IsCancellationRequested())
			{
				OutError = TEXT("ArcForest annual habitat provider acquisition was cancelled.");
				return false;
			}
			FArcEcotopeChunkRect ExpectedChunk;
			if (!Layout.GetChunk(Id, ExpectedChunk))
			{
				OutError = TEXT("ArcForest annual habitat provider received an invalid layout chunk id.");
				return false;
			}
			if (!Store.AcquireChunk(Id, OutLease.FactorLease, OutError))
			{
				return false;
			}
			const FArcForestHabitatFactorChunk& Chunk = OutLease.FactorLease.GetChunk();
			const int32 CoreCellCount = Chunk.GetCoreCellCount();
			const int64 ProductCount = static_cast<int64>(CoreCellCount) * Scoring.SpeciesResponses.Num();
			if (Chunk.Id != Id
				|| Chunk.CoreRect != ExpectedChunk.BaseCoreRect
				|| CoreCellCount <= 0
				|| Chunk.SpeciesCount != Scoring.SpeciesResponses.Num()
				|| ProductCount != Chunk.StaticProducts.Num()
				|| ProductCount != Chunk.StaticHardZeros.Num())
			{
				OutLease = FChunkLease();
				OutError = TEXT("ArcForest annual habitat provider acquired an inconsistent factor chunk.");
				return false;
			}
			OutLease.Owner = this;
			OutLease.Chunk = &Chunk;
			return true;
		}

		/** Fast path for a caller already walking a validated leased core. */
		FORCEINLINE uint32 EvaluateQuantizedSiteOnlyCoreLocal(
			const FChunkLease& Lease,
			const int32 SpeciesIndex,
			const int32 LocalIndex) const
		{
			checkSlow(Lease.Owner == this && Lease.Chunk != nullptr && Lease.FactorLease.IsValid());
			const FArcForestHabitatFactorChunk& Chunk = *Lease.Chunk;
			const int32 ProductIndex = SpeciesIndex * Chunk.GetCoreCellCount() + LocalIndex;
			checkSlow(Scoring.SpeciesResponses.IsValidIndex(SpeciesIndex));
			checkSlow(Chunk.StaticProducts.IsValidIndex(ProductIndex));
			checkSlow(Chunk.StaticHardZeros.IsValidIndex(ProductIndex));
			const double Habitat = Chunk.StaticHardZeros[ProductIndex]
				? 0.0
				: static_cast<double>(Chunk.StaticProducts[ProductIndex]);
			return static_cast<uint32>(Habitat > 0.0
				? FMath::Max<int64>(QuantizeProbability(Habitat), 1)
				: 0);
		}

	private:
		const FArcEcotopeSiteMetadata& Site;
		const FHabitatScoringTables& Scoring;
		const FArcEcotopeChunkLayout& Layout;
		const FArcForestHabitatFactorStore& Store;
		const FArcForestCancellationToken& Cancellation;
	};

	struct FFactoredHabitatQueryContext
	{
		const FArcForestRunInput* Input = nullptr;
		const FArcEcotopeSiteMetadata* Site = nullptr;
		const FHabitatScoringTables* Scoring = nullptr;
		const FArcEcotopeChunkLayout* Layout = nullptr;
		const FValidBaseCellTable* ValidBaseCells = nullptr;
		const FRegionalHabitatRouteSeedTable* RegionalRouteSeeds = nullptr;
		const FArcForestHabitatFactorStore* Store = nullptr;
		const FArcForestCancellationToken* Cancellation = nullptr;

		bool IsValid() const
		{
			return Input != nullptr
				&& Site != nullptr
				&& Scoring != nullptr
				&& Layout != nullptr
				&& ValidBaseCells != nullptr
				&& Store != nullptr
				&& Cancellation != nullptr
				&& Store->IsBuilt()
				&& Layout->GetBaseDimensions() == Site->GridDimensions
				&& (RegionalRouteSeeds == nullptr
					|| (RegionalRouteSeeds->Seeds.Num() == ValidBaseCells->Entries.Num()
						&& RegionalRouteSeeds->SiteIdentity == Site
						&& RegionalRouteSeeds->LayoutIdentity == Layout
						&& RegionalRouteSeeds->ValidBaseCellsIdentity == ValidBaseCells));
		}
	};

	struct FFactoredHabitatQuery
	{
		int32 SpeciesIndex = INDEX_NONE;
		int32 BaseCellIndex = INDEX_NONE;
		int32 OutputIndex = INDEX_NONE;
		int32 RouteSeedIndex = INDEX_NONE;
		int64 StableOrdinal = 0;
	};
	static_assert(sizeof(FFactoredHabitatQuery) == 24);

	struct FFactoredHabitatQueryTelemetry
	{
		int64 Calls = 0;
		int64 Requests = 0;
		int64 SeededRequests = 0;
		int64 SeedHits = 0;
		int64 UnseededFallbackRequests = 0;
		int64 RouteSeedValidationFailures = 0;
		int64 CompletedSeededWindows = 0;
		int64 CompletedFallbackWindows = 0;
		int64 CompletedMixedWindows = 0;
		int64 ChunkAcquires = 0;
		int64 Batches = 0;
		int64 TotalPlannedShards = 0;
		int64 ParallelLaunches = 0;
		int64 PeakScratchBytes = 0;
		int32 PeakQueriesPerWindow = 0;
		int32 PeakChunksPerBatch = 0;
		int32 PeakShardsPerBatch = 0;
		int32 MinimumRoutesPerShard = MAX_int32;
		int32 PeakRoutesPerShard = 0;
		double RoutingWallMilliseconds = 0.0;
		double SeededRoutingWallMilliseconds = 0.0;
		double FallbackRoutingWallMilliseconds = 0.0;
		double MixedRoutingWallMilliseconds = 0.0;
		double AcquireWallMilliseconds = 0.0;
		double EvaluationWallMilliseconds = 0.0;
		double WallMilliseconds = 0.0;
	};

	struct FFactoredHabitatConsumerTelemetry
	{
		FFactoredHabitatQueryTelemetry Growth;
		FFactoredHabitatQueryTelemetry LocalRecruitment;
		FFactoredHabitatQueryTelemetry RegionalSampler;
	};

	struct FFactoredHabitatQueryRoute
	{
		int32 QueryIndex = INDEX_NONE;
		int32 CoreLocalIndex = INDEX_NONE;
	};

	struct FFactoredHabitatQueryGroup
	{
		FArcEcotopeChunkId ChunkId;
		FIntPoint CoreMin = FIntPoint::ZeroValue;
		int32 CoreWidth = 0;
		int32 CoreHeight = 0;
		int32 CoreCellCount = 0;
		int32 QueryCount = 0;
		int32 RouteBegin = 0;
		int32 RouteEnd = 0;
		int32 NextRouteWrite = 0;
		int32 MaximumUsefulShardCount = 1;
		int32 PlannedShardCount = 1;
	};

	struct FFactoredHabitatQueryShard
	{
		int32 BatchGroupIndex = INDEX_NONE;
		int32 RouteBegin = 0;
		int32 RouteEnd = 0;
	};

	struct FFactoredHabitatQueryShardResult
	{
		bool bCancelled = false;
	};

	/** Conservative TMap storage allowance per unique chunk; actual storage is checked before evaluation. */
	constexpr int64 HabitatQueryChunkMapBytesPerGroup = 64;

	int64 GetFactoredHabitatQueryScratchBudgetBytes()
	{
		return static_cast<int64>(FMath::Max(
			CVarArcForestHabitatQueryScratchBudgetMiB.GetValueOnAnyThread(), 1)) * 1024ll * 1024ll;
	}

	/**
	 * CallerBytesPerQuery includes every caller-owned array that remains alive during provider evaluation,
	 * including the FFactoredHabitatQuery itself. CallerFixedScratchBytes covers non-window storage. The helper
	 * side assumes the worst case of one unique chunk per query, including
	 * its hash-map entry, plus the maximum permitted lease/shard/result batch. This keeps the budget authoritative
	 * before the caller allocates a window; actual TArray/TMap sizes are checked again before evaluation.
	 */
	int32 GetMaximumFactoredHabitatQueriesPerWindow(
		const int64 CallerBytesPerQuery,
		const int64 CallerFixedScratchBytes = 0)
	{
		if (CallerBytesPerQuery <= 0 || CallerFixedScratchBytes < 0)
		{
			return 0;
		}
		int64 FixedScratchBytes = HabitatQueryAllocatorGuardBytes;
		if (!TryAddCapacityValue(
				FixedScratchBytes, CallerFixedScratchBytes, FixedScratchBytes)
			|| !TryAccumulateCapacityBytes(
				MaximumHabitatFactorTaskSlots,
				sizeof(FAnnualFactoredHabitatProvider::FChunkLease),
				FixedScratchBytes)
			|| !TryAccumulateCapacityBytes(
				MaximumHabitatFactorTaskSlots,
				sizeof(FFactoredHabitatQueryShard),
				FixedScratchBytes)
			|| !TryAccumulateCapacityBytes(
				MaximumHabitatFactorTaskSlots,
				sizeof(FFactoredHabitatQueryShardResult),
				FixedScratchBytes))
		{
			return 0;
		}
		int64 BytesPerQuery = CallerBytesPerQuery;
		if (!TryAccumulateCapacityBytes(1, sizeof(int32), BytesPerQuery)
			|| !TryAccumulateCapacityBytes(1, sizeof(FFactoredHabitatQueryRoute), BytesPerQuery)
			|| !TryAccumulateCapacityBytes(1, sizeof(FFactoredHabitatQueryGroup), BytesPerQuery)
			|| !TryAddCapacityValue(
				BytesPerQuery, HabitatQueryChunkMapBytesPerGroup, BytesPerQuery)
			|| !TryAccumulateCapacityBytes(1, sizeof(uint32), BytesPerQuery))
		{
			return 0;
		}
		const int64 BudgetBytes = GetFactoredHabitatQueryScratchBudgetBytes();
		if (FixedScratchBytes >= BudgetBytes)
		{
			return 0;
		}
		return static_cast<int32>(FMath::Min<int64>(
			(BudgetBytes - FixedScratchBytes) / BytesPerQuery,
			MAX_int32));
	}

	bool EvaluateFactoredHabitatQueries(
		const FFactoredHabitatQueryContext& Context,
		const TConstArrayView<FFactoredHabitatQuery> Queries,
		const int64 CallerAllocatedScratchBytes,
		TArray<uint32>& OutQuantums,
		FFactoredHabitatQueryTelemetry& InOutTelemetry,
		FString& OutError)
	{
		const double StartSeconds = FPlatformTime::Seconds();
		FFactoredHabitatQueryTelemetry PendingTelemetry;
		PendingTelemetry.Calls = 1;
		bool bTelemetryCommitFailed = false;
		bool bSawSeededRequests = false;
		bool bSawFallbackRequests = false;
		const auto CommitPendingTelemetry = [&]()
		{
			int64 NewCalls = 0;
			int64 NewRequests = 0;
			int64 NewSeededRequests = 0;
			int64 NewSeedHits = 0;
			int64 NewUnseededFallbackRequests = 0;
			int64 NewRouteSeedValidationFailures = 0;
			int64 NewCompletedSeededWindows = 0;
			int64 NewCompletedFallbackWindows = 0;
			int64 NewCompletedMixedWindows = 0;
			int64 NewChunkAcquires = 0;
			int64 NewBatches = 0;
			int64 NewPlannedShards = 0;
			int64 NewParallelLaunches = 0;
			const double NewRoutingWallMilliseconds =
				InOutTelemetry.RoutingWallMilliseconds + PendingTelemetry.RoutingWallMilliseconds;
			const double NewSeededRoutingWallMilliseconds =
				InOutTelemetry.SeededRoutingWallMilliseconds
				+ PendingTelemetry.SeededRoutingWallMilliseconds;
			const double NewFallbackRoutingWallMilliseconds =
				InOutTelemetry.FallbackRoutingWallMilliseconds
				+ PendingTelemetry.FallbackRoutingWallMilliseconds;
			const double NewMixedRoutingWallMilliseconds =
				InOutTelemetry.MixedRoutingWallMilliseconds
				+ PendingTelemetry.MixedRoutingWallMilliseconds;
			const double NewAcquireWallMilliseconds =
				InOutTelemetry.AcquireWallMilliseconds + PendingTelemetry.AcquireWallMilliseconds;
			const double NewEvaluationWallMilliseconds =
				InOutTelemetry.EvaluationWallMilliseconds + PendingTelemetry.EvaluationWallMilliseconds;
			const double NewWallMilliseconds =
				InOutTelemetry.WallMilliseconds + PendingTelemetry.WallMilliseconds;
			if (!TryAddCapacityValue(InOutTelemetry.Calls, PendingTelemetry.Calls, NewCalls)
				|| !TryAddCapacityValue(
					InOutTelemetry.Requests, PendingTelemetry.Requests, NewRequests)
				|| !TryAddCapacityValue(
					InOutTelemetry.SeededRequests,
					PendingTelemetry.SeededRequests,
					NewSeededRequests)
				|| !TryAddCapacityValue(
					InOutTelemetry.SeedHits, PendingTelemetry.SeedHits, NewSeedHits)
				|| !TryAddCapacityValue(
					InOutTelemetry.UnseededFallbackRequests,
					PendingTelemetry.UnseededFallbackRequests,
					NewUnseededFallbackRequests)
				|| !TryAddCapacityValue(
					InOutTelemetry.RouteSeedValidationFailures,
					PendingTelemetry.RouteSeedValidationFailures,
					NewRouteSeedValidationFailures)
				|| !TryAddCapacityValue(
					InOutTelemetry.CompletedSeededWindows,
					PendingTelemetry.CompletedSeededWindows,
					NewCompletedSeededWindows)
				|| !TryAddCapacityValue(
					InOutTelemetry.CompletedFallbackWindows,
					PendingTelemetry.CompletedFallbackWindows,
					NewCompletedFallbackWindows)
				|| !TryAddCapacityValue(
					InOutTelemetry.CompletedMixedWindows,
					PendingTelemetry.CompletedMixedWindows,
					NewCompletedMixedWindows)
				|| !TryAddCapacityValue(
					InOutTelemetry.ChunkAcquires,
					PendingTelemetry.ChunkAcquires,
					NewChunkAcquires)
				|| !TryAddCapacityValue(InOutTelemetry.Batches, PendingTelemetry.Batches, NewBatches)
				|| !TryAddCapacityValue(
					InOutTelemetry.TotalPlannedShards,
					PendingTelemetry.TotalPlannedShards,
					NewPlannedShards)
				|| !TryAddCapacityValue(
					InOutTelemetry.ParallelLaunches,
					PendingTelemetry.ParallelLaunches,
					NewParallelLaunches)
				|| !FMath::IsFinite(NewRoutingWallMilliseconds)
				|| !FMath::IsFinite(NewSeededRoutingWallMilliseconds)
				|| !FMath::IsFinite(NewFallbackRoutingWallMilliseconds)
				|| !FMath::IsFinite(NewMixedRoutingWallMilliseconds)
				|| !FMath::IsFinite(NewAcquireWallMilliseconds)
				|| !FMath::IsFinite(NewEvaluationWallMilliseconds)
				|| !FMath::IsFinite(NewWallMilliseconds))
			{
				if (OutError.IsEmpty())
				{
					OutError = TEXT("ArcForest grouped habitat query telemetry overflowed.");
				}
				bTelemetryCommitFailed = true;
				return false;
			}

			const bool bHadPlannedShards = InOutTelemetry.TotalPlannedShards > 0;
			InOutTelemetry.Calls = NewCalls;
			InOutTelemetry.Requests = NewRequests;
			InOutTelemetry.SeededRequests = NewSeededRequests;
			InOutTelemetry.SeedHits = NewSeedHits;
			InOutTelemetry.UnseededFallbackRequests = NewUnseededFallbackRequests;
			InOutTelemetry.RouteSeedValidationFailures = NewRouteSeedValidationFailures;
			InOutTelemetry.CompletedSeededWindows = NewCompletedSeededWindows;
			InOutTelemetry.CompletedFallbackWindows = NewCompletedFallbackWindows;
			InOutTelemetry.CompletedMixedWindows = NewCompletedMixedWindows;
			InOutTelemetry.ChunkAcquires = NewChunkAcquires;
			InOutTelemetry.Batches = NewBatches;
			InOutTelemetry.TotalPlannedShards = NewPlannedShards;
			InOutTelemetry.ParallelLaunches = NewParallelLaunches;
			InOutTelemetry.PeakScratchBytes = FMath::Max(
				InOutTelemetry.PeakScratchBytes, PendingTelemetry.PeakScratchBytes);
			InOutTelemetry.PeakQueriesPerWindow = FMath::Max(
				InOutTelemetry.PeakQueriesPerWindow, PendingTelemetry.PeakQueriesPerWindow);
			InOutTelemetry.PeakChunksPerBatch = FMath::Max(
				InOutTelemetry.PeakChunksPerBatch, PendingTelemetry.PeakChunksPerBatch);
			InOutTelemetry.PeakShardsPerBatch = FMath::Max(
				InOutTelemetry.PeakShardsPerBatch, PendingTelemetry.PeakShardsPerBatch);
			if (PendingTelemetry.TotalPlannedShards > 0)
			{
				checkSlow(PendingTelemetry.MinimumRoutesPerShard != MAX_int32);
				InOutTelemetry.MinimumRoutesPerShard = bHadPlannedShards
					? FMath::Min(
						InOutTelemetry.MinimumRoutesPerShard,
						PendingTelemetry.MinimumRoutesPerShard)
					: PendingTelemetry.MinimumRoutesPerShard;
				InOutTelemetry.PeakRoutesPerShard = FMath::Max(
					InOutTelemetry.PeakRoutesPerShard, PendingTelemetry.PeakRoutesPerShard);
			}
			InOutTelemetry.RoutingWallMilliseconds = NewRoutingWallMilliseconds;
			InOutTelemetry.SeededRoutingWallMilliseconds = NewSeededRoutingWallMilliseconds;
			InOutTelemetry.FallbackRoutingWallMilliseconds =
				NewFallbackRoutingWallMilliseconds;
			InOutTelemetry.MixedRoutingWallMilliseconds = NewMixedRoutingWallMilliseconds;
			InOutTelemetry.AcquireWallMilliseconds = NewAcquireWallMilliseconds;
			InOutTelemetry.EvaluationWallMilliseconds = NewEvaluationWallMilliseconds;
			InOutTelemetry.WallMilliseconds = NewWallMilliseconds;
			PendingTelemetry = FFactoredHabitatQueryTelemetry();
			return true;
		};
		const auto Evaluate = [&]()
		{
		PendingTelemetry.Requests = Queries.Num();
		if (!Context.IsValid())
		{
			if (Context.RegionalRouteSeeds != nullptr
				&& (Context.ValidBaseCells == nullptr
					|| Context.RegionalRouteSeeds->Seeds.Num()
						!= Context.ValidBaseCells->Entries.Num()
					|| Context.RegionalRouteSeeds->SiteIdentity != Context.Site
					|| Context.RegionalRouteSeeds->LayoutIdentity != Context.Layout
					|| Context.RegionalRouteSeeds->ValidBaseCellsIdentity
						!= Context.ValidBaseCells))
			{
				PendingTelemetry.RouteSeedValidationFailures = 1;
			}
			OutError = TEXT("ArcForest grouped habitat queries received an invalid annual context.");
			return false;
		}
		if (CallerAllocatedScratchBytes < 0)
		{
			OutError = TEXT("ArcForest grouped habitat query caller reported invalid scratch accounting.");
			return false;
		}
		if (Queries.IsEmpty())
		{
			OutQuantums.SetNumUninitialized(0);
			int64 EmptyScratchBytes = CallerAllocatedScratchBytes;
			if (!TryAddCapacityValue(
					EmptyScratchBytes,
					static_cast<int64>(OutQuantums.GetAllocatedSize()),
					EmptyScratchBytes)
				|| EmptyScratchBytes > GetFactoredHabitatQueryScratchBudgetBytes())
			{
				OutError = TEXT("ArcForest grouped habitat query caller scratch exceeded its configured budget.");
				return false;
			}
			PendingTelemetry.PeakScratchBytes = EmptyScratchBytes;
			return true;
		}
		const FAnnualFactoredHabitatProvider Provider(
			*Context.Site,
			*Context.Scoring,
			*Context.Layout,
			*Context.Store,
			*Context.Cancellation);
		const int32 MaximumBatchChunkCount = Provider.GetMaximumBatchChunkCount();
		if (MaximumBatchChunkCount <= 0)
		{
			OutError = TEXT("ArcForest grouped habitat query provider has no safe lease batch.");
			return false;
		}
		const int32 AvailableShardSlots = FMath::Clamp(
			FPlatformMisc::NumberOfWorkerThreadsToSpawn(), 1, MaximumHabitatFactorTaskSlots);
		const int32 MaximumGroupCount = FMath::Min(Queries.Num(), Context.Layout->GetChunkCount());
		const int32 MaximumLiveBatchGroupCount = FMath::Min(
			MaximumGroupCount, FMath::Min(MaximumBatchChunkCount, AvailableShardSlots));
		const int32 MaximumLiveShardCount = FMath::Min(Queries.Num(), AvailableShardSlots);
		int64 ProjectedScratchBytes = CallerAllocatedScratchBytes;
		int64 ProjectedOutputBytes = 0;
		if (!TryAddCapacityValue(
				ProjectedScratchBytes,
				HabitatQueryAllocatorGuardBytes,
				ProjectedScratchBytes)
			|| !TryMultiplyCapacityValue(Queries.Num(), sizeof(uint32), ProjectedOutputBytes))
		{
			OutError = TEXT("ArcForest grouped habitat query output size overflowed.");
			return false;
		}
		ProjectedOutputBytes = FMath::Max<int64>(
			ProjectedOutputBytes, static_cast<int64>(OutQuantums.GetAllocatedSize()));
		if (!TryAddCapacityValue(ProjectedScratchBytes, ProjectedOutputBytes, ProjectedScratchBytes)
			|| !TryAccumulateCapacityBytes(
				Queries.Num(), sizeof(int32), ProjectedScratchBytes)
			|| !TryAccumulateCapacityBytes(
				Queries.Num(), sizeof(FFactoredHabitatQueryRoute), ProjectedScratchBytes)
			|| !TryAccumulateCapacityBytes(
				MaximumGroupCount, sizeof(FFactoredHabitatQueryGroup), ProjectedScratchBytes)
			|| !TryMultiplyCapacityValue(
				MaximumGroupCount, HabitatQueryChunkMapBytesPerGroup, ProjectedOutputBytes)
			|| !TryAddCapacityValue(
				ProjectedScratchBytes, ProjectedOutputBytes, ProjectedScratchBytes)
			|| !TryAccumulateCapacityBytes(
				MaximumLiveBatchGroupCount,
				sizeof(FAnnualFactoredHabitatProvider::FChunkLease),
				ProjectedScratchBytes)
			|| !TryAccumulateCapacityBytes(
				MaximumLiveShardCount,
				sizeof(FFactoredHabitatQueryShard),
				ProjectedScratchBytes)
			|| !TryAccumulateCapacityBytes(
				MaximumLiveShardCount,
				sizeof(FFactoredHabitatQueryShardResult),
				ProjectedScratchBytes)
			|| ProjectedScratchBytes > GetFactoredHabitatQueryScratchBudgetBytes())
		{
			OutError = TEXT("ArcForest grouped habitat query window exceeded its configured full scratch budget.");
			return false;
		}
		OutQuantums.Reserve(Queries.Num());
		OutQuantums.SetNumUninitialized(Queries.Num());

		const double RoutingStartSeconds = FPlatformTime::Seconds();
		bool bRoutingTelemetryFinalized = false;
		const auto FinalizeRoutingTelemetry = [&]()
		{
			if (bRoutingTelemetryFinalized)
			{
				return;
			}
			bRoutingTelemetryFinalized = true;
			const double RoutingMilliseconds =
				UE::ArcForest::GetElapsedMilliseconds(RoutingStartSeconds);
			PendingTelemetry.RoutingWallMilliseconds += RoutingMilliseconds;
			if (bSawSeededRequests && bSawFallbackRequests)
			{
				PendingTelemetry.MixedRoutingWallMilliseconds += RoutingMilliseconds;
			}
			else if (bSawSeededRequests)
			{
				PendingTelemetry.SeededRoutingWallMilliseconds += RoutingMilliseconds;
			}
			else if (bSawFallbackRequests)
			{
				PendingTelemetry.FallbackRoutingWallMilliseconds += RoutingMilliseconds;
			}
		};
		ON_SCOPE_EXIT
		{
			FinalizeRoutingTelemetry();
		};
		const FIntPoint ChunkDimensions = Context.Layout->GetChunkGridDimensions();
		const FRegionalHabitatRouteSeedTable* RouteSeedTable = Context.RegionalRouteSeeds;
		TArray<int32> QueryGroupIndices;
		QueryGroupIndices.Reserve(Queries.Num());
		QueryGroupIndices.SetNumUninitialized(Queries.Num());
		TArray<FFactoredHabitatQueryGroup> Groups;
		Groups.Reserve(MaximumGroupCount);
		TMap<int32, int32> GroupIndexByChunk;
		GroupIndexByChunk.Reserve(MaximumGroupCount);
		int64 PreviousStableOrdinal = MIN_int64;
		for (int32 QueryIndex = 0; QueryIndex < Queries.Num(); ++QueryIndex)
		{
			const FFactoredHabitatQuery& Query = Queries[QueryIndex];
			if (!Context.Scoring->SpeciesResponses.IsValidIndex(Query.SpeciesIndex)
				|| Query.BaseCellIndex < 0
				|| Query.BaseCellIndex >= Context.Site->GetCellCount()
				|| Query.OutputIndex < 0
				|| Query.OutputIndex >= Queries.Num())
			{
				OutError = TEXT("ArcForest grouped habitat query contained an invalid species, cell or output index.");
				return false;
			}
			if (QueryIndex > 0 && Query.StableOrdinal < PreviousStableOrdinal)
			{
				OutError = TEXT("ArcForest grouped habitat queries were not ordered by stable ordinal.");
				return false;
			}
			PreviousStableOrdinal = Query.StableOrdinal;
			int32 ChunkIndex = INDEX_NONE;
			if (Query.RouteSeedIndex != INDEX_NONE)
			{
				bSawSeededRequests = true;
				++PendingTelemetry.SeededRequests;
				if (RouteSeedTable == nullptr
					|| !RouteSeedTable->Seeds.IsValidIndex(Query.RouteSeedIndex)
					|| !Context.ValidBaseCells->Entries.IsValidIndex(Query.RouteSeedIndex)
					|| Context.ValidBaseCells->Entries[Query.RouteSeedIndex].BaseCellIndex
						!= Query.BaseCellIndex)
				{
					++PendingTelemetry.RouteSeedValidationFailures;
					OutError = TEXT("ArcForest grouped habitat query requested an invalid regional route seed.");
					return false;
				}
				const FRegionalHabitatRouteSeed& Seed =
					RouteSeedTable->Seeds[Query.RouteSeedIndex];
				ChunkIndex = Seed.ChunkIndex;
			}
			else
			{
				bSawFallbackRequests = true;
				++PendingTelemetry.UnseededFallbackRequests;
				const FIntPoint BaseCell(
					Query.BaseCellIndex % Context.Site->GridDimensions.X,
					Query.BaseCellIndex / Context.Site->GridDimensions.X);
				FArcEcotopeChunkId ChunkId;
				if (!Context.Layout->FindChunkForBaseCell(BaseCell, ChunkId))
				{
					OutError = TEXT("ArcForest grouped habitat query could not resolve its owning factor chunk.");
					return false;
				}
				ChunkIndex = ChunkId.X + ChunkId.Y * ChunkDimensions.X;
			}
			int32 GroupIndex = INDEX_NONE;
			if (const int32* ExistingGroupIndex = GroupIndexByChunk.Find(ChunkIndex))
			{
				GroupIndex = *ExistingGroupIndex;
			}
			else
			{
				FArcEcotopeChunkRect Chunk;
				if (!Context.Layout->GetChunk(ChunkIndex, Chunk))
				{
					PendingTelemetry.RouteSeedValidationFailures +=
						Query.RouteSeedIndex != INDEX_NONE ? 1 : 0;
					OutError = TEXT("ArcForest grouped habitat query resolved an invalid chunk index.");
					return false;
				}
				const int64 DescriptorChunkIndex = static_cast<int64>(Chunk.Id.X)
					+ static_cast<int64>(Chunk.Id.Y) * ChunkDimensions.X;
				if (Query.RouteSeedIndex != INDEX_NONE
					&& (ChunkDimensions.X <= 0
						|| ChunkDimensions.Y <= 0
						|| Chunk.Id.X < 0
						|| Chunk.Id.X >= ChunkDimensions.X
						|| Chunk.Id.Y < 0
						|| Chunk.Id.Y >= ChunkDimensions.Y
						|| DescriptorChunkIndex != ChunkIndex))
				{
					++PendingTelemetry.RouteSeedValidationFailures;
					OutError = TEXT("ArcForest grouped habitat query regional route seed did not match its chunk descriptor.");
					return false;
				}
				GroupIndex = Groups.Num();
				FFactoredHabitatQueryGroup& Group = Groups.AddDefaulted_GetRef();
				Group.ChunkId = Chunk.Id;
				Group.CoreMin = Chunk.BaseCoreRect.Min;
				Group.CoreWidth = Chunk.BaseCoreRect.Width();
				Group.CoreHeight = Chunk.BaseCoreRect.Height();
				const int64 CoreCellCount64 = static_cast<int64>(Group.CoreWidth)
					* Group.CoreHeight;
				if (Group.CoreWidth <= 0
					|| Group.CoreHeight <= 0
					|| CoreCellCount64 <= 0
					|| CoreCellCount64 > MAX_int32)
				{
					PendingTelemetry.RouteSeedValidationFailures +=
						Query.RouteSeedIndex != INDEX_NONE ? 1 : 0;
					OutError = TEXT("ArcForest grouped habitat query resolved an empty factor chunk.");
					return false;
				}
				Group.CoreCellCount = static_cast<int32>(CoreCellCount64);
				GroupIndexByChunk.Add(ChunkIndex, GroupIndex);
			}
			FFactoredHabitatQueryGroup& Group = Groups[GroupIndex];
			if (Group.QueryCount == MAX_int32)
			{
				OutError = TEXT("ArcForest grouped habitat query group count overflowed.");
				return false;
			}
			++Group.QueryCount;
			QueryGroupIndices[QueryIndex] = GroupIndex;
		}

		int32 NextRouteBegin = 0;
		for (FFactoredHabitatQueryGroup& Group : Groups)
		{
			Group.RouteBegin = NextRouteBegin;
			Group.RouteEnd = NextRouteBegin + Group.QueryCount;
			Group.NextRouteWrite = Group.RouteBegin;
			NextRouteBegin = Group.RouteEnd;
		}
		if (NextRouteBegin != Queries.Num())
		{
			OutError = TEXT("ArcForest grouped habitat query bucket prefix was inconsistent.");
			return false;
		}
		TArray<FFactoredHabitatQueryRoute> Routes;
		Routes.Reserve(Queries.Num());
		Routes.SetNumUninitialized(Queries.Num());
		for (int32 QueryIndex = 0; QueryIndex < Queries.Num(); ++QueryIndex)
		{
			const FFactoredHabitatQuery& Query = Queries[QueryIndex];
			const int32 GroupIndex = QueryGroupIndices[QueryIndex];
			if (!Groups.IsValidIndex(GroupIndex))
			{
				OutError = TEXT("ArcForest grouped habitat query bucket index was invalid.");
				return false;
			}
			FFactoredHabitatQueryGroup& Group = Groups[GroupIndex];
			int32 CoreLocalIndex = INDEX_NONE;
			if (Query.RouteSeedIndex != INDEX_NONE)
			{
				checkSlow(RouteSeedTable != nullptr);
				checkSlow(RouteSeedTable->Seeds.IsValidIndex(Query.RouteSeedIndex));
				checkSlow(Context.ValidBaseCells->Entries.IsValidIndex(Query.RouteSeedIndex));
				CoreLocalIndex = RouteSeedTable->Seeds[Query.RouteSeedIndex].CoreLocalIndex;
				if (CoreLocalIndex < 0
					|| CoreLocalIndex >= Group.CoreCellCount)
				{
					++PendingTelemetry.RouteSeedValidationFailures;
					OutError = TEXT("ArcForest grouped habitat query could not use validated regional route seed inputs.");
					return false;
				}
				if (Group.NextRouteWrite < Group.RouteBegin
					|| Group.NextRouteWrite >= Group.RouteEnd)
				{
					OutError = TEXT("ArcForest grouped habitat query stable scatter write was out of range.");
					return false;
				}
				++PendingTelemetry.SeedHits;
			}
			else
			{
				const int32 BaseX = Query.BaseCellIndex % Context.Site->GridDimensions.X;
				const int32 BaseY = Query.BaseCellIndex / Context.Site->GridDimensions.X;
				const int64 CoreLocalIndex64 = static_cast<int64>(BaseX - Group.CoreMin.X)
					+ static_cast<int64>(BaseY - Group.CoreMin.Y) * Group.CoreWidth;
				if (BaseX < Group.CoreMin.X
					|| BaseX >= Group.CoreMin.X + Group.CoreWidth
					|| BaseY < Group.CoreMin.Y
					|| BaseY >= Group.CoreMin.Y + Group.CoreHeight
					|| CoreLocalIndex64 < 0
					|| CoreLocalIndex64 >= Group.CoreCellCount
					|| Group.NextRouteWrite < Group.RouteBegin
					|| Group.NextRouteWrite >= Group.RouteEnd)
				{
					OutError = TEXT("ArcForest grouped habitat query could not precompute validated core-local inputs.");
					return false;
				}
				CoreLocalIndex = static_cast<int32>(CoreLocalIndex64);
			}
			FFactoredHabitatQueryRoute& Route = Routes[Group.NextRouteWrite++];
			Route.QueryIndex = QueryIndex;
			Route.CoreLocalIndex = CoreLocalIndex;
		}
		for (const FFactoredHabitatQueryGroup& Group : Groups)
		{
			if (Group.NextRouteWrite != Group.RouteEnd)
			{
				OutError = TEXT("ArcForest grouped habitat query stable scatter was incomplete.");
				return false;
			}
		}
		PendingTelemetry.PeakQueriesPerWindow = Queries.Num();
		FinalizeRoutingTelemetry();
		if (!CommitPendingTelemetry())
		{
			return false;
		}
		int64 ResidentArrayScratchBytes = CallerAllocatedScratchBytes;
		if (!TryAddCapacityValue(
				ResidentArrayScratchBytes,
				static_cast<int64>(QueryGroupIndices.GetAllocatedSize()),
				ResidentArrayScratchBytes)
			|| !TryAddCapacityValue(
				ResidentArrayScratchBytes,
				static_cast<int64>(Routes.GetAllocatedSize()),
				ResidentArrayScratchBytes)
			|| !TryAddCapacityValue(
				ResidentArrayScratchBytes,
				static_cast<int64>(Groups.GetAllocatedSize()),
				ResidentArrayScratchBytes)
			|| !TryAddCapacityValue(
				ResidentArrayScratchBytes,
				static_cast<int64>(GroupIndexByChunk.GetAllocatedSize()),
				ResidentArrayScratchBytes)
			|| !TryAddCapacityValue(
				ResidentArrayScratchBytes,
				static_cast<int64>(OutQuantums.GetAllocatedSize()),
				ResidentArrayScratchBytes))
		{
			OutError = TEXT("ArcForest grouped habitat query resident scratch accounting overflowed.");
			return false;
		}
		if (ResidentArrayScratchBytes > GetFactoredHabitatQueryScratchBudgetBytes())
		{
			OutError = TEXT("ArcForest grouped habitat query routing scratch exceeded its configured budget.");
			return false;
		}
		PendingTelemetry.PeakScratchBytes = ResidentArrayScratchBytes;
		if (!CommitPendingTelemetry())
		{
			return false;
		}

		const int32 MaximumGroupsPerBatch = FMath::Min(
			MaximumBatchChunkCount, AvailableShardSlots);
		for (int32 GroupBegin = 0; GroupBegin < Groups.Num(); GroupBegin += MaximumGroupsPerBatch)
		{
			if (Context.Cancellation->IsCancellationRequested())
			{
				OutError = TEXT("ArcForest grouped habitat queries were cancelled.");
				return false;
			}
			const int32 GroupEnd = FMath::Min(GroupBegin + MaximumGroupsPerBatch, Groups.Num());
			const int32 BatchGroupCount = GroupEnd - GroupBegin;
			TArray<FAnnualFactoredHabitatProvider::FChunkLease> Leases;
			Leases.Reserve(BatchGroupCount);
			const double AcquireStartSeconds = FPlatformTime::Seconds();
			bool bAcquiredBatch = true;
			for (int32 GroupIndex = GroupBegin; GroupIndex < GroupEnd; ++GroupIndex)
			{
				FAnnualFactoredHabitatProvider::FChunkLease Lease;
				if (!Provider.AcquireChunk(Groups[GroupIndex].ChunkId, Lease, OutError))
				{
					bAcquiredBatch = false;
					break;
				}
				Leases.Add(MoveTemp(Lease));
			}
			PendingTelemetry.ChunkAcquires = Leases.Num();
			PendingTelemetry.PeakChunksPerBatch = Leases.Num();
			PendingTelemetry.AcquireWallMilliseconds =
				UE::ArcForest::GetElapsedMilliseconds(AcquireStartSeconds);
			if (!CommitPendingTelemetry())
			{
				return false;
			}
			if (!bAcquiredBatch)
			{
				return false;
			}

			const int32 TotalBatchRouteCount =
				Groups[GroupEnd - 1].RouteEnd - Groups[GroupBegin].RouteBegin;
			if (TotalBatchRouteCount <= 0)
			{
				OutError = TEXT("ArcForest grouped habitat query batch could not plan flat shards.");
				return false;
			}
			const bool bParallelBatch = AvailableShardSlots > 1
				&& TotalBatchRouteCount >= MinimumQueriesForParallelHabitatBatch;
			int32 PlannedShardCount = BatchGroupCount;
			for (int32 GroupIndex = GroupBegin; GroupIndex < GroupEnd; ++GroupIndex)
			{
				FFactoredHabitatQueryGroup& Group = Groups[GroupIndex];
				const int32 GroupRouteCount = Group.RouteEnd - Group.RouteBegin;
				Group.MaximumUsefulShardCount = bParallelBatch
					? FMath::Clamp(
						GroupRouteCount / MinimumQueriesPerParallelHabitatShard,
						1,
						AvailableShardSlots)
					: 1;
				Group.PlannedShardCount = 1;
			}
			int32 RemainingShardSlots = AvailableShardSlots - PlannedShardCount;
			while (RemainingShardSlots > 0)
			{
				int32 BestGroupIndex = INDEX_NONE;
				for (int32 GroupIndex = GroupBegin; GroupIndex < GroupEnd; ++GroupIndex)
				{
					const FFactoredHabitatQueryGroup& Group = Groups[GroupIndex];
					if (Group.PlannedShardCount >= Group.MaximumUsefulShardCount)
					{
						continue;
					}
					if (BestGroupIndex == INDEX_NONE)
					{
						BestGroupIndex = GroupIndex;
						continue;
					}
					const FFactoredHabitatQueryGroup& BestGroup = Groups[BestGroupIndex];
					const int32 GroupRouteCount = Group.RouteEnd - Group.RouteBegin;
					const int32 BestGroupRouteCount = BestGroup.RouteEnd - BestGroup.RouteBegin;
					if (static_cast<int64>(GroupRouteCount) * BestGroup.PlannedShardCount
						> static_cast<int64>(BestGroupRouteCount) * Group.PlannedShardCount)
					{
						BestGroupIndex = GroupIndex;
					}
				}
				if (BestGroupIndex == INDEX_NONE)
				{
					break;
				}
				++Groups[BestGroupIndex].PlannedShardCount;
				++PlannedShardCount;
				--RemainingShardSlots;
			}
			TArray<FFactoredHabitatQueryShard> Shards;
			Shards.Reserve(PlannedShardCount);
			int32 BatchMinimumRoutesPerShard = MAX_int32;
			int32 BatchPeakRoutesPerShard = 0;
			for (int32 GroupIndex = GroupBegin; GroupIndex < GroupEnd; ++GroupIndex)
			{
				const FFactoredHabitatQueryGroup& Group = Groups[GroupIndex];
				const int32 GroupRouteCount = Group.RouteEnd - Group.RouteBegin;
				for (int32 GroupShardIndex = 0;
					GroupShardIndex < Group.PlannedShardCount;
					++GroupShardIndex)
				{
					FFactoredHabitatQueryShard& Shard = Shards.AddDefaulted_GetRef();
					Shard.BatchGroupIndex = GroupIndex - GroupBegin;
					Shard.RouteBegin = Group.RouteBegin + static_cast<int32>(
						(static_cast<int64>(GroupShardIndex) * GroupRouteCount)
							/ Group.PlannedShardCount);
					Shard.RouteEnd = Group.RouteBegin + static_cast<int32>(
						(static_cast<int64>(GroupShardIndex + 1) * GroupRouteCount)
							/ Group.PlannedShardCount);
					if (Shard.RouteBegin >= Shard.RouteEnd)
					{
						OutError = TEXT("ArcForest grouped habitat query planned an empty flat shard.");
						return false;
					}
					const int32 ShardRouteCount = Shard.RouteEnd - Shard.RouteBegin;
					BatchMinimumRoutesPerShard = FMath::Min(
						BatchMinimumRoutesPerShard, ShardRouteCount);
					BatchPeakRoutesPerShard = FMath::Max(
						BatchPeakRoutesPerShard, ShardRouteCount);
				}
			}
			if (Shards.Num() != PlannedShardCount)
			{
				OutError = TEXT("ArcForest grouped habitat query flat shard plan was inconsistent.");
				return false;
			}
			PendingTelemetry.TotalPlannedShards = PlannedShardCount;
			PendingTelemetry.PeakShardsPerBatch = PlannedShardCount;
			PendingTelemetry.MinimumRoutesPerShard = BatchMinimumRoutesPerShard;
			PendingTelemetry.PeakRoutesPerShard = BatchPeakRoutesPerShard;
			if (!CommitPendingTelemetry())
			{
				return false;
			}

			TArray<FFactoredHabitatQueryShardResult> Results;
			Results.Reserve(PlannedShardCount);
			Results.SetNum(PlannedShardCount);
			int64 BatchScratchBytes = 0;
			if (!TryAddCapacityValue(
					static_cast<int64>(Leases.GetAllocatedSize()),
					static_cast<int64>(Shards.GetAllocatedSize()),
					BatchScratchBytes)
				|| !TryAddCapacityValue(
					BatchScratchBytes,
					static_cast<int64>(Results.GetAllocatedSize()),
					BatchScratchBytes)
				|| ResidentArrayScratchBytes
					> GetFactoredHabitatQueryScratchBudgetBytes() - BatchScratchBytes)
			{
				OutError = TEXT("ArcForest grouped habitat query allocated scratch exceeded its configured budget.");
				return false;
			}
			int64 CurrentScratchBytes = ResidentArrayScratchBytes;
			if (!TryAddCapacityValue(CurrentScratchBytes, BatchScratchBytes, CurrentScratchBytes))
			{
				OutError = TEXT("ArcForest grouped habitat query peak scratch telemetry overflowed.");
				return false;
			}
			PendingTelemetry.PeakScratchBytes = CurrentScratchBytes;
			if (!CommitPendingTelemetry())
			{
				return false;
			}
			const auto EvaluateShard = [&](const int32 ShardIndex)
			{
				const FFactoredHabitatQueryShard& Shard = Shards[ShardIndex];
				FFactoredHabitatQueryShardResult& Result = Results[ShardIndex];
				const FAnnualFactoredHabitatProvider::FChunkLease& Lease =
					Leases[Shard.BatchGroupIndex];
				for (int32 RouteIndex = Shard.RouteBegin; RouteIndex < Shard.RouteEnd; ++RouteIndex)
				{
					if (((RouteIndex - Shard.RouteBegin) & 4095) == 0
						&& Context.Cancellation->IsCancellationRequested())
					{
						Result.bCancelled = true;
						return;
					}
					const FFactoredHabitatQueryRoute& Route = Routes[RouteIndex];
					const FFactoredHabitatQuery& Query = Queries[Route.QueryIndex];
					OutQuantums[Query.OutputIndex] = Provider.EvaluateQuantizedSiteOnlyCoreLocal(
						Lease,
						Query.SpeciesIndex,
						Route.CoreLocalIndex);
				}
			};
			const double EvaluationStartSeconds = FPlatformTime::Seconds();
			const bool bLaunchParallel = bParallelBatch && PlannedShardCount > 1;
			if (bLaunchParallel)
			{
				ParallelFor(PlannedShardCount, EvaluateShard);
			}
			else
			{
				for (int32 ShardIndex = 0; ShardIndex < PlannedShardCount; ++ShardIndex)
				{
					EvaluateShard(ShardIndex);
				}
			}
			PendingTelemetry.Batches = 1;
			PendingTelemetry.ParallelLaunches = bLaunchParallel ? 1 : 0;
			PendingTelemetry.EvaluationWallMilliseconds =
				UE::ArcForest::GetElapsedMilliseconds(EvaluationStartSeconds);
			if (!CommitPendingTelemetry())
			{
				return false;
			}
			for (const FFactoredHabitatQueryShardResult& Result : Results)
			{
				if (Result.bCancelled)
				{
					OutError = TEXT("ArcForest grouped habitat queries were cancelled.");
					return false;
				}
			}
		}
		return true;
		};
		const bool bSucceeded = Evaluate();
		if (!bTelemetryCommitFailed)
		{
			if (bSucceeded && !Queries.IsEmpty())
			{
				if (bSawSeededRequests && bSawFallbackRequests)
				{
					PendingTelemetry.CompletedMixedWindows = 1;
				}
				else if (bSawSeededRequests)
				{
					PendingTelemetry.CompletedSeededWindows = 1;
				}
				else if (bSawFallbackRequests)
				{
					PendingTelemetry.CompletedFallbackWindows = 1;
				}
			}
			PendingTelemetry.WallMilliseconds +=
				UE::ArcForest::GetElapsedMilliseconds(StartSeconds);
			if (!CommitPendingTelemetry())
			{
				return false;
			}
		}
		return bSucceeded;
	}

	struct FFactoredHabitatChunkSpan
	{
		const FAnnualFactoredHabitatProvider::FChunkLease* Lease = nullptr;
		FIntRect RecordCoreRect;
		int64 ProductBegin = 0;
		int64 ProductEnd = 0;
	};

	struct FFusedCoverBatchContext
	{
		bool Initialize(
			const FArcForestRunInput& InInput,
			const FValidBaseCellTable& InValidBaseCells,
			const FRegionalRecruitmentRecordIndex& InRecordIndex,
			const TArray<FArcForestFrozenSpeciesSetup>& InSpecies,
			const TMap<FGuid, int32>& InSpeciesIndices,
			const FStratifiedOccupancy& InCellOccupancy,
			const FArcEcotopeDecayGrid& InDecayGrid,
			const FArcForestWorldSnapshot& InSnapshot,
			TArray<double>& InOutCoverFloorCapacity,
			TArray<double>& InOutCoverSuitability,
			FString& OutError);

		bool ConsumeBatch(
			const FAnnualFactoredHabitatProvider& Provider,
			TConstArrayView<FFactoredHabitatChunkSpan> ChunkSpans,
			const FArcForestCancellationToken& Cancellation,
			FString& OutError);

		bool Finalize(
			FCoverHabitatProviderRunTelemetry& InOutTelemetry,
			FString& OutError);

		double GetConsumeWallMilliseconds() const { return ConsumeWallMilliseconds; }

		const FArcForestRunInput* Input = nullptr;
		const FValidBaseCellTable* ValidBaseCells = nullptr;
		const FRegionalRecruitmentRecordIndex* RecordIndex = nullptr;
		TConstArrayView<uint8> BaseValidityMask;
		const TArray<FArcForestFrozenSpeciesSetup>* Species = nullptr;
		const FStratifiedOccupancy* CellOccupancy = nullptr;
		const FArcEcotopeDecayGrid* DecayGrid = nullptr;
		const FArcForestWorldSnapshot* Snapshot = nullptr;
		TArray<double>* OutCoverFloorCapacity = nullptr;
		TArray<double>* OutCoverSuitability = nullptr;
		TArray<int32> CoverSpeciesIndices;
		TMap<int64, double> PatchHeightByCell;
		FArcEcotopeSiteMetadata Site;
		FIntPoint RecordDimensions = FIntPoint::ZeroValue;
		int32 RecordCellCount = 0;
		int64 AnnualRecordCells = 0;
		int64 AnnualSpeciesCells = 0;
		int64 AnnualBatches = 0;
		int64 AnnualReusedChunkLeases = 0;
		int32 AnnualPeakChunksPerBatch = 0;
		double ConsumeWallMilliseconds = 0.0;
		double TotalWallMilliseconds = 0.0;
	};

	bool EvaluateFactoredHabitat(
		const FArcForestRunInput& Input,
		const FArcEcotopeSiteMetadata& Site,
		const FHabitatScoringState& HabitatTables,
		const FArcEcotopeChunkLayout& FactorLayout,
		const uint64 GenerationToken,
		const FArcForestHabitatFactorStore& FactorStore,
		const FArcForestCancellationToken& Cancellation,
		FFusedCoverBatchContext& CoverContext,
		FFactoredHabitatRunTelemetry& InOutTelemetry,
		FString& OutError)
	{
		const int32 CellCount = Site.GetCellCount();
		const int32 SpeciesCount = HabitatTables.Scoring.SpeciesResponses.Num();
		if (CellCount <= 0
			|| HabitatTables.BaseCellCount != CellCount
			|| SpeciesCount <= 0
			|| !FactorStore.IsBuilt()
			|| FactorLayout.GetBaseDimensions() != Site.GridDimensions
			|| FactorLayout.GetRecordDimensions()
				!= Site.GetLevelDimensions(Site.RecordGridLevel))
		{
			OutError = TEXT("ArcForest factored habitat evaluator received inconsistent annual inputs.");
			return false;
		}
		const int32 MaximumBatchChunkCount = FactorStore.GetMaximumBatchChunkCount();
		if (MaximumBatchChunkCount < 1)
		{
			OutError = TEXT("ArcForest factored habitat evaluator received a factor store with no safe batch capacity.");
			return false;
		}
		const FArcForestHabitatFactorStoreStats FactorStoreStatsBefore = FactorStore.GetStats();
		const FAnnualFactoredHabitatProvider Provider(
			Site,
			HabitatTables.Scoring,
			FactorLayout,
			FactorStore,
			Cancellation);
		const int32 ChunkCount = FactorLayout.GetChunkCount();
		const bool bForwardTraversal = (GenerationToken & 1ull) != 0;
		const double StartSeconds = FPlatformTime::Seconds();
		int64 AnnualEvaluationBatchCount = 0;
		int32 AnnualPeakChunksPerBatch = 0;
		bool bTelemetryOverflow = false;
		for (int32 NextTraversalOrdinal = 0; NextTraversalOrdinal < ChunkCount;)
		{
			if (Cancellation.IsCancellationRequested())
			{
				OutError = TEXT("ArcForest authoritative factored habitat evaluator was cancelled.");
				return false;
			}
			const int32 BatchChunkCount = FMath::Min(
				MaximumBatchChunkCount, ChunkCount - NextTraversalOrdinal);
			if (BatchChunkCount < 1)
			{
				OutError = TEXT("ArcForest authoritative factored habitat evaluator derived a zero-progress batch.");
				return false;
			}
			TArray<FAnnualFactoredHabitatProvider::FChunkLease> FactorLeases;
			FactorLeases.Reserve(BatchChunkCount);
			TArray<FFactoredHabitatChunkSpan> ChunkSpans;
			ChunkSpans.Reserve(BatchChunkCount);
			int64 BatchProductCount = 0;
			for (int32 BatchChunkIndex = 0; BatchChunkIndex < BatchChunkCount; ++BatchChunkIndex)
			{
				const int32 TraversalOrdinal = NextTraversalOrdinal + BatchChunkIndex;
				const int32 LayoutChunkIndex = bForwardTraversal
					? TraversalOrdinal
					: ChunkCount - 1 - TraversalOrdinal;
				FArcEcotopeChunkRect ExpectedChunk;
				if (!FactorLayout.GetChunk(LayoutChunkIndex, ExpectedChunk))
				{
					OutError = TEXT("ArcForest authoritative factored habitat evaluator encountered an invalid factor layout entry.");
					return false;
				}
				FAnnualFactoredHabitatProvider::FChunkLease FactorLease;
				if (!Provider.AcquireChunk(ExpectedChunk.Id, FactorLease, OutError))
				{
					OutError = FString::Printf(
						TEXT("ArcForest authoritative factored habitat evaluator could not acquire chunk (%d,%d): %s"),
						ExpectedChunk.Id.X,
						ExpectedChunk.Id.Y,
						*OutError);
					return false;
				}
				FactorLeases.Add(MoveTemp(FactorLease));
				const FArcForestHabitatFactorChunk& FactorChunk = Provider.GetChunk(FactorLeases.Last());
				const int32 ChunkCellCount = FactorLeases.Last().GetCoreCellCount();
				const int64 ExpectedProductCount = static_cast<int64>(ChunkCellCount) * SpeciesCount;
				if (ChunkCellCount <= 0
					|| FactorChunk.Id != ExpectedChunk.Id
					|| FactorChunk.CoreRect != ExpectedChunk.BaseCoreRect
					|| FactorChunk.SpeciesCount != SpeciesCount
					|| ExpectedProductCount != FactorChunk.StaticProducts.Num()
					|| ExpectedProductCount != FactorChunk.StaticHardZeros.Num()
					|| !TryAddCapacityValue(BatchProductCount, ExpectedProductCount, BatchProductCount))
				{
					OutError = TEXT("ArcForest authoritative factored habitat evaluator acquired an inconsistent factor chunk.");
					return false;
				}
				FFactoredHabitatChunkSpan& Span = ChunkSpans.AddDefaulted_GetRef();
				Span.Lease = &FactorLeases.Last();
				Span.RecordCoreRect = ExpectedChunk.RecordCoreRect;
				Span.ProductEnd = BatchProductCount;
				Span.ProductBegin = Span.ProductEnd - ExpectedProductCount;
			}
			if (BatchProductCount <= 0)
			{
				OutError = TEXT("ArcForest authoritative factored habitat evaluator acquired an empty factor batch.");
				return false;
			}
			bTelemetryOverflow |= !TryAddCapacityValue(
				AnnualEvaluationBatchCount, 1, AnnualEvaluationBatchCount);
			AnnualPeakChunksPerBatch = FMath::Max(AnnualPeakChunksPerBatch, BatchChunkCount);
			if (!CoverContext.ConsumeBatch(Provider, ChunkSpans, Cancellation, OutError))
			{
				return false;
			}
			FactorLeases.Reset();
			NextTraversalOrdinal += BatchChunkCount;
		}
		if (Cancellation.IsCancellationRequested())
		{
			OutError = TEXT("ArcForest authoritative factored habitat evaluator was cancelled.");
			return false;
		}
		const double AnnualEvaluationWallMilliseconds =
			(FPlatformTime::Seconds() - StartSeconds) * 1000.0
			- CoverContext.GetConsumeWallMilliseconds();
		if (bTelemetryOverflow || !FMath::IsFinite(AnnualEvaluationWallMilliseconds))
		{
			OutError = TEXT("ArcForest factored habitat evaluator local telemetry overflowed.");
			return false;
		}
		const FArcForestHabitatFactorStoreStats FactorStoreStatsAfter = FactorStore.GetStats();
		if (FactorStoreStatsAfter.CacheHitCount < FactorStoreStatsBefore.CacheHitCount
			|| FactorStoreStatsAfter.PayloadLoadCount < FactorStoreStatsBefore.PayloadLoadCount)
		{
			OutError = TEXT("ArcForest authoritative factored habitat evaluator observed regressing factor-store telemetry.");
			return false;
		}
		const uint64 CacheHitDelta =
			FactorStoreStatsAfter.CacheHitCount - FactorStoreStatsBefore.CacheHitCount;
		const uint64 PayloadLoadDelta =
			FactorStoreStatsAfter.PayloadLoadCount - FactorStoreStatsBefore.PayloadLoadCount;
		if (CacheHitDelta > static_cast<uint64>(MAX_int64)
			|| PayloadLoadDelta > static_cast<uint64>(MAX_int64))
		{
			OutError = TEXT("ArcForest authoritative factored habitat evaluator factor-store telemetry delta overflowed.");
			return false;
		}
		FFactoredHabitatRunTelemetry UpdatedTelemetry = InOutTelemetry;
		bool bRunTelemetryOverflow = false;
		bRunTelemetryOverflow |= !TryAddCapacityValue(
			UpdatedTelemetry.EvaluationBatchCount,
			AnnualEvaluationBatchCount,
			UpdatedTelemetry.EvaluationBatchCount);
		bRunTelemetryOverflow |= !TryAddCapacityValue(
			UpdatedTelemetry.ProductionOnlyYears, 1, UpdatedTelemetry.ProductionOnlyYears);
		if (bForwardTraversal)
		{
			bRunTelemetryOverflow |= !TryAddCapacityValue(
				UpdatedTelemetry.ForwardTraversalYears, 1, UpdatedTelemetry.ForwardTraversalYears);
			bRunTelemetryOverflow |= !TryAddCapacityValue(
				UpdatedTelemetry.ForwardCacheHitCount,
				static_cast<int64>(CacheHitDelta),
				UpdatedTelemetry.ForwardCacheHitCount);
			bRunTelemetryOverflow |= !TryAddCapacityValue(
				UpdatedTelemetry.ForwardPayloadLoadCount,
				static_cast<int64>(PayloadLoadDelta),
				UpdatedTelemetry.ForwardPayloadLoadCount);
		}
		else
		{
			bRunTelemetryOverflow |= !TryAddCapacityValue(
				UpdatedTelemetry.ReverseTraversalYears, 1, UpdatedTelemetry.ReverseTraversalYears);
			bRunTelemetryOverflow |= !TryAddCapacityValue(
				UpdatedTelemetry.ReverseCacheHitCount,
				static_cast<int64>(CacheHitDelta),
				UpdatedTelemetry.ReverseCacheHitCount);
			bRunTelemetryOverflow |= !TryAddCapacityValue(
				UpdatedTelemetry.ReversePayloadLoadCount,
				static_cast<int64>(PayloadLoadDelta),
				UpdatedTelemetry.ReversePayloadLoadCount);
		}
		const double UpdatedEvaluationWallMilliseconds =
			UpdatedTelemetry.EvaluationWallMilliseconds + AnnualEvaluationWallMilliseconds;
		if (bRunTelemetryOverflow || !FMath::IsFinite(UpdatedEvaluationWallMilliseconds))
		{
			OutError = TEXT("ArcForest factored habitat evaluator run telemetry overflowed.");
			return false;
		}
		UpdatedTelemetry.EvaluationWallMilliseconds = UpdatedEvaluationWallMilliseconds;
		UpdatedTelemetry.PeakChunksPerBatch = FMath::Max(
			UpdatedTelemetry.PeakChunksPerBatch, AnnualPeakChunksPerBatch);
		InOutTelemetry = MoveTemp(UpdatedTelemetry);
		return true;
	}

	int64 QuantizeMillimeters(const double Centimeters)
	{
		return FMath::RoundToInt64(Centimeters * 10.0);
	}

	int32 CalculateHeightBin(const double HeightCm)
	{
		const int64 HeightMillimeters = QuantizeMillimeters(HeightCm);
		const int64 MinimumMillimeters = QuantizeMillimeters(MinimumPlantHeightCm);
		const int64 BinMillimeters = QuantizeMillimeters(CohortHeightBinSizeCm);
		return static_cast<int32>(FMath::Max<int64>(
			0,
			(HeightMillimeters - MinimumMillimeters) / BinMillimeters));
	}

	bool HasReachedIndividualization(
		const FArcForestCohortRecord& Cohort,
		const FArcForestFrozenSpeciesSetup& Species)
	{
		return QuantizeMillimeters(Cohort.MeanHeightCm)
			>= QuantizeMillimeters(Species.IndividualizationHeightCm);
	}

	bool IsMature(const double AgeYears, const int32 MaturityAgeYears)
	{
		return FMath::RoundToInt64(AgeYears * 1000.0)
			>= static_cast<int64>(MaturityAgeYears) * 1000;
	}

	bool TryCalculateBiomass(
		const FArcForestFrozenSpeciesSetup& Species,
		const double DbhCm,
		double& OutBiomassKg)
	{
		OutBiomassKg = Species.BiomassAllometryCoefficient
			* FMath::Pow(FMath::Max(0.0, DbhCm), Species.BiomassAllometryExponent);
		return FMath::IsFinite(OutBiomassKg) && OutBiomassKg >= 0.0 && OutBiomassKg <= MAX_flt;
	}

	bool TryDeriveDbhFromAuthoritativeBiomass(
		const FArcForestFrozenSpeciesSetup& Species,
		const double BiomassKg,
		float& OutDbhCm)
	{
		if (!FMath::IsFinite(BiomassKg)
			|| BiomassKg <= 0.0
			|| Species.BiomassAllometryCoefficient <= 0.0f
			|| Species.BiomassAllometryExponent <= 0.0f)
		{
			return false;
		}
		const double ExactDbhCm = FMath::Pow(
			BiomassKg / static_cast<double>(Species.BiomassAllometryCoefficient),
			1.0 / static_cast<double>(Species.BiomassAllometryExponent));
		if (!FMath::IsFinite(ExactDbhCm) || ExactDbhCm < 0.0)
		{
			return false;
		}
		const float SerializedDbhCm =
			static_cast<float>(FMath::Clamp(ExactDbhCm, 0.0, static_cast<double>(Species.MaxDbhCm)));
		if (!FMath::IsFinite(SerializedDbhCm)
			|| SerializedDbhCm < 0.0f
			|| SerializedDbhCm > Species.MaxDbhCm)
		{
			return false;
		}
		OutDbhCm = SerializedDbhCm;
		return true;
	}

	bool TryCalculateDbh(
		const FArcForestFrozenSpeciesSetup& Species,
		const double HeightCm,
		double& OutDbhCm)
	{
		OutDbhCm = FMath::Pow(
			FMath::Max(0.0, HeightCm) / Species.HeightDbhAllometryCoefficient,
			1.0 / Species.HeightDbhAllometryExponent);
		OutDbhCm = FMath::Min(OutDbhCm, static_cast<double>(Species.MaxDbhCm));
		return FMath::IsFinite(OutDbhCm) && OutDbhCm >= 0.0;
	}

	double CalculateSizeLimitation(const double HeightCm, const FArcForestFrozenSpeciesSetup& Species)
	{
		const double SizeRatio = FMath::Clamp(
			HeightCm / FMath::Max(static_cast<double>(Species.MaxHeightCm), 1.0),
			0.0,
			1.0);
		return 1.0 / (1.0 + FMath::Exp(10.0 * (SizeRatio - 0.8)));
	}

	double CalculateMortalityProbability(
		const FArcForestFrozenSpeciesSetup& Species,
		const double AgeYears,
		const double Vigor,
		const double HeightCm,
		double* OutBaselineMortality = nullptr,
		double* OutAgeMortality = nullptr,
		double* OutResourceMortality = nullptr)
	{
		const double AgeRatio = FMath::Clamp(
			AgeYears / FMath::Max(static_cast<double>(Species.MaxAgeYears), 1.0),
			0.0,
			2.0);
		const double AgeMortality = Species.OldAgeMortalityProbabilityAtMaxAge * FMath::Pow(AgeRatio, 4.0);
		const double SizeFraction = FMath::Clamp(
			HeightCm / FMath::Max(static_cast<double>(Species.MaxHeightCm), 1.0),
			0.0,
			1.0);
		const double StressAsymptote = Species.StressMortalityMaximum
			* FMath::Lerp(1.0, Species.StressMortalityCanopyFloor, SizeFraction);
		const double ClampedVigor = FMath::Clamp(Vigor, 0.0, 1.0);
		const double ResourceMortality = StressAsymptote
			/ (1.0 + FMath::Exp(Species.StressMortalitySteepness * (ClampedVigor - Species.StressMortalityVigorThreshold)));
		if (OutBaselineMortality != nullptr)
		{
			*OutBaselineMortality = static_cast<double>(Species.BaseAnnualMortalityProbability);
		}
		if (OutAgeMortality != nullptr)
		{
			*OutAgeMortality = AgeMortality;
		}
		if (OutResourceMortality != nullptr)
		{
			*OutResourceMortality = ResourceMortality;
		}
		return FMath::Clamp(
			static_cast<double>(Species.BaseAnnualMortalityProbability) + AgeMortality + ResourceMortality,
			0.0,
			0.95);
	}

	double ComposeSubstrateMobilityMortality(
		const double BaseProbability,
		const float SubstrateMobility,
		const float MobilityMortalitySensitivity)
	{
		// Additive on the survival share, so a zero term returns BaseProbability bit-for-bit and no
		// mortality RNG key in the run moves.
		const double Undermining = FMath::Clamp(
			static_cast<double>(SubstrateMobility) * static_cast<double>(MobilityMortalitySensitivity),
			0.0,
			1.0);
		return BaseProbability + (1.0 - BaseProbability) * Undermining;
	}

	bool IsCancellationRequested(
		const FArcForestCancellationToken& Cancellation,
		FArcForestProgressState& Progress,
		FArcForestRunResult& OutResult)
	{
		if (!Cancellation.IsCancellationRequested())
		{
			return false;
		}
		OutResult = FArcForestRunResult();
		OutResult.Status = EArcForestRunStatus::Cancelled;
		OutResult.Error = TEXT("ArcForest annual simulation was cancelled.");
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Cancelled));
		return true;
	}

	bool ValidateSpecies(const FArcForestFrozenSpeciesSetup& Species, const int32 SpeciesIndex, FString& OutError)
	{
		const float PositiveFields[] =
		{
			Species.IndividualizationHeightCm,
			Species.MaxHeightCm,
			Species.MaxDbhCm,
			Species.MaxAnnualHeightGrowthCm,
			Species.MaxAnnualDbhGrowthCm,
			Species.HeightDbhAllometryCoefficient,
			Species.HeightDbhAllometryExponent,
			Species.CrownRadiusHeightRatio,
			Species.BiomassAllometryCoefficient,
			Species.BiomassAllometryExponent,
		};
		for (const float Value : PositiveFields)
		{
			if (!FMath::IsFinite(Value) || Value <= 0.0f)
			{
				OutError = FString::Printf(TEXT("Frozen species setup %d has an invalid positive ecology field."), SpeciesIndex);
				return false;
			}
		}

		const bool bIndividualizes = Species.bIndividualizes;
		const bool bInvalidHeights = bIndividualizes
			? (Species.IndividualizationHeightCm <= MinimumPlantHeightCm
				|| Species.MaxHeightCm < Species.IndividualizationHeightCm)
			: UE::ArcForest::CalculateGrowthSpanCm(
				Species.MaxHeightCm,
				UE::ArcForest::RecruitHeightCm) <= 0.0;

		const float UnitIntervalFields[] =
		{
			Species.Toughness,
			Species.ShadeTolerance,
			Species.PreferredTemperature,
			Species.PreferredHumidity,
			Species.ExclusionSensitivity,
			Species.MobilityMortalitySensitivity,
			Species.SunLoadMin,
			Species.WindExposureMax,
			Species.FertilityMin,
		};
		for (const float Value : UnitIntervalFields)
		{
			if (!FMath::IsFinite(Value) || Value < 0.0f || Value > 1.0f)
			{
				OutError = FString::Printf(TEXT("Frozen species setup %d has an out-of-range ecology field."), SpeciesIndex);
				return false;
			}
		}

		if (!Species.SpeciesId.IsValid()
			|| Species.EcologyHash == 0
			|| Species.EcologyHash != UE::ArcForest::ComputeFrozenSpeciesEcologyHash(Species)
			|| !FMath::IsFinite(Species.InitialDensityPerHectare)
			|| Species.InitialDensityPerHectare < 0.0f
			|| !FMath::IsFinite(Species.RegionalArrivalPerHectarePerYear)
			|| Species.RegionalArrivalPerHectarePerYear < 0.0f
			|| Species.TargetStemsPerHectare <= 0
			|| !FMath::IsFinite(Species.TreeSizeM)
			|| Species.TreeSizeM <= 0.0f
			|| !FMath::IsFinite(Species.Slenderness)
			|| !FMath::IsFinite(Species.GrowthSpeed)
			|| !FMath::IsFinite(Species.TemperatureTolerance)
			|| Species.TemperatureTolerance <= 0.0f
			|| !FMath::IsFinite(Species.LateralSpreadRatePerYear)
			|| Species.LateralSpreadRatePerYear < 0.0f
			|| !FMath::IsFinite(Species.CrownOccupancyWeight)
			|| Species.CrownOccupancyWeight < 0.0f
			|| bInvalidHeights
			|| Species.MaxAgeYears <= 0
			|| !FMath::IsFinite(Species.BaseAnnualMortalityProbability)
			|| Species.BaseAnnualMortalityProbability < 0.0f
			|| Species.BaseAnnualMortalityProbability > 1.0f
			|| !FMath::IsFinite(Species.OldAgeMortalityProbabilityAtMaxAge)
			|| Species.OldAgeMortalityProbabilityAtMaxAge < 0.0
			|| Species.OldAgeMortalityProbabilityAtMaxAge > 1.0
			|| !FMath::IsFinite(Species.StressMortalityMaximum)
			|| Species.StressMortalityMaximum < 0.0
			|| Species.StressMortalityMaximum > 1.0
			|| !FMath::IsFinite(Species.StressMortalityVigorThreshold)
			|| Species.StressMortalityVigorThreshold < 0.0
			|| Species.StressMortalityVigorThreshold > 1.0
			|| !FMath::IsFinite(Species.StressMortalitySteepness)
			|| Species.StressMortalitySteepness <= 0.0
			|| !FMath::IsFinite(Species.StressMortalityCanopyFloor)
			|| Species.StressMortalityCanopyFloor < 0.0
			|| Species.StressMortalityCanopyFloor > 1.0
			|| !FMath::IsFinite(Species.VigorSmoothingRetention)
			|| Species.VigorSmoothingRetention < 0.0
			|| Species.VigorSmoothingRetention >= 1.0
			|| Species.MaturityAgeYears <= 0
			|| (bIndividualizes && Species.MaturityAgeYears > Species.MaxAgeYears)
			|| !FMath::IsFinite(Species.RecruitsPerMatureStemPerYear)
			|| Species.RecruitsPerMatureStemPerYear < 0.0f
			|| !FMath::IsFinite(Species.AnnualPropaguleProduction)
			|| Species.AnnualPropaguleProduction < 0.0f
			|| !FMath::IsFinite(Species.MeanDispersalDistanceCm)
			|| Species.MeanDispersalDistanceCm < 0.0f
			|| !FMath::IsFinite(Species.MaxSlopeDeg)
			|| Species.MaxSlopeDeg < 0.0f
			|| Species.MaxSlopeDeg > 90.0f
			|| !IsValidResponse(Species.LightResponse, 0.0f, UE::ArcForest::MaximumLightResponseBound)
			|| !IsValidResponse(Species.MoistureResponse, 0.0f, 1.0f)
			|| !IsValidResponse(
				Species.TemperatureResponse,
				UE::ArcForest::MinimumTemperatureResponseBound,
				UE::ArcForest::MaximumTemperatureResponseBound))
		{
			OutError = FString::Printf(TEXT("Frozen species setup %d has invalid ecology."), SpeciesIndex);
			return false;
		}
		return true;
	}

	bool ValidateInput(
		const FArcForestRunInput& Input,
		const int32 NumYears,
		TArray<FArcForestFrozenSpeciesSetup>& OutSpecies,
		TArray<FArcForestSpeciesEcologyHash>& OutEcologyHashes,
		FString& OutError)
	{
		if (NumYears <= 0)
		{
			OutError = TEXT("ArcForest annual simulation requires at least one year.");
			return false;
		}
		if (!Input.VolumeId.IsValid()
			|| !Input.SiteSource.IsValid()
			|| Input.MaxLiveStems <= 0
			|| Input.MaxLiveRecords <= 0
			|| Input.RecruitmentOversamplingFactor <= 0
			|| Input.MaxStateRecords <= 0
			|| Input.MaxCoverRecords < 0
			|| Input.MaxDeadwoodRecords < 0
			|| Input.MaxBranchRecords < 0
			|| Input.SettingsHash == 0
			|| !FMath::IsFinite(Input.MaximumCrownOccupancy)
			|| Input.MaximumCrownOccupancy <= 0.0
			|| !FMath::IsFinite(Input.UnderstoryShade)
			|| Input.UnderstoryShade < 0.05
			|| Input.UnderstoryShade > 4.0
			|| !FMath::IsFinite(Input.GroundCoverDensity)
			|| Input.GroundCoverDensity < 0.0
			|| Input.GroundCoverDensity > 4.0
			|| !FMath::IsFinite(Input.GroundCoverPatchiness)
			|| Input.GroundCoverPatchiness < 0.0
			|| Input.GroundCoverPatchiness > 0.95)
		{
			OutError = TEXT("ArcForest annual simulation identity, limits, recruitment oversampling factor, settings hash, maximum crown occupancy, understory shade, or ground cover are invalid.");
			return false;
		}
		if (static_cast<int64>(Input.MaxDeadwoodRecords) + Input.MaxBranchRecords > MAX_int32)
		{
			OutError = TEXT("ArcForest annual MaxDeadwoodRecords plus MaxBranchRecords must not exceed 2147483647.");
			return false;
		}
		if (static_cast<int64>(Input.MaxLiveRecords)
				+ Input.MaxCoverRecords
				+ Input.MaxDeadwoodRecords
				+ Input.MaxBranchRecords != Input.MaxStateRecords)
		{
			OutError = TEXT("ArcForest annual MaxStateRecords must equal MaxLiveRecords plus MaxCoverRecords plus MaxDeadwoodRecords plus MaxBranchRecords.");
			return false;
		}

		const FArcEcotopeSiteMetadata& SiteMetadata = GetSiteMetadata(Input);
		if (!Input.RegionalRecruitmentConfig.IsValid())
		{
			OutError = TEXT("ArcForest annual regional recruitment configuration is invalid.");
			return false;
		}
		FText DeadwoodError;
		if (!Input.DeadwoodSettings.IsValid(DeadwoodError))
		{
			OutError = FString::Printf(
				TEXT("ArcForest annual deadwood settings are invalid: %s"),
				*DeadwoodError.ToString());
			return false;
		}

		FText DisturbanceError;
		if (!Input.DisturbanceSettings.IsValid(DisturbanceError))
		{
			OutError = FString::Printf(
				TEXT("ArcForest annual disturbance settings are invalid: %s"),
				*DisturbanceError.ToString());
			return false;
		}

		OutSpecies = Input.SpeciesSetups;
		OutSpecies.Sort(
			[](const FArcForestFrozenSpeciesSetup& Left, const FArcForestFrozenSpeciesSetup& Right)
			{
				return GuidLess(Left.SpeciesId, Right.SpeciesId);
			});
		OutEcologyHashes.Reset();
		OutEcologyHashes.Reserve(OutSpecies.Num());
		FGuid PreviousSpeciesId;
		for (int32 SpeciesIndex = 0; SpeciesIndex < OutSpecies.Num(); ++SpeciesIndex)
		{
			const FArcForestFrozenSpeciesSetup& Species = OutSpecies[SpeciesIndex];
			if (!ValidateSpecies(Species, SpeciesIndex, OutError)
				|| (SpeciesIndex > 0 && Species.SpeciesId == PreviousSpeciesId))
			{
				if (OutError.IsEmpty())
				{
					OutError = FString::Printf(TEXT("Frozen species setup %d duplicates a SpeciesId."), SpeciesIndex);
				}
				return false;
			}
			FArcForestSpeciesEcologyHash& Hash = OutEcologyHashes.AddDefaulted_GetRef();
			Hash.SpeciesId = Species.SpeciesId;
			Hash.EcologyHash = Species.EcologyHash;
			PreviousSpeciesId = Species.SpeciesId;
		}

		TArray<UE::ArcForest::FSettingsHashSpecies> SettingsSpecies;
		SettingsSpecies.Reserve(OutSpecies.Num());
		for (const FArcForestFrozenSpeciesSetup& Species : OutSpecies)
		{
			UE::ArcForest::FSettingsHashSpecies& Entry = SettingsSpecies.AddDefaulted_GetRef();
			Entry.SpeciesId = Species.SpeciesId;
			Entry.RegionalArrivalPerHectarePerYear = Species.RegionalArrivalPerHectarePerYear;
		}
		const FArcEcotopeBudgetSettings RecomputedBudgetSettings = UE::ArcEcotope::MakeBudgetSettings(Input);
		const uint64 RecomputedSettingsHash = UE::ArcForest::ComputeSettingsHash(
			Input.Seed,
			SiteMetadata.CellSizeCm,
			SiteMetadata.RecordGridLevel,
			RecomputedBudgetSettings,
			Input.DeadwoodSettings,
			Input.MaximumCrownOccupancy,
			Input.UnderstoryShade,
			Input.GroundCoverDensity,
			Input.GroundCoverPatchiness,
			SettingsSpecies,
			Input.DisturbanceSettings);
		if (Input.SettingsHash != RecomputedSettingsHash)
		{
			OutError = TEXT("ArcForest annual settings hash does not match the frozen values.");
			return false;
		}

		FText SnapshotError;
		if (Input.SourceSnapshot.Validate(
				SnapshotError,
				Input.SettingsHash,
				&OutEcologyHashes) != EArcForestSimulationStateStatus::Valid)
		{
			OutError = FString::Printf(TEXT("ArcForest annual source snapshot is invalid: %s"), *SnapshotError.ToString());
			return false;
		}
		if (Input.SourceSnapshot.SimulationId != UE::ArcForest::MakeSimulationId(Input.VolumeId, Input.Seed)
			|| Input.SourceSnapshot.Seed != Input.Seed)
		{
			OutError = TEXT("ArcForest annual source snapshot does not belong to this volume and seed.");
			return false;
		}
		if (!(Input.SourceSnapshot.RegionalRecruitmentConfig == Input.RegionalRecruitmentConfig))
		{
			OutError = TEXT("ArcForest annual regional recruitment configuration differs from the authoritative snapshot lineage. Clear State and bootstrap again after changing RegionalRecruitmentMaxProposalRounds.");
			return false;
		}

		const int64 LiveRecordCount =
			static_cast<int64>(Input.SourceSnapshot.Individuals.Num())
			+ Input.SourceSnapshot.Cohorts.Num();
		const int64 CoverRecordCount = Input.SourceSnapshot.Patches.Num();
		int64 NonBranchDeadwoodRecordCount = 0;
		int64 BranchRecordCount = 0;
		for (const FArcForestDeadwoodRecord& Deadwood : Input.SourceSnapshot.Deadwood)
		{
			if (Deadwood.Type == EArcForestDeadwoodType::Branch)
			{
				++BranchRecordCount;
			}
			else
			{
				++NonBranchDeadwoodRecordCount;
			}
		}
		const int64 TotalRecordCount =
			LiveRecordCount + CoverRecordCount + NonBranchDeadwoodRecordCount + BranchRecordCount;
		if (LiveRecordCount > Input.MaxLiveRecords)
		{
			OutError = TEXT("ArcForest annual source snapshot exceeds MaxLiveRecords.");
			return false;
		}
		if (CoverRecordCount > Input.MaxCoverRecords)
		{
			OutError = TEXT("ArcForest annual source snapshot exceeds MaxCoverRecords.");
			return false;
		}
		if (NonBranchDeadwoodRecordCount > Input.MaxDeadwoodRecords)
		{
			OutError = TEXT("ArcForest annual source snapshot exceeds MaxDeadwoodRecords.");
			return false;
		}
		if (BranchRecordCount > Input.MaxBranchRecords)
		{
			OutError = TEXT("ArcForest annual source snapshot exceeds MaxBranchRecords.");
			return false;
		}
		if (TotalRecordCount > Input.MaxStateRecords)
		{
			OutError = TEXT("ArcForest annual source snapshot exceeds MaxStateRecords.");
			return false;
		}

		int64 LiveStems = 0;
		TMap<FGuid, const FArcForestFrozenSpeciesSetup*> SpeciesById;
		for (const FArcForestFrozenSpeciesSetup& Species : OutSpecies)
		{
			SpeciesById.Add(Species.SpeciesId, &Species);
		}
		for (const FArcForestIndividualRecord& Individual : Input.SourceSnapshot.Individuals)
		{
			const FArcForestFrozenSpeciesSetup* Species = SpeciesById.FindRef(Individual.SpeciesId);
			if (Individual.LifeStatus != EArcForestLifeStatus::Alive)
			{
				OutError = TEXT("ArcForest annual input requires dead individuals to be represented as deadwood.");
				return false;
			}
			if (Species == nullptr
				|| Individual.HeightCm > Species->MaxHeightCm
				|| Individual.DbhCm > Species->MaxDbhCm)
			{
				OutError = TEXT("ArcForest annual input contains an individual outside its species bounds.");
				return false;
			}
			++LiveStems;
		}
		for (const FArcForestCohortRecord& Cohort : Input.SourceSnapshot.Cohorts)
		{
			const FArcForestFrozenSpeciesSetup* Species = SpeciesById.FindRef(Cohort.SpeciesId);
			if (Species == nullptr
				|| Cohort.MeanHeightCm > Species->MaxHeightCm)
			{
				OutError = TEXT("ArcForest annual input contains a cohort outside its species bounds.");
				return false;
			}
			if (LiveStems > MAX_int64 - Cohort.StemCount)
			{
				OutError = TEXT("ArcForest annual live-stem total overflowed.");
				return false;
			}
			LiveStems += Cohort.StemCount;
		}
		for (const FArcForestPatchRecord& Patch : Input.SourceSnapshot.Patches)
		{
			const FArcForestFrozenSpeciesSetup* Species = SpeciesById.FindRef(Patch.SpeciesId);
			if (Species == nullptr || Patch.MeanHeightCm > Species->MaxHeightCm)
			{
				OutError = TEXT("ArcForest annual input contains a patch outside its species bounds.");
				return false;
			}
			if (!FMath::IsFinite(Patch.StemOrCoverEquivalent)
				|| Patch.StemOrCoverEquivalent < 0.0f
				|| static_cast<double>(Patch.StemOrCoverEquivalent) > Input.MaxLiveStems)
			{
				OutError = TEXT("ArcForest annual patch cover is outside the live-stem budget.");
				return false;
			}
			const int64 PatchStems = FMath::CeilToInt64(Patch.StemOrCoverEquivalent);
			if (PatchStems > Input.MaxLiveStems
				|| LiveStems > static_cast<int64>(Input.MaxLiveStems) - PatchStems)
			{
				OutError = TEXT("ArcForest annual patch cover exceeds MaxLiveStems when accumulated.");
				return false;
			}
			LiveStems += PatchStems;
		}
		if (LiveStems > Input.MaxLiveStems)
		{
			OutError = TEXT("ArcForest annual source snapshot exceeds MaxLiveStems.");
			return false;
		}
		return true;
	}

	bool TryGetBaseCellIndex(
		const FArcEcotopeSiteMetadata& Site,
		const TConstArrayView<uint8> BaseValidityMask,
		const FVector& LocalPosition,
		int32& OutBaseCellIndex)
	{
		return Site.TryGetCellIndexForLocalPosition(FVector2D(LocalPosition.X, LocalPosition.Y), OutBaseCellIndex)
			&& OutBaseCellIndex >= 0
			&& OutBaseCellIndex < BaseValidityMask.Num()
			&& BaseValidityMask[OutBaseCellIndex] != 0;
	}

	bool TryGetRepresentativeBaseCellForRecordCell(
		const FArcEcotopeSiteMetadata& Site,
		const TConstArrayView<uint8> BaseValidityMask,
		const int32 RecordCellIndex,
		int32& OutBaseCellIndex)
	{
		OutBaseCellIndex = INDEX_NONE;
		const int32 BaseCellCount = Site.GetCellCount();
		const int32 RecordCellCount = Site.GetLevelCellCount(Site.RecordGridLevel);
		if (BaseCellCount <= 0 || RecordCellCount <= 0 || BaseValidityMask.Num() != BaseCellCount
			|| RecordCellIndex < 0 || RecordCellIndex >= RecordCellCount)
		{
			return false;
		}

		const FVector RecordCellCenterLocal =
			Site.GetLevelCellCenterLocal(RecordCellIndex, Site.RecordGridLevel);
		int32 CenterBaseCellIndex = INDEX_NONE;
		if (Site.TryGetCellIndexForLocalPosition(
				FVector2D(RecordCellCenterLocal.X, RecordCellCenterLocal.Y), CenterBaseCellIndex)
			&& BaseValidityMask[CenterBaseCellIndex] != 0)
		{
			OutBaseCellIndex = CenterBaseCellIndex;
			return true;
		}

		FIntPoint MinCornerCoordinates = FIntPoint::ZeroValue;
		const int32 MinCornerBaseCellIndex =
			Site.MapCellIndexBetweenLevels(RecordCellIndex, Site.RecordGridLevel, 0);
		if (MinCornerBaseCellIndex == INDEX_NONE
			|| !Site.TryGetCellCoordinates(MinCornerBaseCellIndex, MinCornerCoordinates))
		{
			return false;
		}

		const int32 ChildSpan = static_cast<int32>(1) << Site.RecordGridLevel;
		for (int32 OffsetY = 0; OffsetY < ChildSpan; ++OffsetY)
		{
			for (int32 OffsetX = 0; OffsetX < ChildSpan; ++OffsetX)
			{
				int32 BaseCellIndex = INDEX_NONE;
				if (Site.TryGetCellIndex(
						MinCornerCoordinates.X + OffsetX,
						MinCornerCoordinates.Y + OffsetY,
						BaseCellIndex)
					&& BaseValidityMask[BaseCellIndex] != 0)
				{
					OutBaseCellIndex = BaseCellIndex;
					return true;
				}
			}
		}
		return false;
	}

	bool TryGetBaseAndRecordCellIndex(
		const FArcEcotopeSiteMetadata& Site,
		const TConstArrayView<uint8> BaseValidityMask,
		const FVector& LocalPosition,
		int32& OutBaseCellIndex,
		int32& OutRecordCellIndex)
	{
		if (!TryGetBaseCellIndex(Site, BaseValidityMask, LocalPosition, OutBaseCellIndex))
		{
			return false;
		}

		OutRecordCellIndex = GetRecordCellIndexForBaseCell(Site, OutBaseCellIndex);
		return OutRecordCellIndex != INDEX_NONE;
	}

	double CalculateCrownRadius(const double HeightCm, const FArcForestFrozenSpeciesSetup& Species)
	{
		return FMath::Max(0.0, HeightCm) * Species.CrownRadiusHeightRatio;
	}

	/** Integral up to Position of the overlap length of [u, u + KernelSide] with [0, SourceLength]. */
	double IntegrateBoxOverlap(const double Position, const double SourceLength, const double KernelSide)
	{
		const double UpperRamp = FMath::Max(Position + KernelSide, 0.0);
		const double LowerRamp = FMath::Max(Position, 0.0);
		const double UpperShiftedRamp = FMath::Max(Position + KernelSide - SourceLength, 0.0);
		const double LowerShiftedRamp = FMath::Max(Position - SourceLength, 0.0);
		return 0.5
			* (UpperRamp * UpperRamp - LowerRamp * LowerRamp - UpperShiftedRamp * UpperShiftedRamp
				+ LowerShiftedRamp * LowerShiftedRamp);
	}

	struct FFootprintAxisSpread
	{
		int32 FirstCell = 0;
		int32 NumCells = 0;
		double CellMasses[MaximumFootprintSpanCells] = {};
	};

	void BuildFootprintAxisSpread(
		const double GridMinLocal,
		const double GridMaxLocal,
		const double CellSizeCm,
		const int32 CellDimension,
		const double SourceMinLocal,
		const double SourceLengthCm,
		const double KernelSideCm,
		FFootprintAxisSpread& OutSpread)
	{
		const double SupportMin = SourceMinLocal - 0.5 * KernelSideCm;
		const double SupportMax = SourceMinLocal + SourceLengthCm + 0.5 * KernelSideCm;
		const double LastCellReal = static_cast<double>(CellDimension - 1);
		int32 FirstCell = static_cast<int32>(
			FMath::Clamp(FMath::Floor((SupportMin - GridMinLocal) / CellSizeCm), 0.0, LastCellReal));
		int32 LastCell = static_cast<int32>(
			FMath::Clamp(FMath::Floor((SupportMax - GridMinLocal) / CellSizeCm), 0.0, LastCellReal));

		const bool bSpanClamped = LastCell - FirstCell + 1 > MaximumFootprintSpanCells;
		if (bSpanClamped)
		{
			const int32 CentreCell = static_cast<int32>(FMath::Clamp(
				FMath::Floor((SourceMinLocal + 0.5 * SourceLengthCm - GridMinLocal) / CellSizeCm),
				static_cast<double>(FirstCell),
				static_cast<double>(LastCell)));
			FirstCell = FMath::Clamp(
				CentreCell - MaximumFootprintSpanCells / 2, FirstCell, LastCell - MaximumFootprintSpanCells + 1);
			LastCell = FirstCell + MaximumFootprintSpanCells - 1;
		}

		OutSpread.FirstCell = FirstCell;
		OutSpread.NumCells = LastCell - FirstCell + 1;

		double CapturedTotal = 0.0;
		for (int32 Offset = 0; Offset < OutSpread.NumCells; ++Offset)
		{
			const double CellMin = FMath::Clamp(
				GridMinLocal + static_cast<double>(FirstCell + Offset) * CellSizeCm, GridMinLocal, GridMaxLocal);
			const double CellMax = FMath::Clamp(
				GridMinLocal + static_cast<double>(FirstCell + Offset + 1) * CellSizeCm, GridMinLocal, GridMaxLocal);
			double CellMass = 0.0;
			if (SourceLengthCm > 0.0)
			{
				const double ShiftOrigin = SourceMinLocal + 0.5 * KernelSideCm;
				CellMass = (IntegrateBoxOverlap(CellMax - ShiftOrigin, SourceLengthCm, KernelSideCm)
					- IntegrateBoxOverlap(CellMin - ShiftOrigin, SourceLengthCm, KernelSideCm))
					/ (SourceLengthCm * KernelSideCm);
			}
			else
			{
				CellMass = (FMath::Min(CellMax, SupportMax) - FMath::Max(CellMin, SupportMin)) / KernelSideCm;
			}
			OutSpread.CellMasses[Offset] = FMath::Max(CellMass, 0.0);
			CapturedTotal += OutSpread.CellMasses[Offset];
		}

		if (bSpanClamped && CapturedTotal > 0.0)
		{
			for (int32 Offset = 0; Offset < OutSpread.NumCells; ++Offset)
			{
				OutSpread.CellMasses[Offset] /= CapturedTotal;
			}
		}
	}

	/** Occupancy the same footprint puts into one named cell, so a removal count can be solved against that cell's excess. */
	double CalculateOwnCellCrownShare(
		const FArcEcotopeSiteMetadata& Site,
		const FVector2D& SourceMinLocal,
		const FVector2D& SourceSizeCm,
		const double CrownRadiusCm,
		const double StemCount,
		const int32 RecordCellIndex,
		const TArray<double>& RecordCellAreas)
	{
		const double TotalCrownArea = UE_DOUBLE_PI * CrownRadiusCm * CrownRadiusCm * StemCount;
		const FIntPoint LevelDimensions = Site.GetLevelDimensions(Site.RecordGridLevel);
		const double CellSizeCm = Site.GetLevelCellSizeCm(Site.RecordGridLevel);
		const double KernelSideCm = CrownRadiusCm * FMath::Sqrt(UE_DOUBLE_PI);
		if (!FMath::IsFinite(CrownRadiusCm) || !FMath::IsFinite(StemCount) || !FMath::IsFinite(TotalCrownArea)
			|| TotalCrownArea <= 0.0 || LevelDimensions.X <= 0 || LevelDimensions.Y <= 0 || CellSizeCm <= 0.0
			|| KernelSideCm <= 0.0 || !Site.LocalBounds.IsValid || !RecordCellAreas.IsValidIndex(RecordCellIndex)
			|| RecordCellAreas[RecordCellIndex] <= 0.0)
		{
			return 0.0;
		}

		FFootprintAxisSpread SpreadX;
		FFootprintAxisSpread SpreadY;
		BuildFootprintAxisSpread(
			Site.LocalBounds.Min.X,
			Site.LocalBounds.Max.X,
			CellSizeCm,
			LevelDimensions.X,
			SourceMinLocal.X,
			SourceSizeCm.X,
			KernelSideCm,
			SpreadX);
		BuildFootprintAxisSpread(
			Site.LocalBounds.Min.Y,
			Site.LocalBounds.Max.Y,
			CellSizeCm,
			LevelDimensions.Y,
			SourceMinLocal.Y,
			SourceSizeCm.Y,
			KernelSideCm,
			SpreadY);

		const int32 OffsetX = RecordCellIndex % LevelDimensions.X - SpreadX.FirstCell;
		const int32 OffsetY = RecordCellIndex / LevelDimensions.X - SpreadY.FirstCell;
		if (OffsetX < 0 || OffsetX >= SpreadX.NumCells || OffsetY < 0 || OffsetY >= SpreadY.NumCells)
		{
			return 0.0;
		}
		const double CellShare = TotalCrownArea * SpreadX.CellMasses[OffsetX] * SpreadY.CellMasses[OffsetY]
			/ RecordCellAreas[RecordCellIndex];
		return FMath::IsFinite(CellShare) && CellShare > 0.0 ? CellShare : 0.0;
	}

	struct FDiagnosticAxisSpread
	{
		int32 FirstCell = 0;
		int32 NumCells = 0;
		double CellMasses[MaximumDiagnosticFootprintSpanCells] = {};
	};

	/** BuildFootprintAxisSpread at the diagnostic span bound. FFootprintAxisSpread's CellMasses is fixed at the record-grid bound, so the base grid needs a wider twin. Returns true when the span was clamped and the retained mass renormalised. */
	bool BuildDiagnosticAxisSpread(
		const double GridMinLocal,
		const double GridMaxLocal,
		const double CellSizeCm,
		const int32 CellDimension,
		const double SourceMinLocal,
		const double SourceLengthCm,
		const double KernelSideCm,
		FDiagnosticAxisSpread& OutSpread)
	{
		const double SupportMin = SourceMinLocal - 0.5 * KernelSideCm;
		const double SupportMax = SourceMinLocal + SourceLengthCm + 0.5 * KernelSideCm;
		const double LastCellReal = static_cast<double>(CellDimension - 1);
		int32 FirstCell = static_cast<int32>(
			FMath::Clamp(FMath::Floor((SupportMin - GridMinLocal) / CellSizeCm), 0.0, LastCellReal));
		int32 LastCell = static_cast<int32>(
			FMath::Clamp(FMath::Floor((SupportMax - GridMinLocal) / CellSizeCm), 0.0, LastCellReal));

		const bool bSpanClamped = LastCell - FirstCell + 1 > MaximumDiagnosticFootprintSpanCells;
		if (bSpanClamped)
		{
			const int32 CentreCell = static_cast<int32>(FMath::Clamp(
				FMath::Floor((SourceMinLocal + 0.5 * SourceLengthCm - GridMinLocal) / CellSizeCm),
				static_cast<double>(FirstCell),
				static_cast<double>(LastCell)));
			FirstCell = FMath::Clamp(
				CentreCell - MaximumDiagnosticFootprintSpanCells / 2,
				FirstCell,
				LastCell - MaximumDiagnosticFootprintSpanCells + 1);
			LastCell = FirstCell + MaximumDiagnosticFootprintSpanCells - 1;
		}

		OutSpread.FirstCell = FirstCell;
		OutSpread.NumCells = LastCell - FirstCell + 1;

		double CapturedTotal = 0.0;
		for (int32 Offset = 0; Offset < OutSpread.NumCells; ++Offset)
		{
			const double CellMin = FMath::Clamp(
				GridMinLocal + static_cast<double>(FirstCell + Offset) * CellSizeCm, GridMinLocal, GridMaxLocal);
			const double CellMax = FMath::Clamp(
				GridMinLocal + static_cast<double>(FirstCell + Offset + 1) * CellSizeCm, GridMinLocal, GridMaxLocal);
			double CellMass = 0.0;
			if (SourceLengthCm > 0.0)
			{
				const double ShiftOrigin = SourceMinLocal + 0.5 * KernelSideCm;
				CellMass = (IntegrateBoxOverlap(CellMax - ShiftOrigin, SourceLengthCm, KernelSideCm)
					- IntegrateBoxOverlap(CellMin - ShiftOrigin, SourceLengthCm, KernelSideCm))
					/ (SourceLengthCm * KernelSideCm);
			}
			else
			{
				CellMass = (FMath::Min(CellMax, SupportMax) - FMath::Max(CellMin, SupportMin)) / KernelSideCm;
			}
			OutSpread.CellMasses[Offset] = FMath::Max(CellMass, 0.0);
			CapturedTotal += OutSpread.CellMasses[Offset];
		}

		if (bSpanClamped && CapturedTotal > 0.0)
		{
			for (int32 Offset = 0; Offset < OutSpread.NumCells; ++Offset)
			{
				OutSpread.CellMasses[Offset] /= CapturedTotal;
			}
		}
		return bSpanClamped;
	}

	/** Diagnostic-only base-grid twin of SpreadCrownArea. It writes a scratch cover field no simulated value reads. */
	void SpreadCrownAreaOnBaseGrid(
		const FArcEcotopeSiteMetadata& Site,
		const FVector2D& SourceMinLocal,
		const FVector2D& SourceSizeCm,
		const double CrownRadiusCm,
		const double StemCount,
		const TArray<double>& BaseCellAreas,
		TArray<double>& BaseCellCover,
		int32& InOutClampedAxisCount)
	{
		const double TotalCrownArea = UE_DOUBLE_PI * CrownRadiusCm * CrownRadiusCm * StemCount;
		if (!FMath::IsFinite(CrownRadiusCm) || CrownRadiusCm <= 0.0 || !FMath::IsFinite(StemCount)
			|| StemCount <= 0.0 || !FMath::IsFinite(TotalCrownArea) || TotalCrownArea <= 0.0)
		{
			return;
		}

		const FIntPoint LevelDimensions = Site.GetLevelDimensions(0);
		const double CellSizeCm = Site.GetLevelCellSizeCm(0);
		const double KernelSideCm = CrownRadiusCm * FMath::Sqrt(UE_DOUBLE_PI);
		if (LevelDimensions.X <= 0 || LevelDimensions.Y <= 0 || CellSizeCm <= 0.0 || KernelSideCm <= 0.0
			|| !Site.LocalBounds.IsValid)
		{
			return;
		}

		FDiagnosticAxisSpread SpreadX;
		FDiagnosticAxisSpread SpreadY;
		InOutClampedAxisCount += BuildDiagnosticAxisSpread(
			Site.LocalBounds.Min.X,
			Site.LocalBounds.Max.X,
			CellSizeCm,
			LevelDimensions.X,
			SourceMinLocal.X,
			SourceSizeCm.X,
			KernelSideCm,
			SpreadX) ? 1 : 0;
		InOutClampedAxisCount += BuildDiagnosticAxisSpread(
			Site.LocalBounds.Min.Y,
			Site.LocalBounds.Max.Y,
			CellSizeCm,
			LevelDimensions.Y,
			SourceMinLocal.Y,
			SourceSizeCm.Y,
			KernelSideCm,
			SpreadY) ? 1 : 0;

		for (int32 OffsetY = 0; OffsetY < SpreadY.NumCells; ++OffsetY)
		{
			const int32 RowOrigin = (SpreadY.FirstCell + OffsetY) * LevelDimensions.X;
			for (int32 OffsetX = 0; OffsetX < SpreadX.NumCells; ++OffsetX)
			{
				const double CellWeight = SpreadX.CellMasses[OffsetX] * SpreadY.CellMasses[OffsetY];
				if (CellWeight <= 0.0)
				{
					continue;
				}
				const int32 BaseCellIndex = RowOrigin + SpreadX.FirstCell + OffsetX;
				if (!BaseCellCover.IsValidIndex(BaseCellIndex) || !(BaseCellAreas[BaseCellIndex] > 0.0))
				{
					continue;
				}
				BaseCellCover[BaseCellIndex] = FMath::Min(
					MaximumOccupancy,
					BaseCellCover[BaseCellIndex]
						+ FMath::Min(MaximumOccupancy, TotalCrownArea * CellWeight / BaseCellAreas[BaseCellIndex]));
			}
		}
	}

	bool BuildRecordCellAreas(
		const FArcEcotopeSiteMetadata& Site,
		const TConstArrayView<uint8> BaseValidityMask,
		TArray<double>& OutRecordCellAreas,
		FString& OutError)
	{
		const int32 RecordCellCount = Site.GetLevelCellCount(Site.RecordGridLevel);
		OutRecordCellAreas.SetNumUninitialized(RecordCellCount);
		for (int32 RecordCellIndex = 0; RecordCellIndex < RecordCellCount; ++RecordCellIndex)
		{
			OutRecordCellAreas[RecordCellIndex] =
				Site.GetLevelCellAreaSquareCentimeters(RecordCellIndex, Site.RecordGridLevel);
			int32 RepresentativeBaseCellIndex = INDEX_NONE;
			if (TryGetRepresentativeBaseCellForRecordCell(
					Site, BaseValidityMask, RecordCellIndex, RepresentativeBaseCellIndex)
				&& OutRecordCellAreas[RecordCellIndex] <= 0.0)
			{
				OutError = TEXT("ArcForest annual simulation found a valid cell with zero area.");
				return false;
			}
		}
		return true;
	}

	/** Per-call-site phase labels, so the two callers of AccumulateCanopyOccupancy do not add into one phase total. */
	struct FCanopyOccupancyPhaseNames
	{
		const TCHAR* Individuals = nullptr;
		const TCHAR* Cohorts = nullptr;
		const TCHAR* Patches = nullptr;
		const TCHAR* IndividualsIndex = nullptr;
		const TCHAR* CohortsIndex = nullptr;
		const TCHAR* PatchesIndex = nullptr;
		const TCHAR* IndividualsTileGather = nullptr;
		const TCHAR* CohortsTileGather = nullptr;
		const TCHAR* PatchesTileGather = nullptr;
	};

	const FCanopyOccupancyPhaseNames CanopyBuildPhaseNames{
		TEXT("CanopyAndHabitat_Individuals"),
		TEXT("CanopyAndHabitat_Cohorts"),
		TEXT("CanopyAndHabitat_Patches"),
		TEXT("CanopyAndHabitat_Individuals_Index"),
		TEXT("CanopyAndHabitat_Cohorts_Index"),
		TEXT("CanopyAndHabitat_Patches_Index"),
		TEXT("CanopyAndHabitat_Individuals_TileGather"),
		TEXT("CanopyAndHabitat_Cohorts_TileGather"),
		TEXT("CanopyAndHabitat_Patches_TileGather")};

	const FCanopyOccupancyPhaseNames SelfThinningPhaseNames{
		TEXT("SelfThinning_Individuals"),
		TEXT("SelfThinning_Cohorts"),
		TEXT("SelfThinning_Patches"),
		TEXT("SelfThinning_Individuals_Index"),
		TEXT("SelfThinning_Cohorts_Index"),
		TEXT("SelfThinning_Patches_Index"),
		TEXT("SelfThinning_Individuals_TileGather"),
		TEXT("SelfThinning_Cohorts_TileGather"),
		TEXT("SelfThinning_Patches_TileGather")};

	/** The occupancy column has to reach the tallest stem the frozen set can grow, and no higher. */
	double CalculateOccupancyColumnTopCm(const TArray<FArcForestFrozenSpeciesSetup>& Species)
	{
		double ColumnTopCm = 0.0;
		for (const FArcForestFrozenSpeciesSetup& Setup : Species)
		{
			ColumnTopCm = FMath::Max(ColumnTopCm, static_cast<double>(Setup.MaxHeightCm));
		}
		return ColumnTopCm;
	}

	/**
	 * The scalar footprint is capped to nine cells per axis. In the interior it is centered on the source,
	 * but a site edge may shift the retained window fully inward, so a capped source can reach eight cells
	 * from its rooted record cell. Smaller uncapped crowns use a correspondingly smaller read halo; the cap
	 * and its renormalisation remain unchanged.
	 */
	int32 CalculateOccupancyRecordHaloCells(
		const FArcEcotopeSiteMetadata& Site,
		const TArray<FArcForestFrozenSpeciesSetup>& Species)
	{
		const double RecordCellSizeCm = Site.GetLevelCellSizeCm(Site.RecordGridLevel);
		if (!(RecordCellSizeCm > 0.0))
		{
			return MaximumFootprintSpanCells - 1;
		}

		double MaximumHalfKernelCm = 0.0;
		for (const FArcForestFrozenSpeciesSetup& Setup : Species)
		{
			const double MaximumCrownRadiusCm = CalculateCrownRadius(Setup.MaxHeightCm, Setup);
			MaximumHalfKernelCm = FMath::Max(
				MaximumHalfKernelCm,
				0.5 * MaximumCrownRadiusCm * FMath::Sqrt(UE_DOUBLE_PI));
		}
		const double FrozenSupportCells = MaximumHalfKernelCm / RecordCellSizeCm;
		if (FrozenSupportCells >= static_cast<double>(MaximumFootprintSpanCells / 2))
		{
			return MaximumFootprintSpanCells - 1;
		}
		return FrozenSupportCells > 0.0 ? FMath::CeilToInt32(FrozenSupportCells) : 0;
	}

	struct FOccupancySource
	{
		FVector2D SourceMinLocal = FVector2D::ZeroVector;
		FVector2D SourceSizeCm = FVector2D::ZeroVector;
		double HeightCm = 0.0;
		double CrownRadiusCm = 0.0;
		double StemCount = 0.0;
		int32 RecordCellIndex = INDEX_NONE;
		int32 SourceIndex = INDEX_NONE;
	};

	enum class ETileGatherError : uint8
	{
		None,
		InvalidGeometry,
	};

	struct FTileGatherTaskResult
	{
		ETileGatherError Error = ETileGatherError::None;
		int32 SourceIndex = INDEX_NONE;
	};

	enum class EOccupancySourceBuildError : uint8
	{
		None,
		SpeciesMissing,
		PositionOutsideSite,
		OwnerChunkInvalid,
	};

	struct FOccupancySourceBuildResult
	{
		EOccupancySourceBuildError Error = EOccupancySourceBuildError::None;
		int32 OwnerChunkIndex = INDEX_NONE;
	};

	bool ApplyCrownAreaToRecordCore(
		const FArcEcotopeSiteMetadata& Site,
		const FOccupancySource& Source,
		const FIntRect& RecordCoreRect,
		FStratifiedOccupancy& CellOccupancy,
		const TArray<double>& RecordCellAreas)
	{
		const double TotalCrownArea = UE_DOUBLE_PI * Source.CrownRadiusCm * Source.CrownRadiusCm * Source.StemCount;
		if (!FMath::IsFinite(Source.CrownRadiusCm) || Source.CrownRadiusCm < 0.0
			|| !FMath::IsFinite(Source.StemCount) || Source.StemCount < 0.0
			|| !FMath::IsFinite(TotalCrownArea) || TotalCrownArea < 0.0)
		{
			return false;
		}
		if (TotalCrownArea == 0.0)
		{
			return true;
		}

		const FIntPoint LevelDimensions = Site.GetLevelDimensions(Site.RecordGridLevel);
		const double CellSizeCm = Site.GetLevelCellSizeCm(Site.RecordGridLevel);
		const double KernelSideCm = Source.CrownRadiusCm * FMath::Sqrt(UE_DOUBLE_PI);
		if (LevelDimensions.X <= 0 || LevelDimensions.Y <= 0 || CellSizeCm <= 0.0 || KernelSideCm <= 0.0
			|| !Site.LocalBounds.IsValid)
		{
			return false;
		}

		FFootprintAxisSpread SpreadX;
		FFootprintAxisSpread SpreadY;
		BuildFootprintAxisSpread(
			Site.LocalBounds.Min.X,
			Site.LocalBounds.Max.X,
			CellSizeCm,
			LevelDimensions.X,
			Source.SourceMinLocal.X,
			Source.SourceSizeCm.X,
			KernelSideCm,
			SpreadX);
		BuildFootprintAxisSpread(
			Site.LocalBounds.Min.Y,
			Site.LocalBounds.Max.Y,
			CellSizeCm,
			LevelDimensions.Y,
			Source.SourceMinLocal.Y,
			Source.SourceSizeCm.Y,
			KernelSideCm,
			SpreadY);

		double StratumWeights[DefaultCanopyStratumCount];
		CalculateCrownStratumWeights(
			Source.HeightCm - CalculateCrownDepthCm(Source.HeightCm, Source.CrownRadiusCm),
			Source.HeightCm,
			CellOccupancy.GetStratumHeightCm(),
			StratumWeights);
		int32 FirstStratumIndex = 0;
		int32 LastStratumIndex = DefaultCanopyStratumCount - 1;
		while (FirstStratumIndex < DefaultCanopyStratumCount && StratumWeights[FirstStratumIndex] == 0.0)
		{
			++FirstStratumIndex;
		}
		while (LastStratumIndex >= FirstStratumIndex && StratumWeights[LastStratumIndex] == 0.0)
		{
			--LastStratumIndex;
		}

		const int32 FirstX = FMath::Max(SpreadX.FirstCell, RecordCoreRect.Min.X);
		const int32 LastX = FMath::Min(SpreadX.FirstCell + SpreadX.NumCells, RecordCoreRect.Max.X);
		const int32 FirstY = FMath::Max(SpreadY.FirstCell, RecordCoreRect.Min.Y);
		const int32 LastY = FMath::Min(SpreadY.FirstCell + SpreadY.NumCells, RecordCoreRect.Max.Y);
		for (int32 CellY = FirstY; CellY < LastY; ++CellY)
		{
			const int32 SpreadOffsetY = CellY - SpreadY.FirstCell;
			for (int32 CellX = FirstX; CellX < LastX; ++CellX)
			{
				const int32 SpreadOffsetX = CellX - SpreadX.FirstCell;
				const double CellWeight = SpreadX.CellMasses[SpreadOffsetX] * SpreadY.CellMasses[SpreadOffsetY];
				if (CellWeight <= 0.0)
				{
					continue;
				}
				const int32 RecordCellIndex = CellX + CellY * LevelDimensions.X;
				if (!CellOccupancy.IsValidCell(RecordCellIndex)
					|| !RecordCellAreas.IsValidIndex(RecordCellIndex)
					|| !(RecordCellAreas[RecordCellIndex] > 0.0))
				{
					return false;
				}
				const double CrownArea = TotalCrownArea * CellWeight;
				if (!FMath::IsFinite(CrownArea) || CrownArea < 0.0)
				{
					return false;
				}
				const double CellShare = FMath::Min(MaximumOccupancy, CrownArea / RecordCellAreas[RecordCellIndex]);
				const TArrayView<double> Strata = CellOccupancy.GetCellStrata(RecordCellIndex);
				for (int32 StratumIndex = FirstStratumIndex; StratumIndex <= LastStratumIndex; ++StratumIndex)
				{
					Strata[StratumIndex] = FMath::Min(
						MaximumOccupancy,
						Strata[StratumIndex] + CellShare * StratumWeights[StratumIndex]);
				}
			}
		}
		return true;
	}

	bool BuildTileGatherAdjacency(
		const FArcEcotopeChunkLayout& ChunkLayout,
		TArray<TArray<int32>>& OutSourceOwnerChunksByDestination,
		FString& OutError)
	{
		const TConstArrayView<FArcEcotopeChunkRect> Chunks = ChunkLayout.GetChunks();
		OutSourceOwnerChunksByDestination.SetNum(Chunks.Num());
		for (int32 DestinationChunkIndex = 0; DestinationChunkIndex < Chunks.Num(); ++DestinationChunkIndex)
		{
			TArray<int32>& SourceOwners = OutSourceOwnerChunksByDestination[DestinationChunkIndex];
			for (int32 SourceChunkIndex = 0; SourceChunkIndex < Chunks.Num(); ++SourceChunkIndex)
			{
				if (Chunks[SourceChunkIndex].RecordCoreRect.Intersect(Chunks[DestinationChunkIndex].RecordReadRect))
				{
					SourceOwners.Add(SourceChunkIndex);
				}
			}
			if (SourceOwners.IsEmpty())
			{
				OutError = TEXT("ArcForest TileGather found a destination core without a source-owner neighbourhood.");
				return false;
			}
		}
		return true;
	}

	bool TryResolveOccupancyOwnerChunkIndex(
		const FArcEcotopeSiteMetadata& Site,
		const FArcEcotopeChunkLayout& ChunkLayout,
		const int32 RecordCellIndex,
		int32& OutOwnerChunkIndex)
	{
		OutOwnerChunkIndex = INDEX_NONE;
		const FIntPoint RecordDimensions = Site.GetLevelDimensions(Site.RecordGridLevel);
		if (RecordDimensions.X <= 0
			|| RecordCellIndex < 0
			|| RecordCellIndex >= Site.GetLevelCellCount(Site.RecordGridLevel))
		{
			return false;
		}
		FArcEcotopeChunkId OwnerChunkId;
		if (!ChunkLayout.FindChunkForRecordCell(
				FIntPoint(RecordCellIndex % RecordDimensions.X, RecordCellIndex / RecordDimensions.X),
				OwnerChunkId))
		{
			return false;
		}
		const FIntPoint ChunkDimensions = ChunkLayout.GetChunkGridDimensions();
		OutOwnerChunkIndex = OwnerChunkId.X + OwnerChunkId.Y * ChunkDimensions.X;
		return OutOwnerChunkIndex >= 0 && OutOwnerChunkIndex < ChunkLayout.GetChunkCount();
	}

	bool AccumulateTileGatherCategory(
		const FArcEcotopeSiteMetadata& Site,
		const FArcEcotopeChunkLayout& ChunkLayout,
		const TCHAR* CategoryName,
		const TArray<TArray<int32>>& SourceOwnerChunksByDestination,
		const TArray<FOccupancySource>& Sources,
		const TArray<TArray<int32>>& SourcesByOwnerChunk,
		const TArray<double>& RecordCellAreas,
		FStratifiedOccupancy& OutCellOccupancy,
		FString& OutError)
	{
		const TConstArrayView<FArcEcotopeChunkRect> Chunks = ChunkLayout.GetChunks();
		TArray<FTileGatherTaskResult> TaskResults;
		TaskResults.SetNum(Chunks.Num());
		ParallelFor(
			Chunks.Num(),
			[&Site, &Chunks, &SourceOwnerChunksByDestination, &Sources, &SourcesByOwnerChunk,
				&RecordCellAreas, &OutCellOccupancy, &TaskResults](const int32 DestinationChunkIndex)
			{
				const FArcEcotopeChunkRect& DestinationChunk = Chunks[DestinationChunkIndex];
				const FIntPoint RecordDimensions = Site.GetLevelDimensions(Site.RecordGridLevel);
				const TArray<int32>& OwnerChunks = SourceOwnerChunksByDestination[DestinationChunkIndex];
				TArray<int32, TInlineAllocator<16>> OwnerCursors;
				OwnerCursors.Init(0, OwnerChunks.Num());
				for (;;)
				{
					int32 SelectedOwnerOffset = INDEX_NONE;
					int32 SelectedSourceIndex = MAX_int32;
					for (int32 OwnerOffset = 0; OwnerOffset < OwnerChunks.Num(); ++OwnerOffset)
					{
						const TArray<int32>& OwnerSources = SourcesByOwnerChunk[OwnerChunks[OwnerOffset]];
						const int32 Cursor = OwnerCursors[OwnerOffset];
						if (OwnerSources.IsValidIndex(Cursor) && OwnerSources[Cursor] < SelectedSourceIndex)
						{
							SelectedOwnerOffset = OwnerOffset;
							SelectedSourceIndex = OwnerSources[Cursor];
						}
					}
					if (SelectedOwnerOffset == INDEX_NONE)
					{
						break;
					}
					++OwnerCursors[SelectedOwnerOffset];
					const FOccupancySource& Source = Sources[SelectedSourceIndex];
					const FIntPoint SourceRecordCell(
						Source.RecordCellIndex % RecordDimensions.X,
						Source.RecordCellIndex / RecordDimensions.X);
					if (!DestinationChunk.RecordReadRect.Contains(SourceRecordCell))
					{
						continue;
					}
					if (!ApplyCrownAreaToRecordCore(
							Site,
							Source,
							DestinationChunk.RecordCoreRect,
							OutCellOccupancy,
							RecordCellAreas))
					{
						TaskResults[DestinationChunkIndex].Error = ETileGatherError::InvalidGeometry;
						TaskResults[DestinationChunkIndex].SourceIndex = Source.SourceIndex;
						return;
					}
				}
			});

		int32 LowestFailingSourceIndex = MAX_int32;
		for (const FTileGatherTaskResult& Result : TaskResults)
		{
			if (Result.Error != ETileGatherError::None)
			{
				LowestFailingSourceIndex = FMath::Min(LowestFailingSourceIndex, Result.SourceIndex);
			}
		}
		if (LowestFailingSourceIndex != MAX_int32)
		{
			OutError = FString::Printf(
				TEXT("ArcForest TileGather %s canopy accumulation encountered invalid geometry at source index %d."),
				CategoryName,
				LowestFailingSourceIndex);
			return false;
		}
		return true;
	}

	bool AddOccupancySource(
		const FArcEcotopeSiteMetadata& Site,
		const FArcEcotopeChunkLayout& ChunkLayout,
		const FVector2D& SourceMinLocal,
		const FVector2D& SourceSizeCm,
		const double HeightCm,
		const double CrownRadiusCm,
		const double StemCount,
		const int32 RecordCellIndex,
		const int32 SourceIndex,
		TArray<FOccupancySource>& OutSources,
		TArray<TArray<int32>>& OutSourcesByOwnerChunk,
		FString& OutError)
	{
		if (RecordCellIndex < 0 || RecordCellIndex >= Site.GetLevelCellCount(Site.RecordGridLevel))
		{
			OutError = TEXT("ArcForest TileGather source references a record cell outside its frozen site.");
			return false;
		}
		int32 OwnerChunkIndex = INDEX_NONE;
		if (!TryResolveOccupancyOwnerChunkIndex(Site, ChunkLayout, RecordCellIndex, OwnerChunkIndex))
		{
			OutError = TEXT("ArcForest TileGather could not resolve the source chunk owner.");
			return false;
		}
		if (!OutSourcesByOwnerChunk.IsValidIndex(OwnerChunkIndex))
		{
			OutError = TEXT("ArcForest TileGather resolved an invalid source chunk owner.");
			return false;
		}

		FOccupancySource& Source = OutSources.AddDefaulted_GetRef();
		Source.SourceMinLocal = SourceMinLocal;
		Source.SourceSizeCm = SourceSizeCm;
		Source.HeightCm = HeightCm;
		Source.CrownRadiusCm = CrownRadiusCm;
		Source.StemCount = StemCount;
		Source.RecordCellIndex = RecordCellIndex;
		Source.SourceIndex = SourceIndex;
		OutSourcesByOwnerChunk[OwnerChunkIndex].Add(OutSources.Num() - 1);
		return true;
	}

	/**
	 * Crown occupancy of every live record, spread through the separable kernel when OutCellOccupancy is requested.
	 * OutOwnRootedOccupancy, when asked for, collects the share each INDIVIDUALIZING record puts into its own
	 * record cell, through CalculateOwnCellCrownShare, the same function the thinning pass subtracts with;
	 * patches reach OutCellOccupancy only, so they shade without entering the thinning budget. Unlike
	 * OutCellOccupancy it is left unclamped so a removal subtracts exactly what the record deposited, and it
	 * stays one-dimensional because only its per-cell sum is ever read.
	 */
	bool AccumulateCanopyOccupancy(
		const FArcForestRunInput& Input,
		const TConstArrayView<uint8> BaseValidityMask,
		const TArray<FArcForestFrozenSpeciesSetup>& Species,
		const TMap<FGuid, int32>& SpeciesIndices,
		const FArcForestWorldSnapshot& Snapshot,
		const FArcEcotopeChunkLayout& ChunkLayout,
		const TArray<TArray<int32>>* TileGatherSourceOwnerChunksByDestination,
		const TArray<double>& RecordCellAreas,
		const FCanopyOccupancyPhaseNames& PhaseNames,
		UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics,
		FStratifiedOccupancy* OutCellOccupancy,
		TArray<double>* OutOwnRootedOccupancy,
		FString& OutError)
	{
		const FArcEcotopeSiteMetadata& Site = GetSiteMetadata(Input);
		const int32 RecordCellCount = Site.GetLevelCellCount(Site.RecordGridLevel);
		if (OutCellOccupancy != nullptr)
		{
			OutCellOccupancy->Reset(
				RecordCellCount, DefaultCanopyStratumCount, CalculateOccupancyColumnTopCm(Species));
		}
		if (OutOwnRootedOccupancy != nullptr)
		{
			OutOwnRootedOccupancy->Init(0.0, RecordCellCount);
		}
		const bool bGatherCellOccupancy = OutCellOccupancy != nullptr;
		if (bGatherCellOccupancy
			&& (TileGatherSourceOwnerChunksByDestination == nullptr
				|| TileGatherSourceOwnerChunksByDestination->Num() != ChunkLayout.GetChunkCount()))
		{
			OutError = TEXT("ArcForest TileGather adjacency is unavailable or does not match the chunk layout.");
			return false;
		}
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ArcForest_CanopyOccupancy_Individuals);
			const UE::ArcForest::FDiagnosticScopeTimer IndividualsTimer(Diagnostics, PhaseNames.Individuals);
			if (bGatherCellOccupancy)
			{
				TArray<FOccupancySource> Sources;
				TArray<FOccupancySourceBuildResult> SourceBuildResults;
				TArray<TArray<int32>> SourcesByOwnerChunk;
				Sources.SetNumUninitialized(Snapshot.Individuals.Num());
				SourceBuildResults.SetNumUninitialized(Snapshot.Individuals.Num());
				SourcesByOwnerChunk.SetNum(ChunkLayout.GetChunkCount());
				{
					const UE::ArcForest::FDiagnosticScopeTimer IndexTimer(Diagnostics, PhaseNames.IndividualsIndex);
					ParallelFor(
						Snapshot.Individuals.Num(),
						[&Input, &Site, BaseValidityMask, &Species, &SpeciesIndices, &Snapshot, &ChunkLayout, &Sources,
							&SourceBuildResults](const int32 SourceIndex)
						{
							FOccupancySourceBuildResult Result;
							const FArcForestIndividualRecord& Record = Snapshot.Individuals[SourceIndex];
							const int32* SpeciesIndex = SpeciesIndices.Find(Record.SpeciesId);
							if (SpeciesIndex == nullptr)
							{
								Result.Error = EOccupancySourceBuildError::SpeciesMissing;
								SourceBuildResults[SourceIndex] = Result;
								return;
							}
							int32 BaseCellIndex = INDEX_NONE;
							int32 RecordCellIndex = INDEX_NONE;
							if (!TryGetBaseAndRecordCellIndex(
									Site,
									BaseValidityMask,
									Record.LocalPosition,
									BaseCellIndex,
									RecordCellIndex))
							{
								Result.Error = EOccupancySourceBuildError::PositionOutsideSite;
								SourceBuildResults[SourceIndex] = Result;
								return;
							}
							if (!TryResolveOccupancyOwnerChunkIndex(
									Site, ChunkLayout, RecordCellIndex, Result.OwnerChunkIndex))
							{
								Result.Error = EOccupancySourceBuildError::OwnerChunkInvalid;
								SourceBuildResults[SourceIndex] = Result;
								return;
							}
							FOccupancySource Source;
							Source.SourceMinLocal = FVector2D(Record.LocalPosition.X, Record.LocalPosition.Y);
							Source.SourceSizeCm = FVector2D::ZeroVector;
							Source.HeightCm = static_cast<double>(Record.HeightCm);
							Source.CrownRadiusCm = CalculateCrownRadius(Record.HeightCm, Species[*SpeciesIndex]);
							Source.StemCount = 1.0;
							Source.RecordCellIndex = RecordCellIndex;
							Source.SourceIndex = SourceIndex;
							Sources[SourceIndex] = Source;
							SourceBuildResults[SourceIndex] = Result;
						});

					int32 LowestFailingSourceIndex = MAX_int32;
					EOccupancySourceBuildError LowestFailure = EOccupancySourceBuildError::None;
					for (int32 SourceIndex = 0; SourceIndex < SourceBuildResults.Num(); ++SourceIndex)
					{
						const FOccupancySourceBuildResult& Result = SourceBuildResults[SourceIndex];
						if (Result.Error != EOccupancySourceBuildError::None)
						{
							LowestFailingSourceIndex = SourceIndex;
							LowestFailure = Result.Error;
							break;
						}
					}
					if (LowestFailingSourceIndex != MAX_int32)
					{
						const TCHAR* FailureReason = LowestFailure == EOccupancySourceBuildError::SpeciesMissing
							? TEXT("references a species outside its frozen set")
							: LowestFailure == EOccupancySourceBuildError::PositionOutsideSite
								? TEXT("is outside its frozen site")
								: TEXT("has no valid source chunk owner");
						OutError = FString::Printf(
							TEXT("ArcForest TileGather individual source index %d %s."),
							LowestFailingSourceIndex,
							FailureReason);
						return false;
					}
					for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); ++SourceIndex)
					{
						SourcesByOwnerChunk[SourceBuildResults[SourceIndex].OwnerChunkIndex].Add(SourceIndex);
					}
				}
				{
					const UE::ArcForest::FDiagnosticScopeTimer TileGatherTimer(
						Diagnostics, PhaseNames.IndividualsTileGather);
					if (!AccumulateTileGatherCategory(
							Site,
							ChunkLayout,
							TEXT("individual"),
							*TileGatherSourceOwnerChunksByDestination,
							Sources,
							SourcesByOwnerChunk,
							RecordCellAreas,
							*OutCellOccupancy,
							OutError))
					{
						return false;
					}
				}
				if (OutOwnRootedOccupancy != nullptr)
				{
					for (const FOccupancySource& Source : Sources)
					{
						(*OutOwnRootedOccupancy)[Source.RecordCellIndex] += CalculateOwnCellCrownShare(
							Site,
							Source.SourceMinLocal,
							Source.SourceSizeCm,
							Source.CrownRadiusCm,
							Source.StemCount,
							Source.RecordCellIndex,
							RecordCellAreas);
					}
				}
			}
			else
			{
				for (const FArcForestIndividualRecord& Record : Snapshot.Individuals)
				{
					const int32* SpeciesIndex = SpeciesIndices.Find(Record.SpeciesId);
					int32 BaseCellIndex = INDEX_NONE;
					int32 RecordCellIndex = INDEX_NONE;
					if (SpeciesIndex == nullptr
						|| !TryGetBaseAndRecordCellIndex(
							Site,
							BaseValidityMask,
							Record.LocalPosition,
							BaseCellIndex,
							RecordCellIndex))
					{
						return false;
					}
					if (OutOwnRootedOccupancy != nullptr)
					{
						(*OutOwnRootedOccupancy)[RecordCellIndex] += CalculateOwnCellCrownShare(
							Site,
							FVector2D(Record.LocalPosition.X, Record.LocalPosition.Y),
							FVector2D::ZeroVector,
							CalculateCrownRadius(Record.HeightCm, Species[*SpeciesIndex]),
							1.0,
							RecordCellIndex,
							RecordCellAreas);
					}
				}
			}
		}
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ArcForest_CanopyOccupancy_Cohorts);
			const UE::ArcForest::FDiagnosticScopeTimer CohortsTimer(Diagnostics, PhaseNames.Cohorts);
			if (bGatherCellOccupancy)
			{
				TArray<FOccupancySource> Sources;
				TArray<TArray<int32>> SourcesByOwnerChunk;
				Sources.Reserve(Snapshot.Cohorts.Num());
				SourcesByOwnerChunk.SetNum(ChunkLayout.GetChunkCount());
				{
					const UE::ArcForest::FDiagnosticScopeTimer IndexTimer(Diagnostics, PhaseNames.CohortsIndex);
					for (int32 SourceIndex = 0; SourceIndex < Snapshot.Cohorts.Num(); ++SourceIndex)
					{
						const FArcForestCohortRecord& Record = Snapshot.Cohorts[SourceIndex];
						const int32* SpeciesIndex = SpeciesIndices.Find(Record.SpeciesId);
						FVector2D CellMinLocal = FVector2D::ZeroVector;
						FVector2D CellMaxLocal = FVector2D::ZeroVector;
						if (SpeciesIndex == nullptr)
						{
							OutError = TEXT("ArcForest cohort references a species outside its frozen set.");
							return false;
						}
						if (!Site.TryGetLevelCellRect(
								Record.CellIndex, Site.RecordGridLevel, CellMinLocal, CellMaxLocal)
							|| !AddOccupancySource(
								Site,
								ChunkLayout,
								CellMinLocal,
								CellMaxLocal - CellMinLocal,
								static_cast<double>(Record.MeanHeightCm),
								CalculateCrownRadius(Record.MeanHeightCm, Species[*SpeciesIndex]),
								static_cast<double>(Record.StemCount),
								Record.CellIndex,
								SourceIndex,
								Sources,
								SourcesByOwnerChunk,
								OutError))
						{
							if (OutError.IsEmpty())
							{
								OutError = TEXT("ArcForest cohort references a record cell outside its frozen site.");
							}
							return false;
						}
					}
				}
				{
					const UE::ArcForest::FDiagnosticScopeTimer TileGatherTimer(
						Diagnostics, PhaseNames.CohortsTileGather);
					if (!AccumulateTileGatherCategory(
							Site,
							ChunkLayout,
							TEXT("cohort"),
							*TileGatherSourceOwnerChunksByDestination,
							Sources,
							SourcesByOwnerChunk,
							RecordCellAreas,
							*OutCellOccupancy,
							OutError))
					{
						return false;
					}
				}
				if (OutOwnRootedOccupancy != nullptr)
				{
					for (const FOccupancySource& Source : Sources)
					{
						(*OutOwnRootedOccupancy)[Source.RecordCellIndex] += CalculateOwnCellCrownShare(
							Site,
							Source.SourceMinLocal,
							Source.SourceSizeCm,
							Source.CrownRadiusCm,
							Source.StemCount,
							Source.RecordCellIndex,
							RecordCellAreas);
					}
				}
			}
			else
			{
				for (const FArcForestCohortRecord& Record : Snapshot.Cohorts)
				{
					const int32* SpeciesIndex = SpeciesIndices.Find(Record.SpeciesId);
					FVector2D CellMinLocal = FVector2D::ZeroVector;
					FVector2D CellMaxLocal = FVector2D::ZeroVector;
					if (SpeciesIndex == nullptr)
					{
						OutError = TEXT("ArcForest cohort references a species outside its frozen set.");
						return false;
					}
					if (!Site.TryGetLevelCellRect(
							Record.CellIndex, Site.RecordGridLevel, CellMinLocal, CellMaxLocal))
					{
						OutError = TEXT("ArcForest cohort references a record cell outside its frozen site.");
						return false;
					}
					if (OutOwnRootedOccupancy != nullptr)
					{
						(*OutOwnRootedOccupancy)[Record.CellIndex] += CalculateOwnCellCrownShare(
							Site,
							CellMinLocal,
							CellMaxLocal - CellMinLocal,
							CalculateCrownRadius(Record.MeanHeightCm, Species[*SpeciesIndex]),
							static_cast<double>(Record.StemCount),
							Record.CellIndex,
							RecordCellAreas);
					}
				}
			}
		}
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ArcForest_CanopyOccupancy_Patches);
			const UE::ArcForest::FDiagnosticScopeTimer PatchesTimer(Diagnostics, PhaseNames.Patches);
			if (bGatherCellOccupancy)
			{
				TArray<FOccupancySource> Sources;
				TArray<TArray<int32>> SourcesByOwnerChunk;
				Sources.Reserve(Snapshot.Patches.Num());
				SourcesByOwnerChunk.SetNum(ChunkLayout.GetChunkCount());
				{
					const UE::ArcForest::FDiagnosticScopeTimer IndexTimer(Diagnostics, PhaseNames.PatchesIndex);
					for (int32 SourceIndex = 0; SourceIndex < Snapshot.Patches.Num(); ++SourceIndex)
					{
						const FArcForestPatchRecord& Record = Snapshot.Patches[SourceIndex];
						const int32* SpeciesIndex = SpeciesIndices.Find(Record.SpeciesId);
						FVector2D CellMinLocal = FVector2D::ZeroVector;
						FVector2D CellMaxLocal = FVector2D::ZeroVector;
						if (SpeciesIndex == nullptr)
						{
							OutError = TEXT("ArcForest patch references a species outside its frozen set.");
							return false;
						}
						if (!Site.TryGetLevelCellRect(
								Record.CellIndex, Site.RecordGridLevel, CellMinLocal, CellMaxLocal))
						{
							OutError = TEXT("ArcForest patch references a record cell outside its frozen site.");
							return false;
						}
						const double WeightedEquivalent = static_cast<double>(Record.StemOrCoverEquivalent)
							* static_cast<double>(Species[*SpeciesIndex].CrownOccupancyWeight);
						if (!AddOccupancySource(
								Site,
								ChunkLayout,
								CellMinLocal,
								CellMaxLocal - CellMinLocal,
								static_cast<double>(Record.MeanHeightCm),
								CalculateCrownRadius(Record.MeanHeightCm, Species[*SpeciesIndex]),
								WeightedEquivalent,
								Record.CellIndex,
								SourceIndex,
								Sources,
								SourcesByOwnerChunk,
								OutError))
						{
							return false;
						}
					}
				}
				{
					const UE::ArcForest::FDiagnosticScopeTimer TileGatherTimer(
						Diagnostics, PhaseNames.PatchesTileGather);
					if (!AccumulateTileGatherCategory(
							Site,
							ChunkLayout,
							TEXT("patch"),
							*TileGatherSourceOwnerChunksByDestination,
							Sources,
							SourcesByOwnerChunk,
							RecordCellAreas,
							*OutCellOccupancy,
							OutError))
					{
						return false;
					}
				}
				if (OutOwnRootedOccupancy != nullptr)
				{
					for (const FOccupancySource& Source : Sources)
					{
						const FArcForestPatchRecord& Record = Snapshot.Patches[Source.SourceIndex];
						const int32* SpeciesIndex = SpeciesIndices.Find(Record.SpeciesId);
						if (SpeciesIndex != nullptr
							&& Species[*SpeciesIndex].bIndividualizes)
						{
							(*OutOwnRootedOccupancy)[Source.RecordCellIndex] += CalculateOwnCellCrownShare(
								Site,
								Source.SourceMinLocal,
								Source.SourceSizeCm,
								Source.CrownRadiusCm,
								Source.StemCount,
								Source.RecordCellIndex,
								RecordCellAreas);
						}
					}
				}
			}
			else
			{
				for (const FArcForestPatchRecord& Record : Snapshot.Patches)
				{
					const int32* SpeciesIndex = SpeciesIndices.Find(Record.SpeciesId);
					FVector2D CellMinLocal = FVector2D::ZeroVector;
					FVector2D CellMaxLocal = FVector2D::ZeroVector;
					if (SpeciesIndex == nullptr)
					{
						OutError = TEXT("ArcForest patch references a species outside its frozen set.");
						return false;
					}
					if (!Site.TryGetLevelCellRect(
							Record.CellIndex, Site.RecordGridLevel, CellMinLocal, CellMaxLocal))
					{
						OutError = TEXT("ArcForest patch references a record cell outside its frozen site.");
						return false;
					}
					// For a patch the weight is a pure light multiplier: no patch crown reaches the
					// own-rooted budget, so a patch culls nobody through self-thinning.
					const double WeightedEquivalent = static_cast<double>(Record.StemOrCoverEquivalent)
						* static_cast<double>(Species[*SpeciesIndex].CrownOccupancyWeight);
					if (OutOwnRootedOccupancy != nullptr
						&& Species[*SpeciesIndex].bIndividualizes)
					{
						(*OutOwnRootedOccupancy)[Record.CellIndex] += CalculateOwnCellCrownShare(
							Site,
							CellMinLocal,
							CellMaxLocal - CellMinLocal,
							CalculateCrownRadius(Record.MeanHeightCm, Species[*SpeciesIndex]),
							WeightedEquivalent,
							Record.CellIndex,
							RecordCellAreas);
					}
				}
			}
		}
		return true;
	}

	bool BuildCanopyAndHabitat(
		const FArcForestRunInput& Input,
		const FValidBaseCellTable& ValidBaseCells,
		const FRegionalRecruitmentRecordIndex& RegionalRecordIndex,
		const TArray<FArcForestFrozenSpeciesSetup>& Species,
		const TMap<FGuid, int32>& SpeciesIndices,
		const FArcEcotopeChunkLayout& ChunkLayout,
		const TArray<TArray<int32>>& TileGatherSourceOwnerChunksByDestination,
		const FArcEcotopeDecayGrid& DecayGrid,
		const TArray<double>& RecordCellAreas,
		const FHabitatScoringState& HabitatTables,
		const FArcEcotopeChunkLayout& ReaderLayout,
		const FArcForestWorldSnapshot& Snapshot,
		const int32 Year,
		const uint64 HabitatGenerationToken,
		const FArcForestCancellationToken& Cancellation,
		UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics,
		const FArcForestHabitatFactorStore& FactoredHabitatStore,
		FFactoredHabitatRunTelemetry& InOutFactoredTelemetry,
		FStratifiedOccupancy& OutCellOccupancy,
		TArray<double>* OutOwnRootedOccupancy,
		FCoverHabitatProviderRunTelemetry& InOutCoverTelemetry,
		TArray<double>& OutCoverFloorCapacity,
		TArray<double>& OutCoverSuitability,
		FString& OutError)
	{
		const FArcEcotopeSiteMetadata& SiteMetadata = GetSiteMetadata(Input);
		const int32 CellCount = SiteMetadata.GetCellCount();
		const TConstArrayView<uint8> BaseValidityMask = ValidBaseCells.BaseValidityMask;
		if (!AccumulateCanopyOccupancy(
				Input,
				BaseValidityMask,
				Species,
				SpeciesIndices,
				Snapshot,
				ChunkLayout,
				&TileGatherSourceOwnerChunksByDestination,
				RecordCellAreas,
				CanopyBuildPhaseNames,
				Diagnostics,
				&OutCellOccupancy,
				OutOwnRootedOccupancy,
				OutError))
		{
			return false;
		}
		if (HabitatTables.BaseCellCount != CellCount
			|| HabitatTables.Scoring.SpeciesResponses.Num() != Species.Num())
		{
			OutError = TEXT("ArcForest habitat scoring tables are inconsistent.");
			return false;
		}
		if (!FactoredHabitatStore.IsBuilt())
		{
			OutError = TEXT("ArcForest factored habitat store was not built before annual evaluation.");
			return false;
		}
		FFactoredHabitatRunTelemetry UpdatedFactoredTelemetry = InOutFactoredTelemetry;
		FFusedCoverBatchContext CoverContext;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, CanopyAndHabitat_HabitatGrid);
			if (!CoverContext.Initialize(
					Input,
					ValidBaseCells,
					RegionalRecordIndex,
					Species,
					SpeciesIndices,
					OutCellOccupancy,
					DecayGrid,
					Snapshot,
					OutCoverFloorCapacity,
					OutCoverSuitability,
					OutError))
			{
				return false;
			}
			if (!EvaluateFactoredHabitat(
					Input,
					SiteMetadata,
					HabitatTables,
					ReaderLayout,
					HabitatGenerationToken,
					FactoredHabitatStore,
					Cancellation,
					CoverContext,
					UpdatedFactoredTelemetry,
					OutError))
			{
				return false;
			}
			if (!CoverContext.Finalize(InOutCoverTelemetry, OutError))
			{
				return false;
			}
		}
		if (!TryAddCapacityValue(
				UpdatedFactoredTelemetry.FactorPrimaryYears,
				1,
				UpdatedFactoredTelemetry.FactorPrimaryYears))
		{
			OutError = TEXT("ArcForest habitat production run telemetry overflowed.");
			return false;
		}
		InOutFactoredTelemetry = MoveTemp(UpdatedFactoredTelemetry);
		return true;
	}

	double CalculateBeerLambertLight(const double Occupancy, const double UnderstoryShade)
	{
		return FMath::Exp(-UnderstoryShade * FMath::Min(FMath::Max(Occupancy, 0.0), 50.0));
	}

	/**
	 * Light reaching a receiver at HeightCm: only the crown mass standing above its own crown base
	 * shades it, so a canopy stem misses the understory it sits over while a seedling sums the column.
	 */
	double CalculateLightAtHeight(
		const FStratifiedOccupancy& CellOccupancy,
		const int32 RecordCellIndex,
		const double HeightCm,
		const FArcForestFrozenSpeciesSetup& Species,
		const double UnderstoryShade)
	{
		const double CrownBaseCm =
			HeightCm - CalculateCrownDepthCm(HeightCm, CalculateCrownRadius(HeightCm, Species));
		return CalculateBeerLambertLight(
			CellOccupancy.AboveHeight(RecordCellIndex, CrownBaseCm), UnderstoryShade);
	}

	/** Applies an already evaluated light response while preserving the resource-factor clamp. */
	/** Applies an already evaluated light response while preserving the resource-factor clamp. */
	FORCEINLINE double CalculateResourceFactorFromQuantum(
		const uint32 HabitatQuantum,
		const FArcForestFrozenSpeciesSetup& Species,
		const double Light)
	{
		return FMath::Clamp(
			static_cast<double>(HabitatQuantum) / ProbabilityQuantum
				* EvaluateResponse(Species.LightResponse, static_cast<float>(Light)),
			0.0,
			1.0);
	}

	/** Habitat carries the site-only score, so the receiver's own light response is applied here. */
	/** Stem counts in equal deciles of one species' own MaxHeightCm, over individuals and cohort stems alike. */
	struct FSpeciesHeightCensus
	{
		double MaxHeightCm = 0.0;
		int64 IndividualStems = 0;
		int64 CohortStems = 0;
		int64 StemsAboveThreshold = 0;
		int64 BucketStems[HeightCensusBucketCount] = {};
	};

	/**
	 * Mean within-record-cell Spearman rank correlation of (height, vigor) for one species.
	 * MeanCorrelation is -2.0 when unmeasured, because -1.0 is a legal correlation.
	 */
	struct FSpeciesHeightVigorCorrelation
	{
		double MeanCorrelation = -2.0;
		int32 QualifyingCellCount = 0;
		int32 QualifyingSampleCount = 0;
		int32 QualifyingIndividualSampleCount = 0;
		int32 QualifyingCohortSampleCount = 0;
		double IndividualMeanCorrelation = -2.0;
		int32 IndividualQualifyingCellCount = 0;
		int32 IndividualQualifyingSampleCount = 0;
	};

	struct FSpeciesSizeAgeStructure
	{
		int64 DbhBucketStems[HeightCensusBucketCount] = {};
		int64 AgeBucketStems[HeightCensusBucketCount] = {};
		int64 StemsPerCohortBucketRecords[HeightCensusBucketCount] = {};
		int64 BiomassBucketRecords[HeightCensusBucketCount] = {};
		double LargestDbhCm = -1.0;
		uint64 LargestDbhId = 0;
		double OldestAgeYears = -1.0;
		uint64 OldestId = 0;
		double TallestAgeYears = -1.0;
	};

	struct FSpeciesGrowthRate
	{
		double HeightIncrementSum = 0.0;
		double HeightIncrementMin = TNumericLimits<double>::Max();
		double HeightIncrementMax = TNumericLimits<double>::Lowest();
		int64 HeightIncrementSamples = 0;
		double HeightAchievedFractionSum[HeightCensusBucketCount] = {};
		int64 HeightAchievedFractionSamples[HeightCensusBucketCount] = {};
		double DbhIncrementSum = 0.0;
		double DbhIncrementMin = TNumericLimits<double>::Max();
		double DbhIncrementMax = TNumericLimits<double>::Lowest();
		int64 DbhIncrementSamples = 0;
		double DbhAchievedFractionSum[HeightCensusBucketCount] = {};
		int64 DbhAchievedFractionSamples[HeightCensusBucketCount] = {};
	};

	struct FSpeciesGrowthDrivers
	{
		TArray<double> ResourceSamples;
		TArray<double> LightAtCrownBaseSamples;
		double VigorSum = 0.0;
		int64 VigorSamples = 0;
	};

	/** Per-batch growth diagnostics, merged in batch order after the join so the sums do not depend on thread order. */
	struct FGrowthDiagnosticsBatch
	{
		TArray<FSpeciesGrowthRate> GrowthRate;
		TArray<FSpeciesGrowthDrivers> GrowthDrivers;
	};

	void MergeGrowthRate(const FSpeciesGrowthRate& Source, FSpeciesGrowthRate& Target)
	{
		Target.HeightIncrementSum += Source.HeightIncrementSum;
		Target.HeightIncrementMin = FMath::Min(Target.HeightIncrementMin, Source.HeightIncrementMin);
		Target.HeightIncrementMax = FMath::Max(Target.HeightIncrementMax, Source.HeightIncrementMax);
		Target.HeightIncrementSamples += Source.HeightIncrementSamples;
		Target.DbhIncrementSum += Source.DbhIncrementSum;
		Target.DbhIncrementMin = FMath::Min(Target.DbhIncrementMin, Source.DbhIncrementMin);
		Target.DbhIncrementMax = FMath::Max(Target.DbhIncrementMax, Source.DbhIncrementMax);
		Target.DbhIncrementSamples += Source.DbhIncrementSamples;
		for (int32 Bucket = 0; Bucket < HeightCensusBucketCount; ++Bucket)
		{
			Target.HeightAchievedFractionSum[Bucket] += Source.HeightAchievedFractionSum[Bucket];
			Target.HeightAchievedFractionSamples[Bucket] += Source.HeightAchievedFractionSamples[Bucket];
			Target.DbhAchievedFractionSum[Bucket] += Source.DbhAchievedFractionSum[Bucket];
			Target.DbhAchievedFractionSamples[Bucket] += Source.DbhAchievedFractionSamples[Bucket];
		}
	}

	void MergeGrowthDiagnosticsBatches(
		const TArray<FGrowthDiagnosticsBatch>& Batches,
		TArray<FSpeciesGrowthRate>& InOutGrowthRate,
		TArray<FSpeciesGrowthDrivers>& InOutGrowthDrivers)
	{
		for (const FGrowthDiagnosticsBatch& Batch : Batches)
		{
			for (int32 SpeciesIndex = 0; SpeciesIndex < Batch.GrowthRate.Num(); ++SpeciesIndex)
			{
				if (InOutGrowthRate.IsValidIndex(SpeciesIndex))
				{
					MergeGrowthRate(Batch.GrowthRate[SpeciesIndex], InOutGrowthRate[SpeciesIndex]);
				}
			}
			for (int32 SpeciesIndex = 0; SpeciesIndex < Batch.GrowthDrivers.Num(); ++SpeciesIndex)
			{
				if (!InOutGrowthDrivers.IsValidIndex(SpeciesIndex))
				{
					continue;
				}
				const FSpeciesGrowthDrivers& Source = Batch.GrowthDrivers[SpeciesIndex];
				FSpeciesGrowthDrivers& Target = InOutGrowthDrivers[SpeciesIndex];
				Target.ResourceSamples.Reserve(Target.ResourceSamples.Num() + Source.ResourceSamples.Num());
				Target.ResourceSamples.Append(Source.ResourceSamples);
				Target.LightAtCrownBaseSamples.Reserve(
					Target.LightAtCrownBaseSamples.Num() + Source.LightAtCrownBaseSamples.Num());
				Target.LightAtCrownBaseSamples.Append(Source.LightAtCrownBaseSamples);
				Target.VigorSum += Source.VigorSum;
				Target.VigorSamples += Source.VigorSamples;
			}
		}
	}

	struct FSpeciesMortalityBreakdown
	{
		int64 BaselineStems[HeightCensusBucketCount] = {};
		int64 StressStems[HeightCensusBucketCount] = {};
		int64 OldAgeStems[HeightCensusBucketCount] = {};
		int64 MobilityStems[HeightCensusBucketCount] = {};
		int64 DisturbanceStems[HeightCensusBucketCount] = {};
		int64 SelfThinningStems[HeightCensusBucketCount] = {};
		int64 CoverFloorEvictionStems[HeightCensusBucketCount] = {};
	};

	enum class EMortalityCause : uint8
	{
		Baseline,
		OldAge,
		Stress,
		Mobility,
	};

	EMortalityCause SelectMortalityCause(
		const double BaselineMortality,
		const double AgeMortality,
		const double ResourceMortality,
		const double MobilityMortality)
	{
		EMortalityCause Cause = EMortalityCause::Baseline;
		double Largest = BaselineMortality;
		if (AgeMortality > Largest)
		{
			Largest = AgeMortality;
			Cause = EMortalityCause::OldAge;
		}
		if (ResourceMortality > Largest)
		{
			Largest = ResourceMortality;
			Cause = EMortalityCause::Stress;
		}
		if (MobilityMortality > Largest)
		{
			Cause = EMortalityCause::Mobility;
		}
		return Cause;
	}

	void AccumulateMortalityCause(
		FSpeciesMortalityBreakdown& Breakdown,
		const EMortalityCause Cause,
		const int32 HeightBucket,
		const int64 Stems)
	{
		switch (Cause)
		{
		case EMortalityCause::Baseline:
			Breakdown.BaselineStems[HeightBucket] += Stems;
			break;
		case EMortalityCause::OldAge:
			Breakdown.OldAgeStems[HeightBucket] += Stems;
			break;
		case EMortalityCause::Stress:
			Breakdown.StressStems[HeightBucket] += Stems;
			break;
		case EMortalityCause::Mobility:
			Breakdown.MobilityStems[HeightBucket] += Stems;
			break;
		}
	}

	struct FRecruitmentPromotionFlow
	{
		int64 CohortsCrossingIndividualization = 0;
		int64 PromotionsGranted = 0;
		int64 PromotionsRefusedChannelBuffer = 0;
		int64 SaturatedMultiStemSkipped = 0;
		int64 RecruitmentGenerated = 0;
		int64 RecruitmentAdmitted = 0;
		int64 RecruitmentRefusedRecordSlot = 0;
		int64 RecruitmentRefusedLiveStem = 0;
		int64 RecruitmentRefusedCoverFloorHeadroom = 0;
		int64 RecruitmentRefusedCoverSaturated = 0;
		int64 RecruitmentRefusedHabitatZero = 0;
	};

	/** The tallest-* factors are recomputed pre-growth, so they equal what ApplyGrowth will use. Fields left at -1.0 were not measured. */
	struct FYearDiagnostics
	{
		int32 Year = 0;
		int32 RecordGridLevel = 0;
		double RecordCellEdgeMeters = 0.0;
		int32 ValidRecordCellCount = 0;
		int32 TotalRecordCellCount = 0;
		int32 ValidBaseCellCount = 0;
		int32 TotalBaseCellCount = 0;

		double LiveBiomassKgPerHectare = -1.0;
		double IndividualBiomassKgPerHectare = -1.0;
		double CohortBiomassKgPerHectare = -1.0;
		double PatchBiomassKgPerHectare = -1.0;

		double MinOccupancy = 0.0;
		double MeanOccupancy = 0.0;
		double MaxOccupancy = 0.0;
		double MeanCompetition = 0.0;

		/** Own-rooted share at year start, before growth; ApplySelfThinning solves against its own later rebuild. */
		double MinOwnRootedOccupancy = -1.0;
		double MeanOwnRootedOccupancy = -1.0;
		double MaxOwnRootedOccupancy = -1.0;
		int32 OwnRootedOccupancyCellCount = -1;

		int32 HabitatSpeciesIndex = INDEX_NONE;
		/** The stored site-only score, which carries no light term at all. */
		double MinHabitat = -1.0;
		double MeanHabitat = -1.0;
		double MaxHabitat = -1.0;

		/** The same score with light re-applied at MinimumPlantHeightCm, the height a recruit is scored at. */
		double MinRecruitHabitat = -1.0;
		double MeanRecruitHabitat = -1.0;
		double MaxRecruitHabitat = -1.0;

		int32 LiveIndividualCount = 0;
		double MeanIndividualHeightCm = 0.0;
		int32 TallestIndex = INDEX_NONE;
		uint64 TallestId = 0;
		double TallestHeightCm = -1.0;
		double TallestResource = -1.0;
		double TallestSizeLimitation = -1.0;
		double TallestGrowthFactor = -1.0;
		double TallestIncrementCm = -1.0;
		double TallestClampedDeltaCm = -1.0;
		double TallestAppliedDeltaCm = -1.0;

		int32 CoverBaseCellCount = -1;
		double MinBaseCover = -1.0;
		double MeanBaseCover = -1.0;
		double MaxBaseCover = -1.0;
		double BaseCoverAboveOneFraction = -1.0;
		int32 CrownSpanClampedAxisCount = -1;
		int32 CrownLightSampleCount = -1;
		int32 CrownLightRatioSampleCount = -1;
		double MeanRecordLight = -1.0;
		double MeanBaseLight = -1.0;
		double MeanLightRatio = -1.0;
		double TallestRecordLight = -1.0;
		double TallestBaseLight = -1.0;
		double TallestBaseCover = -1.0;
		TArray<FSpeciesHeightCensus> HeightCensus;
		TArray<FSpeciesHeightVigorCorrelation> HeightVigorCorrelation;
		int64 CohortStemsBelowIndividualization = -1;
		int32 CohortRecordsBelowIndividualization = -1;
		double CohortBelowMeanRecordLight = -1.0;
		double CohortBelowMeanBaseCover = -1.0;

		int32 IndividualCount = 0;
		int32 CohortCount = 0;
		int32 PatchCount = 0;
		int32 DeadwoodCount = 0;

		TArray<FSpeciesSizeAgeStructure> SizeAgeStructure;
		TArray<FSpeciesGrowthRate> GrowthRate;
		TArray<FSpeciesGrowthDrivers> GrowthDrivers;
		TArray<FSpeciesMortalityBreakdown> MortalityBreakdown;
		FRecruitmentPromotionFlow PromotionRecruitmentFlow;
	};

	/** The cell set every per-cell diagnostic reduction runs over, so separate reductions stay comparable. */
	bool IsDiagnosticRecordCell(
		const FArcEcotopeDecayGrid& DecayGrid,
		const int32 RecordCellCount,
		const int32 RecordCellIndex)
	{
		return DecayGrid.CellAreaHectares.IsValidIndex(RecordCellIndex)
			&& DecayGrid.CellAreaHectares[RecordCellIndex] > 0.0
			&& DecayGrid.DecayModerator.IsValidIndex(RecordCellIndex)
			&& RecordCellIndex >= 0
			&& RecordCellIndex < RecordCellCount;
	}

	/**
	 * Runs after the canopy pass and before growth, so it observes exactly the inputs growth will consume.
	 *
	 * The sampled climate is base-cell-indexed, so ResponseInputSum below averages biased inputs over a
	 * mixture of climates: its mean is a spatial mean across the volume, not a property of one climate.
	 */
	void GatherYearDiagnostics(
		const FArcForestRunInput& Input,
		const TConstArrayView<uint8> BaseValidityMask,
		const FArcEcotopeDecayGrid& DecayGrid,
		const FArcForestWorldSnapshot& Snapshot,
		const FStratifiedOccupancy& CellOccupancy,
		const TArray<double>& OwnRootedOccupancy,
		const int32 Year,
		FYearDiagnostics& Out)
	{
		const FArcEcotopeSiteMetadata& Site = GetSiteMetadata(Input);
		const int32 RecordCellCount = Site.GetLevelCellCount(Site.RecordGridLevel);
		const int32 BaseCellCount = Site.GetCellCount();

		Out.Year = Year;
		Out.RecordGridLevel = static_cast<int32>(Site.RecordGridLevel);
		Out.RecordCellEdgeMeters = Site.GetLevelCellSizeCm(Site.RecordGridLevel) / 100.0;
		Out.TotalRecordCellCount = RecordCellCount;
		Out.TotalBaseCellCount = BaseCellCount;
		Out.IndividualCount = Snapshot.Individuals.Num();
		Out.CohortCount = Snapshot.Cohorts.Num();
		Out.PatchCount = Snapshot.Patches.Num();
		Out.DeadwoodCount = Snapshot.Deadwood.Num();

		TArray<uint8> RecordCellUsable;
		RecordCellUsable.Init(0, RecordCellCount);
		// The per-base-cell sweep below reads this ~1000 times per record cell.
		TArray<double> RecordCellRecruitLight;
		RecordCellRecruitLight.Init(0.0, RecordCellCount);

		double AreaHectaresSum = 0.0;
		double OccupancySum = 0.0;
		double OccupancyMin = TNumericLimits<double>::Max();
		double OccupancyMax = TNumericLimits<double>::Lowest();
		double OwnRootedSum = 0.0;
		double OwnRootedMin = TNumericLimits<double>::Max();
		double OwnRootedMax = TNumericLimits<double>::Lowest();
		int32 OwnRootedCells = 0;
		double CompetitionSum = 0.0;
		int32 ValidRecordCells = 0;
		for (int32 RecordCellIndex = 0; RecordCellIndex < RecordCellCount; ++RecordCellIndex)
		{
			if (!IsDiagnosticRecordCell(
					DecayGrid, CellOccupancy.GetCellCount(), RecordCellIndex))
			{
				continue;
			}
			AreaHectaresSum += DecayGrid.CellAreaHectares[RecordCellIndex];
			RecordCellUsable[RecordCellIndex] = 1;

			const double Occupancy = CellOccupancy.TotalAt(RecordCellIndex);
			RecordCellRecruitLight[RecordCellIndex] = CalculateBeerLambertLight(
				CellOccupancy.AboveHeight(RecordCellIndex, static_cast<double>(MinimumPlantHeightCm)),
				Input.UnderstoryShade);
			OccupancySum += Occupancy;
			OccupancyMin = FMath::Min(OccupancyMin, Occupancy);
			OccupancyMax = FMath::Max(OccupancyMax, Occupancy);
			CompetitionSum += 1.0 / (1.0 + Occupancy);
			if (OwnRootedOccupancy.IsValidIndex(RecordCellIndex))
			{
				const double OwnRooted = OwnRootedOccupancy[RecordCellIndex];
				OwnRootedSum += OwnRooted;
				OwnRootedMin = FMath::Min(OwnRootedMin, OwnRooted);
				OwnRootedMax = FMath::Max(OwnRootedMax, OwnRooted);
				++OwnRootedCells;
			}
			++ValidRecordCells;
		}

		Out.ValidRecordCellCount = ValidRecordCells;
		if (ValidRecordCells > 0)
		{
			const double ValidRecordCellsAsDouble = static_cast<double>(ValidRecordCells);
			Out.MinOccupancy = OccupancyMin;
			Out.MeanOccupancy = OccupancySum / ValidRecordCellsAsDouble;
			Out.MaxOccupancy = OccupancyMax;
			Out.MeanCompetition = CompetitionSum / ValidRecordCellsAsDouble;
		}
		Out.OwnRootedOccupancyCellCount = OwnRootedCells;
		if (OwnRootedCells > 0)
		{
			Out.MinOwnRootedOccupancy = OwnRootedMin;
			Out.MeanOwnRootedOccupancy = OwnRootedSum / static_cast<double>(OwnRootedCells);
			Out.MaxOwnRootedOccupancy = OwnRootedMax;
		}

		double HeightSum = 0.0;
		double IndividualBiomassSum = 0.0;
		double TallestHeightCm = 0.0;
		int32 TallestIndex = INDEX_NONE;
		int32 LiveCount = 0;
		for (int32 RecordIndex = 0; RecordIndex < Snapshot.Individuals.Num(); ++RecordIndex)
		{
			const FArcForestIndividualRecord& Record = Snapshot.Individuals[RecordIndex];
			if (Record.LifeStatus != EArcForestLifeStatus::Alive)
			{
				continue;
			}
			HeightSum += static_cast<double>(Record.HeightCm);
			IndividualBiomassSum += static_cast<double>(Record.BiomassKg);
			++LiveCount;
			if (TallestIndex == INDEX_NONE || static_cast<double>(Record.HeightCm) > TallestHeightCm)
			{
				TallestIndex = RecordIndex;
				TallestHeightCm = static_cast<double>(Record.HeightCm);
			}
		}

		double CohortBiomassSum = 0.0;
		for (const FArcForestCohortRecord& Record : Snapshot.Cohorts)
		{
			CohortBiomassSum += static_cast<double>(Record.BiomassKg);
		}
		double PatchBiomassSum = 0.0;
		for (const FArcForestPatchRecord& Record : Snapshot.Patches)
		{
			PatchBiomassSum += static_cast<double>(Record.BiomassKg);
		}

		Out.LiveIndividualCount = LiveCount;
		Out.MeanIndividualHeightCm = LiveCount > 0 ? HeightSum / static_cast<double>(LiveCount) : 0.0;
		if (AreaHectaresSum > 0.0)
		{
			Out.IndividualBiomassKgPerHectare = IndividualBiomassSum / AreaHectaresSum;
			Out.CohortBiomassKgPerHectare = CohortBiomassSum / AreaHectaresSum;
			Out.PatchBiomassKgPerHectare = PatchBiomassSum / AreaHectaresSum;
			Out.LiveBiomassKgPerHectare =
				(IndividualBiomassSum + CohortBiomassSum + PatchBiomassSum) / AreaHectaresSum;
		}

		if (TallestIndex != INDEX_NONE)
		{
			const FArcForestIndividualRecord& Tallest = Snapshot.Individuals[TallestIndex];
			Out.TallestIndex = TallestIndex;
			Out.TallestId = Tallest.Id;
			Out.TallestHeightCm = TallestHeightCm;

		}

		int32 ValidBaseCells = 0;
		for (int32 BaseCellIndex = 0; BaseCellIndex < BaseCellCount; ++BaseCellIndex)
		{
			if (BaseValidityMask.IsValidIndex(BaseCellIndex)
				&& BaseValidityMask[BaseCellIndex] != 0)
			{
				const int32 RecordCellIndex = GetRecordCellIndexForBaseCell(Site, BaseCellIndex);
				if (RecordCellUsable.IsValidIndex(RecordCellIndex)
					&& RecordCellUsable[RecordCellIndex] != 0)
				{
					++ValidBaseCells;
				}
			}
		}
		Out.ValidBaseCellCount = ValidBaseCells;
	}

	/** The decile a height falls in against MaxHeightCm; the top decile includes MaxHeightCm itself. */
	int32 CalculateStructureHeightBucket(const double HeightCm, const double MaxHeightCm)
	{
		const double NormalizedHeight = MaxHeightCm > 0.0
			? FMath::Clamp(HeightCm / MaxHeightCm, 0.0, 1.0)
			: 0.0;
		return FMath::Min(
			static_cast<int32>(NormalizedHeight * static_cast<double>(HeightCensusBucketCount)),
			HeightCensusBucketCount - 1);
	}

	/** Log2 ladder 1,2,4,...,256,512+ so a handful of stems per cohort resolves without an authored knob. */
	int32 CalculateStemsPerRecordBucket(const int64 StemCount)
	{
		if (StemCount <= 1)
		{
			return 0;
		}
		const int32 Ladder = static_cast<int32>(FMath::FloorLog2_64(static_cast<uint64>(StemCount)));
		return FMath::Min(Ladder, HeightCensusBucketCount - 1);
	}

	/** Charges every stem of one record to the decile its height falls in; the top decile includes MaxHeightCm itself. */
	void AccumulateHeightCensus(
		const double HeightCm,
		const int64 StemCount,
		FSpeciesHeightCensus& Out)
	{
		if (StemCount <= 0)
		{
			return;
		}
		const double NormalizedHeight = Out.MaxHeightCm > 0.0
			? FMath::Clamp(HeightCm / Out.MaxHeightCm, 0.0, 1.0)
			: 0.0;
		const int32 BucketIndex = FMath::Min(
			static_cast<int32>(NormalizedHeight * static_cast<double>(HeightCensusBucketCount)),
			HeightCensusBucketCount - 1);
		Out.BucketStems[BucketIndex] += StemCount;
		if (HeightCm >= HeightCensusThresholdCm)
		{
			Out.StemsAboveThreshold += StemCount;
		}
	}

	/**
	 * One (height, vigor) pair for the within-record-cell rank correlation. A cohort carries a single mean
	 * height and so cannot rank internally: it contributes one sample at its record mean, and patches, being
	 * cover-equivalents rather than stems, contribute none.
	 */
	struct FHeightVigorSample
	{
		int32 SpeciesIndex = INDEX_NONE;
		int32 RecordCellIndex = INDEX_NONE;
		double HeightCm = 0.0;
		double Vigor = 0.0;
		bool bIsIndividual = false;
	};

	constexpr int32 MinimumHeightVigorSamplesPerCell = 4;

	/** Average ("mid") ranks: a tied run shares its mean rank, which ordinal ranks would bias. */
	void AssignAverageRanks(const TArray<double>& Values, TArray<double>& OutRanks)
	{
		const int32 Count = Values.Num();
		TArray<int32> Order;
		Order.SetNumUninitialized(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Order[Index] = Index;
		}
		Order.Sort([&Values](const int32 A, const int32 B) -> bool { return Values[A] < Values[B]; });

		OutRanks.SetNumUninitialized(Count);
		int32 RunStart = 0;
		while (RunStart < Count)
		{
			int32 RunEnd = RunStart + 1;
			while (RunEnd < Count && Values[Order[RunEnd]] == Values[Order[RunStart]])
			{
				++RunEnd;
			}
			const double MeanRank = 0.5 * static_cast<double>(RunStart + RunEnd - 1);
			for (int32 Index = RunStart; Index < RunEnd; ++Index)
			{
				OutRanks[Order[Index]] = MeanRank;
			}
			RunStart = RunEnd;
		}
	}

	/** Fails when either variable is constant, so an undefined correlation is skipped instead of folded in as zero. */
	bool TryCalculateRankCorrelation(
		const TArray<double>& HeightValues,
		const TArray<double>& VigorValues,
		double& OutCorrelation)
	{
		const int32 Count = HeightValues.Num();
		if (Count < MinimumHeightVigorSamplesPerCell || VigorValues.Num() != Count)
		{
			return false;
		}
		TArray<double> HeightRanks;
		TArray<double> VigorRanks;
		AssignAverageRanks(HeightValues, HeightRanks);
		AssignAverageRanks(VigorValues, VigorRanks);

		// Mid-ranks average (Count - 1) / 2 whatever the tie structure.
		const double MeanRank = 0.5 * static_cast<double>(Count - 1);
		double Covariance = 0.0;
		double HeightVariance = 0.0;
		double VigorVariance = 0.0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double HeightDelta = HeightRanks[Index] - MeanRank;
			const double VigorDelta = VigorRanks[Index] - MeanRank;
			Covariance += HeightDelta * VigorDelta;
			HeightVariance += HeightDelta * HeightDelta;
			VigorVariance += VigorDelta * VigorDelta;
		}
		if (!(HeightVariance > 0.0) || !(VigorVariance > 0.0))
		{
			return false;
		}
		OutCorrelation = Covariance / FMath::Sqrt(HeightVariance * VigorVariance);
		return true;
	}

	/** Groups the gathered samples by (species, record cell) through one sort, then means the per-cell rho unweighted. */
	void FinalizeHeightVigorCorrelation(TArray<FHeightVigorSample>& Samples, FYearDiagnostics& Out)
	{
		Out.HeightVigorCorrelation.Reset();
		Out.HeightVigorCorrelation.SetNum(Out.HeightCensus.Num());
		if (Out.HeightVigorCorrelation.Num() <= 0 || Samples.Num() <= 0)
		{
			return;
		}

		Samples.Sort([](const FHeightVigorSample& A, const FHeightVigorSample& B) -> bool
			{
				if (A.SpeciesIndex != B.SpeciesIndex)
				{
					return A.SpeciesIndex < B.SpeciesIndex;
				}
				if (A.RecordCellIndex != B.RecordCellIndex)
				{
					return A.RecordCellIndex < B.RecordCellIndex;
				}
				if (A.HeightCm != B.HeightCm)
				{
					return A.HeightCm < B.HeightCm;
				}
				return A.Vigor < B.Vigor;
			});

		TArray<double> CorrelationSum;
		CorrelationSum.Init(0.0, Out.HeightVigorCorrelation.Num());
		TArray<double> IndividualCorrelationSum;
		IndividualCorrelationSum.Init(0.0, Out.HeightVigorCorrelation.Num());
		TArray<double> HeightValues;
		TArray<double> VigorValues;
		TArray<double> IndividualHeightValues;
		TArray<double> IndividualVigorValues;
		int32 RunStart = 0;
		while (RunStart < Samples.Num())
		{
			int32 RunEnd = RunStart + 1;
			while (RunEnd < Samples.Num()
				&& Samples[RunEnd].SpeciesIndex == Samples[RunStart].SpeciesIndex
				&& Samples[RunEnd].RecordCellIndex == Samples[RunStart].RecordCellIndex)
			{
				++RunEnd;
			}
			const int32 SpeciesIndex = Samples[RunStart].SpeciesIndex;
			const int32 RunCount = RunEnd - RunStart;
			int32 IndividualRunCount = 0;
			for (int32 Index = RunStart; Index < RunEnd; ++Index)
			{
				if (Samples[Index].bIsIndividual)
				{
					++IndividualRunCount;
				}
			}
			if (RunCount >= MinimumHeightVigorSamplesPerCell && CorrelationSum.IsValidIndex(SpeciesIndex))
			{
				HeightValues.Reset(RunCount);
				VigorValues.Reset(RunCount);
				for (int32 Index = RunStart; Index < RunEnd; ++Index)
				{
					HeightValues.Add(Samples[Index].HeightCm);
					VigorValues.Add(Samples[Index].Vigor);
				}
				double Correlation = 0.0;
				if (TryCalculateRankCorrelation(HeightValues, VigorValues, Correlation))
				{
					CorrelationSum[SpeciesIndex] += Correlation;
					++Out.HeightVigorCorrelation[SpeciesIndex].QualifyingCellCount;
					Out.HeightVigorCorrelation[SpeciesIndex].QualifyingSampleCount += RunCount;
					Out.HeightVigorCorrelation[SpeciesIndex].QualifyingIndividualSampleCount += IndividualRunCount;
					Out.HeightVigorCorrelation[SpeciesIndex].QualifyingCohortSampleCount +=
						RunCount - IndividualRunCount;
				}
			}
			// The minimum is against the individual count alone: cohort samples never qualify a cell here.
			if (IndividualRunCount >= MinimumHeightVigorSamplesPerCell
				&& IndividualCorrelationSum.IsValidIndex(SpeciesIndex))
			{
				IndividualHeightValues.Reset(IndividualRunCount);
				IndividualVigorValues.Reset(IndividualRunCount);
				for (int32 Index = RunStart; Index < RunEnd; ++Index)
				{
					if (Samples[Index].bIsIndividual)
					{
						IndividualHeightValues.Add(Samples[Index].HeightCm);
						IndividualVigorValues.Add(Samples[Index].Vigor);
					}
				}
				double IndividualCorrelation = 0.0;
				if (TryCalculateRankCorrelation(
					IndividualHeightValues, IndividualVigorValues, IndividualCorrelation))
				{
					IndividualCorrelationSum[SpeciesIndex] += IndividualCorrelation;
					++Out.HeightVigorCorrelation[SpeciesIndex].IndividualQualifyingCellCount;
					Out.HeightVigorCorrelation[SpeciesIndex].IndividualQualifyingSampleCount += IndividualRunCount;
				}
			}
			RunStart = RunEnd;
		}

		for (int32 SpeciesIndex = 0; SpeciesIndex < Out.HeightVigorCorrelation.Num(); ++SpeciesIndex)
		{
			FSpeciesHeightVigorCorrelation& Entry = Out.HeightVigorCorrelation[SpeciesIndex];
			if (Entry.QualifyingCellCount > 0)
			{
				Entry.MeanCorrelation =
					CorrelationSum[SpeciesIndex] / static_cast<double>(Entry.QualifyingCellCount);
			}
			if (Entry.IndividualQualifyingCellCount > 0)
			{
				Entry.IndividualMeanCorrelation =
					IndividualCorrelationSum[SpeciesIndex]
					/ static_cast<double>(Entry.IndividualQualifyingCellCount);
			}
		}
	}

	/**
	 * Rebuilds the canopy at base resolution in a scratch field, so cover at crown scale can be compared
	 * against cover on the record grid. Both lights here are taken over the whole column, which measures
	 * grid smearing and NOT the light any receiver sees - that is stratified by crown base. Both scratch
	 * arrays are owned by the caller and reused across years.
	 */
	void GatherCrownDiagnostics(
		const FArcForestRunInput& Input,
		const TConstArrayView<uint8> BaseValidityMask,
		const TArray<FArcForestFrozenSpeciesSetup>& Species,
		const TMap<FGuid, int32>& SpeciesIndices,
		const FArcForestWorldSnapshot& Snapshot,
		const FStratifiedOccupancy& CellOccupancy,
		TArray<double>& BaseCellAreas,
		TArray<double>& BaseCellCover,
		FYearDiagnostics& Out)
	{
		const FArcEcotopeSiteMetadata& Site = GetSiteMetadata(Input);
		const int32 BaseCellCount = Site.GetCellCount();
		if (BaseCellCount <= 0)
		{
			return;
		}

		Out.HeightCensus.SetNum(Species.Num());
		for (int32 CensusSpeciesIndex = 0; CensusSpeciesIndex < Species.Num(); ++CensusSpeciesIndex)
		{
			Out.HeightCensus[CensusSpeciesIndex].MaxHeightCm =
				static_cast<double>(Species[CensusSpeciesIndex].MaxHeightCm);
		}
		Out.SizeAgeStructure.SetNum(Species.Num());
		Out.GrowthRate.SetNum(Species.Num());
		Out.GrowthDrivers.SetNum(Species.Num());
		Out.MortalityBreakdown.SetNum(Species.Num());

		TArray<double> MaxBiomassKgBySpecies;
		MaxBiomassKgBySpecies.SetNumZeroed(Species.Num());
		for (int32 BiomassSpeciesIndex = 0; BiomassSpeciesIndex < Species.Num(); ++BiomassSpeciesIndex)
		{
			double MaxBiomassKg = 0.0;
			if (!TryCalculateBiomass(
					Species[BiomassSpeciesIndex],
					static_cast<double>(Species[BiomassSpeciesIndex].MaxDbhCm),
					MaxBiomassKg))
			{
				MaxBiomassKg = 0.0;
			}
			MaxBiomassKgBySpecies[BiomassSpeciesIndex] = MaxBiomassKg;
		}

		for (const FArcForestIndividualRecord& Record : Snapshot.Individuals)
		{
			const int32* SpeciesIndex = SpeciesIndices.Find(Record.SpeciesId);
			if (SpeciesIndex == nullptr
				|| Record.LifeStatus != EArcForestLifeStatus::Alive
				|| !Out.SizeAgeStructure.IsValidIndex(*SpeciesIndex))
			{
				continue;
			}
			FSpeciesSizeAgeStructure& Structure = Out.SizeAgeStructure[*SpeciesIndex];
			const FArcForestFrozenSpeciesSetup& Model = Species[*SpeciesIndex];
			const int32 DbhBucket = CalculateStructureHeightBucket(
				static_cast<double>(Record.DbhCm), static_cast<double>(Model.MaxDbhCm));
			++Structure.DbhBucketStems[DbhBucket];
			const int32 AgeBucket = CalculateStructureHeightBucket(
				static_cast<double>(Record.AgeYears), static_cast<double>(Model.MaxAgeYears));
			++Structure.AgeBucketStems[AgeBucket];
			const int32 BiomassBucket = CalculateStructureHeightBucket(
				static_cast<double>(Record.BiomassKg), MaxBiomassKgBySpecies[*SpeciesIndex]);
			++Structure.BiomassBucketRecords[BiomassBucket];
			if (static_cast<double>(Record.DbhCm) > Structure.LargestDbhCm)
			{
				Structure.LargestDbhCm = static_cast<double>(Record.DbhCm);
				Structure.LargestDbhId = Record.Id;
			}
			if (static_cast<double>(Record.AgeYears) > Structure.OldestAgeYears)
			{
				Structure.OldestAgeYears = static_cast<double>(Record.AgeYears);
				Structure.OldestId = Record.Id;
			}
			if (Record.Id == Out.TallestId)
			{
				Structure.TallestAgeYears = static_cast<double>(Record.AgeYears);
			}
		}
		for (const FArcForestCohortRecord& Record : Snapshot.Cohorts)
		{
			const int32* SpeciesIndex = SpeciesIndices.Find(Record.SpeciesId);
			if (SpeciesIndex == nullptr || !Out.SizeAgeStructure.IsValidIndex(*SpeciesIndex))
			{
				continue;
			}
			FSpeciesSizeAgeStructure& Structure = Out.SizeAgeStructure[*SpeciesIndex];
			const FArcForestFrozenSpeciesSetup& Model = Species[*SpeciesIndex];
			const int32 StemsBucket = CalculateStemsPerRecordBucket(Record.StemCount);
			++Structure.StemsPerCohortBucketRecords[StemsBucket];
			const double BiomassPerStemKg = Record.StemCount > 0
				? static_cast<double>(Record.BiomassKg) / static_cast<double>(Record.StemCount)
				: 0.0;
			const int32 BiomassBucket = CalculateStructureHeightBucket(
				BiomassPerStemKg, MaxBiomassKgBySpecies[*SpeciesIndex]);
			++Structure.BiomassBucketRecords[BiomassBucket];
			const int32 AgeBucket = CalculateStructureHeightBucket(
				static_cast<double>(Record.MeanAgeYears), static_cast<double>(Model.MaxAgeYears));
			Structure.AgeBucketStems[AgeBucket] += FMath::Max(Record.StemCount, static_cast<int64>(0));
		}

		if (BaseCellAreas.Num() != BaseCellCount)
		{
			BaseCellAreas.SetNumUninitialized(BaseCellCount);
			for (int32 BaseCellIndex = 0; BaseCellIndex < BaseCellCount; ++BaseCellIndex)
			{
				BaseCellAreas[BaseCellIndex] = Site.GetLevelCellAreaSquareCentimeters(BaseCellIndex, 0);
			}
		}
		if (BaseCellCover.Num() != BaseCellCount)
		{
			BaseCellCover.SetNumUninitialized(BaseCellCount);
		}
		for (int32 BaseCellIndex = 0; BaseCellIndex < BaseCellCount; ++BaseCellIndex)
		{
			BaseCellCover[BaseCellIndex] = 0.0;
		}

		int32 ClampedAxisCount = 0;
		for (const FArcForestIndividualRecord& Record : Snapshot.Individuals)
		{
			const int32* SpeciesIndex = SpeciesIndices.Find(Record.SpeciesId);
			int32 BaseCellIndex = INDEX_NONE;
			if (SpeciesIndex == nullptr
				|| !TryGetBaseCellIndex(Site, BaseValidityMask, Record.LocalPosition, BaseCellIndex))
			{
				continue;
			}
			SpreadCrownAreaOnBaseGrid(
				Site,
				FVector2D(Record.LocalPosition.X, Record.LocalPosition.Y),
				FVector2D::ZeroVector,
				CalculateCrownRadius(Record.HeightCm, Species[*SpeciesIndex]),
				1.0,
				BaseCellAreas,
				BaseCellCover,
				ClampedAxisCount);
		}
		for (const FArcForestCohortRecord& Record : Snapshot.Cohorts)
		{
			const int32* SpeciesIndex = SpeciesIndices.Find(Record.SpeciesId);
			FVector2D CellMinLocal = FVector2D::ZeroVector;
			FVector2D CellMaxLocal = FVector2D::ZeroVector;
			if (SpeciesIndex == nullptr
				|| !Site.TryGetLevelCellRect(Record.CellIndex, Site.RecordGridLevel, CellMinLocal, CellMaxLocal))
			{
				continue;
			}
			SpreadCrownAreaOnBaseGrid(
				Site,
				CellMinLocal,
				CellMaxLocal - CellMinLocal,
				CalculateCrownRadius(Record.MeanHeightCm, Species[*SpeciesIndex]),
				static_cast<double>(Record.StemCount),
				BaseCellAreas,
				BaseCellCover,
				ClampedAxisCount);
		}
		for (const FArcForestPatchRecord& Record : Snapshot.Patches)
		{
			const int32* SpeciesIndex = SpeciesIndices.Find(Record.SpeciesId);
			FVector2D CellMinLocal = FVector2D::ZeroVector;
			FVector2D CellMaxLocal = FVector2D::ZeroVector;
			if (SpeciesIndex == nullptr
				|| !Site.TryGetLevelCellRect(Record.CellIndex, Site.RecordGridLevel, CellMinLocal, CellMaxLocal))
			{
				continue;
			}
			SpreadCrownAreaOnBaseGrid(
				Site,
				CellMinLocal,
				CellMaxLocal - CellMinLocal,
				CalculateCrownRadius(Record.MeanHeightCm, Species[*SpeciesIndex]),
				static_cast<double>(Record.StemOrCoverEquivalent),
				BaseCellAreas,
				BaseCellCover,
				ClampedAxisCount);
		}
		Out.CrownSpanClampedAxisCount = ClampedAxisCount;

		const int32 RecordCellCount = Site.GetLevelCellCount(Site.RecordGridLevel);
		TArray<double> RecordCellCoverSum;
		TArray<int32> RecordCellCoverCount;
		RecordCellCoverSum.Init(0.0, RecordCellCount);
		RecordCellCoverCount.Init(0, RecordCellCount);

		double CoverSum = 0.0;
		double CoverMin = TNumericLimits<double>::Max();
		double CoverMax = TNumericLimits<double>::Lowest();
		int32 CoverCells = 0;
		int32 CoverAboveOneCells = 0;
		for (int32 BaseCellIndex = 0; BaseCellIndex < BaseCellCount; ++BaseCellIndex)
		{
			if (BaseCellIndex < 0 || BaseCellIndex >= BaseValidityMask.Num()
				|| BaseValidityMask[BaseCellIndex] == 0)
			{
				continue;
			}
			const double Cover = BaseCellCover[BaseCellIndex];
			CoverSum += Cover;
			CoverMin = FMath::Min(CoverMin, Cover);
			CoverMax = FMath::Max(CoverMax, Cover);
			CoverAboveOneCells += Cover > 1.0 ? 1 : 0;
			++CoverCells;
			const int32 RecordCellIndex = GetRecordCellIndexForBaseCell(Site, BaseCellIndex);
			if (RecordCellCoverSum.IsValidIndex(RecordCellIndex))
			{
				RecordCellCoverSum[RecordCellIndex] += Cover;
				++RecordCellCoverCount[RecordCellIndex];
			}
		}
		Out.CoverBaseCellCount = CoverCells;
		if (CoverCells > 0)
		{
			const double CoverCellsAsDouble = static_cast<double>(CoverCells);
			Out.MinBaseCover = CoverMin;
			Out.MeanBaseCover = CoverSum / CoverCellsAsDouble;
			Out.MaxBaseCover = CoverMax;
			Out.BaseCoverAboveOneFraction = static_cast<double>(CoverAboveOneCells) / CoverCellsAsDouble;
		}

		TArray<FHeightVigorSample> HeightVigorSamples;
		HeightVigorSamples.Reserve(Snapshot.Individuals.Num() + Snapshot.Cohorts.Num());

		double RecordLightSum = 0.0;
		double BaseLightSum = 0.0;
		double LightRatioSum = 0.0;
		int32 LightSamples = 0;
		int32 LightRatioSamples = 0;
		for (int32 RecordIndex = 0; RecordIndex < Snapshot.Individuals.Num(); ++RecordIndex)
		{
			const FArcForestIndividualRecord& Record = Snapshot.Individuals[RecordIndex];
			if (Record.LifeStatus != EArcForestLifeStatus::Alive)
			{
				continue;
			}
			const int32* SpeciesIndex = SpeciesIndices.Find(Record.SpeciesId);
			if (SpeciesIndex == nullptr || !Out.HeightCensus.IsValidIndex(*SpeciesIndex))
			{
				continue;
			}
			FSpeciesHeightCensus& Census = Out.HeightCensus[*SpeciesIndex];
			++Census.IndividualStems;
			AccumulateHeightCensus(static_cast<double>(Record.HeightCm), 1, Census);

			int32 BaseCellIndex = INDEX_NONE;
			int32 RecordCellIndex = INDEX_NONE;
			if (!TryGetBaseAndRecordCellIndex(
					Site, BaseValidityMask, Record.LocalPosition, BaseCellIndex, RecordCellIndex)
				|| !CellOccupancy.IsValidCell(RecordCellIndex))
			{
				continue;
			}
			if (FMath::IsFinite(Record.HeightCm) && FMath::IsFinite(Record.Vigor))
			{
				FHeightVigorSample& Sample = HeightVigorSamples.AddDefaulted_GetRef();
				Sample.SpeciesIndex = *SpeciesIndex;
				Sample.RecordCellIndex = RecordCellIndex;
				Sample.HeightCm = static_cast<double>(Record.HeightCm);
				Sample.Vigor = static_cast<double>(Record.Vigor);
				Sample.bIsIndividual = true;
			}

			const double RecordLight =
				CalculateBeerLambertLight(CellOccupancy.TotalAt(RecordCellIndex), Input.UnderstoryShade);
			const double BaseCover = BaseCellCover[BaseCellIndex];
			const double BaseLight = CalculateBeerLambertLight(BaseCover, Input.UnderstoryShade);
			RecordLightSum += RecordLight;
			BaseLightSum += BaseLight;
			++LightSamples;
			if (RecordLight > 0.0)
			{
				LightRatioSum += BaseLight / RecordLight;
				++LightRatioSamples;
			}

			if (RecordIndex == Out.TallestIndex)
			{
				Out.TallestRecordLight = RecordLight;
				Out.TallestBaseLight = BaseLight;
				Out.TallestBaseCover = BaseCover;
			}
		}

		Out.CrownLightSampleCount = LightSamples;
		Out.CrownLightRatioSampleCount = LightRatioSamples;
		if (LightSamples > 0)
		{
			const double LightSamplesAsDouble = static_cast<double>(LightSamples);
			Out.MeanRecordLight = RecordLightSum / LightSamplesAsDouble;
			Out.MeanBaseLight = BaseLightSum / LightSamplesAsDouble;
		}
		if (LightRatioSamples > 0)
		{
			Out.MeanLightRatio = LightRatioSum / static_cast<double>(LightRatioSamples);
		}

		int64 CohortStemsBelow = 0;
		int32 CohortRecordsBelow = 0;
		double CohortWeight = 0.0;
		double CohortWeightedRecordLightSum = 0.0;
		double CohortWeightedCoverSum = 0.0;
		for (const FArcForestCohortRecord& Record : Snapshot.Cohorts)
		{
			const int32* SpeciesIndex = SpeciesIndices.Find(Record.SpeciesId);
			if (SpeciesIndex == nullptr || !Out.HeightCensus.IsValidIndex(*SpeciesIndex))
			{
				continue;
			}
			FSpeciesHeightCensus& Census = Out.HeightCensus[*SpeciesIndex];
			Census.CohortStems += FMath::Max(Record.StemCount, static_cast<int64>(0));
			AccumulateHeightCensus(static_cast<double>(Record.MeanHeightCm), Record.StemCount, Census);

			if (Record.StemCount > 0
				&& CellOccupancy.IsValidCell(Record.CellIndex)
				&& FMath::IsFinite(Record.MeanHeightCm)
				&& FMath::IsFinite(Record.MeanVigor))
			{
				FHeightVigorSample& Sample = HeightVigorSamples.AddDefaulted_GetRef();
				Sample.SpeciesIndex = *SpeciesIndex;
				Sample.RecordCellIndex = Record.CellIndex;
				Sample.HeightCm = static_cast<double>(Record.MeanHeightCm);
				Sample.Vigor = static_cast<double>(Record.MeanVigor);
				Sample.bIsIndividual = false;
			}

			if (HasReachedIndividualization(Record, Species[*SpeciesIndex]))
			{
				continue;
			}
			CohortStemsBelow += Record.StemCount;
			++CohortRecordsBelow;
			if (!CellOccupancy.IsValidCell(Record.CellIndex)
				|| !RecordCellCoverCount.IsValidIndex(Record.CellIndex)
				|| RecordCellCoverCount[Record.CellIndex] <= 0
				|| Record.StemCount <= 0)
			{
				continue;
			}
			const double Weight = static_cast<double>(Record.StemCount);
			CohortWeight += Weight;
			CohortWeightedRecordLightSum += Weight
				* CalculateBeerLambertLight(CellOccupancy.TotalAt(Record.CellIndex), Input.UnderstoryShade);
			CohortWeightedCoverSum += Weight * RecordCellCoverSum[Record.CellIndex]
				/ static_cast<double>(RecordCellCoverCount[Record.CellIndex]);
		}
		Out.CohortStemsBelowIndividualization = CohortStemsBelow;
		Out.CohortRecordsBelowIndividualization = CohortRecordsBelow;
		if (CohortWeight > 0.0)
		{
			Out.CohortBelowMeanRecordLight = CohortWeightedRecordLightSum / CohortWeight;
			Out.CohortBelowMeanBaseCover = CohortWeightedCoverSum / CohortWeight;
		}

		FinalizeHeightVigorCorrelation(HeightVigorSamples, Out);
	}

	/** Reads the post-growth height of the same record the gather selected, so the applied delta is measured, not predicted. */
	void FinalizeYearDiagnostics(const FArcForestWorldSnapshot& Snapshot, FYearDiagnostics& Out)
	{
		if (!Snapshot.Individuals.IsValidIndex(Out.TallestIndex)
			|| Snapshot.Individuals[Out.TallestIndex].Id != Out.TallestId)
		{
			return;
		}
		Out.TallestAppliedDeltaCm =
			static_cast<double>(Snapshot.Individuals[Out.TallestIndex].HeightCm) - Out.TallestHeightCm;
	}

	void LogYearHeightCensus(const FYearDiagnostics& Stats)
	{
		static_assert(HeightCensusBucketCount == 10, "The census log line prints exactly ten buckets.");
		if (Stats.HeightCensus.Num() <= 0)
		{
			return;
		}

		int64 TotalIndividualStems = 0;
		int64 TotalCohortStems = 0;
		int64 TotalStemsAboveThreshold = 0;
		for (const FSpeciesHeightCensus& Census : Stats.HeightCensus)
		{
			TotalIndividualStems += Census.IndividualStems;
			TotalCohortStems += Census.CohortStems;
			TotalStemsAboveThreshold += Census.StemsAboveThreshold;
		}
		const int64 TotalStems = TotalIndividualStems + TotalCohortStems;
		const double AboveThresholdFraction = TotalStems > 0
			? static_cast<double>(TotalStemsAboveThreshold) / static_cast<double>(TotalStems)
			: 0.0;

		UE_LOGF(
			LogArcForestGrowth,
			VeryVerbose,
			"ArcForest year %d heightcensus | stems %lld ind %lld coh %lld"
			" | above %.2fm %lld frac %.4f",
			Stats.Year,
			TotalStems,
			TotalIndividualStems,
			TotalCohortStems,
			HeightCensusThresholdCm / 100.0,
			TotalStemsAboveThreshold,
			AboveThresholdFraction);

		for (int32 SpeciesIndex = 0; SpeciesIndex < Stats.HeightCensus.Num(); ++SpeciesIndex)
		{
			const FSpeciesHeightCensus& Census = Stats.HeightCensus[SpeciesIndex];
			const double BucketEdgeMeters =
				Census.MaxHeightCm / static_cast<double>(HeightCensusBucketCount) / 100.0;
			UE_LOGF(
				LogArcForestGrowth,
				VeryVerbose,
				"ArcForest year %d heightcensus sp %d | maxh %.2fm bucket %.2fm (deciles of this species own MaxHeightCm)"
				" | stems %lld ind %lld coh %lld above %.2fm %lld"
				" | buckets %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld",
				Stats.Year,
				SpeciesIndex,
				Census.MaxHeightCm / 100.0,
				BucketEdgeMeters,
				Census.IndividualStems + Census.CohortStems,
				Census.IndividualStems,
				Census.CohortStems,
				HeightCensusThresholdCm / 100.0,
				Census.StemsAboveThreshold,
				Census.BucketStems[0],
				Census.BucketStems[1],
				Census.BucketStems[2],
				Census.BucketStems[3],
				Census.BucketStems[4],
				Census.BucketStems[5],
				Census.BucketStems[6],
				Census.BucketStems[7],
				Census.BucketStems[8],
				Census.BucketStems[9]);
		}
	}

	void LogYearHeightVigorCorrelation(const FYearDiagnostics& Stats)
	{
		for (int32 SpeciesIndex = 0; SpeciesIndex < Stats.HeightVigorCorrelation.Num(); ++SpeciesIndex)
		{
			const FSpeciesHeightVigorCorrelation& Entry = Stats.HeightVigorCorrelation[SpeciesIndex];
			UE_LOGF(
				LogArcForestGrowth,
				VeryVerbose,
				"ArcForest year %d hvcorr sp %d | spearman %.4f cells %d samples %d minsamples %d"
				" splitind %d splitcoh %d | indspearman %.4f indcells %d indsamples %d"
				" (within record cell, average ranks, one sample per cohort, unweighted cell mean;"
				" split* is the individual/cohort mix of the qualifying all-sample cells;"
				" ind* drops cohort samples and applies minsamples to the individual count)",
				Stats.Year,
				SpeciesIndex,
				Entry.MeanCorrelation,
				Entry.QualifyingCellCount,
				Entry.QualifyingSampleCount,
				MinimumHeightVigorSamplesPerCell,
				Entry.QualifyingIndividualSampleCount,
				Entry.QualifyingCohortSampleCount,
				Entry.IndividualMeanCorrelation,
				Entry.IndividualQualifyingCellCount,
				Entry.IndividualQualifyingSampleCount);
		}
	}

	double CalculateSortedPercentile(TArray<double>& SortedValues, const double Fraction)
	{
		if (SortedValues.Num() <= 0)
		{
			return -1.0;
		}
		const int32 Index = FMath::Clamp(
			static_cast<int32>(Fraction * static_cast<double>(SortedValues.Num() - 1)),
			0,
			SortedValues.Num() - 1);
		return SortedValues[Index];
	}

	void LogYearSizeAgeGrowthMortality(
		const FArcForestRunInput& Input,
		const int32 Year,
		FYearDiagnostics& Stats)
	{
		static_assert(HeightCensusBucketCount == 10, "The structure log lines print exactly ten buckets.");
		for (int32 SpeciesIndex = 0; SpeciesIndex < Stats.SizeAgeStructure.Num(); ++SpeciesIndex)
		{
			const FSpeciesSizeAgeStructure& Structure = Stats.SizeAgeStructure[SpeciesIndex];
			UE_LOGF(
				LogArcForestGrowth,
				Log,
				"ArcForest year %d sizeage sp %d | largestDbh %.2fcm id %llu oldestAge %.1fy id %llu tallestAge %.1fy"
				" | dbhBuckets %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld"
				" | ageBuckets %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld"
				" | stemsPerCohortBuckets[log2 1,2,4..512+] %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld"
				" | biomassPerStemBuckets[deciles of maxDbh biomass] %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld",
				Year,
				SpeciesIndex,
				Structure.LargestDbhCm,
				Structure.LargestDbhId,
				Structure.OldestAgeYears,
				Structure.OldestId,
				Structure.TallestAgeYears,
				Structure.DbhBucketStems[0], Structure.DbhBucketStems[1], Structure.DbhBucketStems[2],
				Structure.DbhBucketStems[3], Structure.DbhBucketStems[4], Structure.DbhBucketStems[5],
				Structure.DbhBucketStems[6], Structure.DbhBucketStems[7], Structure.DbhBucketStems[8],
				Structure.DbhBucketStems[9],
				Structure.AgeBucketStems[0], Structure.AgeBucketStems[1], Structure.AgeBucketStems[2],
				Structure.AgeBucketStems[3], Structure.AgeBucketStems[4], Structure.AgeBucketStems[5],
				Structure.AgeBucketStems[6], Structure.AgeBucketStems[7], Structure.AgeBucketStems[8],
				Structure.AgeBucketStems[9],
				Structure.StemsPerCohortBucketRecords[0], Structure.StemsPerCohortBucketRecords[1],
				Structure.StemsPerCohortBucketRecords[2], Structure.StemsPerCohortBucketRecords[3],
				Structure.StemsPerCohortBucketRecords[4], Structure.StemsPerCohortBucketRecords[5],
				Structure.StemsPerCohortBucketRecords[6], Structure.StemsPerCohortBucketRecords[7],
				Structure.StemsPerCohortBucketRecords[8], Structure.StemsPerCohortBucketRecords[9],
				Structure.BiomassBucketRecords[0], Structure.BiomassBucketRecords[1],
				Structure.BiomassBucketRecords[2], Structure.BiomassBucketRecords[3],
				Structure.BiomassBucketRecords[4], Structure.BiomassBucketRecords[5],
				Structure.BiomassBucketRecords[6], Structure.BiomassBucketRecords[7],
				Structure.BiomassBucketRecords[8], Structure.BiomassBucketRecords[9]);
		}

		for (int32 SpeciesIndex = 0; SpeciesIndex < Stats.GrowthRate.Num(); ++SpeciesIndex)
		{
			const FSpeciesGrowthRate& Rate = Stats.GrowthRate[SpeciesIndex];
			const double MeanHeightIncrement = Rate.HeightIncrementSamples > 0
				? Rate.HeightIncrementSum / static_cast<double>(Rate.HeightIncrementSamples)
				: 0.0;
			const double MeanDbhIncrement = Rate.DbhIncrementSamples > 0
				? Rate.DbhIncrementSum / static_cast<double>(Rate.DbhIncrementSamples)
				: 0.0;
			UE_LOGF(
				LogArcForestGrowth,
				Log,
				"ArcForest year %d growthrate sp %d | height[mean/min/max cm] %.4f %.4f %.4f samples %lld"
				" | dbh[mean/min/max cm] %.4f %.4f %.4f samples %lld",
				Year,
				SpeciesIndex,
				MeanHeightIncrement,
				Rate.HeightIncrementSamples > 0 ? Rate.HeightIncrementMin : 0.0,
				Rate.HeightIncrementSamples > 0 ? Rate.HeightIncrementMax : 0.0,
				Rate.HeightIncrementSamples,
				MeanDbhIncrement,
				Rate.DbhIncrementSamples > 0 ? Rate.DbhIncrementMin : 0.0,
				Rate.DbhIncrementSamples > 0 ? Rate.DbhIncrementMax : 0.0,
				Rate.DbhIncrementSamples);
			for (int32 HeightBucket = 0; HeightBucket < HeightCensusBucketCount; ++HeightBucket)
			{
				const int64 HeightSamples = Rate.HeightAchievedFractionSamples[HeightBucket];
				const int64 DbhSamples = Rate.DbhAchievedFractionSamples[HeightBucket];
				if (HeightSamples <= 0 && DbhSamples <= 0)
				{
					continue;
				}
				UE_LOGF(
					LogArcForestGrowth,
					Log,
					"ArcForest year %d growthrate sp %d heightclass %d | heightAchievedFraction %.4f samples %lld"
					" dbhAchievedFraction %.4f samples %lld",
					Year,
					SpeciesIndex,
					HeightBucket,
					HeightSamples > 0 ? Rate.HeightAchievedFractionSum[HeightBucket] / static_cast<double>(HeightSamples) : 0.0,
					HeightSamples,
					DbhSamples > 0 ? Rate.DbhAchievedFractionSum[HeightBucket] / static_cast<double>(DbhSamples) : 0.0,
					DbhSamples);
			}
		}

		for (int32 SpeciesIndex = 0; SpeciesIndex < Stats.GrowthDrivers.Num(); ++SpeciesIndex)
		{
			FSpeciesGrowthDrivers& Drivers = Stats.GrowthDrivers[SpeciesIndex];
			Drivers.ResourceSamples.Sort();
			Drivers.LightAtCrownBaseSamples.Sort();
			const double ResourceMean = Drivers.ResourceSamples.Num() > 0
				? Algo::Accumulate(Drivers.ResourceSamples, 0.0) / static_cast<double>(Drivers.ResourceSamples.Num())
				: -1.0;
			const double LightMean = Drivers.LightAtCrownBaseSamples.Num() > 0
				? Algo::Accumulate(Drivers.LightAtCrownBaseSamples, 0.0)
					/ static_cast<double>(Drivers.LightAtCrownBaseSamples.Num())
				: -1.0;
			UE_LOGF(
				LogArcForestGrowth,
				Log,
				"ArcForest year %d growthdrivers sp %d | resource[mean/p10/p50/p90] %.4f %.4f %.4f %.4f samples %d"
				" | lightAtCrownBase[mean/p10/p50/p90] %.4f %.4f %.4f %.4f samples %d"
				" | meanVigor %.4f samples %lld | cellOcc[min/mean/max] %.4f %.4f %.4f cap %.4f",
				Year,
				SpeciesIndex,
				ResourceMean,
				CalculateSortedPercentile(Drivers.ResourceSamples, 0.10),
				CalculateSortedPercentile(Drivers.ResourceSamples, 0.50),
				CalculateSortedPercentile(Drivers.ResourceSamples, 0.90),
				Drivers.ResourceSamples.Num(),
				LightMean,
				CalculateSortedPercentile(Drivers.LightAtCrownBaseSamples, 0.10),
				CalculateSortedPercentile(Drivers.LightAtCrownBaseSamples, 0.50),
				CalculateSortedPercentile(Drivers.LightAtCrownBaseSamples, 0.90),
				Drivers.LightAtCrownBaseSamples.Num(),
				Drivers.VigorSamples > 0 ? Drivers.VigorSum / static_cast<double>(Drivers.VigorSamples) : -1.0,
				Drivers.VigorSamples,
				Stats.MinOccupancy,
				Stats.MeanOccupancy,
				Stats.MaxOccupancy,
				Input.MaximumCrownOccupancy);
		}

		for (int32 SpeciesIndex = 0; SpeciesIndex < Stats.MortalityBreakdown.Num(); ++SpeciesIndex)
		{
			const FSpeciesMortalityBreakdown& Breakdown = Stats.MortalityBreakdown[SpeciesIndex];
			for (int32 HeightBucket = 0; HeightBucket < HeightCensusBucketCount; ++HeightBucket)
			{
				const int64 Baseline = Breakdown.BaselineStems[HeightBucket];
				const int64 Stress = Breakdown.StressStems[HeightBucket];
				const int64 OldAge = Breakdown.OldAgeStems[HeightBucket];
				const int64 Mobility = Breakdown.MobilityStems[HeightBucket];
				const int64 Disturbance = Breakdown.DisturbanceStems[HeightBucket];
				const int64 SelfThinning = Breakdown.SelfThinningStems[HeightBucket];
				const int64 CoverFloorEviction = Breakdown.CoverFloorEvictionStems[HeightBucket];
				if (Baseline <= 0
					&& Stress <= 0
					&& OldAge <= 0
					&& Mobility <= 0
					&& Disturbance <= 0
					&& SelfThinning <= 0
					&& CoverFloorEviction <= 0)
				{
					continue;
				}
				UE_LOGF(
					LogArcForestGrowth,
					Log,
					"ArcForest year %d mortality sp %d heightclass %d | baseline %lld stress %lld oldAge %lld"
					" mobility %lld disturbance %lld selfThinning %lld coverFloorEviction %lld",
					Year,
					SpeciesIndex,
					HeightBucket,
					Baseline,
					Stress,
					OldAge,
					Mobility,
					Disturbance,
					SelfThinning,
					CoverFloorEviction);
			}
		}
	}

	void LogYearPromotionRecruitmentFlow(const int32 Year, const FRecruitmentPromotionFlow& Flow)
	{
		const int64 PromotionsUnaccounted = Flow.CohortsCrossingIndividualization
			- Flow.PromotionsGranted
			- Flow.SaturatedMultiStemSkipped
			- Flow.PromotionsRefusedChannelBuffer;
		const int64 RecruitmentUnaccounted = Flow.RecruitmentGenerated
			- Flow.RecruitmentAdmitted
			- Flow.RecruitmentRefusedRecordSlot
			- Flow.RecruitmentRefusedLiveStem
			- Flow.RecruitmentRefusedCoverFloorHeadroom
			- Flow.RecruitmentRefusedCoverSaturated
			- Flow.RecruitmentRefusedHabitatZero;
		UE_LOGF(
			LogArcForestGrowth,
			Log,
			"ArcForest year %d flow | cohortsCrossingIndividualization %lld promotionsGranted %lld"
			" saturatedMultiStemSkipped %lld promotionsRefusedChannelBuffer %lld promotionsUnaccounted %lld"
			" | recruitment generated %lld admitted %lld"
			" refused[recordSlot/liveStem/coverFloorHeadroom/coverSaturated/habitatZero] %lld %lld %lld %lld %lld"
			" unaccounted %lld",
			Year,
			Flow.CohortsCrossingIndividualization,
			Flow.PromotionsGranted,
			Flow.SaturatedMultiStemSkipped,
			Flow.PromotionsRefusedChannelBuffer,
			PromotionsUnaccounted,
			Flow.RecruitmentGenerated,
			Flow.RecruitmentAdmitted,
			Flow.RecruitmentRefusedRecordSlot,
			Flow.RecruitmentRefusedLiveStem,
			Flow.RecruitmentRefusedCoverFloorHeadroom,
			Flow.RecruitmentRefusedCoverSaturated,
			Flow.RecruitmentRefusedHabitatZero,
			RecruitmentUnaccounted);
	}

	void LogYearBudgetHeadroom(
		const FArcForestRunInput& Input,
		const int32 Year,
		const int64 LiveStemsRemaining,
		const int64 LiveRecordsRemaining,
		const int64 CoverRecordsRemaining,
		const int64 DeadwoodRecordsRemaining,
		const int64 BranchRecordsRemaining)
	{
		UE_LOGF(
			LogArcForestGrowth,
			Log,
			"ArcForest year %d budget headroom | liveStems remaining %lld of %d"
			" | liveRecords remaining %lld of %d | coverRecords remaining %lld of %d"
			" | deadwoodRecords remaining %lld of %d | branchRecords remaining %lld of %d",
			Year,
			LiveStemsRemaining,
			Input.MaxLiveStems,
			LiveRecordsRemaining,
			Input.MaxLiveRecords,
			CoverRecordsRemaining,
			Input.MaxCoverRecords,
			DeadwoodRecordsRemaining,
			Input.MaxDeadwoodRecords,
			BranchRecordsRemaining,
			Input.MaxBranchRecords);
	}

	void LogYearDiagnostics(const FYearDiagnostics& Stats)
	{
		UE_LOGF(
			LogArcForestGrowth,
			VeryVerbose,
			"ArcForest year %d | grid L%d edge %.2fm recordcells %d/%d basecells %d/%d"
			" | live kg/ha %.2f ind %.2f coh %.2f pat %.2f"
			" | occ[min/mean/max] %.4f %.4f %.4f comp %.4f"
			" | preown[min/mean/max, individualizing records only] %.4f %.4f %.4f cells %d"
			" | site[sp %d min/mean/max] %.4f %.4f %.4f"
			" | hab[min/mean/max] %.4f %.4f %.4f"
			" | tallest h %.2fcm res %.4f size %.4f gf %.6f inc %.4fcm clamped %.4fcm applied %.4fcm"
			" | livecount %d meanh %.2fcm | ind %d coh %d pat %d dead %d",
			Stats.Year,
			Stats.RecordGridLevel,
			Stats.RecordCellEdgeMeters,
			Stats.ValidRecordCellCount,
			Stats.TotalRecordCellCount,
			Stats.ValidBaseCellCount,
			Stats.TotalBaseCellCount,
			Stats.LiveBiomassKgPerHectare,
			Stats.IndividualBiomassKgPerHectare,
			Stats.CohortBiomassKgPerHectare,
			Stats.PatchBiomassKgPerHectare,
			Stats.MinOccupancy,
			Stats.MeanOccupancy,
			Stats.MaxOccupancy,
			Stats.MeanCompetition,
			Stats.MinOwnRootedOccupancy,
			Stats.MeanOwnRootedOccupancy,
			Stats.MaxOwnRootedOccupancy,
			Stats.OwnRootedOccupancyCellCount,
			Stats.HabitatSpeciesIndex,
			Stats.MinHabitat,
			Stats.MeanHabitat,
			Stats.MaxHabitat,
			Stats.MinRecruitHabitat,
			Stats.MeanRecruitHabitat,
			Stats.MaxRecruitHabitat,
			Stats.TallestHeightCm,
			Stats.TallestResource,
			Stats.TallestSizeLimitation,
			Stats.TallestGrowthFactor,
			Stats.TallestIncrementCm,
			Stats.TallestClampedDeltaCm,
			Stats.TallestAppliedDeltaCm,
			Stats.LiveIndividualCount,
			Stats.MeanIndividualHeightCm,
			Stats.IndividualCount,
			Stats.CohortCount,
			Stats.PatchCount,
			Stats.DeadwoodCount);

		UE_LOGF(
			LogArcForestGrowth,
			VeryVerbose,
			"ArcForest year %d crown | cover[min/mean/max] %.4f %.4f %.4f over1frac %.4f basecells %d spanclampaxes %d"
			" | light[rec/base/ratio] %.4f %.4f %.4f n %d ration %d"
			" | tallest res %.4f lightrec %.4f lightbase %.4f cover %.4f"
			" | cohortbelowind[stems/recs/stemwlightrec/stemwcover] %lld %d %.4f %.4f",
			Stats.Year,
			Stats.MinBaseCover,
			Stats.MeanBaseCover,
			Stats.MaxBaseCover,
			Stats.BaseCoverAboveOneFraction,
			Stats.CoverBaseCellCount,
			Stats.CrownSpanClampedAxisCount,
			Stats.MeanRecordLight,
			Stats.MeanBaseLight,
			Stats.MeanLightRatio,
			Stats.CrownLightSampleCount,
			Stats.CrownLightRatioSampleCount,
			Stats.TallestResource,
			Stats.TallestRecordLight,
			Stats.TallestBaseLight,
			Stats.TallestBaseCover,
			Stats.CohortStemsBelowIndividualization,
			Stats.CohortRecordsBelowIndividualization,
			Stats.CohortBelowMeanRecordLight,
			Stats.CohortBelowMeanBaseCover);

		LogYearHeightCensus(Stats);
		LogYearHeightVigorCorrelation(Stats);
	}

	struct FLiveRecordCounts
	{
		int32 IndividualCount = 0;
		int32 CohortCount = 0;
		int32 PatchCount = 0;
		int32 DeadwoodCount = 0;
		int64 CohortStemCount = 0;
	};

	void CountLiveRecords(const FArcForestWorldSnapshot& Snapshot, FLiveRecordCounts& Out)
	{
		Out.IndividualCount = Snapshot.Individuals.Num();
		Out.CohortCount = Snapshot.Cohorts.Num();
		Out.PatchCount = Snapshot.Patches.Num();
		Out.DeadwoodCount = Snapshot.Deadwood.Num();
		Out.CohortStemCount = 0;
		for (const FArcForestCohortRecord& Record : Snapshot.Cohorts)
		{
			Out.CohortStemCount += Record.StemCount;
		}
	}

	struct FYearDemographics
	{
		int32 Year = 0;

		int64 LocalDesiredAttempts = 0;
		int64 LocalAllocatedAttempts = 0;
		int64 RegionalDesiredAttempts = 0;
		int64 RegionalAllocatedAttempts = 0;
		int64 GeneratedStems = 0;
		int64 AdmittedStems = 0;
		bool bAdmittedStemsMeasured = false;

		int32 IndividualDeaths = 0;
		int64 CohortStemDeaths = 0;
		int32 CohortRecordDeaths = 0;
		int32 PatchRecordDeaths = 0;
		int32 DeadwoodCreated = 0;
		int32 DeadwoodDecayed = 0;
		int32 DeadwoodRetired = 0;

		int32 DisturbanceEvents = 0;
		int32 DisturbedIndividuals = 0;
		int64 DisturbedCohortStems = 0;
		int32 DisturbedRecords = 0;

		int32 ThinnedIndividuals = 0;
		int64 ThinnedCohortStems = 0;
		int32 ThinnedRecords = 0;

		int32 Promotions = 0;
	};

	void LogYearDemographics(const FYearDemographics& Stats)
	{
		const FString AdmittedText = Stats.bAdmittedStemsMeasured
			? FString::Printf(TEXT("%lld"), Stats.AdmittedStems)
			: FString(TEXT("?"));
		UE_LOGF(
			LogArcForestGrowth,
			VeryVerbose,
			"ArcForest year %d demog | attempts local[des/alloc] %lld %lld regional[des/alloc] %lld %lld"
			" | recruit generated %lld admitted %ls"
			" | mortality ind %d cohortstems %lld cohortrecs %d patchrecs %d"
			" | disturb events %d ind %d cohortstems %lld recs %d"
			" | thin ind %d cohortstems %lld recs %d"
			" | deadwood created %d decayed %d retired %d | promoted %d",
			Stats.Year,
			Stats.LocalDesiredAttempts,
			Stats.LocalAllocatedAttempts,
			Stats.RegionalDesiredAttempts,
			Stats.RegionalAllocatedAttempts,
			Stats.GeneratedStems,
			*AdmittedText,
			Stats.IndividualDeaths,
			Stats.CohortStemDeaths,
			Stats.CohortRecordDeaths,
			Stats.PatchRecordDeaths,
			Stats.DisturbanceEvents,
			Stats.DisturbedIndividuals,
			Stats.DisturbedCohortStems,
			Stats.DisturbedRecords,
			Stats.ThinnedIndividuals,
			Stats.ThinnedCohortStems,
			Stats.ThinnedRecords,
			Stats.DeadwoodCreated,
			Stats.DeadwoodDecayed,
			Stats.DeadwoodRetired,
			Stats.Promotions);
	}

	/** Group 0 is the record cells holding no live stem and no patch cover-equivalent, group 1 the rest, so the two means print side by side. */
	constexpr int32 ZeroStemGroup = 0;
	constexpr int32 StockedGroup = 1;
	constexpr int32 CellGroupCount = 2;

	struct FYearCellStats
	{
		int32 Year = 0;
		int32 ValidRecordCellCount = 0;
		int32 HabitatSpeciesIndex = INDEX_NONE;

		int64 MinStems = -1;
		double MeanStems = -1.0;
		int64 MaxStems = -1;
		int32 ZeroStemCellCount = -1;
		int32 PriorSampleZeroStemCellCount = -1;
		int32 PatchOnlyCellCount = -1;

		int32 GroupCellCount[CellGroupCount] = { -1, -1 };
		double GroupHabitat[CellGroupCount] = { -1.0, -1.0 };
		double GroupFertilityCapacity[CellGroupCount] = { -1.0, -1.0 };
		double GroupMoisture[CellGroupCount] = { -1.0, -1.0 };
		double GroupLocalAllocatedAttempts[CellGroupCount] = { -1.0, -1.0 };
		double GroupArrivingStems[CellGroupCount] = { -1.0, -1.0 };

		int64 MinLocalAllocatedAttempts = -1;
		double MeanLocalAllocatedAttempts = -1.0;
		int64 MaxLocalAllocatedAttempts = -1;
		int32 ZeroLocalAllocatedCellCount = -1;
		int64 RegionalAllocatedAttempts = -1;
		/** Attempts whose source record cell is out of range or excluded from the census, so mean times valid-cell count plus this reconciles against the local total. */
		int64 UnattributedLocalAllocatedAttempts = -1;

		int64 MinArrivingStems = -1;
		double MeanArrivingStems = -1.0;
		int64 MaxArrivingStems = -1;
		int32 ZeroArrivingCellCount = -1;
		/** The same reconciliation term for arriving stems, whose target record cell is out of range or excluded. */
		int64 UnattributedArrivingStems = -1;

		TArray<uint8> Usable;
		/** Live stems per record cell, patch cover-equivalents included exactly as TryCountBudgets counts them. */
		TArray<int64> StemsPerCell;
		TArray<int64> PatchEquivalentPerCell;
		TArray<double> HabitatPerCell;
		TArray<double> FertilityCapacityPerCell;
		TArray<double> MoisturePerCell;
		TArray<int64> LocalAllocatedPerCell;
		TArray<int64> ArrivingStemsPerCell;
	};

	/**
	 * Per-record-cell census taken beside GatherYearDiagnostics, on the same pre-growth state. The
	 * caller carries the zero-stem mask of the previous sampled year, so a stable bare set is separable
	 * from a drifting one; under a sampling interval the mask says nothing about the years in between.
	 */
	void GatherYearCellStats(
		const FArcForestRunInput& Input,
		const TConstArrayView<uint8> BaseValidityMask,
		const FArcEcotopeDecayGrid& DecayGrid,
		const FArcForestWorldSnapshot& Snapshot,
		const int32 HabitatSpeciesIndex,
		const int32 Year,
		TArray<uint8>& InOutPreviousZeroStemMask,
		FYearCellStats& Out)
	{
		const FArcEcotopeSiteMetadata& Site = GetSiteMetadata(Input);
		const int32 RecordCellCount = Site.GetLevelCellCount(Site.RecordGridLevel);
		const int32 BaseCellCount = Site.GetCellCount();

		Out.Year = Year;
		Out.HabitatSpeciesIndex = HabitatSpeciesIndex;
		Out.Usable.Init(0, RecordCellCount);
		Out.StemsPerCell.Init(0, RecordCellCount);
		Out.PatchEquivalentPerCell.Init(0, RecordCellCount);
		Out.HabitatPerCell.Init(-1.0, RecordCellCount);
		Out.FertilityCapacityPerCell.Init(-1.0, RecordCellCount);
		Out.MoisturePerCell.Init(-1.0, RecordCellCount);
		Out.LocalAllocatedPerCell.Init(0, RecordCellCount);
		Out.ArrivingStemsPerCell.Init(0, RecordCellCount);

		int32 ValidRecordCells = 0;
		for (int32 RecordCellIndex = 0; RecordCellIndex < RecordCellCount; ++RecordCellIndex)
		{
			if (!DecayGrid.CellAreaHectares.IsValidIndex(RecordCellIndex)
				|| !(DecayGrid.CellAreaHectares[RecordCellIndex] > 0.0)
				|| !DecayGrid.DecayModerator.IsValidIndex(RecordCellIndex))
			{
				continue;
			}
			Out.Usable[RecordCellIndex] = 1;
		}

		TArray<int32> BaseCellCounts;
		BaseCellCounts.Init(0, RecordCellCount);
		for (int32 BaseCellIndex = 0; BaseCellIndex < BaseCellCount; ++BaseCellIndex)
		{
			if (!BaseValidityMask.IsValidIndex(BaseCellIndex)
				|| BaseValidityMask[BaseCellIndex] == 0)
			{
				continue;
			}
			const int32 RecordCellIndex = GetRecordCellIndexForBaseCell(Site, BaseCellIndex);
			if (Out.Usable.IsValidIndex(RecordCellIndex) && Out.Usable[RecordCellIndex] != 0)
			{
				++BaseCellCounts[RecordCellIndex];
			}
		}
		for (int32 RecordCellIndex = 0; RecordCellIndex < RecordCellCount; ++RecordCellIndex)
		{
			if (BaseCellCounts[RecordCellIndex] <= 0)
			{
				Out.Usable[RecordCellIndex] = 0;
				continue;
			}
			++ValidRecordCells;
		}
		Out.ValidRecordCellCount = ValidRecordCells;

		for (const FArcForestIndividualRecord& Record : Snapshot.Individuals)
		{
			int32 BaseCellIndex = INDEX_NONE;
			int32 RecordCellIndex = INDEX_NONE;
			if (Record.LifeStatus != EArcForestLifeStatus::Alive
				|| !TryGetBaseAndRecordCellIndex(
					Site, BaseValidityMask, Record.LocalPosition, BaseCellIndex, RecordCellIndex)
				|| !Out.StemsPerCell.IsValidIndex(RecordCellIndex))
			{
				continue;
			}
			++Out.StemsPerCell[RecordCellIndex];
		}
		for (const FArcForestCohortRecord& Record : Snapshot.Cohorts)
		{
			if (Out.StemsPerCell.IsValidIndex(Record.CellIndex))
			{
				Out.StemsPerCell[Record.CellIndex] += Record.StemCount;
			}
		}
		for (const FArcForestPatchRecord& Record : Snapshot.Patches)
		{
			if (!Out.StemsPerCell.IsValidIndex(Record.CellIndex)
				|| !FMath::IsFinite(Record.StemOrCoverEquivalent)
				|| Record.StemOrCoverEquivalent <= 0.0f)
			{
				continue;
			}
			const int64 PatchStems = FMath::CeilToInt64(Record.StemOrCoverEquivalent);
			Out.StemsPerCell[Record.CellIndex] += PatchStems;
			Out.PatchEquivalentPerCell[Record.CellIndex] += PatchStems;
		}

		int64 StemSum = 0;
		int64 StemMin = TNumericLimits<int64>::Max();
		int64 StemMax = TNumericLimits<int64>::Lowest();
		int32 ZeroStemCells = 0;
		int32 PriorSampleZeroStemCells = 0;
		int32 PatchOnlyCells = 0;
		const bool bPreviousMaskValid = InOutPreviousZeroStemMask.Num() == RecordCellCount;
		TArray<uint8> ZeroStemMask;
		ZeroStemMask.Init(0, RecordCellCount);
		for (int32 RecordCellIndex = 0; RecordCellIndex < RecordCellCount; ++RecordCellIndex)
		{
			if (Out.Usable[RecordCellIndex] == 0)
			{
				continue;
			}
			const int64 Stems = Out.StemsPerCell[RecordCellIndex];
			StemSum += Stems;
			StemMin = FMath::Min(StemMin, Stems);
			StemMax = FMath::Max(StemMax, Stems);
			if (Stems == 0)
			{
				ZeroStemMask[RecordCellIndex] = 1;
				++ZeroStemCells;
				if (bPreviousMaskValid && InOutPreviousZeroStemMask[RecordCellIndex] != 0)
				{
					++PriorSampleZeroStemCells;
				}
			}
			else if (Stems == Out.PatchEquivalentPerCell[RecordCellIndex])
			{
				++PatchOnlyCells;
			}
		}
		if (ValidRecordCells > 0)
		{
			Out.MinStems = StemMin;
			Out.MeanStems = static_cast<double>(StemSum) / static_cast<double>(ValidRecordCells);
			Out.MaxStems = StemMax;
			Out.ZeroStemCellCount = ZeroStemCells;
			Out.PriorSampleZeroStemCellCount = bPreviousMaskValid ? PriorSampleZeroStemCells : -1;
			Out.PatchOnlyCellCount = PatchOnlyCells;
		}
		InOutPreviousZeroStemMask = MoveTemp(ZeroStemMask);
	}

	/**
	 * Attributes this year's recruitment onto the record grid: allocated attempts by the source cell the
	 * local groups carry, and generated candidate stems by their target cell. Regional groups carry no
	 * source cell, so only their total is attributable.
	 */
	void FinalizeYearCellStats(
		const FArcForestRunInput& Input,
		const TArray<FRecruitmentAttemptGroup>& Groups,
		const TArray<FRecruitmentCandidate>& Candidates,
		FYearCellStats& Out)
	{
		const FArcEcotopeSiteMetadata& Site = GetSiteMetadata(Input);
		int64 RegionalAllocated = 0;
		int64 UnattributedLocalAllocated = 0;
		for (const FRecruitmentAttemptGroup& Group : Groups)
		{
			if (Group.Source == ERecruitmentSource::Regional)
			{
				RegionalAllocated += Group.AllocatedAttempts;
				continue;
			}
			const int32 RecordCellIndex = Group.SourceCellIndex != INDEX_NONE
				? GetRecordCellIndexForBaseCell(Site, Group.SourceCellIndex)
				: INDEX_NONE;
			if (Out.LocalAllocatedPerCell.IsValidIndex(RecordCellIndex))
			{
				Out.LocalAllocatedPerCell[RecordCellIndex] += Group.AllocatedAttempts;
			}
			else
			{
				UnattributedLocalAllocated += Group.AllocatedAttempts;
			}
		}
		Out.RegionalAllocatedAttempts = RegionalAllocated;

		int64 UnattributedArriving = 0;
		for (const FRecruitmentCandidate& Candidate : Candidates)
		{
			const int32 RecordCellIndex = Candidate.TargetCellIndex != INDEX_NONE
				? GetRecordCellIndexForBaseCell(Site, Candidate.TargetCellIndex)
				: INDEX_NONE;
			if (Out.ArrivingStemsPerCell.IsValidIndex(RecordCellIndex))
			{
				Out.ArrivingStemsPerCell[RecordCellIndex] += Candidate.StemCount;
			}
			else
			{
				UnattributedArriving += Candidate.StemCount;
			}
		}

		int64 AllocatedSum = 0;
		int64 AllocatedMin = TNumericLimits<int64>::Max();
		int64 AllocatedMax = TNumericLimits<int64>::Lowest();
		int32 ZeroAllocatedCells = 0;
		int64 ArrivingSum = 0;
		int64 ArrivingMin = TNumericLimits<int64>::Max();
		int64 ArrivingMax = TNumericLimits<int64>::Lowest();
		int32 ZeroArrivingCells = 0;
		int32 GroupCells[CellGroupCount] = { 0, 0 };
		double GroupHabitatSum[CellGroupCount] = { 0.0, 0.0 };
		double GroupFertilitySum[CellGroupCount] = { 0.0, 0.0 };
		double GroupMoistureSum[CellGroupCount] = { 0.0, 0.0 };
		double GroupAllocatedSum[CellGroupCount] = { 0.0, 0.0 };
		double GroupArrivingSum[CellGroupCount] = { 0.0, 0.0 };
		for (int32 RecordCellIndex = 0; RecordCellIndex < Out.Usable.Num(); ++RecordCellIndex)
		{
			if (Out.Usable[RecordCellIndex] == 0)
			{
				UnattributedLocalAllocated += Out.LocalAllocatedPerCell[RecordCellIndex];
				UnattributedArriving += Out.ArrivingStemsPerCell[RecordCellIndex];
				continue;
			}
			const int64 Allocated = Out.LocalAllocatedPerCell[RecordCellIndex];
			AllocatedSum += Allocated;
			AllocatedMin = FMath::Min(AllocatedMin, Allocated);
			AllocatedMax = FMath::Max(AllocatedMax, Allocated);
			ZeroAllocatedCells += Allocated == 0 ? 1 : 0;

			const int64 Arriving = Out.ArrivingStemsPerCell[RecordCellIndex];
			ArrivingSum += Arriving;
			ArrivingMin = FMath::Min(ArrivingMin, Arriving);
			ArrivingMax = FMath::Max(ArrivingMax, Arriving);
			ZeroArrivingCells += Arriving == 0 ? 1 : 0;

			const int32 GroupIndex = Out.StemsPerCell[RecordCellIndex] == 0 ? ZeroStemGroup : StockedGroup;
			GroupHabitatSum[GroupIndex] += Out.HabitatPerCell[RecordCellIndex];
			GroupFertilitySum[GroupIndex] += Out.FertilityCapacityPerCell[RecordCellIndex];
			GroupMoistureSum[GroupIndex] += Out.MoisturePerCell[RecordCellIndex];
			GroupAllocatedSum[GroupIndex] += static_cast<double>(Allocated);
			GroupArrivingSum[GroupIndex] += static_cast<double>(Arriving);
			++GroupCells[GroupIndex];
		}

		Out.UnattributedLocalAllocatedAttempts = UnattributedLocalAllocated;
		Out.UnattributedArrivingStems = UnattributedArriving;
		if (Out.ValidRecordCellCount > 0)
		{
			const double ValidCellsAsDouble = static_cast<double>(Out.ValidRecordCellCount);
			Out.MinLocalAllocatedAttempts = AllocatedMin;
			Out.MeanLocalAllocatedAttempts = static_cast<double>(AllocatedSum) / ValidCellsAsDouble;
			Out.MaxLocalAllocatedAttempts = AllocatedMax;
			Out.ZeroLocalAllocatedCellCount = ZeroAllocatedCells;
			Out.MinArrivingStems = ArrivingMin;
			Out.MeanArrivingStems = static_cast<double>(ArrivingSum) / ValidCellsAsDouble;
			Out.MaxArrivingStems = ArrivingMax;
			Out.ZeroArrivingCellCount = ZeroArrivingCells;
		}
		for (int32 GroupIndex = 0; GroupIndex < CellGroupCount; ++GroupIndex)
		{
			Out.GroupCellCount[GroupIndex] = GroupCells[GroupIndex];
			if (GroupCells[GroupIndex] <= 0)
			{
				continue;
			}
			const double GroupCellsAsDouble = static_cast<double>(GroupCells[GroupIndex]);
			Out.GroupHabitat[GroupIndex] = GroupHabitatSum[GroupIndex] / GroupCellsAsDouble;
			Out.GroupFertilityCapacity[GroupIndex] = GroupFertilitySum[GroupIndex] / GroupCellsAsDouble;
			Out.GroupMoisture[GroupIndex] = GroupMoistureSum[GroupIndex] / GroupCellsAsDouble;
			Out.GroupLocalAllocatedAttempts[GroupIndex] = GroupAllocatedSum[GroupIndex] / GroupCellsAsDouble;
			Out.GroupArrivingStems[GroupIndex] = GroupArrivingSum[GroupIndex] / GroupCellsAsDouble;
		}
	}

	void LogYearCellStats(const FYearCellStats& Stats)
	{
		UE_LOGF(
			LogArcForestGrowth,
			VeryVerbose,
			"ArcForest year %d cells | sp %d recordcells %d zero %d stocked %d"
			" | stemswithpatch[min/mean/max] %lld %.2f %lld zerocells %d zeroalsoprevsample %d patchonly %d"
			" | zero[sitehab/fertcap/moist/localalloc/allarriv] %.4f %.4f %.4f %.2f %.2f"
			" | stocked[sitehab/fertcap/moist/localalloc/allarriv] %.4f %.4f %.4f %.2f %.2f"
			" | localalloc[min/mean/max] %lld %.2f %lld zerocells %d unattributed %lld regional %lld"
			" | allarriv[min/mean/max] %lld %.2f %lld zerocells %d unattributed %lld",
			Stats.Year,
			Stats.HabitatSpeciesIndex,
			Stats.ValidRecordCellCount,
			Stats.GroupCellCount[ZeroStemGroup],
			Stats.GroupCellCount[StockedGroup],
			Stats.MinStems,
			Stats.MeanStems,
			Stats.MaxStems,
			Stats.ZeroStemCellCount,
			Stats.PriorSampleZeroStemCellCount,
			Stats.PatchOnlyCellCount,
			Stats.GroupHabitat[ZeroStemGroup],
			Stats.GroupFertilityCapacity[ZeroStemGroup],
			Stats.GroupMoisture[ZeroStemGroup],
			Stats.GroupLocalAllocatedAttempts[ZeroStemGroup],
			Stats.GroupArrivingStems[ZeroStemGroup],
			Stats.GroupHabitat[StockedGroup],
			Stats.GroupFertilityCapacity[StockedGroup],
			Stats.GroupMoisture[StockedGroup],
			Stats.GroupLocalAllocatedAttempts[StockedGroup],
			Stats.GroupArrivingStems[StockedGroup],
			Stats.MinLocalAllocatedAttempts,
			Stats.MeanLocalAllocatedAttempts,
			Stats.MaxLocalAllocatedAttempts,
			Stats.ZeroLocalAllocatedCellCount,
			Stats.UnattributedLocalAllocatedAttempts,
			Stats.RegionalAllocatedAttempts,
			Stats.MinArrivingStems,
			Stats.MeanArrivingStems,
			Stats.MaxArrivingStems,
			Stats.ZeroArrivingCellCount,
			Stats.UnattributedArrivingStems);
	}

	struct FParallelGrowthSettings
	{
		/** Zero means derive the cap from the logical processor count. This is a planned task count, not workers. */
		int32 RequestedTaskSlots = 0;
	};

	struct FPromotionWorkTotals
	{
		int64 EligibleBeforeBudgetFilter = 0;
		int64 SaturatedMultiStemSkipped = 0;
		int64 PromotedCohorts = 0;
		int64 PromotedStems = 0;
		int64 SingletonPromotions = 0;
		int64 OrdinalsScanned = 0;
		int64 PlantableChildren = 0;
		int64 SurfaceEntryLookups = 0;
		int64 ChannelBufferSkippedCohorts = 0;
		int32 SaturatedYears = 0;
	};

	enum class EGrowthRecordStatus : uint8
	{
		Success,
		IndividualAgeOverflow,
		IndividualOutsideFrozenSite,
		IndividualInvalidAllometry,
		IndividualInvalidBiomass,
		CohortInvalidRecordCell,
		CohortMeanAgeOverflow,
		CohortInvalidAllometry,
		PatchInvalidRecordCell,
		PatchMeanAgeOverflow,
		PatchInvalidAllometry,
	};

	/** One result per planned batch, avoiding a full per-record status buffer for multi-million-record runs. */
	struct FParallelGrowthBatchResult
	{
		int32 FirstErrorIndex = INDEX_NONE;
		EGrowthRecordStatus Status = EGrowthRecordStatus::Success;
	};

	/** Upper bound on the batch index any ProcessGrowthBatches call can pass, so per-batch state can be sized ahead. */
	int32 GetMaximumParallelGrowthTaskSlotCount(const FParallelGrowthSettings& Settings)
	{
		return Settings.RequestedTaskSlots > 0
			? Settings.RequestedTaskSlots
			: FMath::Max(1, FPlatformMisc::NumberOfCoresIncludingHyperthreads());
	}

	int32 GetParallelGrowthTaskSlotCount(
		const int32 RecordCount,
		const FParallelGrowthSettings& Settings)
	{
		check(RecordCount > 0);
		if (Settings.RequestedTaskSlots == 0 && RecordCount < MinimumRecordsForParallelGrowth)
		{
			return 1;
		}
		return FMath::Clamp(GetMaximumParallelGrowthTaskSlotCount(Settings), 1, RecordCount);
	}

	template <typename RecordType, typename ProcessRecordType>
	EGrowthRecordStatus ProcessGrowthBatches(
		const TCHAR* DebugName,
		const TCHAR* ParallelTimingName,
		const TCHAR* SerialTimingName,
		TArray<RecordType>& Records,
		const int32 StartRecordIndex,
		const int32 EndRecordIndex,
		const FParallelGrowthSettings& Settings,
		UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics,
		const ProcessRecordType& ProcessRecord,
		int32& OutFailingRecordIndex)
	{
		OutFailingRecordIndex = INDEX_NONE;
		if (StartRecordIndex < 0
			|| EndRecordIndex < StartRecordIndex
			|| EndRecordIndex > Records.Num())
		{
			return EGrowthRecordStatus::Success;
		}
		const int32 RecordCount = EndRecordIndex - StartRecordIndex;
		if (RecordCount == 0)
		{
			return EGrowthRecordStatus::Success;
		}

		const int32 PlannedTaskSlots = GetParallelGrowthTaskSlotCount(RecordCount, Settings);
		const bool bUseParallel = PlannedTaskSlots > 1;
		const auto ProcessRange = [&](
			const int32 StartIndex,
			const int32 EndIndex,
			const int32 BatchIndex,
			FParallelGrowthBatchResult& Result)
		{
			for (int32 RecordIndex = StartIndex; RecordIndex < EndIndex; ++RecordIndex)
			{
				const EGrowthRecordStatus Status = ProcessRecord(Records[RecordIndex], RecordIndex, BatchIndex);
				if (Status != EGrowthRecordStatus::Success)
				{
					Result.FirstErrorIndex = RecordIndex;
					Result.Status = Status;
					return;
				}
			}
		};

		const double BatchStartSeconds = FPlatformTime::Seconds();
		EGrowthRecordStatus FirstErrorStatus = EGrowthRecordStatus::Success;
		if (bUseParallel)
		{
			TArray<FParallelGrowthBatchResult> BatchResults;
			BatchResults.SetNum(PlannedTaskSlots);
			ParallelForTemplate(
				DebugName,
				PlannedTaskSlots,
				1,
				[&](const int32 BatchIndex)
				{
					const int32 StartIndex = StartRecordIndex + static_cast<int32>(
						(static_cast<int64>(BatchIndex) * static_cast<int64>(RecordCount)) / PlannedTaskSlots);
					const int32 EndIndex = StartRecordIndex + static_cast<int32>(
						(static_cast<int64>(BatchIndex + 1) * static_cast<int64>(RecordCount)) / PlannedTaskSlots);
					ProcessRange(StartIndex, EndIndex, BatchIndex, BatchResults[BatchIndex]);
				},
				EParallelForFlags::None);

			for (const FParallelGrowthBatchResult& Result : BatchResults)
			{
				if (Result.FirstErrorIndex != INDEX_NONE
					&& (OutFailingRecordIndex == INDEX_NONE || Result.FirstErrorIndex < OutFailingRecordIndex))
				{
					OutFailingRecordIndex = Result.FirstErrorIndex;
					FirstErrorStatus = Result.Status;
				}
			}
		}
		else
		{
			FParallelGrowthBatchResult SerialResult;
			ProcessRange(StartRecordIndex, EndRecordIndex, 0, SerialResult);
			OutFailingRecordIndex = SerialResult.FirstErrorIndex;
			FirstErrorStatus = SerialResult.Status;
		}
		Diagnostics.AddSample(
			bUseParallel ? ParallelTimingName : SerialTimingName,
			UE::ArcForest::GetElapsedMilliseconds(BatchStartSeconds));
		return FirstErrorStatus;
	}

	const TCHAR* GetGrowthErrorText(const EGrowthRecordStatus Status)
	{
		switch (Status)
		{
		case EGrowthRecordStatus::IndividualAgeOverflow:
			return TEXT("ArcForest individual age cannot be incremented without overflow.");
		case EGrowthRecordStatus::IndividualOutsideFrozenSite:
			return TEXT("ArcForest individual moved outside its frozen site.");
		case EGrowthRecordStatus::IndividualInvalidAllometry:
			return TEXT("ArcForest individual growth produced invalid allometry.");
		case EGrowthRecordStatus::IndividualInvalidBiomass:
			return TEXT("ArcForest individual growth produced invalid biomass.");
		case EGrowthRecordStatus::CohortInvalidRecordCell:
			return TEXT("ArcForest cohort references a record cell with no valid base cell in its frozen site.");
		case EGrowthRecordStatus::CohortMeanAgeOverflow:
			return TEXT("ArcForest cohort mean age overflowed.");
		case EGrowthRecordStatus::CohortInvalidAllometry:
			return TEXT("ArcForest cohort growth produced invalid allometry.");
		case EGrowthRecordStatus::PatchInvalidRecordCell:
			return TEXT("ArcForest patch references a record cell with no valid base cell in its frozen site.");
		case EGrowthRecordStatus::PatchMeanAgeOverflow:
			return TEXT("ArcForest patch mean age overflowed.");
		case EGrowthRecordStatus::PatchInvalidAllometry:
			return TEXT("ArcForest patch growth produced invalid allometry.");
		case EGrowthRecordStatus::Success:
		default:
			return TEXT("ArcForest growth failed with an invalid internal status.");
		}
	}

	template <typename RecordType, typename ResolveQueryType>
	bool EvaluateFactoredGrowthWindow(
		const TArray<RecordType>& Records,
		const int32 StartRecordIndex,
		const int32 EndRecordIndex,
		const FFactoredHabitatQueryContext& QueryContext,
		const ResolveQueryType& ResolveQuery,
		TArray<uint32>& OutQuantums,
		EGrowthRecordStatus& OutStatus,
		int32& OutFailingRecordIndex,
		FFactoredHabitatQueryTelemetry& InOutTelemetry,
		FString& OutError)
	{
		OutStatus = EGrowthRecordStatus::Success;
		OutFailingRecordIndex = INDEX_NONE;
		const int32 WindowCount = EndRecordIndex - StartRecordIndex;
		TArray<FFactoredHabitatQuery> Queries;
		Queries.Reserve(WindowCount);
		Queries.SetNumUninitialized(WindowCount);
		for (int32 LocalIndex = 0; LocalIndex < WindowCount; ++LocalIndex)
		{
			const int32 RecordIndex = StartRecordIndex + LocalIndex;
			int32 SpeciesIndex = INDEX_NONE;
			int32 BaseCellIndex = INDEX_NONE;
			int32 RecordCellIndex = INDEX_NONE;
			const EGrowthRecordStatus Status = ResolveQuery(
				Records[RecordIndex], SpeciesIndex, BaseCellIndex, RecordCellIndex);
			if (Status != EGrowthRecordStatus::Success)
			{
				OutStatus = Status;
				OutFailingRecordIndex = RecordIndex;
				return true;
			}
			FFactoredHabitatQuery& Query = Queries[LocalIndex];
			Query.SpeciesIndex = SpeciesIndex;
			Query.BaseCellIndex = BaseCellIndex;
			Query.OutputIndex = LocalIndex;
			Query.RouteSeedIndex = INDEX_NONE;
			Query.StableOrdinal = RecordIndex;
		}
		return EvaluateFactoredHabitatQueries(
			QueryContext,
			Queries,
			static_cast<int64>(Queries.GetAllocatedSize()),
			OutQuantums,
			InOutTelemetry,
			OutError);
	}

	/**
	 * Requires the validated immutable species map. A parallel failure can leave higher-index records
	 * modified by other joined batches; callers must discard Snapshot when this function returns false.
	 */
	bool ApplyGrowth(
		const FArcForestRunInput& Input,
		const TConstArrayView<uint8> BaseValidityMask,
		const TArray<FArcForestFrozenSpeciesSetup>& Species,
		const TMap<FGuid, int32>& SpeciesIndices,
		const FFactoredHabitatQueryContext& FactoredQueryContext,
		const FStratifiedOccupancy& CellOccupancy,
		const FParallelGrowthSettings& ParallelGrowthSettings,
		UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics,
		FFactoredHabitatQueryTelemetry& InOutQueryTelemetry,
		const bool bGrowthDiagnosticsEnabled,
		FYearDiagnostics& InOutYearStats,
		FArcForestWorldSnapshot& Snapshot,
		FString& OutError)
	{
		const FArcEcotopeSiteMetadata& SiteMetadata = GetSiteMetadata(Input);
		const int32 MaximumFactoredGrowthWindowCount =
			GetMaximumFactoredHabitatQueriesPerWindow(sizeof(FFactoredHabitatQuery));
		if (MaximumFactoredGrowthWindowCount <= 0)
		{
			OutError = TEXT("ArcForest grouped habitat query budget cannot hold one growth query window.");
			return false;
		}
		TArray<FGrowthDiagnosticsBatch> DiagnosticBatches;
		if (bGrowthDiagnosticsEnabled)
		{
			DiagnosticBatches.SetNum(GetMaximumParallelGrowthTaskSlotCount(ParallelGrowthSettings));
			for (FGrowthDiagnosticsBatch& Batch : DiagnosticBatches)
			{
				Batch.GrowthRate.SetNum(InOutYearStats.GrowthRate.Num());
				Batch.GrowthDrivers.SetNum(InOutYearStats.GrowthDrivers.Num());
			}
		}
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, ApplyGrowth_Individuals);
			int32 FirstFailingRecordIndex = INDEX_NONE;
			int32 ActiveWindowStart = 0;
			TArray<uint32> WindowQuantums;
			const auto ProcessWindow = [&](const int32 StartRecordIndex, const int32 EndRecordIndex)
			{
				ActiveWindowStart = StartRecordIndex;
				return ProcessGrowthBatches(
				TEXT("arcecotope.GrowthIndividuals"),
				TEXT("ApplyGrowth_IndividualsParallelBatches"),
				TEXT("ApplyGrowth_IndividualsSerialBatches"),
				Snapshot.Individuals,
				StartRecordIndex,
				EndRecordIndex,
				ParallelGrowthSettings,
				Diagnostics,
				[&](FArcForestIndividualRecord& Record, const int32 RecordIndex, const int32 BatchIndex)
				{
					if (Record.AgeYears == MAX_int32)
					{
						return EGrowthRecordStatus::IndividualAgeOverflow;
					}
					const int32 SpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
					const FArcForestFrozenSpeciesSetup& Model = Species[SpeciesIndex];
					int32 BaseCellIndex = INDEX_NONE;
					int32 RecordCellIndex = INDEX_NONE;
					if (!TryGetBaseAndRecordCellIndex(
							SiteMetadata,
							BaseValidityMask,
							Record.LocalPosition,
							BaseCellIndex,
							RecordCellIndex))
					{
						return EGrowthRecordStatus::IndividualOutsideFrozenSite;
					}
					// No modification cut here, deliberately: full regeneration
					// means the bootstrap has already re-decided every
					// individual, so a mask need not starve one that survived it.
					const uint32 HabitatQuantum = WindowQuantums[RecordIndex - ActiveWindowStart];
					const double OldHeightCm = static_cast<double>(Record.HeightCm);
					const double OldDbhCm = static_cast<double>(Record.DbhCm);
					const double LightAtCrownBase = CalculateLightAtHeight(
						CellOccupancy, RecordCellIndex, OldHeightCm, Model, Input.UnderstoryShade);
					const double Resource =
						CalculateResourceFactorFromQuantum(HabitatQuantum, Model, LightAtCrownBase);
					const double GrowthFactor = Resource * CalculateSizeLimitation(Record.HeightCm, Model);
					const double NewHeight =
						FMath::Min(static_cast<double>(Model.MaxHeightCm),
								   static_cast<double>(Record.HeightCm) + Model.MaxAnnualHeightGrowthCm * GrowthFactor);
					double AllometricDbh = 0.0;
					if (!TryCalculateDbh(Model, NewHeight, AllometricDbh))
					{
						return EGrowthRecordStatus::IndividualInvalidAllometry;
					}
					Record.HeightCm = static_cast<float>(NewHeight);
					Record.DbhCm = static_cast<float>(
						FMath::Min(static_cast<double>(Model.MaxDbhCm),
								   FMath::Max(static_cast<double>(Record.DbhCm),
											  FMath::Min(AllometricDbh,
														 static_cast<double>(Record.DbhCm) +
															 Model.MaxAnnualDbhGrowthCm * GrowthFactor))));
					double BiomassKg = 0.0;
					if (!TryCalculateBiomass(Model, Record.DbhCm, BiomassKg) ||
						!FMath::IsFinite(static_cast<float>(BiomassKg)) || static_cast<float>(BiomassKg) <= 0.0f)
					{
						return EGrowthRecordStatus::IndividualInvalidBiomass;
					}
					Record.BiomassKg = static_cast<float>(BiomassKg);
					Record.CrownRadiusCm = FMath::Max(0.0f, Record.HeightCm * Model.CrownRadiusHeightRatio);
					Record.Vigor = static_cast<float>(FMath::Clamp(Model.VigorSmoothingRetention * Record.Vigor +
																	   (1.0 - Model.VigorSmoothingRetention) * Resource,
																   0.0,
																   1.0));
					++Record.AgeYears;
					if (bGrowthDiagnosticsEnabled)
					{
						const double HeightIncrementCm = static_cast<double>(Record.HeightCm) - OldHeightCm;
						const double DbhIncrementCm = static_cast<double>(Record.DbhCm) - OldDbhCm;
						const int32 HeightBucket = CalculateStructureHeightBucket(
							OldHeightCm, static_cast<double>(Model.MaxHeightCm));
						FGrowthDiagnosticsBatch& Batch = DiagnosticBatches[BatchIndex];
						if (Batch.GrowthRate.IsValidIndex(SpeciesIndex))
						{
							FSpeciesGrowthRate& Rate = Batch.GrowthRate[SpeciesIndex];
							Rate.HeightIncrementSum += HeightIncrementCm;
							Rate.HeightIncrementMin = FMath::Min(Rate.HeightIncrementMin, HeightIncrementCm);
							Rate.HeightIncrementMax = FMath::Max(Rate.HeightIncrementMax, HeightIncrementCm);
							++Rate.HeightIncrementSamples;
							if (Model.MaxAnnualHeightGrowthCm > 0.0f)
							{
								Rate.HeightAchievedFractionSum[HeightBucket] +=
									HeightIncrementCm / static_cast<double>(Model.MaxAnnualHeightGrowthCm);
								++Rate.HeightAchievedFractionSamples[HeightBucket];
							}
							Rate.DbhIncrementSum += DbhIncrementCm;
							Rate.DbhIncrementMin = FMath::Min(Rate.DbhIncrementMin, DbhIncrementCm);
							Rate.DbhIncrementMax = FMath::Max(Rate.DbhIncrementMax, DbhIncrementCm);
							++Rate.DbhIncrementSamples;
							if (Model.MaxAnnualDbhGrowthCm > 0.0f)
							{
								Rate.DbhAchievedFractionSum[HeightBucket] +=
									DbhIncrementCm / static_cast<double>(Model.MaxAnnualDbhGrowthCm);
								++Rate.DbhAchievedFractionSamples[HeightBucket];
							}
						}
						if (Batch.GrowthDrivers.IsValidIndex(SpeciesIndex))
						{
							FSpeciesGrowthDrivers& Drivers = Batch.GrowthDrivers[SpeciesIndex];
							Drivers.ResourceSamples.Add(Resource);
							Drivers.LightAtCrownBaseSamples.Add(LightAtCrownBase);
							Drivers.VigorSum += static_cast<double>(Record.Vigor);
							++Drivers.VigorSamples;
						}
					}
					return EGrowthRecordStatus::Success;
				},
				FirstFailingRecordIndex);
			};
			EGrowthRecordStatus Status = EGrowthRecordStatus::Success;
			{
				const int32 MaximumWindowCount = MaximumFactoredGrowthWindowCount;
				for (int32 WindowStart = 0; WindowStart < Snapshot.Individuals.Num(); WindowStart += MaximumWindowCount)
				{
					const int32 WindowEnd = FMath::Min(
						WindowStart + MaximumWindowCount, Snapshot.Individuals.Num());
					if (!EvaluateFactoredGrowthWindow(
							Snapshot.Individuals,
							WindowStart,
							WindowEnd,
							FactoredQueryContext,
							[&](const FArcForestIndividualRecord& Record, int32& OutSpeciesIndex,
								int32& OutBaseCellIndex, int32& OutRecordCellIndex)
							{
								if (Record.AgeYears == MAX_int32)
								{
									return EGrowthRecordStatus::IndividualAgeOverflow;
								}
								OutSpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
								return TryGetBaseAndRecordCellIndex(
									SiteMetadata,
									BaseValidityMask,
									Record.LocalPosition,
									OutBaseCellIndex,
									OutRecordCellIndex)
									? EGrowthRecordStatus::Success
									: EGrowthRecordStatus::IndividualOutsideFrozenSite;
							},
							WindowQuantums,
							Status,
							FirstFailingRecordIndex,
							InOutQueryTelemetry,
							OutError))
					{
						return false;
					}
					if (Status != EGrowthRecordStatus::Success)
					{
						break;
					}
					Status = ProcessWindow(WindowStart, WindowEnd);
					if (Status != EGrowthRecordStatus::Success)
					{
						break;
					}
				}
			}
			if (Status != EGrowthRecordStatus::Success)
			{
				check(FirstFailingRecordIndex != INDEX_NONE);
				OutError = GetGrowthErrorText(Status);
				return false;
			}
		}

		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, ApplyGrowth_Cohorts);
			int32 FirstFailingRecordIndex = INDEX_NONE;
			int32 ActiveWindowStart = 0;
			TArray<uint32> WindowQuantums;
			const auto ProcessWindow = [&](const int32 StartRecordIndex, const int32 EndRecordIndex)
			{
				ActiveWindowStart = StartRecordIndex;
				return ProcessGrowthBatches(
				TEXT("arcecotope.GrowthCohorts"),
				TEXT("ApplyGrowth_CohortsParallelBatches"),
				TEXT("ApplyGrowth_CohortsSerialBatches"),
				Snapshot.Cohorts,
				StartRecordIndex,
				EndRecordIndex,
				ParallelGrowthSettings,
				Diagnostics,
				[&](FArcForestCohortRecord& Record, const int32 RecordIndex, const int32 BatchIndex)
				{
					const int32 SpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
					const FArcForestFrozenSpeciesSetup& Model = Species[SpeciesIndex];
					int32 BaseCellIndex = INDEX_NONE;
					if (!TryGetRepresentativeBaseCellForRecordCell(
							SiteMetadata, BaseValidityMask, Record.CellIndex, BaseCellIndex))
					{
						return EGrowthRecordStatus::CohortInvalidRecordCell;
					}
					const uint32 HabitatQuantum = WindowQuantums[RecordIndex - ActiveWindowStart];
					const double OldMeanHeightCm = static_cast<double>(Record.MeanHeightCm);
					const double LightAtCrownBase = CalculateLightAtHeight(
						CellOccupancy, Record.CellIndex, OldMeanHeightCm, Model, Input.UnderstoryShade);
					const double Resource =
						CalculateResourceFactorFromQuantum(HabitatQuantum, Model, LightAtCrownBase);
					const double GrowthFactor = Resource * CalculateSizeLimitation(Record.MeanHeightCm, Model);
					Record.MeanHeightCm =
						static_cast<float>(FMath::Max(static_cast<double>(Record.MeanHeightCm),
													  FMath::Min(static_cast<double>(Model.MaxHeightCm),
																 static_cast<double>(Record.MeanHeightCm) +
																	 Model.MaxAnnualHeightGrowthCm * GrowthFactor)));
					if (!FMath::IsFinite(Record.MeanAgeYears + 1.0f))
					{
						return EGrowthRecordStatus::CohortMeanAgeOverflow;
					}
					Record.MeanAgeYears += 1.0f;
					Record.MeanVigor =
						static_cast<float>(FMath::Clamp(Model.VigorSmoothingRetention * Record.MeanVigor +
															(1.0 - Model.VigorSmoothingRetention) * Resource,
														0.0,
														1.0));
					Record.HeightBin = CalculateHeightBin(Record.MeanHeightCm);
					double DbhCm = 0.0;
					double BiomassPerStemKg = 0.0;
					if (!TryCalculateDbh(Model, Record.MeanHeightCm, DbhCm) ||
						!TryCalculateBiomass(Model, DbhCm, BiomassPerStemKg) ||
						BiomassPerStemKg * static_cast<double>(Record.StemCount) > MAX_flt)
					{
						return EGrowthRecordStatus::CohortInvalidAllometry;
					}
					Record.BiomassKg = static_cast<float>(BiomassPerStemKg * static_cast<double>(Record.StemCount));
					if (bGrowthDiagnosticsEnabled)
					{
						const double HeightIncrementCm = static_cast<double>(Record.MeanHeightCm) - OldMeanHeightCm;
						const int32 HeightBucket = CalculateStructureHeightBucket(
							OldMeanHeightCm, static_cast<double>(Model.MaxHeightCm));
						FGrowthDiagnosticsBatch& Batch = DiagnosticBatches[BatchIndex];
						if (Batch.GrowthRate.IsValidIndex(SpeciesIndex))
						{
							FSpeciesGrowthRate& Rate = Batch.GrowthRate[SpeciesIndex];
							Rate.HeightIncrementSum += HeightIncrementCm;
							Rate.HeightIncrementMin = FMath::Min(Rate.HeightIncrementMin, HeightIncrementCm);
							Rate.HeightIncrementMax = FMath::Max(Rate.HeightIncrementMax, HeightIncrementCm);
							++Rate.HeightIncrementSamples;
							if (Model.MaxAnnualHeightGrowthCm > 0.0f)
							{
								Rate.HeightAchievedFractionSum[HeightBucket] +=
									HeightIncrementCm / static_cast<double>(Model.MaxAnnualHeightGrowthCm);
								++Rate.HeightAchievedFractionSamples[HeightBucket];
							}
						}
						if (Batch.GrowthDrivers.IsValidIndex(SpeciesIndex))
						{
							FSpeciesGrowthDrivers& Drivers = Batch.GrowthDrivers[SpeciesIndex];
							Drivers.ResourceSamples.Add(Resource);
							Drivers.LightAtCrownBaseSamples.Add(LightAtCrownBase);
							Drivers.VigorSum += static_cast<double>(Record.MeanVigor);
							++Drivers.VigorSamples;
						}
					}
					return EGrowthRecordStatus::Success;
				},
				FirstFailingRecordIndex);
			};
			EGrowthRecordStatus Status = EGrowthRecordStatus::Success;
			{
				const int32 MaximumWindowCount = MaximumFactoredGrowthWindowCount;
				for (int32 WindowStart = 0; WindowStart < Snapshot.Cohorts.Num(); WindowStart += MaximumWindowCount)
				{
					const int32 WindowEnd = FMath::Min(WindowStart + MaximumWindowCount, Snapshot.Cohorts.Num());
					if (!EvaluateFactoredGrowthWindow(
							Snapshot.Cohorts,
							WindowStart,
							WindowEnd,
							FactoredQueryContext,
							[&](const FArcForestCohortRecord& Record, int32& OutSpeciesIndex,
								int32& OutBaseCellIndex, int32& OutRecordCellIndex)
							{
								OutSpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
								OutRecordCellIndex = Record.CellIndex;
								return TryGetRepresentativeBaseCellForRecordCell(
									SiteMetadata, BaseValidityMask, Record.CellIndex, OutBaseCellIndex)
									? EGrowthRecordStatus::Success
									: EGrowthRecordStatus::CohortInvalidRecordCell;
							},
							WindowQuantums,
							Status,
							FirstFailingRecordIndex,
							InOutQueryTelemetry,
							OutError))
					{
						return false;
					}
					if (Status != EGrowthRecordStatus::Success)
					{
						break;
					}
					Status = ProcessWindow(WindowStart, WindowEnd);
					if (Status != EGrowthRecordStatus::Success)
					{
						break;
					}
				}
			}
			if (Status != EGrowthRecordStatus::Success)
			{
				check(FirstFailingRecordIndex != INDEX_NONE);
				OutError = GetGrowthErrorText(Status);
				return false;
			}
		}

		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, ApplyGrowth_Patches);
			int32 FirstFailingRecordIndex = INDEX_NONE;
			int32 ActiveWindowStart = 0;
			TArray<uint32> WindowQuantums;
			const auto ProcessWindow = [&](const int32 StartRecordIndex, const int32 EndRecordIndex)
			{
				ActiveWindowStart = StartRecordIndex;
				return ProcessGrowthBatches(
				TEXT("arcecotope.GrowthPatches"),
				TEXT("ApplyGrowth_PatchesParallelBatches"),
				TEXT("ApplyGrowth_PatchesSerialBatches"),
				Snapshot.Patches,
				StartRecordIndex,
				EndRecordIndex,
				ParallelGrowthSettings,
				Diagnostics,
				[&](FArcForestPatchRecord& Record, const int32 RecordIndex, const int32 /*BatchIndex*/)
				{
					const int32 SpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
					const FArcForestFrozenSpeciesSetup& Model = Species[SpeciesIndex];
					int32 BaseCellIndex = INDEX_NONE;
					if (!TryGetRepresentativeBaseCellForRecordCell(
							SiteMetadata, BaseValidityMask, Record.CellIndex, BaseCellIndex))
					{
						return EGrowthRecordStatus::PatchInvalidRecordCell;
					}
					const uint32 HabitatQuantum = WindowQuantums[RecordIndex - ActiveWindowStart];
					const double Resource =
						CalculateResourceFactorFromQuantum(HabitatQuantum,
												Model,
												CalculateLightAtHeight(CellOccupancy,
																	   Record.CellIndex,
																	   static_cast<double>(Record.MeanHeightCm),
																	   Model,
																	   Input.UnderstoryShade));
					const double GrowthFactor = Resource * CalculateSizeLimitation(Record.MeanHeightCm, Model);
					Record.MeanHeightCm = static_cast<float>(FMath::Min(
						static_cast<double>(Model.MaxHeightCm),
						static_cast<double>(Record.MeanHeightCm) + Model.MaxAnnualHeightGrowthCm * GrowthFactor));
					if (!FMath::IsFinite(Record.MeanAgeYears + 1.0f))
					{
						return EGrowthRecordStatus::PatchMeanAgeOverflow;
					}
					Record.MeanAgeYears += 1.0f;
					Record.MeanVigor =
						static_cast<float>(FMath::Clamp(Model.VigorSmoothingRetention * Record.MeanVigor +
															(1.0 - Model.VigorSmoothingRetention) * Resource,
														0.0,
														1.0));
					double DbhCm = 0.0;
					double BiomassPerEquivalentKg = 0.0;
					if (!TryCalculateDbh(Model, Record.MeanHeightCm, DbhCm) ||
						!TryCalculateBiomass(Model, DbhCm, BiomassPerEquivalentKg) ||
						BiomassPerEquivalentKg * Record.StemOrCoverEquivalent > MAX_flt)
					{
						return EGrowthRecordStatus::PatchInvalidAllometry;
					}
					Record.BiomassKg = static_cast<float>(BiomassPerEquivalentKg * Record.StemOrCoverEquivalent);
					return EGrowthRecordStatus::Success;
				},
				FirstFailingRecordIndex);
			};
			EGrowthRecordStatus Status = EGrowthRecordStatus::Success;
			{
				const int32 MaximumWindowCount = MaximumFactoredGrowthWindowCount;
				for (int32 WindowStart = 0; WindowStart < Snapshot.Patches.Num(); WindowStart += MaximumWindowCount)
				{
					const int32 WindowEnd = FMath::Min(WindowStart + MaximumWindowCount, Snapshot.Patches.Num());
					if (!EvaluateFactoredGrowthWindow(
							Snapshot.Patches,
							WindowStart,
							WindowEnd,
							FactoredQueryContext,
							[&](const FArcForestPatchRecord& Record, int32& OutSpeciesIndex,
								int32& OutBaseCellIndex, int32& OutRecordCellIndex)
							{
								OutSpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
								OutRecordCellIndex = Record.CellIndex;
								return TryGetRepresentativeBaseCellForRecordCell(
									SiteMetadata, BaseValidityMask, Record.CellIndex, OutBaseCellIndex)
									? EGrowthRecordStatus::Success
									: EGrowthRecordStatus::PatchInvalidRecordCell;
							},
							WindowQuantums,
							Status,
							FirstFailingRecordIndex,
							InOutQueryTelemetry,
							OutError))
					{
						return false;
					}
					if (Status != EGrowthRecordStatus::Success)
					{
						break;
					}
					Status = ProcessWindow(WindowStart, WindowEnd);
					if (Status != EGrowthRecordStatus::Success)
					{
						break;
					}
				}
			}
			if (Status != EGrowthRecordStatus::Success)
			{
				check(FirstFailingRecordIndex != INDEX_NONE);
				OutError = GetGrowthErrorText(Status);
				return false;
			}
		}
		if (bGrowthDiagnosticsEnabled)
		{
			MergeGrowthDiagnosticsBatches(
				DiagnosticBatches, InOutYearStats.GrowthRate, InOutYearStats.GrowthDrivers);
		}
		return true;
	}

	/** True when a dead stem is thick enough to leave a visible log or snag. */
	bool IsCoarseWoodyDebris(const FArcEcotopeDeadwoodSettings& Settings, const double DbhCm)
	{
		return DbhCm >= static_cast<double>(Settings.CoarseWoodyDebrisMinimumDbhCm);
	}

	struct FDeadwoodBudgetCounters
	{
		int32 RecordsRefusedByDeadwoodCap = 0;
		int32 RecordsRefusedByBranchCap = 0;
		int32 BranchPositionFallbacks = 0;
		int32 FallSatelliteGroupsSkipped = 0;
	};

	uint64 MakeAdditionalDeadwoodRecordId(
		const FArcForestWorldSnapshot& Snapshot,
		const FArcForestRunInput& Input,
		const int32 Year,
		const FGuid& SpeciesId,
		const int32 BaseCellIndex,
		const uint64 SourceRecordId,
		const int32 DecisionOrdinal)
	{
		return UE::ArcForest::MakeAnnualKey(
			Snapshot.SimulationId,
			Input.Seed,
			Year,
			UE::ArcForest::EDeterministicDomain::DeadwoodRecordIdentity,
			SpeciesId,
			BaseCellIndex,
			SourceRecordId,
			DecisionOrdinal,
			/*Salt*/ 0);
	}

	bool IsLyingDeadwoodForm(const EArcForestDeadwoodType Type)
	{
		return Type == EArcForestDeadwoodType::Log || Type == EArcForestDeadwoodType::Uprooted;
	}

	FQuat MakeStemBearingRotation(const double BearingRadians)
	{
		return FRotationMatrix::MakeFromZ(
			FVector(FMath::Cos(BearingRadians), FMath::Sin(BearingRadians), 0.0)).ToQuat();
	}

	double MakeFallenStemBearing(
		const FArcForestWorldSnapshot& Snapshot,
		const FArcForestRunInput& Input,
		const int32 Year,
		const FGuid& SpeciesId,
		const int32 BaseCellIndex,
		const uint64 RecordId,
		const TOptional<double>& EventWindBearingRadians)
	{
		const uint64 BearingKey = UE::ArcForest::MakeAnnualKey(
			Snapshot.SimulationId,
			Input.Seed,
			Year,
			UE::ArcForest::EDeterministicDomain::DeadwoodFallBearing,
			SpeciesId,
			BaseCellIndex,
			RecordId,
			0,
			/*Salt*/ 0);
		const double BearingUnit = UE::ArcForest::KeyToUnitOpen(BearingKey);
		if (!EventWindBearingRadians.IsSet())
		{
			return BearingUnit * UE_DOUBLE_TWO_PI;
		}
		const double SpreadRadians =
			FMath::DegreesToRadians(UE::ArcEcotope::Deadwood::FallBearingSpreadDeg);
		return EventWindBearingRadians.GetValue() + (BearingUnit * 2.0 - 1.0) * SpreadRadians;
	}

	FQuat MakeFallenStemRotation(
		const FArcForestWorldSnapshot& Snapshot,
		const FArcForestRunInput& Input,
		const int32 Year,
		const FGuid& SpeciesId,
		const int32 BaseCellIndex,
		const uint64 RecordId,
		const TOptional<double>& EventWindBearingRadians)
	{
		return MakeStemBearingRotation(MakeFallenStemBearing(
			Snapshot, Input, Year, SpeciesId, BaseCellIndex, RecordId, EventWindBearingRadians));
	}

	void AppendDeadwoodRecord(
		FArcForestWorldSnapshot& Snapshot,
		const FArcForestRunInput& Input,
		const FArcEcotopeDecayGrid& DecayGrid,
		const FArcForestIndividualRecord& Record,
		const uint64 RecordId,
		const EArcForestDeadwoodType Type,
		const int32 RecordCellIndex,
		const FVector& LocalPosition,
		const FQuat& StemRotation,
		const double WidthCm,
		const double StemLengthCm,
		const double MassKg,
		const int32 Year)
	{
		const FArcEcotopeDeadwoodSettings& Settings = Input.DeadwoodSettings;
		FArcForestDeadwoodRecord& Deadwood = Snapshot.Deadwood.AddDefaulted_GetRef();
		Deadwood.Id = RecordId;
		Deadwood.SourceEntityId = Record.Id;
		Deadwood.SpeciesId = Record.SpeciesId;
		Deadwood.Type = Type;
		Deadwood.LocalTransform = FTransform(StemRotation, LocalPosition);
		const double ClampedWidthCm = FMath::Max(1.0, WidthCm);
		Deadwood.DimensionsCm = FVector(ClampedWidthCm, ClampedWidthCm, FMath::Max(1.0, StemLengthCm));
		Deadwood.DeathYear = Year;
		Deadwood.InitialBiomassKg = static_cast<float>(FMath::Max(0.0, MassKg));
		Deadwood.RetireYear = CalculateRetireYear(
			Year,
			static_cast<double>(Deadwood.InitialBiomassKg),
			static_cast<double>(Settings.MinimumTrackedDeadwoodKg),
			CalculateDecayedFraction(
				Settings.DeadwoodDecayPerYear, DecayGrid.DecayModerator[RecordCellIndex]));
		Deadwood.FallYear = MAX_int32;
		if (Type != EArcForestDeadwoodType::Snag)
		{
			return;
		}
		const uint64 FallKey = UE::ArcForest::MakeAnnualKey(
			Snapshot.SimulationId,
			Input.Seed,
			Year,
			UE::ArcForest::EDeterministicDomain::DeadwoodSnagFall,
			Record.SpeciesId,
			RecordCellIndex,
			RecordId,
			0,
			/*Salt*/ 0);
		Deadwood.FallYear = CalculateGeometricEventYear(
			Year,
			static_cast<double>(Settings.SnagFallProbabilityPerYear),
			UE::ArcForest::KeyToUnitOpen(FallKey));
	}

	struct FBranchGeometry
	{
		double DiameterCm = 0.0;
		double LengthCm = 0.0;
		double MassKg = 0.0;
	};

	FBranchGeometry CalculateBranchGeometry(const FArcForestIndividualRecord& Record)
	{
		constexpr double DiameterFraction = UE::ArcEcotope::Deadwood::BranchDiameterFractionOfDbh;
		constexpr double LengthFraction = UE::ArcEcotope::Deadwood::BranchLengthFractionOfHeight;
		FBranchGeometry Geometry;
		Geometry.DiameterCm = DiameterFraction * static_cast<double>(Record.DbhCm);
		Geometry.LengthCm = LengthFraction * static_cast<double>(Record.HeightCm);
		Geometry.MassKg = static_cast<double>(Record.BiomassKg)
			* DiameterFraction * DiameterFraction * LengthFraction;
		return Geometry;
	}

	FVector ResolveBranchPosition(
		const FArcEcotopeSiteMetadata& SiteMetadata,
		const TConstArrayView<uint8> BaseValidityMask,
		const FVector& ParentPosition,
		const int32 ParentRecordCellIndex,
		const FVector& OffsetPosition,
		int32& OutRecordCellIndex,
		FDeadwoodBudgetCounters& InOutCounters)
	{
		int32 OffsetBaseCellIndex = INDEX_NONE;
		int32 OffsetRecordCellIndex = INDEX_NONE;
		if (TryGetBaseAndRecordCellIndex(
				SiteMetadata, BaseValidityMask, OffsetPosition, OffsetBaseCellIndex, OffsetRecordCellIndex))
		{
			OutRecordCellIndex = OffsetRecordCellIndex;
			return OffsetPosition;
		}
		++InOutCounters.BranchPositionFallbacks;
		OutRecordCellIndex = ParentRecordCellIndex;
		return ParentPosition;
	}

	/** Branch is exempt from the CoarseWoodyDebrisMinimumDbhCm gate: at a realistic fraction of parent DBH it would clear 10 cm only under a parent above roughly 67 cm, so the gate would delete the form. */
	bool TryEmitBranchRecord(
		const FArcForestRunInput& Input,
		const FArcEcotopeSiteMetadata& SiteMetadata,
		const TConstArrayView<uint8> BaseValidityMask,
		const FArcEcotopeDecayGrid& DecayGrid,
		const int32 Year,
		const FArcForestIndividualRecord& Record,
		const int32 BaseCellIndex,
		const int32 RecordCellIndex,
		const FBranchGeometry& Geometry,
		const uint64 BranchId,
		const FVector& OffsetPosition,
		const TOptional<double>& EventWindBearingRadians,
		FArcForestWorldSnapshot& Snapshot,
		int32& InOutBranchRecordCount,
		FDeadwoodBudgetCounters& InOutCounters,
		FString& OutError)
	{
		if (InOutBranchRecordCount >= Input.MaxBranchRecords)
		{
			++InOutCounters.RecordsRefusedByBranchCap;
			return true;
		}
		int32 BranchRecordCellIndex = INDEX_NONE;
		const FVector BranchPosition = ResolveBranchPosition(
			SiteMetadata,
			BaseValidityMask,
			Record.LocalPosition,
			RecordCellIndex,
			OffsetPosition,
			BranchRecordCellIndex,
			InOutCounters);
		AppendDeadwoodRecord(
			Snapshot,
			Input,
			DecayGrid,
			Record,
			BranchId,
			EArcForestDeadwoodType::Branch,
			BranchRecordCellIndex,
			BranchPosition,
			MakeFallenStemRotation(
				Snapshot, Input, Year, Record.SpeciesId, BaseCellIndex, BranchId, EventWindBearingRadians),
			Geometry.DiameterCm,
			Geometry.LengthCm,
			Geometry.MassKg,
			Year);
		++InOutBranchRecordCount;
		return true;
	}

	bool EmitFallSatelliteBranches(
		const FArcForestRunInput& Input,
		const FArcEcotopeSiteMetadata& SiteMetadata,
		const TConstArrayView<uint8> BaseValidityMask,
		const FArcEcotopeDecayGrid& DecayGrid,
		const int32 Year,
		const FArcForestIndividualRecord& Record,
		const int32 BaseCellIndex,
		const int32 RecordCellIndex,
		const double StemBearing,
		const TOptional<double>& EventWindBearingRadians,
		FArcForestWorldSnapshot& Snapshot,
		int32& InOutBranchRecordCount,
		FDeadwoodBudgetCounters& InOutCounters,
		FString& OutError)
	{
		const FArcEcotopeDeadwoodSettings& Settings = Input.DeadwoodSettings;
		constexpr int32 SatelliteCount = UE::ArcEcotope::Deadwood::FallSatelliteBranchCount;
		const FBranchGeometry Geometry = CalculateBranchGeometry(Record);
		if (!FMath::IsFinite(Geometry.MassKg)
			|| Geometry.MassKg < static_cast<double>(Settings.MinimumTrackedDeadwoodKg))
		{
			return true;
		}
		const FVector StemAxis(FMath::Cos(StemBearing), FMath::Sin(StemBearing), 0.0);
		const FVector LateralAxis(-StemAxis.Y, StemAxis.X, 0.0);
		const double StemLengthCm = FMath::Max(0.0, static_cast<double>(Record.HeightCm));
		constexpr double SpreadCm = UE::ArcEcotope::Deadwood::FallSatelliteSpreadCm;
		for (int32 SatelliteIndex = 0; SatelliteIndex < SatelliteCount; ++SatelliteIndex)
		{
			const uint64 BranchId = MakeAdditionalDeadwoodRecordId(
				Snapshot,
				Input,
				Year,
				Record.SpeciesId,
				BaseCellIndex,
				Record.Id,
				/*DecisionOrdinal*/ 2 + SatelliteIndex);
			const uint64 LateralKey = UE::ArcForest::MakeAnnualKey(
				Snapshot.SimulationId,
				Input.Seed,
				Year,
				UE::ArcForest::EDeterministicDomain::BranchShedPlacement,
				Record.SpeciesId,
				BaseCellIndex,
				Record.Id,
				2 + SatelliteIndex,
				/*Salt*/ 0);
			const double AlongFraction =
				static_cast<double>(SatelliteIndex + 1) / static_cast<double>(SatelliteCount + 1);
			const FVector OffsetPosition = Record.LocalPosition
				+ StemAxis * (AlongFraction * StemLengthCm)
				+ LateralAxis * ((UE::ArcForest::KeyToUnitOpen(LateralKey) * 2.0 - 1.0) * SpreadCm);
			if (!TryEmitBranchRecord(
					Input,
					SiteMetadata,
					BaseValidityMask,
					DecayGrid,
					Year,
					Record,
					BaseCellIndex,
					RecordCellIndex,
					Geometry,
					BranchId,
					OffsetPosition,
					EventWindBearingRadians,
					Snapshot,
					InOutBranchRecordCount,
					InOutCounters,
					OutError))
			{
				return false;
			}
		}
		return true;
	}

	enum class EArcForestDeathCause : uint8
	{
		Background,
		SelfThinning,
		Wind,
		Fire,
	};

	/** TypeDrawSalt closes the annual key for the uproot-versus-snap draw. */
	bool RouteIndividualDeath(
		const FArcForestRunInput& Input,
		const FArcEcotopeSiteMetadata& SiteMetadata,
		const TConstArrayView<uint8> BaseValidityMask,
		const FArcEcotopeDecayGrid& DecayGrid,
		const int32 Year,
		const FArcForestIndividualRecord& Record,
		const int32 BaseCellIndex,
		const int32 RecordCellIndex,
		const EArcForestDeathCause DeathCause,
		const int64 TypeDrawSalt,
		FArcForestWorldSnapshot& Snapshot,
		TSet<uint64>& InOutDeadwoodIds,
		int32& InOutBranchRecordCount,
		FDeadwoodBudgetCounters& InOutCounters,
		FString& OutError,
		const double BiomassOverrideKg = -1.0,
		const TOptional<double>& EventWindBearingRadians = TOptional<double>())
	{
		const double BiomassKg = BiomassOverrideKg >= 0.0 ? BiomassOverrideKg : Record.BiomassKg;
		if (!FMath::IsFinite(BiomassKg) || BiomassKg <= 0.0)
		{
			OutError = TEXT("ArcForest mortality produced invalid individual biomass.");
			return false;
		}
		if (!IsCoarseWoodyDebris(Input.DeadwoodSettings, Record.DbhCm))
		{
			return true;
		}
		EArcForestDeadwoodType DeadwoodType = EArcForestDeadwoodType::Snag;
		bool bSnapped = false;
		if (DeathCause == EArcForestDeathCause::Wind)
		{
			const uint64 TypeKey = UE::ArcForest::MakeAnnualKey(
				Snapshot.SimulationId,
				Input.Seed,
				Year,
				UE::ArcForest::EDeterministicDomain::GrowthDeadwoodType,
				Record.SpeciesId,
				BaseCellIndex,
				Record.Id,
				0,
				TypeDrawSalt);
			bSnapped = UE::ArcForest::KeyToUnitOpen(TypeKey)
				>= static_cast<double>(Input.DeadwoodSettings.UprootFractionOfWindKills);
			DeadwoodType = bSnapped ? EArcForestDeadwoodType::Log : EArcForestDeadwoodType::Uprooted;
		}

		const double MinimumTrackedKg = static_cast<double>(Input.DeadwoodSettings.MinimumTrackedDeadwoodKg);
		const double StemWidthCm = static_cast<double>(Record.DbhCm);
		const int32 NonBranchRecordCount = Snapshot.Deadwood.Num() - InOutBranchRecordCount;
		const uint64 FallenRecordId = bSnapped
			? MakeAdditionalDeadwoodRecordId(
				Snapshot, Input, Year, Record.SpeciesId, BaseCellIndex, Record.Id, /*DecisionOrdinal*/ 1)
			: Record.Id;
		const double StemBearing = IsLyingDeadwoodForm(DeadwoodType)
			? MakeFallenStemBearing(
				Snapshot, Input, Year, Record.SpeciesId, BaseCellIndex, FallenRecordId, EventWindBearingRadians)
			: 0.0;
		bool bProducedFallenRecord = false;
		if (bSnapped)
		{
			constexpr double BreakFraction = UE::ArcEcotope::Deadwood::SnapBreakHeightFraction;
			const double StumpKg = BiomassKg * BreakFraction;
			const double LogKg = BiomassKg - StumpKg;
			// Both slots are reserved before either record exists: testing the cap per record lets the array fill between them and leaves a stump whose log was never created.
			const bool bPairFitsRecordBudget = NonBranchRecordCount + 2 <= Input.MaxDeadwoodRecords;
			if (bPairFitsRecordBudget
				&& !InOutDeadwoodIds.Contains(Record.Id)
				&& (StumpKg >= MinimumTrackedKg || LogKg >= MinimumTrackedKg))
			{
				const double StumpHeightCm = static_cast<double>(Record.HeightCm) * BreakFraction;
				const double LogLengthCm = static_cast<double>(Record.HeightCm) - StumpHeightCm;
				if (StumpKg >= MinimumTrackedKg)
				{
					AppendDeadwoodRecord(
						Snapshot,
						Input,
						DecayGrid,
						Record,
						Record.Id,
						EArcForestDeadwoodType::Stump,
						RecordCellIndex,
						Record.LocalPosition,
						FQuat::Identity,
						StemWidthCm,
						StumpHeightCm,
						StumpKg,
						Year);
				}
				if (LogKg >= MinimumTrackedKg)
				{
					AppendDeadwoodRecord(
						Snapshot,
						Input,
						DecayGrid,
						Record,
						FallenRecordId,
						EArcForestDeadwoodType::Log,
						RecordCellIndex,
						Record.LocalPosition,
						MakeStemBearingRotation(StemBearing),
						StemWidthCm,
						LogLengthCm,
						LogKg,
						Year);
				}
				InOutDeadwoodIds.Add(Record.Id);
				InOutDeadwoodIds.Add(FallenRecordId);
				bProducedFallenRecord = true;
			}
			else
			{
				if (!bPairFitsRecordBudget)
				{
					InOutCounters.RecordsRefusedByDeadwoodCap += 2;
				}
			}
		}
		else
		{
			const bool bFitsRecordBudget = NonBranchRecordCount < Input.MaxDeadwoodRecords;
			if (BiomassKg >= MinimumTrackedKg
				&& bFitsRecordBudget
				&& !InOutDeadwoodIds.Contains(Record.Id))
			{
				const FQuat StemRotation = IsLyingDeadwoodForm(DeadwoodType)
					? MakeStemBearingRotation(StemBearing)
					: FQuat::Identity;
				AppendDeadwoodRecord(
					Snapshot,
					Input,
					DecayGrid,
					Record,
					Record.Id,
					DeadwoodType,
					RecordCellIndex,
					Record.LocalPosition,
					StemRotation,
					StemWidthCm,
					static_cast<double>(Record.HeightCm),
					BiomassKg,
					Year);
				InOutDeadwoodIds.Add(Record.Id);
				bProducedFallenRecord = true;
			}
			else
			{
				if (!bFitsRecordBudget)
				{
					++InOutCounters.RecordsRefusedByDeadwoodCap;
				}
			}
		}

		if (DeathCause != EArcForestDeathCause::Wind)
		{
			return true;
		}
		if (!bProducedFallenRecord)
		{
			++InOutCounters.FallSatelliteGroupsSkipped;
			return true;
		}
		return EmitFallSatelliteBranches(
			Input,
			SiteMetadata,
			BaseValidityMask,
			DecayGrid,
			Year,
			Record,
			BaseCellIndex,
			RecordCellIndex,
			StemBearing,
			EventWindBearingRadians,
			Snapshot,
			InOutBranchRecordCount,
			InOutCounters,
			OutError);
	}

	template <typename RecordType>
	void CompactRemovedRecords(TArray<RecordType>& Records, const TArray<uint8>& RemovalMask)
	{
		int32 WriteIndex = 0;
		for (int32 ReadIndex = 0; ReadIndex < Records.Num(); ++ReadIndex)
		{
			if (RemovalMask[ReadIndex] != 0)
			{
				continue;
			}
			if (WriteIndex != ReadIndex)
			{
				Records[WriteIndex] = MoveTemp(Records[ReadIndex]);
			}
			++WriteIndex;
		}
		Records.SetNum(WriteIndex, EAllowShrinking::No);
	}

	double CalculateCoverLogisticRate(const FArcForestFrozenSpeciesSetup& Species)
	{
		return FMath::Lerp(
			MinimumCoverLogisticRate,
			MaximumCoverLogisticRate,
			FMath::Clamp(static_cast<double>(Species.GrowthSpeed), 0.0, 1.0));
	}

	/** Reads the BuildCoverFloorCapacity array, whose layout is SpeciesIndex * RecordCellCount + RecordCellIndex. */
	double GetCoverFloorCapacity(
		const TArray<double>& CoverFloorCapacity,
		const int32 SpeciesIndex,
		const int32 RecordCellCount,
		const int32 RecordCellIndex)
	{
		if (SpeciesIndex < 0 || RecordCellIndex < 0 || RecordCellIndex >= RecordCellCount)
		{
			return 0.0;
		}
		const int64 Index = static_cast<int64>(SpeciesIndex) * RecordCellCount + RecordCellIndex;
		return Index < static_cast<int64>(CoverFloorCapacity.Num())
			? CoverFloorCapacity[static_cast<int32>(Index)]
			: 0.0;
	}

	double GetCoverSuitability(
		const TArray<double>& CoverSuitability,
		const int32 SpeciesIndex,
		const int32 RecordCellCount,
		const int32 RecordCellIndex)
	{
		if (SpeciesIndex < 0 || RecordCellIndex < 0 || RecordCellIndex >= RecordCellCount)
		{
			return 0.0;
		}
		const int64 Index = static_cast<int64>(SpeciesIndex) * RecordCellCount + RecordCellIndex;
		return Index < static_cast<int64>(CoverSuitability.Num())
			? CoverSuitability[static_cast<int32>(Index)]
			: 0.0;
	}

	bool ApplyMortality(
		const FArcForestRunInput& Input,
		const TConstArrayView<uint8> BaseValidityMask,
		const TConstArrayView<float> SubstrateMobilityByBaseCell,
		const TArray<FArcForestFrozenSpeciesSetup>& Species,
		const TMap<FGuid, int32>& SpeciesIndices,
		const TArray<double>& CoverFloorCapacity,
		const FArcEcotopeDecayGrid& DecayGrid,
		const int32 Year,
		UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics,
		const bool bGrowthDiagnosticsEnabled,
		FYearDiagnostics& InOutYearStats,
		FArcForestWorldSnapshot& Snapshot,
		FDeadwoodBudgetCounters& InOutCounters,
		FString& OutError)
	{
		const FArcEcotopeSiteMetadata& SiteMetadata = GetSiteMetadata(Input);
		const int32 RecordCellCount = SiteMetadata.GetLevelCellCount(SiteMetadata.RecordGridLevel);
		if (SubstrateMobilityByBaseCell.Num() != BaseValidityMask.Num())
		{
			OutError = TEXT("ArcForest mortality received a substrate mobility array that is not base-cell shaped.");
			return false;
		}
		TSet<uint64> DeadwoodIds;
		int32 BranchRecordCount = 0;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, ApplyMortality_DeadwoodIndex);
			for (const FArcForestDeadwoodRecord& Record : Snapshot.Deadwood)
			{
				DeadwoodIds.Add(Record.Id);
				if (Record.Type == EArcForestDeadwoodType::Branch)
				{
					++BranchRecordCount;
				}
			}
		}

		TArray<uint8> RemovedIndividuals;
		TArray<uint8> RemovedCohorts;
		TArray<uint8> RemovedPatches;
		RemovedIndividuals.Init(0, Snapshot.Individuals.Num());
		RemovedCohorts.Init(0, Snapshot.Cohorts.Num());
		RemovedPatches.Init(0, Snapshot.Patches.Num());

		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, ApplyMortality_Individuals);
			for (int32 RecordIndex = Snapshot.Individuals.Num() - 1; RecordIndex >= 0; --RecordIndex)
			{
				const FArcForestIndividualRecord& Record = Snapshot.Individuals[RecordIndex];
				const FArcForestFrozenSpeciesSetup& Model = Species[SpeciesIndices.FindChecked(Record.SpeciesId)];
				int32 BaseCellIndex = INDEX_NONE;
				int32 RecordCellIndex = INDEX_NONE;
				if (!TryGetBaseAndRecordCellIndex(
					SiteMetadata,
					BaseValidityMask,
					Record.LocalPosition,
					BaseCellIndex,
					RecordCellIndex))
				{
					OutError = TEXT("ArcForest mortality found an individual outside its frozen site.");
					return false;
				}
				if (!SubstrateMobilityByBaseCell.IsValidIndex(BaseCellIndex))
				{
					OutError = TEXT("ArcForest mortality could not read substrate mobility for an individual.");
					return false;
				}
				double BaselineMortality = 0.0;
				double AgeMortality = 0.0;
				double ResourceMortality = 0.0;
				const double BaseProbability = CalculateMortalityProbability(
					Model,
					Record.AgeYears,
					Record.Vigor,
					Record.HeightCm,
					&BaselineMortality,
					&AgeMortality,
					&ResourceMortality);
				const double ComposedProbability = ComposeSubstrateMobilityMortality(
					BaseProbability,
					SubstrateMobilityByBaseCell[BaseCellIndex],
					Model.MobilityMortalitySensitivity);
				const int64 QuantizedProbability = QuantizeProbability(ComposedProbability);
				const uint64 MortalityKey = UE::ArcForest::MakeAnnualKey(
					Snapshot.SimulationId,
					Input.Seed,
					Year,
					UE::ArcForest::EDeterministicDomain::GrowthMortality,
					Record.SpeciesId,
					BaseCellIndex,
					Record.Id,
					0,
					QuantizedProbability);
				if (UE::ArcForest::KeyToUnitOpen(MortalityKey) >= DequantizeProbability(QuantizedProbability))
				{
					continue;
				}

				if (bGrowthDiagnosticsEnabled)
				{
					const int32 SpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
					if (InOutYearStats.MortalityBreakdown.IsValidIndex(SpeciesIndex))
					{
						const int32 HeightBucket = CalculateStructureHeightBucket(
							static_cast<double>(Record.HeightCm), static_cast<double>(Model.MaxHeightCm));
						AccumulateMortalityCause(
							InOutYearStats.MortalityBreakdown[SpeciesIndex],
							SelectMortalityCause(
								BaselineMortality,
								AgeMortality,
								ResourceMortality,
								ComposedProbability - BaseProbability),
							HeightBucket,
							1);
					}
				}
				if (!RouteIndividualDeath(
						Input,
						SiteMetadata,
						BaseValidityMask,
						DecayGrid,
						Year,
						Record,
						BaseCellIndex,
						RecordCellIndex,
						EArcForestDeathCause::Background,
						QuantizedProbability,
						Snapshot,
						DeadwoodIds,
						BranchRecordCount,
						InOutCounters,
						OutError))
				{
					return false;
				}
				RemovedIndividuals[RecordIndex] = 1;
			}
		}

		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, ApplyMortality_Cohorts);
			for (int32 RecordIndex = Snapshot.Cohorts.Num() - 1; RecordIndex >= 0; --RecordIndex)
			{
				FArcForestCohortRecord& Record = Snapshot.Cohorts[RecordIndex];
				const FArcForestFrozenSpeciesSetup& Model = Species[SpeciesIndices.FindChecked(Record.SpeciesId)];
				const int32 MinCornerBaseCellIndex = SiteMetadata.MapCellIndexBetweenLevels(
					Record.CellIndex, SiteMetadata.RecordGridLevel, 0);
				if (MinCornerBaseCellIndex == INDEX_NONE
					|| !SubstrateMobilityByBaseCell.IsValidIndex(MinCornerBaseCellIndex))
				{
					OutError = TEXT("ArcForest mortality could not map a cohort record cell onto the base grid.");
					return false;
				}
				double BaselineMortality = 0.0;
				double AgeMortality = 0.0;
				double ResourceMortality = 0.0;
				const double BaseProbability = CalculateMortalityProbability(
					Model,
					Record.MeanAgeYears,
					Record.MeanVigor,
					Record.MeanHeightCm,
					&BaselineMortality,
					&AgeMortality,
					&ResourceMortality);
				const double ComposedProbability = ComposeSubstrateMobilityMortality(
					BaseProbability,
					SubstrateMobilityByBaseCell[MinCornerBaseCellIndex],
					Model.MobilityMortalitySensitivity);
				const int64 QuantizedProbability = QuantizeProbability(ComposedProbability);
				const uint64 MortalityKey = UE::ArcForest::MakeAnnualKey(
					Snapshot.SimulationId,
					Input.Seed,
					Year,
					UE::ArcForest::EDeterministicDomain::GrowthMortality,
					Record.SpeciesId,
					Record.CellIndex,
					Record.Id,
					1,
					QuantizedProbability);
				int64 DeathCount = 0;
				if (!UE::ArcForest::TryRoundNonNegative(
					static_cast<double>(Record.StemCount) * DequantizeProbability(QuantizedProbability),
					MortalityKey,
					DeathCount))
				{
					OutError = TEXT("ArcForest cohort mortality count was not representable.");
					return false;
				}
				DeathCount = FMath::Min(DeathCount, Record.StemCount);
				if (bGrowthDiagnosticsEnabled && DeathCount > 0)
				{
					const int32 SpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
					if (InOutYearStats.MortalityBreakdown.IsValidIndex(SpeciesIndex))
					{
						const int32 HeightBucket = CalculateStructureHeightBucket(
							static_cast<double>(Record.MeanHeightCm), static_cast<double>(Model.MaxHeightCm));
						AccumulateMortalityCause(
							InOutYearStats.MortalityBreakdown[SpeciesIndex],
							SelectMortalityCause(
								BaselineMortality,
								AgeMortality,
								ResourceMortality,
								ComposedProbability - BaseProbability),
							HeightBucket,
							DeathCount);
					}
				}
				const double LostBiomass = Record.BiomassKg
					* (static_cast<double>(DeathCount) / static_cast<double>(Record.StemCount));
				if (DeathCount == Record.StemCount)
				{
					RemovedCohorts[RecordIndex] = 1;
				}
				else
				{
					Record.StemCount -= DeathCount;
					Record.BiomassKg = static_cast<float>(FMath::Max(0.0, Record.BiomassKg - LostBiomass));
				}
			}
		}

		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, ApplyMortality_Patches);
			for (int32 RecordIndex = Snapshot.Patches.Num() - 1; RecordIndex >= 0; --RecordIndex)
			{
				FArcForestPatchRecord& Record = Snapshot.Patches[RecordIndex];
				const FArcForestFrozenSpeciesSetup& Model = Species[SpeciesIndices.FindChecked(Record.SpeciesId)];
				const int32 MinCornerBaseCellIndex = SiteMetadata.MapCellIndexBetweenLevels(
					Record.CellIndex, SiteMetadata.RecordGridLevel, 0);
				if (MinCornerBaseCellIndex == INDEX_NONE
					|| !SubstrateMobilityByBaseCell.IsValidIndex(MinCornerBaseCellIndex))
				{
					OutError = TEXT("ArcForest mortality could not map a patch record cell onto the base grid.");
					return false;
				}
				const int64 QuantizedProbability = QuantizeProbability(
					ComposeSubstrateMobilityMortality(
						CalculateMortalityProbability(Model, Record.MeanAgeYears, Record.MeanVigor, Record.MeanHeightCm),
						SubstrateMobilityByBaseCell[MinCornerBaseCellIndex],
						Model.MobilityMortalitySensitivity));
				const uint64 MortalityKey = UE::ArcForest::MakeAnnualKey(
					Snapshot.SimulationId,
					Input.Seed,
					Year,
					UE::ArcForest::EDeterministicDomain::GrowthMortality,
					Record.SpeciesId,
					Record.CellIndex,
					Record.Id,
					2,
					QuantizedProbability);
				const double RealizedFraction = FMath::Clamp(
					DequantizeProbability(QuantizedProbability)
						* (0.5 + UE::ArcForest::KeyToUnitOpen(MortalityKey)),
					0.0,
					1.0);
				const double LostBiomass = Record.BiomassKg * RealizedFraction;
				const int64 StemEquivalentBeforeMortality =
					FMath::CeilToInt64(FMath::Max(0.0f, Record.StemOrCoverEquivalent));
				Record.StemOrCoverEquivalent = static_cast<float>(
					Record.StemOrCoverEquivalent * (1.0 - RealizedFraction));
				Record.BiomassKg = static_cast<float>(FMath::Max(0.0, Record.BiomassKg - LostBiomass));
				bool bCoverFloorShrinkApplied = false;

				// The shared floor budget only bounds capacity, so a patch left above it after a species
				// arrived or GroundCoverDensity fell needs a restoring force; the growth pass cannot own it
				// because the dead biomass sinks are re-initialised here.
				const double SurvivingEquivalent = static_cast<double>(Record.StemOrCoverEquivalent);
				if (!Model.bIndividualizes && SurvivingEquivalent > 0.0)
				{
					const double Capacity = GetCoverFloorCapacity(
						CoverFloorCapacity,
						SpeciesIndices.FindChecked(Record.SpeciesId),
						RecordCellCount,
						Record.CellIndex);
					const double LogisticRate = CalculateCoverLogisticRate(Model);
					double ShrunkEquivalent = SurvivingEquivalent;
					if (Capacity > 0.0)
					{
						if (SurvivingEquivalent > Capacity)
						{
							ShrunkEquivalent = FMath::Max(
								SurvivingEquivalent
									+ LogisticRate * SurvivingEquivalent * (1.0 - SurvivingEquivalent / Capacity),
								Capacity);
						}
					}
					else
					{
						// The logistic form is undefined at zero capacity, so cover decays at the full rate instead;
						// that is what makes GroundCoverDensity = 0 clear standing cover rather than freeze it.
						ShrunkEquivalent = SurvivingEquivalent - LogisticRate * SurvivingEquivalent;
					}
					if (FMath::IsFinite(ShrunkEquivalent) && ShrunkEquivalent < SurvivingEquivalent)
					{
						const double ShrinkFraction = FMath::Clamp(
							1.0 - FMath::Max(ShrunkEquivalent, 0.0) / SurvivingEquivalent, 0.0, 1.0);
						const double ShrunkBiomass = Record.BiomassKg * ShrinkFraction;
						Record.StemOrCoverEquivalent = static_cast<float>(FMath::Max(ShrunkEquivalent, 0.0));
						Record.BiomassKg =
							static_cast<float>(FMath::Max(0.0, Record.BiomassKg - ShrunkBiomass));
						bCoverFloorShrinkApplied = true;
					}
				}
				if (Record.StemOrCoverEquivalent <= UE_SMALL_NUMBER)
				{
					RemovedPatches[RecordIndex] = 1;
					if (bGrowthDiagnosticsEnabled)
					{
						const int32 SpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
						if (InOutYearStats.MortalityBreakdown.IsValidIndex(SpeciesIndex))
						{
							const int32 HeightBucket = CalculateStructureHeightBucket(
								static_cast<double>(Record.MeanHeightCm), static_cast<double>(Model.MaxHeightCm));
							FSpeciesMortalityBreakdown& Breakdown = InOutYearStats.MortalityBreakdown[SpeciesIndex];
							if (bCoverFloorShrinkApplied)
							{
								Breakdown.CoverFloorEvictionStems[HeightBucket] += StemEquivalentBeforeMortality;
							}
							else
							{
								Breakdown.StressStems[HeightBucket] += StemEquivalentBeforeMortality;
							}
						}
					}
				}
			}
		}
		CompactRemovedRecords(Snapshot.Individuals, RemovedIndividuals);
		CompactRemovedRecords(Snapshot.Cohorts, RemovedCohorts);
		CompactRemovedRecords(Snapshot.Patches, RemovedPatches);
		return true;
	}

	constexpr int32 DisturbanceKindCount = 2;

	struct FArcForestDisturbanceCounts
	{
		int32 EventsFired = 0;
		int32 IndividualsKilled = 0;
		int64 CohortStemsKilled = 0;
		int32 RecordsRemoved = 0;
		int32 DeadwoodCreated = 0;
	};

	bool ConsumeFireFuel(
		const FArcForestRunInput& Input,
		const FArcEcotopeSiteMetadata& SiteMetadata,
		const TConstArrayView<uint8> BaseValidityMask,
		const FArcEcotopeDecayGrid& DecayGrid,
		const FArcForestDisturbanceSettings& Settings,
		const FArcForestDisturbanceEvent& Event,
		const uint32 NoiseSeed,
		const int32 FireDeadwoodStart,
		const int32 FireDeadwoodEnd,
		FArcForestWorldSnapshot& Snapshot,
		FString& OutError)
	{
		const uint8 RecordLevel = SiteMetadata.RecordGridLevel;
		const FIntPoint Dimensions = SiteMetadata.GetLevelDimensions(RecordLevel);
		const double LevelCellSizeCm = SiteMetadata.GetLevelCellSizeCm(RecordLevel);
		if (Dimensions.X <= 0
			|| Dimensions.Y <= 0
			|| LevelCellSizeCm <= 0.0)
		{
			OutError = TEXT("ArcForest fire fuel consumption found a degenerate record grid.");
			return false;
		}

		const double OriginX = SiteMetadata.LocalBounds.Min.X;
		const double OriginY = SiteMetadata.LocalBounds.Min.Y;
		const int32 MinCellX = FMath::Clamp(
			FMath::FloorToInt32((Event.CentreLocal.X - Event.RadiusCm - OriginX) / LevelCellSizeCm),
			0,
			Dimensions.X - 1);
		const int32 MaxCellX = FMath::Clamp(
			FMath::FloorToInt32((Event.CentreLocal.X + Event.RadiusCm - OriginX) / LevelCellSizeCm),
			0,
			Dimensions.X - 1);
		const int32 MinCellY = FMath::Clamp(
			FMath::FloorToInt32((Event.CentreLocal.Y - Event.RadiusCm - OriginY) / LevelCellSizeCm),
			0,
			Dimensions.Y - 1);
		const int32 MaxCellY = FMath::Clamp(
			FMath::FloorToInt32((Event.CentreLocal.Y + Event.RadiusCm - OriginY) / LevelCellSizeCm),
			0,
			Dimensions.Y - 1);
		const int32 FootprintWidth = MaxCellX - MinCellX + 1;
		const int32 FootprintHeight = MaxCellY - MinCellY + 1;

		TArray<double> SeverityByFootprintCell;
		SeverityByFootprintCell.Init(0.0, FootprintWidth * FootprintHeight);
		for (int32 CellY = MinCellY; CellY <= MaxCellY; ++CellY)
		{
			for (int32 CellX = MinCellX; CellX <= MaxCellX; ++CellX)
			{
				const FVector CellCentreLocal =
					SiteMetadata.GetLevelCellCenterLocal(CellX + CellY * Dimensions.X, RecordLevel);
				SeverityByFootprintCell[(CellX - MinCellX) + (CellY - MinCellY) * FootprintWidth] =
					CalculateDisturbanceSeverity(
						Settings,
						Event,
						NoiseSeed,
						FVector2D(CellCentreLocal.X, CellCentreLocal.Y));
			}
		}

		const double DeadwoodFraction =
			FMath::Clamp(static_cast<double>(Settings.DeadwoodConsumedFraction), 0.0, 1.0);

		for (int32 RecordIndex = 0; RecordIndex < Snapshot.Deadwood.Num(); ++RecordIndex)
		{
			if (RecordIndex >= FireDeadwoodStart && RecordIndex < FireDeadwoodEnd)
			{
				continue;
			}
			FArcForestDeadwoodRecord& Record = Snapshot.Deadwood[RecordIndex];
			int32 BaseCellIndex = INDEX_NONE;
			int32 RecordCellIndex = INDEX_NONE;
			if (!TryGetBaseAndRecordCellIndex(
				SiteMetadata,
				BaseValidityMask,
				Record.LocalTransform.GetTranslation(),
				BaseCellIndex,
				RecordCellIndex))
			{
				OutError = TEXT("ArcForest fire fuel consumption found a deadwood record outside its frozen site.");
				return false;
			}
			const int32 CellX = RecordCellIndex % Dimensions.X;
			const int32 CellY = RecordCellIndex / Dimensions.X;
			if (CellX < MinCellX || CellX > MaxCellX || CellY < MinCellY || CellY > MaxCellY)
			{
				continue;
			}
			const int32 FootprintIndex = (CellX - MinCellX) + (CellY - MinCellY) * FootprintWidth;
			const double ConsumedFraction =
				FMath::Clamp(DeadwoodFraction * SeverityByFootprintCell[FootprintIndex], 0.0, 1.0);
			Record.InitialBiomassKg = static_cast<float>(FMath::Max(
				0.0,
				static_cast<double>(Record.InitialBiomassKg) * (1.0 - ConsumedFraction)));
			Record.RetireYear = CalculateRetireYear(
				Record.DeathYear,
				static_cast<double>(Record.InitialBiomassKg),
				static_cast<double>(Input.DeadwoodSettings.MinimumTrackedDeadwoodKg),
				CalculateDecayedFraction(
					Input.DeadwoodSettings.DeadwoodDecayPerYear,
					DecayGrid.DecayModerator[RecordCellIndex]));
		}
		return true;
	}

	/**
	 * Draws at most one fire and one windthrow per year and kills stems inside each footprint, with the
	 * severity field acting as a per-record kill probability so survivor patches and a ragged edge fall
	 * out of the draw rather than being stamped.
	 */
	bool ApplyDisturbance(
		const FArcForestRunInput& Input,
		const TConstArrayView<uint8> BaseValidityMask,
		const FArcEcotopeDecayGrid& DecayGrid,
		const TArray<FArcForestFrozenSpeciesSetup>& Species,
		const TMap<FGuid, int32>& SpeciesIndices,
		const int32 Year,
		UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics,
		const bool bGrowthDiagnosticsEnabled,
		FYearDiagnostics& InOutYearStats,
		FArcForestWorldSnapshot& Snapshot,
		FDeadwoodBudgetCounters& InOutCounters,
		FArcForestDisturbanceCounts& OutCounts,
		FString& OutError)
	{
		const FArcForestDisturbanceSettings& Settings = Input.DisturbanceSettings;
		bool bKindFired[DisturbanceKindCount] = { false, false };
		int64 KindQuantizedOccurrence[DisturbanceKindCount] = { 0, 0 };
		bool bAnyFired = false;
		for (int32 KindIndex = 0; KindIndex < DisturbanceKindCount; ++KindIndex)
		{
			const double IntervalYears = static_cast<double>(
				KindIndex == static_cast<int32>(EArcForestDisturbanceKind::Fire)
					? Settings.FireReturnIntervalYears
					: Settings.WindReturnIntervalYears);
			const int64 Quantized = QuantizeProbability(IntervalYears > 0.0 ? 1.0 / IntervalYears : 0.0);
			const uint64 OccurrenceKey = UE::ArcForest::MakeAnnualKey(
				Snapshot.SimulationId,
				Input.Seed,
				Year,
				UE::ArcForest::EDeterministicDomain::DisturbanceOccurrence,
				FGuid(),
				INDEX_NONE,
				0,
				KindIndex,
				Quantized);
			KindQuantizedOccurrence[KindIndex] = Quantized;
			bKindFired[KindIndex] =
				UE::ArcForest::KeyToUnitOpen(OccurrenceKey) < DequantizeProbability(Quantized);
			bAnyFired = bAnyFired || bKindFired[KindIndex];
		}
		if (!bAnyFired)
		{
			return true;
		}

		const FArcEcotopeSiteMetadata& SiteMetadata = GetSiteMetadata(Input);
		TArray<FArcForestDisturbanceEvent> Events;
		Events.Reserve(DisturbanceKindCount);
		for (int32 KindIndex = 0; KindIndex < DisturbanceKindCount; ++KindIndex)
		{
			if (!bKindFired[KindIndex])
			{
				continue;
			}
			const int64 Quantized = KindQuantizedOccurrence[KindIndex];
			const uint64 CentreXKey = UE::ArcForest::MakeAnnualKey(
				Snapshot.SimulationId,
				Input.Seed,
				Year,
				UE::ArcForest::EDeterministicDomain::DisturbanceCentre,
				FGuid(),
				INDEX_NONE,
				0,
				KindIndex * 2,
				Quantized);
			const uint64 CentreYKey = UE::ArcForest::MakeAnnualKey(
				Snapshot.SimulationId,
				Input.Seed,
				Year,
				UE::ArcForest::EDeterministicDomain::DisturbanceCentre,
				FGuid(),
				INDEX_NONE,
				0,
				KindIndex * 2 + 1,
				Quantized);
			const uint64 RadiusKey = UE::ArcForest::MakeAnnualKey(
				Snapshot.SimulationId,
				Input.Seed,
				Year,
				UE::ArcForest::EDeterministicDomain::DisturbanceRadius,
				FGuid(),
				INDEX_NONE,
				0,
				KindIndex,
				Quantized);
			const uint64 WindBearingKey = UE::ArcForest::MakeAnnualKey(
				Snapshot.SimulationId,
				Input.Seed,
				Year,
				UE::ArcForest::EDeterministicDomain::DisturbanceWindBearing,
				FGuid(),
				INDEX_NONE,
				0,
				KindIndex,
				Quantized);
			const double MinRadiusCm = static_cast<double>(Settings.MinEventRadiusCm);
			const double MaxRadiusCm = FMath::Max(MinRadiusCm, static_cast<double>(Settings.MaxEventRadiusCm));
			const double RadiusUnit = UE::ArcForest::KeyToUnitOpen(RadiusKey);
			FArcForestDisturbanceEvent Event;
			Event.Kind = static_cast<EArcForestDisturbanceKind>(KindIndex);
			Event.CentreLocal = FVector2D(
				FMath::Lerp(
					SiteMetadata.LocalBounds.Min.X,
					SiteMetadata.LocalBounds.Max.X,
					UE::ArcForest::KeyToUnitOpen(CentreXKey)),
				FMath::Lerp(
					SiteMetadata.LocalBounds.Min.Y,
					SiteMetadata.LocalBounds.Max.Y,
					UE::ArcForest::KeyToUnitOpen(CentreYKey)));
			Event.RadiusCm = MinRadiusCm + (MaxRadiusCm - MinRadiusCm) * RadiusUnit * RadiusUnit;
			Event.WindBearingRadians = UE::ArcForest::KeyToUnitOpen(WindBearingKey) * UE_DOUBLE_TWO_PI;
			if (!FMath::IsFinite(Event.CentreLocal.X)
				|| !FMath::IsFinite(Event.CentreLocal.Y)
				|| !FMath::IsFinite(Event.RadiusCm)
				|| Event.RadiusCm <= 0.0
				|| !FMath::IsFinite(Event.WindBearingRadians))
			{
				OutError = TEXT("ArcForest disturbance produced an invalid event footprint.");
				return false;
			}
			Events.Add(Event);
		}

		const int32 DeadwoodCountBefore = Snapshot.Deadwood.Num();
		TSet<uint64> DeadwoodIds;
		int32 BranchRecordCount = 0;
		for (const FArcForestDeadwoodRecord& Record : Snapshot.Deadwood)
		{
			DeadwoodIds.Add(Record.Id);
			if (Record.Type == EArcForestDeadwoodType::Branch)
			{
				++BranchRecordCount;
			}
		}
		TArray<uint8> RemovedIndividuals;
		TArray<uint8> RemovedCohorts;
		TArray<uint8> RemovedPatches;
		RemovedIndividuals.Init(0, Snapshot.Individuals.Num());
		RemovedCohorts.Init(0, Snapshot.Cohorts.Num());
		RemovedPatches.Init(0, Snapshot.Patches.Num());
		int32 FireDeadwoodStart = DeadwoodCountBefore;
		int32 FireDeadwoodEnd = DeadwoodCountBefore;

		for (const FArcForestDisturbanceEvent& Event : Events)
		{
			const int32 KindIndex = static_cast<int32>(Event.Kind);
			const bool bIsFire = Event.Kind == EArcForestDisturbanceKind::Fire;
			const uint32 NoiseSeed = HashCombine(
				HashCombine(static_cast<uint32>(Input.Seed), static_cast<uint32>(Year)),
				static_cast<uint32>(KindIndex));
			const double SinkFraction = bIsFire
				? 1.0 - FMath::Clamp(static_cast<double>(Settings.FireCombustedFraction), 0.0, 0.99)
				: 1.0;
			int32 EventIndividualsKilled = 0;
			int64 EventCohortStemsKilled = 0;
			int32 EventRecordsRemoved = 0;
			if (bIsFire)
			{
				FireDeadwoodStart = Snapshot.Deadwood.Num();
			}

			{
				ARCFOREST_SCOPED_PHASE(Diagnostics, ApplyDisturbance_Individuals);
				for (int32 RecordIndex = Snapshot.Individuals.Num() - 1; RecordIndex >= 0; --RecordIndex)
				{
					if (RemovedIndividuals[RecordIndex] != 0)
					{
						continue;
					}
					const FArcForestIndividualRecord& Record = Snapshot.Individuals[RecordIndex];
					const double BaseSeverity = CalculateDisturbanceSeverity(
						Settings,
						Event,
						NoiseSeed,
						FVector2D(Record.LocalPosition.X, Record.LocalPosition.Y));
					if (!(BaseSeverity > 0.0))
					{
						continue;
					}
					const int64 QuantizedSeverity = QuantizeProbability(
						BaseSeverity
							* CalculateDisturbanceHeightFactor(Event.Kind, Settings, Record.HeightCm));
					if (QuantizedSeverity <= 0)
					{
						continue;
					}
					int32 BaseCellIndex = INDEX_NONE;
					int32 RecordCellIndex = INDEX_NONE;
					if (!TryGetBaseAndRecordCellIndex(
						SiteMetadata,
						BaseValidityMask,
						Record.LocalPosition,
						BaseCellIndex,
						RecordCellIndex))
					{
						OutError = TEXT("ArcForest disturbance found an individual outside its frozen site.");
						return false;
					}
					// KindIndex folds into the ordinal so an overlapping fire and windthrow do not key one record identically and spare exactly the same stems.
					const uint64 KillKey = UE::ArcForest::MakeAnnualKey(
						Snapshot.SimulationId,
						Input.Seed,
						Year,
						UE::ArcForest::EDeterministicDomain::DisturbanceKill,
						Record.SpeciesId,
						BaseCellIndex,
						Record.Id,
						KindIndex * 3,
						QuantizedSeverity);
					if (UE::ArcForest::KeyToUnitOpen(KillKey) >= DequantizeProbability(QuantizedSeverity))
					{
						continue;
					}
					if (bGrowthDiagnosticsEnabled)
					{
						const int32* DisturbanceSpeciesIndex = SpeciesIndices.Find(Record.SpeciesId);
						if (DisturbanceSpeciesIndex != nullptr
							&& InOutYearStats.MortalityBreakdown.IsValidIndex(*DisturbanceSpeciesIndex))
						{
							const int32 HeightBucket = CalculateStructureHeightBucket(
								static_cast<double>(Record.HeightCm),
								static_cast<double>(Species[*DisturbanceSpeciesIndex].MaxHeightCm));
							++InOutYearStats.MortalityBreakdown[*DisturbanceSpeciesIndex]
								.DisturbanceStems[HeightBucket];
						}
					}
					// A -1 salt cannot collide with mortality's quantized probability or self-thinning's literal 0: QuantizeProbability never returns a negative.
					if (!RouteIndividualDeath(
							Input,
							SiteMetadata,
							BaseValidityMask,
							DecayGrid,
							Year,
							Record,
							BaseCellIndex,
							RecordCellIndex,
							bIsFire ? EArcForestDeathCause::Fire : EArcForestDeathCause::Wind,
							-1,
							Snapshot,
							DeadwoodIds,
							BranchRecordCount,
							InOutCounters,
							OutError,
							static_cast<double>(Record.BiomassKg) * SinkFraction,
							TOptional<double>(Event.WindBearingRadians)))
					{
						return false;
					}
					RemovedIndividuals[RecordIndex] = 1;
					++EventIndividualsKilled;
				}
			}

			{
				ARCFOREST_SCOPED_PHASE(Diagnostics, ApplyDisturbance_Cohorts);
				for (int32 RecordIndex = Snapshot.Cohorts.Num() - 1; RecordIndex >= 0; --RecordIndex)
				{
					if (RemovedCohorts[RecordIndex] != 0)
					{
						continue;
					}
					FArcForestCohortRecord& Record = Snapshot.Cohorts[RecordIndex];
					if (Record.StemCount <= 0)
					{
						continue;
					}
					const FVector CellCentreLocal =
						SiteMetadata.GetLevelCellCenterLocal(Record.CellIndex, SiteMetadata.RecordGridLevel);
					const double BaseSeverity = CalculateDisturbanceSeverity(
						Settings,
						Event,
						NoiseSeed,
						FVector2D(CellCentreLocal.X, CellCentreLocal.Y));
					if (!(BaseSeverity > 0.0))
					{
						continue;
					}
					const int64 QuantizedSeverity = QuantizeProbability(
						BaseSeverity
							* CalculateDisturbanceHeightFactor(Event.Kind, Settings, Record.MeanHeightCm));
					if (QuantizedSeverity <= 0)
					{
						continue;
					}
					const uint64 KillKey = UE::ArcForest::MakeAnnualKey(
						Snapshot.SimulationId,
						Input.Seed,
						Year,
						UE::ArcForest::EDeterministicDomain::DisturbanceKill,
						Record.SpeciesId,
						Record.CellIndex,
						Record.Id,
						KindIndex * 3 + 1,
						QuantizedSeverity);
					int64 DeathCount = 0;
					if (!UE::ArcForest::TryRoundNonNegative(
						static_cast<double>(Record.StemCount) * DequantizeProbability(QuantizedSeverity),
						KillKey,
						DeathCount))
					{
						OutError = TEXT("ArcForest disturbance cohort kill count was not representable.");
						return false;
					}
					DeathCount = FMath::Min(DeathCount, Record.StemCount);
					if (DeathCount <= 0)
					{
						continue;
					}
					if (bGrowthDiagnosticsEnabled)
					{
						const int32* DisturbanceSpeciesIndex = SpeciesIndices.Find(Record.SpeciesId);
						if (DisturbanceSpeciesIndex != nullptr
							&& InOutYearStats.MortalityBreakdown.IsValidIndex(*DisturbanceSpeciesIndex))
						{
							const int32 HeightBucket = CalculateStructureHeightBucket(
								static_cast<double>(Record.MeanHeightCm),
								static_cast<double>(Species[*DisturbanceSpeciesIndex].MaxHeightCm));
							InOutYearStats.MortalityBreakdown[*DisturbanceSpeciesIndex]
								.DisturbanceStems[HeightBucket] += DeathCount;
						}
					}
					const double LostBiomass = static_cast<double>(Record.BiomassKg)
						* (static_cast<double>(DeathCount) / static_cast<double>(Record.StemCount));
					EventCohortStemsKilled += DeathCount;
					if (DeathCount == Record.StemCount)
					{
						RemovedCohorts[RecordIndex] = 1;
						++EventRecordsRemoved;
					}
					else
					{
						Record.StemCount -= DeathCount;
						Record.BiomassKg = static_cast<float>(FMath::Max(0.0, Record.BiomassKg - LostBiomass));
					}
				}
			}

			{
				ARCFOREST_SCOPED_PHASE(Diagnostics, ApplyDisturbance_Patches);
				for (int32 RecordIndex = Snapshot.Patches.Num() - 1; RecordIndex >= 0; --RecordIndex)
				{
					if (RemovedPatches[RecordIndex] != 0)
					{
						continue;
					}
					FArcForestPatchRecord& Record = Snapshot.Patches[RecordIndex];
					const FVector CellCentreLocal =
						SiteMetadata.GetLevelCellCenterLocal(Record.CellIndex, SiteMetadata.RecordGridLevel);
					const int64 QuantizedSeverity = QuantizeProbability(CalculateDisturbanceSeverity(
						Settings,
						Event,
						NoiseSeed,
						FVector2D(CellCentreLocal.X, CellCentreLocal.Y)));
					if (QuantizedSeverity <= 0)
					{
						continue;
					}
					const uint64 KillKey = UE::ArcForest::MakeAnnualKey(
						Snapshot.SimulationId,
						Input.Seed,
						Year,
						UE::ArcForest::EDeterministicDomain::DisturbanceKill,
						Record.SpeciesId,
						Record.CellIndex,
						Record.Id,
						KindIndex * 3 + 2,
						QuantizedSeverity);
					const double LostFraction = FMath::Clamp(
						DequantizeProbability(QuantizedSeverity)
							* (0.5 + UE::ArcForest::KeyToUnitOpen(KillKey)),
						0.0,
						1.0);
					if (!(LostFraction > 0.0))
					{
						continue;
					}
					const double LostBiomass = static_cast<double>(Record.BiomassKg) * LostFraction;
					Record.StemOrCoverEquivalent = static_cast<float>(
						Record.StemOrCoverEquivalent * (1.0 - LostFraction));
					Record.BiomassKg = static_cast<float>(FMath::Max(0.0, Record.BiomassKg - LostBiomass));
					if (Record.StemOrCoverEquivalent <= UE_SMALL_NUMBER)
					{
						RemovedPatches[RecordIndex] = 1;
						++EventRecordsRemoved;
					}
				}
			}

			if (bIsFire)
			{
				FireDeadwoodEnd = Snapshot.Deadwood.Num();
				ARCFOREST_SCOPED_PHASE(Diagnostics, ApplyDisturbance_FireFuel);
				if (!ConsumeFireFuel(
					Input,
					SiteMetadata,
					BaseValidityMask,
					DecayGrid,
					Settings,
					Event,
					NoiseSeed,
					FireDeadwoodStart,
					FireDeadwoodEnd,
					Snapshot,
					OutError))
				{
					return false;
				}
			}
			OutCounts.IndividualsKilled += EventIndividualsKilled;
			OutCounts.CohortStemsKilled += EventCohortStemsKilled;
			OutCounts.RecordsRemoved += EventRecordsRemoved;
			UE_LOGF(
				LogArcForestGrowth,
				Log,
				"ArcForest year %d disturbance %ls | centre (%.1f, %.1f) radius %.1f"
				" | killed ind %d cohortstems %lld recs %d",
				Year,
				bIsFire ? TEXT("fire") : TEXT("windthrow"),
				Event.CentreLocal.X,
				Event.CentreLocal.Y,
				Event.RadiusCm,
				EventIndividualsKilled,
				EventCohortStemsKilled,
				EventRecordsRemoved);
		}

		CompactRemovedRecords(Snapshot.Individuals, RemovedIndividuals);
		CompactRemovedRecords(Snapshot.Cohorts, RemovedCohorts);
		CompactRemovedRecords(Snapshot.Patches, RemovedPatches);
		OutCounts.EventsFired = Events.Num();
		OutCounts.DeadwoodCreated = Snapshot.Deadwood.Num() - DeadwoodCountBefore;
		return true;
	}

	struct FThinningCandidate
	{
		int32 RecordCellIndex = INDEX_NONE;
		double Vigor = 0.0;
		uint64 Id = 0;
		int32 Kind = 0;
		int32 RecordIndex = INDEX_NONE;
		int32 BaseCellIndex = INDEX_NONE;
		int32 SpeciesIndex = INDEX_NONE;
	};

	struct FSelfThinningCounts
	{
		int32 IndividualsRemoved = 0;
		int64 CohortStemsRemoved = 0;
		int32 RecordsRemoved = 0;
		int32 DeadwoodCreated = 0;

		double OwnRootedMin = -1.0;
		double OwnRootedMean = -1.0;
		double OwnRootedMax = -1.0;
		double OwnRootedMaxAfterRemoval = -1.0;
		int32 OwnRootedCellCount = -1;
	};

	/**
	 * Solves each record cell down to a crown occupancy limit by removing stems, lowest vigor first and ties
	 * broken by the unique record Id, so the pass needs no draw of its own. The constraint is on the crown
	 * ROOTED in the cell: shade cast by crowns rooted in neighbouring cells is already priced by light ->
	 * habitat -> vigor -> mortality, so counting it here would clear-cut an understory that contributes none
	 * of the excess and could never relieve it. The post-mortality pass therefore rebuilds only the unclamped
	 * own-rooted sum; the full stratified shade field is a start-of-year input and is not consumed here.
	 *
	 * The limit is the volume's MaximumCrownOccupancy: carrying capacity is a property of the ground, so every
	 * INDIVIDUALIZING record rooted in a cell competes against one ceiling. Patches are outside the constraint
	 * entirely - they neither fill it nor are removed by it.
	 */
	bool ApplySelfThinning(
		const FArcForestRunInput& Input,
		const TConstArrayView<uint8> BaseValidityMask,
		const TArray<FArcForestFrozenSpeciesSetup>& Species,
		const TMap<FGuid, int32>& SpeciesIndices,
		const FArcEcotopeChunkLayout& ChunkLayout,
		const TArray<double>& RecordCellAreas,
		const FArcEcotopeDecayGrid& DecayGrid,
		const int32 Year,
		const bool bEmitDiagnostics,
		UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics,
		FYearDiagnostics& InOutYearStats,
		FArcForestWorldSnapshot& Snapshot,
		FDeadwoodBudgetCounters& InOutCounters,
		FSelfThinningCounts& OutCounts,
		FString& OutError)
	{
		const FArcEcotopeSiteMetadata& Site = GetSiteMetadata(Input);
		const double CellCapacity = Input.MaximumCrownOccupancy;
		if (!(CellCapacity > 0.0))
		{
			return true;
		}

		TArray<double> OwnRootedOccupancy;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, SelfThinning_Occupancy);
			if (!AccumulateCanopyOccupancy(
					Input,
					BaseValidityMask,
					Species,
					SpeciesIndices,
					Snapshot,
					ChunkLayout,
					nullptr,
					RecordCellAreas,
					SelfThinningPhaseNames,
					Diagnostics,
					nullptr,
					&OwnRootedOccupancy,
					OutError))
			{
				return false;
			}
		}

		// Read before the removal loop, which drives every over-budget cell down to CellCapacity: any later read
		// reports the solved state and so cannot tell a year where the budget bound from one where it never did.
		if (bEmitDiagnostics)
		{
			double OwnRootedSum = 0.0;
			double OwnRootedMin = TNumericLimits<double>::Max();
			double OwnRootedMax = TNumericLimits<double>::Lowest();
			int32 OwnRootedCells = 0;
			for (int32 RecordCellIndex = 0; RecordCellIndex < OwnRootedOccupancy.Num(); ++RecordCellIndex)
			{
				if (!IsDiagnosticRecordCell(
						DecayGrid, OwnRootedOccupancy.Num(), RecordCellIndex))
				{
					continue;
				}
				const double OwnRooted = OwnRootedOccupancy[RecordCellIndex];
				OwnRootedSum += OwnRooted;
				OwnRootedMin = FMath::Min(OwnRootedMin, OwnRooted);
				OwnRootedMax = FMath::Max(OwnRootedMax, OwnRooted);
				++OwnRootedCells;
			}
			OutCounts.OwnRootedCellCount = OwnRootedCells;
			if (OwnRootedCells > 0)
			{
				OutCounts.OwnRootedMin = OwnRootedMin;
				OutCounts.OwnRootedMean = OwnRootedSum / static_cast<double>(OwnRootedCells);
				OutCounts.OwnRootedMax = OwnRootedMax;
			}
		}

		TArray<FThinningCandidate> Candidates;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, SelfThinning_Candidates);
			Candidates.Reserve(Snapshot.Individuals.Num() + Snapshot.Cohorts.Num());
			for (int32 RecordIndex = 0; RecordIndex < Snapshot.Individuals.Num(); ++RecordIndex)
			{
				const FArcForestIndividualRecord& Record = Snapshot.Individuals[RecordIndex];
				int32 BaseCellIndex = INDEX_NONE;
				int32 RecordCellIndex = INDEX_NONE;
				if (!TryGetBaseAndRecordCellIndex(
						Site, BaseValidityMask, Record.LocalPosition, BaseCellIndex, RecordCellIndex))
				{
					OutError = TEXT("ArcForest self-thinning found an individual outside its frozen site.");
					return false;
				}
				FThinningCandidate& Candidate = Candidates.AddDefaulted_GetRef();
				Candidate.RecordCellIndex = RecordCellIndex;
				Candidate.Vigor = Record.Vigor;
				Candidate.Id = Record.Id;
				Candidate.Kind = 0;
				Candidate.RecordIndex = RecordIndex;
				Candidate.BaseCellIndex = BaseCellIndex;
				Candidate.SpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
			}
			for (int32 RecordIndex = 0; RecordIndex < Snapshot.Cohorts.Num(); ++RecordIndex)
			{
				const FArcForestCohortRecord& Record = Snapshot.Cohorts[RecordIndex];
				if (!RecordCellAreas.IsValidIndex(Record.CellIndex))
				{
					OutError = TEXT("ArcForest self-thinning found a cohort outside its frozen site.");
					return false;
				}
				FThinningCandidate& Candidate = Candidates.AddDefaulted_GetRef();
				Candidate.RecordCellIndex = Record.CellIndex;
				Candidate.Vigor = Record.MeanVigor;
				Candidate.Id = Record.Id;
				Candidate.Kind = 1;
				Candidate.RecordIndex = RecordIndex;
				Candidate.SpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
			}
			for (int32 RecordIndex = 0; RecordIndex < Snapshot.Patches.Num(); ++RecordIndex)
			{
				const FArcForestPatchRecord& Record = Snapshot.Patches[RecordIndex];
				// Rejects a patch whose record cell left the frozen grid.
				if (!RecordCellAreas.IsValidIndex(Record.CellIndex))
				{
					OutError = TEXT("ArcForest self-thinning found a patch outside its frozen site.");
					return false;
				}
				const int32 PatchSpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
				if (!Species[PatchSpeciesIndex].bIndividualizes)
				{
					continue;
				}
				FThinningCandidate& Candidate = Candidates.AddDefaulted_GetRef();
				Candidate.RecordCellIndex = Record.CellIndex;
				Candidate.Vigor = Record.MeanVigor;
				Candidate.Id = Record.Id;
				Candidate.Kind = 2;
				Candidate.RecordIndex = RecordIndex;
				Candidate.SpeciesIndex = PatchSpeciesIndex;
			}
			Candidates.Sort(
				[](const FThinningCandidate& Left, const FThinningCandidate& Right)
				{
					if (Left.RecordCellIndex != Right.RecordCellIndex)
					{
						return Left.RecordCellIndex < Right.RecordCellIndex;
					}
					if (Left.Vigor != Right.Vigor)
					{
						return Left.Vigor < Right.Vigor;
					}
					return Left.Id < Right.Id;
				});
		}

		ARCFOREST_SCOPED_PHASE(Diagnostics, SelfThinning_Removal);
		TSet<uint64> DeadwoodIds;
		int32 BranchRecordCount = 0;
		for (const FArcForestDeadwoodRecord& Record : Snapshot.Deadwood)
		{
			DeadwoodIds.Add(Record.Id);
			if (Record.Type == EArcForestDeadwoodType::Branch)
			{
				++BranchRecordCount;
			}
		}
		const int32 DeadwoodCountBefore = Snapshot.Deadwood.Num();

		TArray<uint8> RemovedIndividuals;
		TArray<uint8> RemovedCohorts;
		TArray<uint8> RemovedPatches;
		RemovedIndividuals.Init(0, Snapshot.Individuals.Num());
		RemovedCohorts.Init(0, Snapshot.Cohorts.Num());
		RemovedPatches.Init(0, Snapshot.Patches.Num());

		for (const FThinningCandidate& Candidate : Candidates)
		{
			const FArcForestFrozenSpeciesSetup& Model = Species[Candidate.SpeciesIndex];
			const int32 CellIndex = Candidate.RecordCellIndex;
			const double Excess = OwnRootedOccupancy[CellIndex] - CellCapacity;
			if (Excess <= 0.0)
			{
				continue;
			}

			if (Candidate.Kind == 0)
			{
				const FArcForestIndividualRecord& Record = Snapshot.Individuals[Candidate.RecordIndex];
				const double CrownRadiusCm = CalculateCrownRadius(Record.HeightCm, Model);
				const double OwnShare = CalculateOwnCellCrownShare(
					Site,
					FVector2D(Record.LocalPosition.X, Record.LocalPosition.Y),
					FVector2D::ZeroVector,
					CrownRadiusCm,
					1.0,
					CellIndex,
					RecordCellAreas);
				if (OwnShare <= 0.0)
				{
					continue;
				}
				if (!RouteIndividualDeath(
						Input,
						Site,
						BaseValidityMask,
						DecayGrid,
						Year,
						Record,
						Candidate.BaseCellIndex,
						Candidate.RecordCellIndex,
						EArcForestDeathCause::SelfThinning,
						0,
						Snapshot,
						DeadwoodIds,
						BranchRecordCount,
						InOutCounters,
						OutError))
				{
					return false;
				}
				OwnRootedOccupancy[CellIndex] = FMath::Max(0.0, OwnRootedOccupancy[CellIndex] - OwnShare);
				RemovedIndividuals[Candidate.RecordIndex] = 1;
				++OutCounts.IndividualsRemoved;
				if (bEmitDiagnostics && InOutYearStats.MortalityBreakdown.IsValidIndex(Candidate.SpeciesIndex))
				{
					const int32 HeightBucket = CalculateStructureHeightBucket(
						static_cast<double>(Record.HeightCm), static_cast<double>(Model.MaxHeightCm));
					++InOutYearStats.MortalityBreakdown[Candidate.SpeciesIndex].SelfThinningStems[HeightBucket];
				}
				continue;
			}

			if (Candidate.Kind == 1)
			{
				FVector2D CellMinLocal = FVector2D::ZeroVector;
				FVector2D CellMaxLocal = FVector2D::ZeroVector;
				if (!Site.TryGetLevelCellRect(CellIndex, Site.RecordGridLevel, CellMinLocal, CellMaxLocal))
				{
					OutError = TEXT("ArcForest self-thinning could not resolve a record cell rect.");
					return false;
				}

				FArcForestCohortRecord& Record = Snapshot.Cohorts[Candidate.RecordIndex];
				if (Record.StemCount <= 0)
				{
					continue;
				}
				const double CrownRadiusCm = CalculateCrownRadius(Record.MeanHeightCm, Model);
				const double PerStemShare = CalculateOwnCellCrownShare(
					Site,
					CellMinLocal,
					CellMaxLocal - CellMinLocal,
					CrownRadiusCm,
					1.0,
					CellIndex,
					RecordCellAreas);
				if (PerStemShare <= 0.0)
				{
					continue;
				}
				const int64 RemovedStems = static_cast<int64>(FMath::Clamp(
					FMath::CeilToDouble(Excess / PerStemShare), 1.0, static_cast<double>(Record.StemCount)));
				const double LostBiomass = static_cast<double>(Record.BiomassKg)
					* (static_cast<double>(RemovedStems) / static_cast<double>(Record.StemCount));
				OwnRootedOccupancy[CellIndex] = FMath::Max(
					0.0, OwnRootedOccupancy[CellIndex] - PerStemShare * static_cast<double>(RemovedStems));
				OutCounts.CohortStemsRemoved += RemovedStems;
				if (bEmitDiagnostics && InOutYearStats.MortalityBreakdown.IsValidIndex(Candidate.SpeciesIndex))
				{
					const int32 HeightBucket = CalculateStructureHeightBucket(
						static_cast<double>(Record.MeanHeightCm), static_cast<double>(Model.MaxHeightCm));
					InOutYearStats.MortalityBreakdown[Candidate.SpeciesIndex].SelfThinningStems[HeightBucket] +=
						RemovedStems;
				}
				if (RemovedStems == Record.StemCount)
				{
					RemovedCohorts[Candidate.RecordIndex] = 1;
					++OutCounts.RecordsRemoved;
				}
				else
				{
					Record.StemCount -= RemovedStems;
					Record.BiomassKg = static_cast<float>(FMath::Max(0.0, Record.BiomassKg - LostBiomass));
				}
			}
		}

		if (bEmitDiagnostics)
		{
			double OwnRootedMaxAfter = TNumericLimits<double>::Lowest();
			int32 OwnRootedCells = 0;
			for (int32 RecordCellIndex = 0; RecordCellIndex < OwnRootedOccupancy.Num(); ++RecordCellIndex)
			{
				if (!IsDiagnosticRecordCell(
						DecayGrid, OwnRootedOccupancy.Num(), RecordCellIndex))
				{
					continue;
				}
				OwnRootedMaxAfter = FMath::Max(OwnRootedMaxAfter, OwnRootedOccupancy[RecordCellIndex]);
				++OwnRootedCells;
			}
			if (OwnRootedCells > 0)
			{
				OutCounts.OwnRootedMaxAfterRemoval = OwnRootedMaxAfter;
			}
		}

		CompactRemovedRecords(Snapshot.Individuals, RemovedIndividuals);
		CompactRemovedRecords(Snapshot.Cohorts, RemovedCohorts);
		CompactRemovedRecords(Snapshot.Patches, RemovedPatches);
		OutCounts.DeadwoodCreated = Snapshot.Deadwood.Num() - DeadwoodCountBefore;
		return true;
	}

	bool ApplyBranchShed(
		const FArcForestRunInput& Input,
		const TConstArrayView<uint8> BaseValidityMask,
		const FArcEcotopeDecayGrid& DecayGrid,
		const int32 Year,
		UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics,
		FArcForestWorldSnapshot& Snapshot,
		FDeadwoodBudgetCounters& InOutCounters,
		int32& OutBranchesCreated,
		FString& OutError)
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, ApplyBranchShed);
		OutBranchesCreated = 0;
		const FArcEcotopeDeadwoodSettings& Settings = Input.DeadwoodSettings;
		const double ShedProbability = static_cast<double>(Settings.BranchShedProbabilityPerYear);
		const FArcEcotopeSiteMetadata& SiteMetadata = GetSiteMetadata(Input);
		const double MinimumTrackedKg = static_cast<double>(Settings.MinimumTrackedDeadwoodKg);
		const int32 DeadwoodCountBefore = Snapshot.Deadwood.Num();
		int32 BranchRecordCount = 0;
		for (const FArcForestDeadwoodRecord& Record : Snapshot.Deadwood)
		{
			if (Record.Type == EArcForestDeadwoodType::Branch)
			{
				++BranchRecordCount;
			}
		}

		for (FArcForestIndividualRecord& Record : Snapshot.Individuals)
		{
			if (Year < Record.NextShedYear)
			{
				continue;
			}
			int32 BaseCellIndex = INDEX_NONE;
			int32 RecordCellIndex = INDEX_NONE;
			if (!TryGetBaseAndRecordCellIndex(
					SiteMetadata, BaseValidityMask, Record.LocalPosition, BaseCellIndex, RecordCellIndex))
			{
				OutError = TEXT("ArcForest branch shed found an individual outside its frozen site.");
				return false;
			}
			const uint64 ShedKey = UE::ArcForest::MakeAnnualKey(
				Snapshot.SimulationId,
				Input.Seed,
				Year,
				UE::ArcForest::EDeterministicDomain::BranchShedOccurrence,
				Record.SpeciesId,
				BaseCellIndex,
				Record.Id,
				0,
				/*Salt*/ 0);
			Record.NextShedYear =
				CalculateGeometricEventYear(Year, ShedProbability, UE::ArcForest::KeyToUnitOpen(ShedKey));
			const FBranchGeometry Geometry = CalculateBranchGeometry(Record);
			if (!FMath::IsFinite(Geometry.MassKg) || Geometry.MassKg < MinimumTrackedKg)
			{
				continue;
			}
			const uint64 BranchId = MakeAdditionalDeadwoodRecordId(
				Snapshot, Input, Year, Record.SpeciesId, BaseCellIndex, Record.Id, /*DecisionOrdinal*/ 0);
			const uint64 PlacementRadiusKey = UE::ArcForest::MakeAnnualKey(
				Snapshot.SimulationId,
				Input.Seed,
				Year,
				UE::ArcForest::EDeterministicDomain::BranchShedPlacement,
				Record.SpeciesId,
				BaseCellIndex,
				Record.Id,
				0,
				/*Salt*/ 0);
			const uint64 PlacementAngleKey = UE::ArcForest::MakeAnnualKey(
				Snapshot.SimulationId,
				Input.Seed,
				Year,
				UE::ArcForest::EDeterministicDomain::BranchShedPlacement,
				Record.SpeciesId,
				BaseCellIndex,
				Record.Id,
				1,
				/*Salt*/ 0);
			const double PlacementRadiusCm = FMath::Sqrt(UE::ArcForest::KeyToUnitOpen(PlacementRadiusKey))
				* FMath::Max(0.0, static_cast<double>(Record.CrownRadiusCm));
			const double PlacementAngleRadians =
				UE::ArcForest::KeyToUnitOpen(PlacementAngleKey) * UE_DOUBLE_TWO_PI;
			const FVector OffsetPosition = Record.LocalPosition
				+ FVector(
					FMath::Cos(PlacementAngleRadians) * PlacementRadiusCm,
					FMath::Sin(PlacementAngleRadians) * PlacementRadiusCm,
					0.0);
			if (!TryEmitBranchRecord(
					Input,
					SiteMetadata,
					BaseValidityMask,
					DecayGrid,
					Year,
					Record,
					BaseCellIndex,
					RecordCellIndex,
					Geometry,
					BranchId,
					OffsetPosition,
					TOptional<double>(),
					Snapshot,
					BranchRecordCount,
					InOutCounters,
					OutError))
			{
				return false;
			}
		}
		OutBranchesCreated = Snapshot.Deadwood.Num() - DeadwoodCountBefore;
		return true;
	}

	bool ApplyDeadwoodDecay(
		const FArcForestRunInput& Input,
		const TConstArrayView<uint8> BaseValidityMask,
		const int32 Year,
		UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics,
		FArcForestWorldSnapshot& Snapshot,
		int32& OutDeadwoodRetired,
		FString& OutError)
	{
		OutDeadwoodRetired = 0;
		const FArcEcotopeSiteMetadata& SiteMetadata = GetSiteMetadata(Input);
		TArray<uint8> RemovedDeadwood;
		RemovedDeadwood.Init(0, Snapshot.Deadwood.Num());
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Decay_DeadwoodDecay);
			for (int32 RecordIndex = 0; RecordIndex < Snapshot.Deadwood.Num(); ++RecordIndex)
			{
				FArcForestDeadwoodRecord& Record = Snapshot.Deadwood[RecordIndex];
				if (Year >= Record.RetireYear)
				{
					RemovedDeadwood[RecordIndex] = 1;
					++OutDeadwoodRetired;
					continue;
				}
				if (Record.Type != EArcForestDeadwoodType::Snag || Year < Record.FallYear)
				{
					continue;
				}
				int32 BaseCellIndex = INDEX_NONE;
				int32 RecordCellIndex = INDEX_NONE;
				if (!TryGetBaseAndRecordCellIndex(
					SiteMetadata,
					BaseValidityMask,
					Record.LocalTransform.GetTranslation(),
					BaseCellIndex,
					RecordCellIndex))
				{
					OutError = TEXT("ArcForest deadwood decay found a record outside its frozen site.");
					return false;
				}
				Record.Type = EArcForestDeadwoodType::Log;
				Record.LocalTransform.SetRotation(MakeFallenStemRotation(
					Snapshot,
					Input,
					Year,
					Record.SpeciesId,
					BaseCellIndex,
					Record.Id,
					TOptional<double>()));
			}
		}
		CompactRemovedRecords(Snapshot.Deadwood, RemovedDeadwood);
		return true;
	}

	struct FPropaguleContribution
	{
		int32 SpeciesIndex = INDEX_NONE;
		int32 CellIndex = INDEX_NONE;
		uint64 SourceId = 0;
		uint8 SourceType = 0;
		double ExpectedSurvivors = 0.0;
	};

	bool TryCountBudgets(
		const FArcForestWorldSnapshot& Snapshot,
		int64& OutLiveStems,
		int64& OutLiveRecordCount,
		int64& OutCoverRecordCount,
		int64& OutNonBranchDeadwoodRecordCount,
		int64& OutBranchRecordCount,
		int64& OutTotalRecordCount,
		FString& OutError)
	{
		OutLiveStems = Snapshot.Individuals.Num();
		OutLiveRecordCount = static_cast<int64>(Snapshot.Individuals.Num())
			+ Snapshot.Cohorts.Num();
		OutCoverRecordCount = Snapshot.Patches.Num();
		OutNonBranchDeadwoodRecordCount = 0;
		OutBranchRecordCount = 0;
		for (const FArcForestDeadwoodRecord& Record : Snapshot.Deadwood)
		{
			if (Record.Type == EArcForestDeadwoodType::Branch)
			{
				++OutBranchRecordCount;
			}
			else
			{
				++OutNonBranchDeadwoodRecordCount;
			}
		}
		OutTotalRecordCount = OutLiveRecordCount
			+ OutCoverRecordCount
			+ OutNonBranchDeadwoodRecordCount
			+ OutBranchRecordCount;
		for (const FArcForestCohortRecord& Record : Snapshot.Cohorts)
		{
			if (OutLiveStems > MAX_int64 - Record.StemCount)
			{
				OutError = TEXT("ArcForest live-stem count overflowed.");
				return false;
			}
			OutLiveStems += Record.StemCount;
		}
		for (const FArcForestPatchRecord& Record : Snapshot.Patches)
		{
			const int64 PatchStems = FMath::CeilToInt64(Record.StemOrCoverEquivalent);
			if (PatchStems < 0 || OutLiveStems > MAX_int64 - PatchStems)
			{
				OutError = TEXT("ArcForest patch live-stem count overflowed.");
				return false;
			}
			OutLiveStems += PatchStems;
		}
		return true;
	}

	bool MergeCanonicalLiveRecords(FArcForestWorldSnapshot& Snapshot, FString& OutError)
	{
		for (FArcForestCohortRecord& Record : Snapshot.Cohorts)
		{
			Record.HeightBin = CalculateHeightBin(Record.MeanHeightCm);
		}
		Snapshot.Cohorts.Sort(
			[](const FArcForestCohortRecord& Left, const FArcForestCohortRecord& Right)
			{
				if (Left.SpeciesId != Right.SpeciesId)
				{
					return GuidLess(Left.SpeciesId, Right.SpeciesId);
				}
				if (Left.CellIndex != Right.CellIndex)
				{
					return Left.CellIndex < Right.CellIndex;
				}
				if (Left.BirthYear != Right.BirthYear)
				{
					return Left.BirthYear < Right.BirthYear;
				}
				if (Left.HeightBin != Right.HeightBin)
				{
					return Left.HeightBin < Right.HeightBin;
				}
				return Left.Id < Right.Id;
			});

		TArray<FArcForestCohortRecord> MergedCohorts;
		MergedCohorts.Reserve(Snapshot.Cohorts.Num());
		for (const FArcForestCohortRecord& Source : Snapshot.Cohorts)
		{
			FArcForestCohortRecord* Target = MergedCohorts.IsEmpty() ? nullptr : &MergedCohorts.Last();
			const bool bSameKey = Target != nullptr
				&& Target->SpeciesId == Source.SpeciesId
				&& Target->CellIndex == Source.CellIndex
				&& Target->BirthYear == Source.BirthYear
				&& Target->HeightBin == Source.HeightBin;
			if (!bSameKey)
			{
				MergedCohorts.Add(Source);
				continue;
			}
			if (Target->StemCount > MAX_int64 - Source.StemCount)
			{
				OutError = TEXT("ArcForest canonical cohort merge overflowed its stem count.");
				return false;
			}
			const int64 NewStemCount = Target->StemCount + Source.StemCount;
			const double TargetWeight = static_cast<double>(Target->StemCount);
			const double SourceWeight = static_cast<double>(Source.StemCount);
			const double NewHeight =
				(Target->MeanHeightCm * TargetWeight + Source.MeanHeightCm * SourceWeight) / NewStemCount;
			const double NewAge =
				(Target->MeanAgeYears * TargetWeight + Source.MeanAgeYears * SourceWeight) / NewStemCount;
			const double NewVigor =
				(Target->MeanVigor * TargetWeight + Source.MeanVigor * SourceWeight) / NewStemCount;
			const double NewBiomass = static_cast<double>(Target->BiomassKg) + Source.BiomassKg;
			if (!FMath::IsFinite(NewHeight)
				|| !FMath::IsFinite(NewAge)
				|| !FMath::IsFinite(NewVigor)
				|| !FMath::IsFinite(NewBiomass)
				|| NewBiomass > MAX_flt)
			{
				OutError = TEXT("ArcForest canonical cohort merge produced unrepresentable state.");
				return false;
			}
			Target->StemCount = NewStemCount;
			Target->MeanHeightCm = static_cast<float>(NewHeight);
			Target->MeanAgeYears = static_cast<float>(NewAge);
			Target->MeanVigor = static_cast<float>(NewVigor);
			Target->BiomassKg = static_cast<float>(NewBiomass);
		}
		Snapshot.Cohorts = MoveTemp(MergedCohorts);

		Snapshot.Patches.Sort(
			[](const FArcForestPatchRecord& Left, const FArcForestPatchRecord& Right)
			{
				if (Left.SpeciesId != Right.SpeciesId)
				{
					return GuidLess(Left.SpeciesId, Right.SpeciesId);
				}
				if (Left.CellIndex != Right.CellIndex)
				{
					return Left.CellIndex < Right.CellIndex;
				}
				return Left.Id < Right.Id;
			});
		TArray<FArcForestPatchRecord> MergedPatches;
		MergedPatches.Reserve(Snapshot.Patches.Num());
		for (const FArcForestPatchRecord& Source : Snapshot.Patches)
		{
			FArcForestPatchRecord* Target = MergedPatches.IsEmpty() ? nullptr : &MergedPatches.Last();
			const bool bSameKey = Target != nullptr
				&& Target->SpeciesId == Source.SpeciesId
				&& Target->CellIndex == Source.CellIndex;
			if (!bSameKey)
			{
				MergedPatches.Add(Source);
				continue;
			}
			const double TargetWeight = Target->StemOrCoverEquivalent;
			const double SourceWeight = Source.StemOrCoverEquivalent;
			const double NewWeight = TargetWeight + SourceWeight;
			const double NewHeight =
				(Target->MeanHeightCm * TargetWeight + Source.MeanHeightCm * SourceWeight) / NewWeight;
			const double NewAge =
				(Target->MeanAgeYears * TargetWeight + Source.MeanAgeYears * SourceWeight) / NewWeight;
			const double NewVigor =
				(Target->MeanVigor * TargetWeight + Source.MeanVigor * SourceWeight) / NewWeight;
			const double NewBiomass = static_cast<double>(Target->BiomassKg) + Source.BiomassKg;
			if (!FMath::IsFinite(NewWeight)
				|| NewWeight > MAX_flt
				|| !FMath::IsFinite(NewHeight)
				|| !FMath::IsFinite(NewAge)
				|| !FMath::IsFinite(NewVigor)
				|| !FMath::IsFinite(NewBiomass)
				|| NewBiomass > MAX_flt)
			{
				OutError = TEXT("ArcForest canonical patch merge produced unrepresentable state.");
				return false;
			}
			Target->StemOrCoverEquivalent = static_cast<float>(NewWeight);
			Target->MeanHeightCm = static_cast<float>(NewHeight);
			Target->MeanAgeYears = static_cast<float>(NewAge);
			Target->MeanVigor = static_cast<float>(NewVigor);
			Target->BiomassKg = static_cast<float>(NewBiomass);
		}
		Snapshot.Patches = MoveTemp(MergedPatches);
		return true;
	}

	double CalculateSeedOutputFactor(
		const FArcForestFrozenSpeciesSetup& Species,
		const double HeightCm,
		const double Vigor)
	{
		const double NormalizedSize = FMath::Clamp(
			HeightCm / FMath::Max(1.0, static_cast<double>(Species.MaxHeightCm)),
			0.0,
			1.0);
		return NormalizedSize * FMath::Clamp(Vigor, 0.0, 1.0);
	}

	bool BuildRecruitmentAttemptGroups(
		const FArcForestRunInput& Input,
		const TConstArrayView<uint8> BaseValidityMask,
		const TArray<FArcForestFrozenSpeciesSetup>& Species,
		const TMap<FGuid, int32>& SpeciesIndices,
		const FValidBaseCellTable& ValidBaseCells,
		const int32 Year,
		const FArcForestWorldSnapshot& Snapshot,
		const FArcForestCancellationToken& Cancellation,
		UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics,
		TArray<FRecruitmentAttemptGroup>& OutGroups,
		bool& bOutCancelled,
		FString& OutError)
	{
		const FArcEcotopeSiteMetadata& SiteMetadata = GetSiteMetadata(Input);
		bOutCancelled = false;
		TArray<FPropaguleContribution> Contributions;
		Contributions.Reserve(Snapshot.Individuals.Num() + Snapshot.Cohorts.Num() + Snapshot.Patches.Num());
		int32 WorkCounter = 0;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Reproduction_ContributionsIndividuals);
			for (const FArcForestIndividualRecord& Record : Snapshot.Individuals)
			{
				if ((++WorkCounter & 4095) == 0 && Cancellation.IsCancellationRequested())
				{
					bOutCancelled = true;
					return false;
				}
				const int32 SpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
				const FArcForestFrozenSpeciesSetup& Model = Species[SpeciesIndex];
				if (!IsMature(Record.AgeYears, Model.MaturityAgeYears))
				{
					continue;
				}
				int32 BaseCellIndex = INDEX_NONE;
				if (!TryGetBaseCellIndex(
						SiteMetadata, BaseValidityMask, Record.LocalPosition, BaseCellIndex))
				{
					OutError = TEXT("ArcForest local reproduction found an individual outside its frozen site.");
					return false;
				}
				const double Expected = Model.AnnualPropaguleProduction
					* CalculateSeedOutputFactor(Model, Record.HeightCm, Record.Vigor);
				if (!FMath::IsFinite(Expected) || Expected < 0.0)
				{
					OutError = TEXT("ArcForest individual fecundity was not finite.");
					return false;
				}
				if (Expected > 0.0)
				{
					Contributions.Add({SpeciesIndex, BaseCellIndex, Record.Id, 0, Expected});
				}
			}
		}
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Reproduction_ContributionsCohorts);
			for (const FArcForestCohortRecord& Record : Snapshot.Cohorts)
			{
				if ((++WorkCounter & 4095) == 0 && Cancellation.IsCancellationRequested())
				{
					bOutCancelled = true;
					return false;
				}
				const int32 SpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
				const FArcForestFrozenSpeciesSetup& Model = Species[SpeciesIndex];
				if (!IsMature(Record.MeanAgeYears, Model.MaturityAgeYears))
				{
					continue;
				}
				int32 BaseCellIndex = INDEX_NONE;
				if (!TryGetRepresentativeBaseCellForRecordCell(
						SiteMetadata, BaseValidityMask, Record.CellIndex, BaseCellIndex))
				{
					OutError = TEXT("ArcForest local reproduction found a cohort on a record cell with no valid base cell.");
					return false;
				}
				const double Expected = Model.AnnualPropaguleProduction
					* CalculateSeedOutputFactor(Model, Record.MeanHeightCm, Record.MeanVigor)
					* static_cast<double>(Record.StemCount);
				if (!FMath::IsFinite(Expected) || Expected < 0.0)
				{
					OutError = TEXT("ArcForest cohort fecundity was not finite.");
					return false;
				}
				if (Expected > 0.0)
				{
					Contributions.Add({SpeciesIndex, BaseCellIndex, Record.Id, 1, Expected});
				}
			}
		}
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Reproduction_ContributionsPatches);
			for (const FArcForestPatchRecord& Record : Snapshot.Patches)
			{
				if ((++WorkCounter & 4095) == 0 && Cancellation.IsCancellationRequested())
				{
					bOutCancelled = true;
					return false;
				}
				const int32 SpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
				const FArcForestFrozenSpeciesSetup& Model = Species[SpeciesIndex];
				// A form that never individualizes spreads laterally instead of by propagule; its
				// equivalent scaled this contribution and drove the whole per-year candidate count.
				if (!Model.bIndividualizes)
				{
					continue;
				}
				if (!IsMature(Record.MeanAgeYears, Model.MaturityAgeYears))
				{
					continue;
				}
				int32 BaseCellIndex = INDEX_NONE;
				if (!TryGetRepresentativeBaseCellForRecordCell(
						SiteMetadata, BaseValidityMask, Record.CellIndex, BaseCellIndex))
				{
					OutError = TEXT("ArcForest local reproduction found a patch on a record cell with no valid base cell.");
					return false;
				}
				const double Expected = Model.AnnualPropaguleProduction
					* CalculateSeedOutputFactor(Model, Record.MeanHeightCm, Record.MeanVigor)
					* Record.StemOrCoverEquivalent;
				if (!FMath::IsFinite(Expected) || Expected < 0.0)
				{
					OutError = TEXT("ArcForest patch fecundity was not finite.");
					return false;
				}
				if (Expected > 0.0)
				{
					Contributions.Add({SpeciesIndex, BaseCellIndex, Record.Id, 2, Expected});
				}
			}
		}

		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Reproduction_ContributionSort);
			Contributions.Sort(
				[](const FPropaguleContribution& Left, const FPropaguleContribution& Right)
				{
					if (Left.SpeciesIndex != Right.SpeciesIndex)
					{
						return Left.SpeciesIndex < Right.SpeciesIndex;
					}
					if (Left.CellIndex != Right.CellIndex)
					{
						return Left.CellIndex < Right.CellIndex;
					}
					if (Left.SourceType != Right.SourceType)
					{
						return Left.SourceType < Right.SourceType;
					}
					return Left.SourceId < Right.SourceId;
				});
		}

		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Reproduction_LocalGroups);
			for (int32 ContributionIndex = 0; ContributionIndex < Contributions.Num();)
			{
				const int32 SpeciesIndex = Contributions[ContributionIndex].SpeciesIndex;
				const int32 CellIndex = Contributions[ContributionIndex].CellIndex;
				double Expected = 0.0;
				do
				{
					Expected = FMath::Min(
						Expected + Contributions[ContributionIndex].ExpectedSurvivors,
						static_cast<double>(MaximumRecruitmentAttemptsPerYear));
					++ContributionIndex;
				}
				while (ContributionIndex < Contributions.Num()
					&& Contributions[ContributionIndex].SpeciesIndex == SpeciesIndex
					&& Contributions[ContributionIndex].CellIndex == CellIndex);

				int64 Desired = 0;
				const FArcForestFrozenSpeciesSetup& Model = Species[SpeciesIndex];
				const uint64 RoundingKey = UE::ArcForest::MakeAnnualKey(
					Snapshot.SimulationId,
					Input.Seed,
					Year,
					UE::ArcForest::EDeterministicDomain::LocalReproductionRounding,
					Model.SpeciesId,
					CellIndex,
					0,
					0,
					FMath::RoundToInt64(Expected * ProbabilityQuantum));
				if (!UE::ArcForest::TryRoundNonNegative(Expected, RoundingKey, Desired))
				{
					OutError = TEXT("ArcForest local reproduction count was not representable.");
					return false;
				}
				if (Desired > 0)
				{
					FRecruitmentAttemptGroup& Group = OutGroups.AddDefaulted_GetRef();
					Group.SpeciesId = Model.SpeciesId;
					Group.SpeciesIndex = SpeciesIndex;
					Group.SourceCellIndex = CellIndex;
					Group.Source = ERecruitmentSource::Local;
					Group.DesiredAttempts = Desired;
					Group.GroupIdentity = RoundingKey;
				}
			}
		}

		double ValidAreaSquareCentimeters = 0.0;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Reproduction_ValidArea);
			ValidAreaSquareCentimeters = ValidBaseCells.TotalAreaSquareCentimeters;
		}
		if (!FMath::IsFinite(ValidAreaSquareCentimeters))
		{
			OutError = TEXT("ArcForest valid regional-arrival area overflowed.");
			return false;
		}
		const double ValidAreaHectares = ValidAreaSquareCentimeters / SquareCentimetersPerHectare;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Reproduction_RegionalGroups);
			for (int32 SpeciesIndex = 0; SpeciesIndex < Species.Num(); ++SpeciesIndex)
			{
				const FArcForestFrozenSpeciesSetup& Model = Species[SpeciesIndex];
				const double Expected = FMath::Min(
					static_cast<double>(Model.RegionalArrivalPerHectarePerYear) * ValidAreaHectares,
					static_cast<double>(MaximumRecruitmentAttemptsPerYear));
				if (!FMath::IsFinite(Expected) || Expected < 0.0)
				{
					OutError = TEXT("ArcForest regional-arrival expectation was not finite.");
					return false;
				}
				int64 Desired = 0;
				const uint64 RoundingKey = UE::ArcForest::MakeAnnualKey(
					Snapshot.SimulationId,
					Input.Seed,
					Year,
					UE::ArcForest::EDeterministicDomain::RegionalArrivalRounding,
					Model.SpeciesId,
					INDEX_NONE,
					0,
					0,
					FMath::RoundToInt64(Expected * ProbabilityQuantum));
				if (!UE::ArcForest::TryRoundNonNegative(Expected, RoundingKey, Desired))
				{
					OutError = TEXT("ArcForest regional-arrival count was not representable.");
					return false;
				}
				if (Desired > 0)
				{
					FRecruitmentAttemptGroup& Group = OutGroups.AddDefaulted_GetRef();
					Group.SpeciesId = Model.SpeciesId;
					Group.SpeciesIndex = SpeciesIndex;
					Group.SourceCellIndex = INDEX_NONE;
					Group.Source = ERecruitmentSource::Regional;
					Group.DesiredAttempts = Desired;
					Group.GroupIdentity = RoundingKey;
				}
			}
		}
		return true;
	}

	bool AllocateRecruitmentAttempts(
		const FArcForestRunInput& Input,
		const int32 Year,
		const FArcForestWorldSnapshot& Snapshot,
		const int64 AttemptBudget,
		TArray<FRecruitmentAttemptGroup>& Groups,
		FString& OutError)
	{
		int64 TotalDesired = 0;
		for (const FRecruitmentAttemptGroup& Group : Groups)
		{
			if (TotalDesired > MAX_int64 - Group.DesiredAttempts)
			{
				OutError = TEXT("ArcForest recruitment demand overflowed.");
				return false;
			}
			TotalDesired += Group.DesiredAttempts;
		}
		const int64 AllocatedTotal = FMath::Min(TotalDesired, AttemptBudget);
		if (AllocatedTotal <= 0)
		{
			return true;
		}
		if (AllocatedTotal == TotalDesired)
		{
			for (FRecruitmentAttemptGroup& Group : Groups)
			{
				Group.AllocatedAttempts = Group.DesiredAttempts;
			}
			return true;
		}

		int64 Assigned = 0;
		for (FRecruitmentAttemptGroup& Group : Groups)
		{
			const double Exact =
				static_cast<double>(AllocatedTotal) * static_cast<double>(Group.DesiredAttempts)
				/ static_cast<double>(TotalDesired);
			Group.AllocatedAttempts = FMath::FloorToInt64(Exact);
			const double RawFraction = Exact - static_cast<double>(Group.AllocatedAttempts);
			Group.AllocationFraction = RawFraction > 0.0
				? FMath::Max(
					1.0 / ProbabilityQuantum,
					DequantizeProbability(QuantizeProbability(RawFraction)))
				: 0.0;
			Assigned += Group.AllocatedAttempts;
			const uint64 SelectionKey = UE::ArcForest::MakeAnnualKey(
				Snapshot.SimulationId,
				Input.Seed,
				Year,
				UE::ArcForest::EDeterministicDomain::RecruitmentAttemptAllocation,
				Group.SpeciesId,
				Group.SourceCellIndex,
				Group.GroupIdentity,
				static_cast<int32>(Group.Source),
				Group.DesiredAttempts);
			Group.AllocationScore = Group.AllocationFraction > 0.0
				? -FMath::Loge(UE::ArcForest::KeyToUnitOpen(SelectionKey)) / Group.AllocationFraction
				: TNumericLimits<double>::Max();
		}
		const int64 Remainder = AllocatedTotal - Assigned;
		Groups.Sort(
			[](const FRecruitmentAttemptGroup& Left, const FRecruitmentAttemptGroup& Right)
			{
				if (Left.AllocationScore != Right.AllocationScore)
				{
					return Left.AllocationScore < Right.AllocationScore;
				}
				if (Left.SpeciesId != Right.SpeciesId)
				{
					return GuidLess(Left.SpeciesId, Right.SpeciesId);
				}
				if (Left.Source != Right.Source)
				{
					return Left.Source < Right.Source;
				}
				return Left.SourceCellIndex < Right.SourceCellIndex;
			});
		if (Remainder < 0
			|| Remainder > Groups.Num()
			|| (Remainder > 0 && Groups[static_cast<int32>(Remainder - 1)].AllocationFraction <= 0.0))
		{
			OutError = TEXT("ArcForest recruitment attempt allocation was not representable.");
			return false;
		}
		for (int64 GroupIndex = 0; GroupIndex < Remainder; ++GroupIndex)
		{
			++Groups[static_cast<int32>(GroupIndex)].AllocatedAttempts;
		}
		return true;
	}

	struct FLazyRegionalSpeciesRange
	{
		FGuid SpeciesId;
		int32 SpeciesIndex = INDEX_NONE;
		int32 OutputOffset = 0;
		int32 AttemptCount = 0;
		bool bZeroProposalMass = false;
	};

	struct FLazyRegionalRecruitmentOutput
	{
		TArray<FLazyRegionalSpeciesRange> SpeciesRanges;
		TArray<int32> AcceptedBaseCellByRegionalAttempt;

		void Empty()
		{
			SpeciesRanges.Empty();
			AcceptedBaseCellByRegionalAttempt.Empty();
		}
	};

	struct FLazyRegionalRecruitmentTelemetry
	{
		struct FSpeciesTelemetry
		{
			FGuid SpeciesId;
			int32 SpeciesIndex = INDEX_NONE;
			int32 AllocatedAttemptCount = 0;
			int64 ProposalCount = 0;
			int64 AcceptedAttemptCount = 0;
			int64 UnresolvedAttemptCount = 0;
			bool bProposalMassEvaluated = false;
			bool bZeroProposalMass = false;
			int32 MaximumRetryOrdinal = INDEX_NONE;
		};

		int64 AllocatedNonZeroMassAttempts = 0;
		int64 ProposalCount = 0;
		int64 HabitatQueryCount = 0;
		int64 QueryWindowCount = 0;
		int64 AcceptedAttemptCount = 0;
		int64 UnresolvedAttemptCount = 0;
		int32 ZeroMassGroupCount = 0;
		int32 EnvelopeViolationCount = 0;
		int32 CompletedRoundCount = 0;
		int32 MaximumRetryOrdinal = INDEX_NONE;
		int64 PeakProjectedScratchBytes = 0;
		int64 PeakActualScratchBytes = 0;
		double WallMilliseconds = 0.0;
		TArray<FSpeciesTelemetry> Species;
	};

	enum class ELazyRegionalSamplerOutcome : uint8
	{
		NotAttempted,
		Success,
		EnvelopeViolation,
		Exhaustion,
		Cancelled,
		Error,
		PostSamplerFailure,
	};

	struct FLazyRegionalPendingAttempt
	{
		int32 AttemptOrdinal = INDEX_NONE;
		int32 RetryOrdinal = 0;
	};

	struct FLazyRegionalProposalWork
	{
		int32 AttemptOrdinal = INDEX_NONE;
		int32 RetryOrdinal = 0;
		int32 ValidEntryIndex = INDEX_NONE;
		double Envelope = 0.0;
	};

	bool GenerateLazyRegionalRecruitment(
		const FArcForestRunInput& Input,
		const TConstArrayView<FArcForestFrozenSpeciesSetup> Species,
		const FValidBaseCellTable& ValidBaseCells,
		const FRegionalRecruitmentRecordIndex& RecordIndex,
		const FRegionalRecruitmentEnvelopeTable& EnvelopeTable,
		const FFactoredHabitatQueryContext& FactoredQueryContext,
		const FArcForestRegionalRecruitmentConfig& RecruitmentConfig,
		const TConstArrayView<double> RecordCellRecruitLight,
		const TConstArrayView<double> RecordCellCrowding,
		const int32 Year,
		const FArcForestWorldSnapshot& Snapshot,
		const FArcForestCancellationToken& Cancellation,
		const TConstArrayView<FRecruitmentAttemptGroup> Groups,
		FFactoredHabitatQueryTelemetry& QueryTelemetry,
		FLazyRegionalRecruitmentOutput& OutOutput,
		FLazyRegionalRecruitmentTelemetry& OutTelemetry,
		bool& bOutCancelled,
		FString& OutError)
	{
		const double StartSeconds = FPlatformTime::Seconds();
		OutOutput.Empty();
		OutTelemetry = FLazyRegionalRecruitmentTelemetry();
		bOutCancelled = false;
		ON_SCOPE_EXIT
		{
			OutTelemetry.WallMilliseconds =
				UE::ArcForest::GetElapsedMilliseconds(StartSeconds);
		};

		const FArcEcotopeSiteMetadata& Site = GetSiteMetadata(Input);
		const int32 RecordCellCount = Site.GetLevelCellCount(Site.RecordGridLevel);
		const int32 BaseCellCount = Site.GetCellCount();
		if (!FactoredQueryContext.IsValid()
			|| FactoredQueryContext.Input != &Input
			|| FactoredQueryContext.Site != &Site
			|| FactoredQueryContext.ValidBaseCells != &ValidBaseCells
			|| FactoredQueryContext.Cancellation != &Cancellation
			|| RecordCellCount <= 0
			|| RecordCellRecruitLight.Num() != RecordCellCount
			|| RecordCellCrowding.Num() != RecordCellCount
			|| RecordIndex.SiteIdentity != &Site
			|| RecordIndex.ValidBaseCellsIdentity != &ValidBaseCells
			|| RecordIndex.RecordOffsets.Num() != RecordCellCount + 1
			|| RecordIndex.RecordOffsets[0] != 0
			|| RecordIndex.RecordOffsets.Last() != ValidBaseCells.Entries.Num()
			|| RecordIndex.ValidEntryIndicesByRecord.Num() != ValidBaseCells.Entries.Num()
			|| RecordIndex.ValidEntryIndexByBaseCell.Num() != BaseCellCount
			|| EnvelopeTable.SiteIdentity != &Site
			|| EnvelopeTable.LayoutIdentity != FactoredQueryContext.Layout
			|| EnvelopeTable.ValidBaseCellsIdentity != &ValidBaseCells
			|| EnvelopeTable.RecordIndexIdentity != &RecordIndex
			|| EnvelopeTable.FactorStoreIdentity != FactoredQueryContext.Store
			|| EnvelopeTable.EnvelopeRowBySpecies.Num() != Species.Num())
		{
			OutError = TEXT("ArcForest lazy regional recruitment received inconsistent run-local tables or annual habitat context.");
			return false;
		}
		if (Cancellation.IsCancellationRequested())
		{
			bOutCancelled = true;
			return false;
		}

		const int64 ScratchBudgetBytes = static_cast<int64>(FMath::Max(
			CVarArcForestRegionalRecruitmentSamplerScratchBudgetMiB.GetValueOnAnyThread(), 1))
			* 1024ll * 1024ll;
		int64 TotalAllocatedAttempts = 0;
		int64 RegionalGroupCount = 0;
		int64 MaximumSpeciesAttemptCount = 0;
		for (const FRecruitmentAttemptGroup& Group : Groups)
		{
			if (Group.Source != ERecruitmentSource::Regional || Group.AllocatedAttempts <= 0)
			{
				continue;
			}
			if (!Species.IsValidIndex(Group.SpeciesIndex)
				|| Group.SpeciesId != Species[Group.SpeciesIndex].SpeciesId
				|| Group.AllocatedAttempts > MAX_int32
				|| !TryAddCapacityValue(
					TotalAllocatedAttempts, Group.AllocatedAttempts, TotalAllocatedAttempts)
				|| TotalAllocatedAttempts > MaximumRecruitmentAttemptsPerYear
				|| TotalAllocatedAttempts > MAX_int32)
			{
				OutError = TEXT("ArcForest lazy regional recruitment groups exceeded their bounded or canonical shape.");
				return false;
			}
			if (!TryAddCapacityValue(RegionalGroupCount, 1, RegionalGroupCount))
			{
				OutError = TEXT("ArcForest lazy regional recruitment group count overflowed.");
				return false;
			}
			MaximumSpeciesAttemptCount = FMath::Max(
				MaximumSpeciesAttemptCount, Group.AllocatedAttempts);
		}

		int64 PreflightScratchBytes = 0;
		if (!TryAccumulateCapacityBytes(
				RegionalGroupCount, sizeof(FLazyRegionalSpeciesRange), PreflightScratchBytes)
			|| !TryAccumulateCapacityBytes(
				RegionalGroupCount,
				sizeof(FLazyRegionalRecruitmentTelemetry::FSpeciesTelemetry),
				PreflightScratchBytes)
			|| !TryAccumulateCapacityBytes(
				TotalAllocatedAttempts, sizeof(int32), PreflightScratchBytes)
			|| !TryAccumulateCapacityBytes(
				RecordCellCount, sizeof(double), PreflightScratchBytes)
			|| !TryAccumulateCapacityBytes(
				RecordCellCount, sizeof(double), PreflightScratchBytes)
			|| !TryAccumulateCapacityBytes(
				MaximumSpeciesAttemptCount,
				sizeof(FLazyRegionalPendingAttempt),
				PreflightScratchBytes)
			|| !TryAccumulateCapacityBytes(
				MaximumSpeciesAttemptCount,
				sizeof(FLazyRegionalPendingAttempt),
				PreflightScratchBytes)
			|| (RegionalGroupCount > 0
				&& (!TryAccumulateCapacityBytes(1, sizeof(FFactoredHabitatQuery), PreflightScratchBytes)
					|| !TryAccumulateCapacityBytes(1, sizeof(FLazyRegionalProposalWork), PreflightScratchBytes)
					|| !TryAccumulateCapacityBytes(1, sizeof(uint32), PreflightScratchBytes))))
		{
			OutError = TEXT("ArcForest lazy regional recruitment projected scratch accounting overflowed.");
			return false;
		}
		OutTelemetry.PeakProjectedScratchBytes = PreflightScratchBytes;
		if (PreflightScratchBytes > ScratchBudgetBytes)
		{
			OutError = TEXT("ArcForest lazy regional recruitment projected fixed scratch exceeded its configured budget before allocation.");
			return false;
		}

		OutOutput.SpeciesRanges.Reserve(static_cast<int32>(RegionalGroupCount));
		for (const FRecruitmentAttemptGroup& Group : Groups)
		{
			if (Group.Source != ERecruitmentSource::Regional || Group.AllocatedAttempts <= 0)
			{
				continue;
			}
			FLazyRegionalSpeciesRange& Range = OutOutput.SpeciesRanges.AddDefaulted_GetRef();
			Range.SpeciesId = Group.SpeciesId;
			Range.SpeciesIndex = Group.SpeciesIndex;
			Range.AttemptCount = static_cast<int32>(Group.AllocatedAttempts);
		}
		OutOutput.SpeciesRanges.Sort(
			[](const FLazyRegionalSpeciesRange& Left, const FLazyRegionalSpeciesRange& Right)
			{
				if (Left.SpeciesIndex != Right.SpeciesIndex)
				{
					return Left.SpeciesIndex < Right.SpeciesIndex;
				}
				return GuidLess(Left.SpeciesId, Right.SpeciesId);
			});
		OutTelemetry.Species.SetNum(OutOutput.SpeciesRanges.Num());
		int32 OutputOffset = 0;
		for (int32 RangeIndex = 0; RangeIndex < OutOutput.SpeciesRanges.Num(); ++RangeIndex)
		{
			FLazyRegionalSpeciesRange& Range = OutOutput.SpeciesRanges[RangeIndex];
			if (RangeIndex > 0
				&& OutOutput.SpeciesRanges[RangeIndex - 1].SpeciesIndex == Range.SpeciesIndex)
			{
				OutError = TEXT("ArcForest lazy regional recruitment found duplicate regional groups for one species.");
				OutOutput.Empty();
				return false;
			}
			Range.OutputOffset = OutputOffset;
			OutputOffset += Range.AttemptCount;
			FLazyRegionalRecruitmentTelemetry::FSpeciesTelemetry& SpeciesTelemetry =
				OutTelemetry.Species[RangeIndex];
			SpeciesTelemetry.SpeciesId = Range.SpeciesId;
			SpeciesTelemetry.SpeciesIndex = Range.SpeciesIndex;
			SpeciesTelemetry.AllocatedAttemptCount = Range.AttemptCount;
		}
		if (OutputOffset != TotalAllocatedAttempts)
		{
			OutError = TEXT("ArcForest lazy regional recruitment output offsets were inconsistent.");
			OutOutput.Empty();
			return false;
		}
		int64 AcceptedOutputAllocationPreflightBytes = 0;
		if (!TryAccumulateAllocatedBytes(
				OutOutput.SpeciesRanges.GetAllocatedSize(), AcceptedOutputAllocationPreflightBytes)
			|| !TryAccumulateAllocatedBytes(
				OutTelemetry.Species.GetAllocatedSize(), AcceptedOutputAllocationPreflightBytes)
			|| !TryAccumulateCapacityBytes(
				TotalAllocatedAttempts, sizeof(int32), AcceptedOutputAllocationPreflightBytes)
			|| AcceptedOutputAllocationPreflightBytes > ScratchBudgetBytes)
		{
			OutError = TEXT("ArcForest lazy regional recruitment accepted-output allocation preflight exceeded its configured scratch budget.");
			OutOutput.Empty();
			return false;
		}
		OutOutput.AcceptedBaseCellByRegionalAttempt.Init(INDEX_NONE, OutputOffset);
		const auto AccumulatePersistentScratch = [&OutOutput, &OutTelemetry](int64& OutBytes)
		{
			OutBytes = 0;
			return TryAccumulateAllocatedBytes(
					OutOutput.SpeciesRanges.GetAllocatedSize(), OutBytes)
				&& TryAccumulateAllocatedBytes(
					OutTelemetry.Species.GetAllocatedSize(), OutBytes)
				&& TryAccumulateAllocatedBytes(
					OutOutput.AcceptedBaseCellByRegionalAttempt.GetAllocatedSize(), OutBytes);
		};
		int64 OutputActualScratchBytes = 0;
		if (!AccumulatePersistentScratch(OutputActualScratchBytes)
			|| OutputActualScratchBytes > ScratchBudgetBytes)
		{
			OutError = TEXT("ArcForest lazy regional recruitment output capacity exceeded its configured scratch budget.");
			OutOutput.Empty();
			return false;
		}
		OutTelemetry.PeakActualScratchBytes = OutputActualScratchBytes;
		if (OutOutput.SpeciesRanges.IsEmpty())
		{
			return true;
		}

		int64 RecordArrayAllocationPreflightBytes = OutputActualScratchBytes;
		if (!TryAccumulateCapacityBytes(
				RecordCellCount, sizeof(double), RecordArrayAllocationPreflightBytes)
			|| !TryAccumulateCapacityBytes(
				RecordCellCount, sizeof(double), RecordArrayAllocationPreflightBytes)
			|| RecordArrayAllocationPreflightBytes > ScratchBudgetBytes)
		{
			OutError = TEXT("ArcForest lazy regional recruitment record-array allocation preflight exceeded its configured scratch budget.");
			OutOutput.Empty();
			return false;
		}
		TArray<double> RecordLightResponse;
		TArray<double> RecordProposalPrefix;
		RecordLightResponse.SetNumUninitialized(RecordCellCount);
		RecordProposalPrefix.SetNumUninitialized(RecordCellCount);
		int64 RecordArrayActualScratchBytes = 0;
		if (!AccumulatePersistentScratch(RecordArrayActualScratchBytes)
			|| !TryAccumulateAllocatedBytes(
				RecordLightResponse.GetAllocatedSize(), RecordArrayActualScratchBytes)
			|| !TryAccumulateAllocatedBytes(
				RecordProposalPrefix.GetAllocatedSize(), RecordArrayActualScratchBytes)
			|| RecordArrayActualScratchBytes > ScratchBudgetBytes)
		{
			OutError = TEXT("ArcForest lazy regional recruitment record-array capacity exceeded its configured scratch budget.");
			OutOutput.Empty();
			return false;
		}
		OutTelemetry.PeakActualScratchBytes = FMath::Max(
			OutTelemetry.PeakActualScratchBytes, RecordArrayActualScratchBytes);
		for (int32 RangeIndex = 0; RangeIndex < OutOutput.SpeciesRanges.Num(); ++RangeIndex)
		{
			FLazyRegionalSpeciesRange& Range = OutOutput.SpeciesRanges[RangeIndex];
			FLazyRegionalRecruitmentTelemetry::FSpeciesTelemetry& SpeciesTelemetry =
				OutTelemetry.Species[RangeIndex];
			if (Cancellation.IsCancellationRequested())
			{
				bOutCancelled = true;
				OutOutput.Empty();
				return false;
			}
			if (!EnvelopeTable.EnvelopeRowBySpecies.IsValidIndex(Range.SpeciesIndex))
			{
				OutError = TEXT("ArcForest lazy regional recruitment species had no envelope row mapping.");
				OutOutput.Empty();
				return false;
			}
			const int32 EnvelopeRow = EnvelopeTable.EnvelopeRowBySpecies[Range.SpeciesIndex];
			if (!EnvelopeTable.SpeciesIndexByEnvelopeRow.IsValidIndex(EnvelopeRow)
				|| EnvelopeTable.SpeciesIndexByEnvelopeRow[EnvelopeRow] != Range.SpeciesIndex)
			{
				OutError = TEXT("ArcForest lazy regional recruitment envelope species mapping was inconsistent.");
				OutOutput.Empty();
				return false;
			}
			const int64 EnvelopeRowOffset = static_cast<int64>(EnvelopeRow) * RecordCellCount;
			if (EnvelopeRowOffset < 0
				|| EnvelopeRowOffset + RecordCellCount > EnvelopeTable.MaxStaticCellWeight.Num())
			{
				OutError = TEXT("ArcForest lazy regional recruitment envelope row exceeded its table.");
				OutOutput.Empty();
				return false;
			}

			const FArcForestFrozenSpeciesSetup& Model = Species[Range.SpeciesIndex];
			double TotalProposalMass = 0.0;
			int32 LastSelectableRecord = INDEX_NONE;
			for (int32 RecordCellIndex = 0; RecordCellIndex < RecordCellCount; ++RecordCellIndex)
			{
				if ((RecordCellIndex & 4095) == 0 && Cancellation.IsCancellationRequested())
				{
					bOutCancelled = true;
					OutOutput.Empty();
					return false;
				}
				const int32 Begin = RecordIndex.RecordOffsets[RecordCellIndex];
				const int32 End = RecordIndex.RecordOffsets[RecordCellIndex + 1];
				const double Envelope = EnvelopeTable.MaxStaticCellWeight[
					static_cast<int32>(EnvelopeRowOffset + RecordCellIndex)];
				const double LightResponse = EvaluateResponse(
					Model.LightResponse,
					static_cast<float>(RecordCellRecruitLight[RecordCellIndex]));
				const double Crowding = RecordCellCrowding[RecordCellIndex];
				if (Begin < 0
					|| End < Begin
					|| End > RecordIndex.ValidEntryIndicesByRecord.Num()
					|| !FMath::IsFinite(Envelope)
					|| Envelope < 0.0
					|| !FMath::IsFinite(LightResponse)
					|| LightResponse < 0.0
					|| !FMath::IsFinite(Crowding)
					|| Crowding < 0.0)
				{
					OutError = TEXT("ArcForest lazy regional recruitment found an invalid record proposal input.");
					OutOutput.Empty();
					return false;
				}
				RecordLightResponse[RecordCellIndex] = LightResponse;
				const double RecordMass = static_cast<double>(End - Begin)
					* Envelope * RecordLightResponse[RecordCellIndex] * Crowding;
				const double NewTotalProposalMass = TotalProposalMass + RecordMass;
				if (!FMath::IsFinite(RecordMass)
					|| RecordMass < 0.0
					|| !FMath::IsFinite(NewTotalProposalMass))
				{
					OutError = TEXT("ArcForest lazy regional recruitment record proposal mass was invalid or overflowed.");
					OutOutput.Empty();
					return false;
				}
				RecordProposalPrefix[RecordCellIndex] = NewTotalProposalMass;
				if (NewTotalProposalMass > TotalProposalMass)
				{
					LastSelectableRecord = RecordCellIndex;
				}
				TotalProposalMass = NewTotalProposalMass;
			}
			SpeciesTelemetry.bProposalMassEvaluated = true;
			if (TotalProposalMass == 0.0)
			{
				Range.bZeroProposalMass = true;
				SpeciesTelemetry.bZeroProposalMass = true;
				++OutTelemetry.ZeroMassGroupCount;
				continue;
			}
			if (!RecordProposalPrefix.IsValidIndex(LastSelectableRecord))
			{
				OutError = TEXT("ArcForest positive lazy regional proposal mass had no selectable record.");
				OutOutput.Empty();
				return false;
			}
			OutTelemetry.AllocatedNonZeroMassAttempts += Range.AttemptCount;

			TArray<FLazyRegionalPendingAttempt> PendingAttempts;
			TArray<FLazyRegionalPendingAttempt> NextPendingAttempts;
			int64 PendingAllocationPreflightBytes = RecordArrayActualScratchBytes;
			if (!TryAccumulateCapacityBytes(
					Range.AttemptCount,
					sizeof(FLazyRegionalPendingAttempt),
					PendingAllocationPreflightBytes)
				|| !TryAccumulateCapacityBytes(
					Range.AttemptCount,
					sizeof(FLazyRegionalPendingAttempt),
					PendingAllocationPreflightBytes)
				|| !TryAccumulateCapacityBytes(
					1, sizeof(FFactoredHabitatQuery), PendingAllocationPreflightBytes)
				|| !TryAccumulateCapacityBytes(
					1, sizeof(FLazyRegionalProposalWork), PendingAllocationPreflightBytes)
				|| !TryAccumulateCapacityBytes(
					1, sizeof(uint32), PendingAllocationPreflightBytes)
				|| PendingAllocationPreflightBytes > ScratchBudgetBytes)
			{
				OutError = TEXT("ArcForest lazy regional recruitment pending-attempt allocation preflight exceeded its configured scratch budget.");
				OutOutput.Empty();
				return false;
			}
			PendingAttempts.Reserve(Range.AttemptCount);
			NextPendingAttempts.Reserve(Range.AttemptCount);
			int64 PendingActualScratchBytes = 0;
			if (!AccumulatePersistentScratch(PendingActualScratchBytes)
				|| !TryAccumulateAllocatedBytes(
					RecordLightResponse.GetAllocatedSize(), PendingActualScratchBytes)
				|| !TryAccumulateAllocatedBytes(
					RecordProposalPrefix.GetAllocatedSize(), PendingActualScratchBytes)
				|| !TryAccumulateAllocatedBytes(
					PendingAttempts.GetAllocatedSize(), PendingActualScratchBytes)
				|| !TryAccumulateAllocatedBytes(
					NextPendingAttempts.GetAllocatedSize(), PendingActualScratchBytes)
				|| PendingActualScratchBytes > ScratchBudgetBytes)
			{
				OutError = TEXT("ArcForest lazy regional recruitment pending-attempt capacity exceeded its configured scratch budget.");
				OutOutput.Empty();
				return false;
			}
			OutTelemetry.PeakActualScratchBytes = FMath::Max(
				OutTelemetry.PeakActualScratchBytes, PendingActualScratchBytes);
			for (int32 AttemptOrdinal = 0; AttemptOrdinal < Range.AttemptCount; ++AttemptOrdinal)
			{
				if ((AttemptOrdinal & 4095) == 0 && Cancellation.IsCancellationRequested())
				{
					bOutCancelled = true;
					OutOutput.Empty();
					return false;
				}
				FLazyRegionalPendingAttempt& Pending = PendingAttempts.AddDefaulted_GetRef();
				Pending.AttemptOrdinal = AttemptOrdinal;
			}

			for (int32 RoundIndex = 0;
				RoundIndex < RecruitmentConfig.MaxProposalRounds
					&& !PendingAttempts.IsEmpty();
				++RoundIndex)
			{
				if (Cancellation.IsCancellationRequested())
				{
					bOutCancelled = true;
					OutOutput.Empty();
					return false;
				}
				OutTelemetry.CompletedRoundCount = FMath::Max(
					OutTelemetry.CompletedRoundCount, RoundIndex + 1);
				OutTelemetry.MaximumRetryOrdinal = FMath::Max(
					OutTelemetry.MaximumRetryOrdinal, RoundIndex);
				SpeciesTelemetry.MaximumRetryOrdinal = FMath::Max(
					SpeciesTelemetry.MaximumRetryOrdinal, RoundIndex);
				NextPendingAttempts.Reset();

				int64 FixedScratchBytes = 0;
				if (!AccumulatePersistentScratch(FixedScratchBytes)
					|| !TryAccumulateAllocatedBytes(
						RecordLightResponse.GetAllocatedSize(), FixedScratchBytes)
					|| !TryAccumulateAllocatedBytes(
						RecordProposalPrefix.GetAllocatedSize(), FixedScratchBytes)
					|| !TryAccumulateAllocatedBytes(
						PendingAttempts.GetAllocatedSize(), FixedScratchBytes)
					|| !TryAccumulateAllocatedBytes(
						NextPendingAttempts.GetAllocatedSize(), FixedScratchBytes))
				{
					OutError = TEXT("ArcForest lazy regional recruitment scratch accounting overflowed.");
					OutOutput.Empty();
					return false;
				}
				const int64 PerQueryScratchBytes = sizeof(FFactoredHabitatQuery)
					+ sizeof(FLazyRegionalProposalWork) + sizeof(uint32);
				const int64 RemainingSamplerBytes = ScratchBudgetBytes - FixedScratchBytes;
				const int32 ProviderMaximumWindow = GetMaximumFactoredHabitatQueriesPerWindow(
					sizeof(FFactoredHabitatQuery) + sizeof(FLazyRegionalProposalWork),
					FixedScratchBytes);
				const int64 SamplerMaximumWindow = RemainingSamplerBytes > 0
					? RemainingSamplerBytes / PerQueryScratchBytes
					: 0;
				const int32 MaximumWindowCount = static_cast<int32>(FMath::Min<int64>(
					PendingAttempts.Num(),
					FMath::Min<int64>(ProviderMaximumWindow, SamplerMaximumWindow)));
				if (ProviderMaximumWindow <= 0 || MaximumWindowCount <= 0)
				{
					OutError = TEXT("ArcForest lazy regional recruitment scratch budgets cannot hold one proposal window.");
					OutOutput.Empty();
					return false;
				}
				int64 ProjectedScratchBytes = 0;
				if (!TryMultiplyCapacityValue(
						MaximumWindowCount, PerQueryScratchBytes, ProjectedScratchBytes)
					|| !TryAddCapacityValue(
						FixedScratchBytes, ProjectedScratchBytes, ProjectedScratchBytes)
					|| ProjectedScratchBytes > ScratchBudgetBytes)
				{
					OutError = TEXT("ArcForest lazy regional recruitment projected scratch exceeded its configured budget.");
					OutOutput.Empty();
					return false;
				}
				OutTelemetry.PeakProjectedScratchBytes = FMath::Max(
					OutTelemetry.PeakProjectedScratchBytes, ProjectedScratchBytes);

				TArray<FFactoredHabitatQuery> Queries;
				TArray<FLazyRegionalProposalWork> ProposalWork;
				Queries.Reserve(MaximumWindowCount);
				ProposalWork.Reserve(MaximumWindowCount);
				int64 QueryWindowActualPreflightBytes = FixedScratchBytes;
				if (!TryAccumulateAllocatedBytes(
						Queries.GetAllocatedSize(), QueryWindowActualPreflightBytes)
					|| !TryAccumulateAllocatedBytes(
						ProposalWork.GetAllocatedSize(), QueryWindowActualPreflightBytes)
					|| !TryAccumulateCapacityBytes(
						MaximumWindowCount, sizeof(uint32), QueryWindowActualPreflightBytes)
					|| QueryWindowActualPreflightBytes > ScratchBudgetBytes)
				{
					OutError = TEXT("ArcForest lazy regional recruitment allocated proposal-window capacity exceeded its configured scratch budget.");
					OutOutput.Empty();
					return false;
				}
				OutTelemetry.PeakActualScratchBytes = FMath::Max(
					OutTelemetry.PeakActualScratchBytes, QueryWindowActualPreflightBytes);
				for (int32 WindowStart = 0;
					WindowStart < PendingAttempts.Num();
					WindowStart += MaximumWindowCount)
				{
					if (Cancellation.IsCancellationRequested())
					{
						bOutCancelled = true;
						OutOutput.Empty();
						return false;
					}
					Queries.Reset();
					ProposalWork.Reset();
					bool bProposalEvidenceCommitted = false;
					const auto CommitProposalEvidence = [&]()
					{
						if (bProposalEvidenceCommitted)
						{
							return true;
						}
						int64 NewGlobalProposalCount = 0;
						int64 NewSpeciesProposalCount = 0;
						if (!TryAddCapacityValue(
								OutTelemetry.ProposalCount,
								ProposalWork.Num(),
								NewGlobalProposalCount)
							|| !TryAddCapacityValue(
								SpeciesTelemetry.ProposalCount,
								ProposalWork.Num(),
								NewSpeciesProposalCount))
						{
							if (OutError.IsEmpty())
							{
								OutError = TEXT("ArcForest lazy regional recruitment partial proposal telemetry overflowed.");
							}
							return false;
						}
						OutTelemetry.ProposalCount = NewGlobalProposalCount;
						SpeciesTelemetry.ProposalCount = NewSpeciesProposalCount;
						bProposalEvidenceCommitted = true;
						return true;
					};
					ON_SCOPE_EXIT
					{
						if (!bProposalEvidenceCommitted)
						{
							CommitProposalEvidence();
						}
					};
					const int32 WindowEnd = FMath::Min(
						WindowStart + MaximumWindowCount, PendingAttempts.Num());
					for (int32 PendingIndex = WindowStart; PendingIndex < WindowEnd; ++PendingIndex)
					{
						const FLazyRegionalPendingAttempt& Pending = PendingAttempts[PendingIndex];
						if ((PendingIndex & 4095) == 0 && Cancellation.IsCancellationRequested())
						{
							bOutCancelled = true;
							OutOutput.Empty();
							return false;
						}
						const uint64 RecordKey = UE::ArcForest::MakeAnnualKey(
							Snapshot.SimulationId,
							Input.Seed,
							Year,
							UE::ArcForest::EDeterministicDomain::RegionalEnvelopeRecordSelection,
							Range.SpeciesId,
							INDEX_NONE,
							static_cast<uint64>(Pending.AttemptOrdinal),
							Pending.RetryOrdinal,
							0);
						const double RecordSelection =
							UE::ArcForest::KeyToUnitOpen(RecordKey) * TotalProposalMass;
						int32 Low = 0;
						int32 High = LastSelectableRecord;
						while (Low < High)
						{
							const int32 Mid = Low + (High - Low) / 2;
							if (RecordProposalPrefix[Mid] > RecordSelection)
							{
								High = Mid;
							}
							else
							{
								Low = Mid + 1;
							}
						}
						int32 RecordCellIndex = Low;
						if (!(RecordProposalPrefix[RecordCellIndex] > RecordSelection))
						{
							RecordCellIndex = LastSelectableRecord;
						}
						const int32 Begin = RecordIndex.RecordOffsets[RecordCellIndex];
						const int32 End = RecordIndex.RecordOffsets[RecordCellIndex + 1];
						const int32 ValidEntryCount = End - Begin;
						const double Envelope = EnvelopeTable.MaxStaticCellWeight[
							static_cast<int32>(EnvelopeRowOffset + RecordCellIndex)];
						if (ValidEntryCount <= 0 || !(Envelope > 0.0))
						{
							OutError = TEXT("ArcForest lazy regional recruitment selected an empty or zero-envelope record.");
							OutOutput.Empty();
							return false;
						}
						const uint64 EntryKey = UE::ArcForest::MakeAnnualKey(
							Snapshot.SimulationId,
							Input.Seed,
							Year,
							UE::ArcForest::EDeterministicDomain::RegionalEnvelopeEntrySelection,
							Range.SpeciesId,
							INDEX_NONE,
							static_cast<uint64>(Pending.AttemptOrdinal),
							Pending.RetryOrdinal,
							0);
						const int32 EntryOrdinal = FMath::Min(
							static_cast<int32>(UE::ArcForest::KeyToUnitOpen(EntryKey) * ValidEntryCount),
							ValidEntryCount - 1);
						const int32 GroupedEntryIndex = Begin + EntryOrdinal;
						if (!RecordIndex.ValidEntryIndicesByRecord.IsValidIndex(GroupedEntryIndex))
						{
							OutError = TEXT("ArcForest lazy regional recruitment selected an invalid CSR entry ordinal.");
							OutOutput.Empty();
							return false;
						}
						const int32 ValidEntryIndex =
							RecordIndex.ValidEntryIndicesByRecord[GroupedEntryIndex];
						if (!ValidBaseCells.Entries.IsValidIndex(ValidEntryIndex)
							|| ValidBaseCells.Entries[ValidEntryIndex].RecordCellIndex != RecordCellIndex)
						{
							OutError = TEXT("ArcForest lazy regional recruitment CSR entry mapping was inconsistent.");
							OutOutput.Empty();
							return false;
						}

						const int32 QueryIndex = Queries.Num();
						FFactoredHabitatQuery& Query = Queries.AddDefaulted_GetRef();
						Query.SpeciesIndex = Range.SpeciesIndex;
						Query.BaseCellIndex = ValidBaseCells.Entries[ValidEntryIndex].BaseCellIndex;
						Query.OutputIndex = QueryIndex;
						Query.RouteSeedIndex = FactoredQueryContext.RegionalRouteSeeds != nullptr
							? ValidEntryIndex
							: INDEX_NONE;
						Query.StableOrdinal =
							(static_cast<int64>(Pending.RetryOrdinal) << 32)
							| static_cast<uint32>(Pending.AttemptOrdinal);
						FLazyRegionalProposalWork& Work = ProposalWork.AddDefaulted_GetRef();
						Work.AttemptOrdinal = Pending.AttemptOrdinal;
						Work.RetryOrdinal = Pending.RetryOrdinal;
						Work.ValidEntryIndex = ValidEntryIndex;
						Work.Envelope = Envelope;
					}

					int64 CallerScratchBytes = FixedScratchBytes;
					if (!TryAccumulateAllocatedBytes(Queries.GetAllocatedSize(), CallerScratchBytes)
						|| !TryAccumulateAllocatedBytes(
							ProposalWork.GetAllocatedSize(), CallerScratchBytes))
					{
						OutError = TEXT("ArcForest lazy regional proposal window scratch accounting overflowed.");
						OutOutput.Empty();
						return false;
					}
					TArray<uint32> Quantums;
					if (!CommitProposalEvidence())
					{
						OutOutput.Empty();
						return false;
					}
					int64 NewHabitatQueryCount = 0;
					int64 NewQueryWindowCount = 0;
					if (!TryAddCapacityValue(
							OutTelemetry.HabitatQueryCount,
							Queries.Num(),
							NewHabitatQueryCount)
						|| !TryAddCapacityValue(
							OutTelemetry.QueryWindowCount, 1, NewQueryWindowCount))
					{
						OutError = TEXT("ArcForest lazy regional recruitment submitted-query telemetry overflowed.");
						OutOutput.Empty();
						return false;
					}
					OutTelemetry.HabitatQueryCount = NewHabitatQueryCount;
					OutTelemetry.QueryWindowCount = NewQueryWindowCount;
					if (!EvaluateFactoredHabitatQueries(
							FactoredQueryContext,
							Queries,
							CallerScratchBytes,
							Quantums,
							QueryTelemetry,
							OutError))
					{
						bOutCancelled = Cancellation.IsCancellationRequested();
						OutOutput.Empty();
						return false;
					}
					int64 ActualScratchBytes = CallerScratchBytes;
					if (!TryAccumulateAllocatedBytes(Quantums.GetAllocatedSize(), ActualScratchBytes)
						|| ActualScratchBytes > ScratchBudgetBytes)
					{
						OutError = TEXT("ArcForest lazy regional recruitment actual scratch exceeded its configured budget.");
						OutOutput.Empty();
						return false;
					}
					OutTelemetry.PeakActualScratchBytes = FMath::Max(
						OutTelemetry.PeakActualScratchBytes, ActualScratchBytes);
					for (int32 WorkIndex = 0; WorkIndex < ProposalWork.Num(); ++WorkIndex)
					{
						if ((WorkIndex & 4095) == 0 && Cancellation.IsCancellationRequested())
						{
							bOutCancelled = true;
							OutOutput.Empty();
							return false;
						}
						const FLazyRegionalProposalWork& Work = ProposalWork[WorkIndex];
						const FValidBaseCellEntry& Entry =
							ValidBaseCells.Entries[Work.ValidEntryIndex];
						const double CutFactor = UE::ArcForest::Habitat::MakeExclusionFactor(
								static_cast<double>(ValidBaseCells.ModificationCutByValidEntry[Work.ValidEntryIndex]),
								Model.ExclusionSensitivity)
							* UE::ArcForest::Habitat::MakeExclusionFactor(
								static_cast<double>(ValidBaseCells.BareRockByValidEntry[Work.ValidEntryIndex]),
								Model.ExclusionSensitivity)
							* UE::ArcForest::Habitat::MakeExclusionFactor(
								static_cast<double>(ValidBaseCells.ChannelBufferByValidEntry[Work.ValidEntryIndex]),
								Model.ExclusionSensitivity)
							* UE::ArcForest::Habitat::MakeExclusionFactor(
								static_cast<double>(ValidBaseCells.EstablishmentWearByValidEntry[Work.ValidEntryIndex]),
								Model.ExclusionSensitivity)
							* UE::ArcForest::Habitat::MakeExclusionFactor(
								static_cast<double>(ValidBaseCells.SubstrateMobilityByValidEntry[Work.ValidEntryIndex]),
								Model.ExclusionSensitivity);
						const double AcceptedStaticWeight = Entry.CellAreaSquareCentimeters
							* DequantizeProbability(Quantums[WorkIndex]) * CutFactor;
						const double AcceptanceRatio = AcceptedStaticWeight / Work.Envelope;
						if (!FMath::IsFinite(AcceptedStaticWeight)
							|| AcceptedStaticWeight < 0.0
							|| !FMath::IsFinite(AcceptanceRatio)
							|| AcceptanceRatio < 0.0)
						{
							OutError = TEXT("ArcForest lazy regional recruitment acceptance ratio was invalid.");
							OutOutput.Empty();
							return false;
						}
						if (AcceptanceRatio > 1.0 + 1.0e-12)
						{
							++OutTelemetry.EnvelopeViolationCount;
							SpeciesTelemetry.UnresolvedAttemptCount =
								Range.AttemptCount - SpeciesTelemetry.AcceptedAttemptCount;
							OutTelemetry.UnresolvedAttemptCount =
								SpeciesTelemetry.UnresolvedAttemptCount;
							OutError = FString::Printf(
								TEXT("ArcForest regional recruitment envelope violation | year=%d species=%s speciesIndex=%d")
								TEXT(" attempt=%d retry=%d acceptedStaticWeight=%.17g envelope=%.17g ratio=%.17g."),
								Year,
								*Range.SpeciesId.ToString(EGuidFormats::DigitsWithHyphensLower),
								Range.SpeciesIndex,
								Work.AttemptOrdinal,
								Work.RetryOrdinal,
								AcceptedStaticWeight,
								Work.Envelope,
								AcceptanceRatio);
							OutOutput.Empty();
							return false;
						}
						const double ClampedAcceptanceRatio = FMath::Min(AcceptanceRatio, 1.0);
						const uint64 AcceptanceKey = UE::ArcForest::MakeAnnualKey(
							Snapshot.SimulationId,
							Input.Seed,
							Year,
							UE::ArcForest::EDeterministicDomain::RegionalEnvelopeAcceptance,
							Range.SpeciesId,
							INDEX_NONE,
							static_cast<uint64>(Work.AttemptOrdinal),
							Work.RetryOrdinal,
							0);
						if (UE::ArcForest::KeyToUnitOpen(AcceptanceKey) < ClampedAcceptanceRatio)
						{
							const int32 OutputIndex = Range.OutputOffset + Work.AttemptOrdinal;
							if (!OutOutput.AcceptedBaseCellByRegionalAttempt.IsValidIndex(OutputIndex)
								|| OutOutput.AcceptedBaseCellByRegionalAttempt[OutputIndex] != INDEX_NONE)
							{
								OutError = TEXT("ArcForest lazy regional recruitment attempted duplicate publication.");
								OutOutput.Empty();
								return false;
							}
							OutOutput.AcceptedBaseCellByRegionalAttempt[OutputIndex] = Entry.BaseCellIndex;
							++OutTelemetry.AcceptedAttemptCount;
							++SpeciesTelemetry.AcceptedAttemptCount;
						}
						else
						{
							FLazyRegionalPendingAttempt& Next =
								NextPendingAttempts.AddDefaulted_GetRef();
							Next.AttemptOrdinal = Work.AttemptOrdinal;
							Next.RetryOrdinal = Work.RetryOrdinal + 1;
						}
					}
				}
				Swap(PendingAttempts, NextPendingAttempts);
			}

			if (!PendingAttempts.IsEmpty())
			{
				OutTelemetry.UnresolvedAttemptCount = PendingAttempts.Num();
				SpeciesTelemetry.UnresolvedAttemptCount = PendingAttempts.Num();
				OutOutput.Empty();
				if (Cancellation.IsCancellationRequested())
				{
					bOutCancelled = true;
					return false;
				}
				OutError = FString::Printf(
					TEXT("ArcForest regional recruitment exhausted proposal rounds | year=%d species=%s speciesIndex=%d")
					TEXT(" rounds=%d unresolved=%d allocatedAttempts=%d proposals=%lld."),
					Year,
					*Range.SpeciesId.ToString(EGuidFormats::DigitsWithHyphensLower),
					Range.SpeciesIndex,
					RecruitmentConfig.MaxProposalRounds,
					PendingAttempts.Num(),
					Range.AttemptCount,
					SpeciesTelemetry.ProposalCount);
				return false;
			}
		}

		if (OutTelemetry.AcceptedAttemptCount != OutTelemetry.AllocatedNonZeroMassAttempts)
		{
			OutError = TEXT("ArcForest lazy regional recruitment accepted-attempt accounting was incomplete.");
			OutOutput.Empty();
			return false;
		}
		return true;
	}

	bool GenerateRecruitmentCandidates(
		const FArcForestRunInput& Input,
		const TArray<FArcForestFrozenSpeciesSetup>& Species,
		const FValidBaseCellTable& ValidBaseCells,
		const FFactoredHabitatQueryContext& FactoredQueryContext,
		const FRegionalRecruitmentRecordIndex& RegionalRecordIndex,
		const FRegionalRecruitmentEnvelopeTable& RegionalEnvelopeTable,
		const FArcForestRegionalRecruitmentConfig& RegionalRecruitmentConfig,
		const FStratifiedOccupancy& CellOccupancy,
		const int32 Year,
		const FArcForestWorldSnapshot& Snapshot,
		const FArcForestCancellationToken& Cancellation,
		const TArray<FRecruitmentAttemptGroup>& Groups,
		UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics,
		FRegionalTargetRunTelemetry& RunTelemetry,
		FFactoredHabitatConsumerTelemetry& FactoredConsumerTelemetry,
		TArray<FRecruitmentCandidate>& OutCandidates,
		bool& bOutCancelled,
		FString& OutError)
	{
		bOutCancelled = false;
		const FArcEcotopeSiteMetadata& SiteMetadata = GetSiteMetadata(Input);
		const int32 CellCount = SiteMetadata.GetCellCount();
		if (!(RegionalRecruitmentConfig == Input.RegionalRecruitmentConfig))
		{
			OutError = TEXT("ArcForest recruitment received a regional sampler config that differed from the captured run input.");
			return false;
		}
		const auto EvaluateQueries = [&](
			const TConstArrayView<FFactoredHabitatQuery> Queries,
			const int64 CallerAllocatedScratchBytes,
			FFactoredHabitatQueryTelemetry& Telemetry,
			TArray<uint32>& OutQuantums)
		{
			return EvaluateFactoredHabitatQueries(
				FactoredQueryContext,
				Queries,
				CallerAllocatedScratchBytes,
				OutQuantums,
				Telemetry,
				OutError);
		};
		// A recruit establishes at MinimumPlantHeightCm, so it is shaded by everything above that height.
		TArray<double> RecordCellRecruitLight;
		RecordCellRecruitLight.SetNumUninitialized(CellOccupancy.GetCellCount());
		for (int32 RecordCellIndex = 0; RecordCellIndex < CellOccupancy.GetCellCount(); ++RecordCellIndex)
		{
			RecordCellRecruitLight[RecordCellIndex] = CalculateBeerLambertLight(
				CellOccupancy.AboveHeight(RecordCellIndex, static_cast<double>(MinimumPlantHeightCm)),
				Input.UnderstoryShade);
		}
		bool bHasAllocatedRegionalGroups = false;
		for (const FRecruitmentAttemptGroup& Group : Groups)
		{
			bHasAllocatedRegionalGroups |=
				Group.Source == ERecruitmentSource::Regional && Group.AllocatedAttempts > 0;
		}
		TArray<double> RecordCellCrowding;
		int64 RegionalCacheWorkCounter = 0;
		if (bHasAllocatedRegionalGroups)
		{
			// Regional recruitment reuses the current-year record-cell crowding prelude.
			TArray<double> RecordCellTotalOccupancy;
			RecordCellTotalOccupancy.SetNumUninitialized(CellOccupancy.GetCellCount());
			for (int32 RecordCellIndex = 0; RecordCellIndex < CellOccupancy.GetCellCount(); ++RecordCellIndex)
			{
				RecordCellTotalOccupancy[RecordCellIndex] = CellOccupancy.TotalAt(RecordCellIndex);
			}
			RecordCellCrowding.SetNumUninitialized(CellOccupancy.GetCellCount());
			for (int32 RecordCellIndex = 0; RecordCellIndex < CellOccupancy.GetCellCount(); ++RecordCellIndex)
			{
				if ((++RegionalCacheWorkCounter & 4095) == 0
					&& Cancellation.IsCancellationRequested())
				{
					bOutCancelled = true;
					return false;
				}
				RecordCellCrowding[RecordCellIndex] =
					1.0 / (1.0 + RecordCellTotalOccupancy[RecordCellIndex]);
			}
		}
		TMap<int64, int64> CandidateCounts;
		int64 WorkCounter = 0;
		bool bLazySamplerAttempted = false;
		ELazyRegionalSamplerOutcome LazySamplerOutcome =
			ELazyRegionalSamplerOutcome::NotAttempted;
		double LazyRegionalProviderWallMilliseconds = 0.0;
		FLazyRegionalRecruitmentOutput LazyRegionalOutput;
		FLazyRegionalRecruitmentTelemetry LazyRegionalTelemetry;
		const auto GenerateCandidates = [&]()
		{
		if (bHasAllocatedRegionalGroups)
		{
			bLazySamplerAttempted = true;
			const double LazyProviderWallBefore =
				FactoredConsumerTelemetry.RegionalSampler.WallMilliseconds;
			const bool bLazySamplerInvocationSucceeded = GenerateLazyRegionalRecruitment(
					Input,
					Species,
					ValidBaseCells,
					RegionalRecordIndex,
					RegionalEnvelopeTable,
					FactoredQueryContext,
					RegionalRecruitmentConfig,
					RecordCellRecruitLight,
					RecordCellCrowding,
					Year,
					Snapshot,
					Cancellation,
					Groups,
					FactoredConsumerTelemetry.RegionalSampler,
					LazyRegionalOutput,
					LazyRegionalTelemetry,
					bOutCancelled,
					OutError);
			LazyRegionalProviderWallMilliseconds =
				FactoredConsumerTelemetry.RegionalSampler.WallMilliseconds
				- LazyProviderWallBefore;
			Diagnostics.AddSample(
				TEXT("Recruitment_RegionalSampler"), LazyRegionalTelemetry.WallMilliseconds);
			if (!bLazySamplerInvocationSucceeded)
			{
				LazySamplerOutcome = bOutCancelled
					? ELazyRegionalSamplerOutcome::Cancelled
					: (LazyRegionalTelemetry.EnvelopeViolationCount > 0
						? ELazyRegionalSamplerOutcome::EnvelopeViolation
						: (LazyRegionalTelemetry.UnresolvedAttemptCount > 0
							? ELazyRegionalSamplerOutcome::Exhaustion
							: ELazyRegionalSamplerOutcome::Error));
				return false;
			}
			LazySamplerOutcome = ELazyRegionalSamplerOutcome::Success;
			{
				if (Cancellation.IsCancellationRequested())
				{
					bOutCancelled = true;
					LazyRegionalOutput.Empty();
					return false;
				}
				int64 LazyFoldWorkCounter = 0;
				for (const FLazyRegionalSpeciesRange& Range : LazyRegionalOutput.SpeciesRanges)
				{
					if (Range.bZeroProposalMass)
					{
						continue;
					}
					for (int32 AttemptOrdinal = 0;
						AttemptOrdinal < Range.AttemptCount;
						++AttemptOrdinal)
					{
						if ((++LazyFoldWorkCounter & 4095) == 0
							&& Cancellation.IsCancellationRequested())
						{
							bOutCancelled = true;
							LazyRegionalOutput.Empty();
							return false;
						}
						const int32 OutputIndex = Range.OutputOffset + AttemptOrdinal;
						if (!LazyRegionalOutput.AcceptedBaseCellByRegionalAttempt.IsValidIndex(OutputIndex)
							|| LazyRegionalOutput.AcceptedBaseCellByRegionalAttempt[OutputIndex] == INDEX_NONE)
						{
							OutError = TEXT("ArcForest lazy regional recruitment succeeded with an unresolved non-zero-mass attempt.");
							LazyRegionalOutput.Empty();
							return false;
						}
						const int32 TargetCellIndex =
							LazyRegionalOutput.AcceptedBaseCellByRegionalAttempt[OutputIndex];
						const int64 CandidateKey = static_cast<int64>(Range.SpeciesIndex)
							* static_cast<int64>(CellCount) + TargetCellIndex;
						int64& Count = CandidateCounts.FindOrAdd(CandidateKey);
						if (!TryAddCapacityValue(Count, 1, Count))
						{
							OutError = TEXT("ArcForest recruitment candidate count overflowed.");
							LazyRegionalOutput.Empty();
							return false;
						}
					}
				}
				LazyRegionalOutput.Empty();
			}
		}

		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Recruitment_Attempts);
			for (const FRecruitmentAttemptGroup& Group : Groups)
			{
				if (Group.AllocatedAttempts <= 0)
				{
					continue;
				}
				if (Group.Source == ERecruitmentSource::Regional)
				{
					continue;
				}
				const FArcForestFrozenSpeciesSetup& Model = Species[Group.SpeciesIndex];
				FIntPoint SourceCoordinates;
					if (!SiteMetadata.TryGetCellCoordinates(Group.SourceCellIndex, SourceCoordinates))
					{
						OutError = TEXT("ArcForest local dispersal source cell was invalid.");
						return false;
					}
					const FVector SourceCenter =
						SiteMetadata.GetCellCenterLocal(SourceCoordinates.X, SourceCoordinates.Y);
					struct FLocalAttemptWork
					{
						int64 AttemptIndex = 0;
						int32 TargetCellIndex = INDEX_NONE;
						int32 TargetRecordCellIndex = INDEX_NONE;
						int32 ValidEntryIndex = INDEX_NONE;
					};
					const int32 MaximumWindowCount = GetMaximumFactoredHabitatQueriesPerWindow(
						sizeof(FFactoredHabitatQuery) + sizeof(FLocalAttemptWork));
					if (MaximumWindowCount <= 0)
					{
						OutError = TEXT("ArcForest grouped habitat query budget cannot hold one local recruitment window.");
						return false;
					}
					for (int64 WindowStart = 0; WindowStart < Group.AllocatedAttempts; WindowStart += MaximumWindowCount)
					{
						const int64 WindowEnd = FMath::Min<int64>(
							WindowStart + MaximumWindowCount, Group.AllocatedAttempts);
						TArray<FLocalAttemptWork> WorkItems;
						WorkItems.Reserve(static_cast<int32>(WindowEnd - WindowStart));
						for (int64 AttemptIndex = WindowStart; AttemptIndex < WindowEnd; ++AttemptIndex)
						{
							if ((++WorkCounter & 4095) == 0 && Cancellation.IsCancellationRequested())
							{
								bOutCancelled = true;
								return false;
							}
							double DistanceCm = 0.0;
							if (Model.MeanDispersalDistanceCm > 0.0f)
							{
								const uint64 DistanceKey = UE::ArcForest::MakeAnnualKey(
									Snapshot.SimulationId, Input.Seed, Year,
									UE::ArcForest::EDeterministicDomain::LocalDispersalDistance,
									Model.SpeciesId, Group.SourceCellIndex, Group.GroupIdentity,
									static_cast<int32>(AttemptIndex), Group.DesiredAttempts);
								DistanceCm = -static_cast<double>(Model.MeanDispersalDistanceCm)
									* FMath::Loge(UE::ArcForest::KeyToUnitOpen(DistanceKey));
							}
							const uint64 AngleKey = UE::ArcForest::MakeAnnualKey(
								Snapshot.SimulationId, Input.Seed, Year,
								UE::ArcForest::EDeterministicDomain::LocalDispersalAngle,
								Model.SpeciesId, Group.SourceCellIndex, Group.GroupIdentity,
								static_cast<int32>(AttemptIndex), Group.DesiredAttempts);
							const double Angle = UE_DOUBLE_TWO_PI * UE::ArcForest::KeyToUnitOpen(AngleKey);
							const FVector2D TargetPosition(
								SourceCenter.X + DistanceCm * FMath::Cos(Angle),
								SourceCenter.Y + DistanceCm * FMath::Sin(Angle));
							int32 TargetCellIndex = INDEX_NONE;
							if (!SiteMetadata.TryGetCellIndexForLocalPosition(TargetPosition, TargetCellIndex)
								|| !ValidBaseCells.BaseValidityMask.IsValidIndex(TargetCellIndex)
								|| ValidBaseCells.BaseValidityMask[TargetCellIndex] == 0)
							{
								continue;
							}
							const int32 TargetRecordCellIndex =
								GetRecordCellIndexForBaseCell(SiteMetadata, TargetCellIndex);
							const int32 ValidEntryIndex =
								FindValidBaseCellEntryIndex(ValidBaseCells, TargetCellIndex);
							if (TargetRecordCellIndex == INDEX_NONE
								|| !CellOccupancy.IsValidCell(TargetRecordCellIndex)
								|| ValidEntryIndex == INDEX_NONE
								|| !ValidBaseCells.ModificationCutByValidEntry.IsValidIndex(ValidEntryIndex)
								|| !ValidBaseCells.BareRockByValidEntry.IsValidIndex(ValidEntryIndex)
								|| !ValidBaseCells.ChannelBufferByValidEntry.IsValidIndex(ValidEntryIndex)
								|| !ValidBaseCells.EstablishmentWearByValidEntry.IsValidIndex(ValidEntryIndex)
								|| !ValidBaseCells.SubstrateMobilityByValidEntry.IsValidIndex(ValidEntryIndex))
							{
								OutError = TEXT("ArcForest local dispersal target could not be mapped onto frozen habitat inputs.");
								return false;
							}
							FLocalAttemptWork& Work = WorkItems.AddDefaulted_GetRef();
							Work.AttemptIndex = AttemptIndex;
							Work.TargetCellIndex = TargetCellIndex;
							Work.TargetRecordCellIndex = TargetRecordCellIndex;
							Work.ValidEntryIndex = ValidEntryIndex;
						}
						TArray<FFactoredHabitatQuery> Queries;
						Queries.Reserve(WorkItems.Num());
						Queries.SetNumUninitialized(WorkItems.Num());
						for (int32 WorkIndex = 0; WorkIndex < WorkItems.Num(); ++WorkIndex)
						{
							FFactoredHabitatQuery& Query = Queries[WorkIndex];
							Query.SpeciesIndex = Group.SpeciesIndex;
							Query.BaseCellIndex = WorkItems[WorkIndex].TargetCellIndex;
							Query.OutputIndex = WorkIndex;
							Query.RouteSeedIndex = INDEX_NONE;
							Query.StableOrdinal = WorkItems[WorkIndex].AttemptIndex;
						}
						TArray<uint32> Quantums;
						if (!EvaluateQueries(
								Queries,
								static_cast<int64>(Queries.GetAllocatedSize())
									+ static_cast<int64>(WorkItems.GetAllocatedSize()),
								FactoredConsumerTelemetry.LocalRecruitment,
								Quantums))
						{
							bOutCancelled = Cancellation.IsCancellationRequested();
							return false;
						}
						for (int32 WorkIndex = 0; WorkIndex < WorkItems.Num(); ++WorkIndex)
						{
							const FLocalAttemptWork& Work = WorkItems[WorkIndex];
							const double TargetSuitability = CalculateResourceFactorFromQuantum(
								Quantums[WorkIndex], Model,
								RecordCellRecruitLight[Work.TargetRecordCellIndex]);
							const double CutTargetSuitability = TargetSuitability
								* UE::ArcForest::Habitat::MakeExclusionFactor(
									static_cast<double>(ValidBaseCells.ModificationCutByValidEntry[Work.ValidEntryIndex]),
									Model.ExclusionSensitivity)
								* UE::ArcForest::Habitat::MakeExclusionFactor(
									static_cast<double>(ValidBaseCells.BareRockByValidEntry[Work.ValidEntryIndex]),
									Model.ExclusionSensitivity)
								* UE::ArcForest::Habitat::MakeExclusionFactor(
									static_cast<double>(ValidBaseCells.ChannelBufferByValidEntry[Work.ValidEntryIndex]),
									Model.ExclusionSensitivity)
								* UE::ArcForest::Habitat::MakeExclusionFactor(
									static_cast<double>(ValidBaseCells.EstablishmentWearByValidEntry[Work.ValidEntryIndex]),
									Model.ExclusionSensitivity)
								* UE::ArcForest::Habitat::MakeExclusionFactor(
									static_cast<double>(ValidBaseCells.SubstrateMobilityByValidEntry[Work.ValidEntryIndex]),
									Model.ExclusionSensitivity);
							const int64 QuantizedSuitability = CutTargetSuitability > 0.0
								? FMath::Max<int64>(QuantizeProbability(CutTargetSuitability), 1)
								: 0;
							if (QuantizedSuitability <= 0)
							{
								continue;
							}
							const uint64 EstablishmentKey = UE::ArcForest::MakeAnnualKey(
								Snapshot.SimulationId, Input.Seed, Year,
								UE::ArcForest::EDeterministicDomain::LocalEstablishment,
								Model.SpeciesId, Work.TargetCellIndex, Group.GroupIdentity,
								static_cast<int32>(Work.AttemptIndex), QuantizedSuitability);
							if (UE::ArcForest::KeyToUnitOpen(EstablishmentKey)
								>= DequantizeProbability(QuantizedSuitability))
							{
								continue;
							}
							const int64 CandidateKey = static_cast<int64>(Group.SpeciesIndex)
								* static_cast<int64>(CellCount) + Work.TargetCellIndex;
							int64& Count = CandidateCounts.FindOrAdd(CandidateKey);
							if (Count == MAX_int64)
							{
								OutError = TEXT("ArcForest recruitment candidate count overflowed.");
								return false;
							}
							++Count;
						}
					}
			}
		}

		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Recruitment_CandidateCanonicalization);
			TArray<int64> CanonicalKeys;
			CandidateCounts.GetKeys(CanonicalKeys);
			CanonicalKeys.Sort();
			OutCandidates.Reserve(CanonicalKeys.Num());
			for (const int64 CandidateKey : CanonicalKeys)
			{
				const int32 SpeciesIndex = static_cast<int32>(CandidateKey / CellCount);
				const int32 CellIndex = static_cast<int32>(CandidateKey % CellCount);
				FRecruitmentCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
				Candidate.SpeciesId = Species[SpeciesIndex].SpeciesId;
				Candidate.SpeciesIndex = SpeciesIndex;
				Candidate.TargetCellIndex = CellIndex;
				Candidate.StemCount = CandidateCounts.FindChecked(CandidateKey);
				Candidate.AdmissionPriority = UE::ArcForest::MakeAnnualKey(
					Snapshot.SimulationId,
					Input.Seed,
					Year,
					UE::ArcForest::EDeterministicDomain::RecruitmentAdmission,
					Candidate.SpeciesId,
					CellIndex,
					0,
					0,
					Candidate.StemCount);
			}
			OutCandidates.Sort(
				[](const FRecruitmentCandidate& Left, const FRecruitmentCandidate& Right)
				{
					if (Left.AdmissionPriority != Right.AdmissionPriority)
					{
						return Left.AdmissionPriority < Right.AdmissionPriority;
					}
					if (Left.SpeciesId != Right.SpeciesId)
					{
						return GuidLess(Left.SpeciesId, Right.SpeciesId);
					}
					return Left.TargetCellIndex < Right.TargetCellIndex;
				});
		}
		return true;
		};
		const bool bGeneratedCandidates = GenerateCandidates();
		if (bLazySamplerAttempted
			&& !bGeneratedCandidates
			&& LazySamplerOutcome == ELazyRegionalSamplerOutcome::Success)
		{
			LazySamplerOutcome = bOutCancelled
				? ELazyRegionalSamplerOutcome::Cancelled
				: ELazyRegionalSamplerOutcome::PostSamplerFailure;
		}
		if (bLazySamplerAttempted)
		{
			const auto AddRunInt64 = [&OutError](int64& Target, const int64 Value)
			{
				int64 NewValue = 0;
				if (!TryAddCapacityValue(Target, Value, NewValue))
				{
					if (OutError.IsEmpty())
					{
						OutError = TEXT("ArcForest lazy regional recruitment run telemetry overflowed.");
					}
					return false;
				}
				Target = NewValue;
				return true;
			};
			const auto AddRunInt32 = [&OutError](int32& Target, const int32 Value)
			{
				if (Value < 0 || Target > MAX_int32 - Value)
				{
					if (OutError.IsEmpty())
					{
						OutError = TEXT("ArcForest lazy regional recruitment run year telemetry overflowed.");
					}
					return false;
				}
				Target += Value;
				return true;
			};

			bool bLazyTelemetryValid = AddRunInt32(
				RunTelemetry.LazySamplerAttemptedYearCount, 1);
			bLazyTelemetryValid = AddRunInt64(
				RunTelemetry.LazyProposalCount, LazyRegionalTelemetry.ProposalCount)
				&& bLazyTelemetryValid;
			bLazyTelemetryValid = AddRunInt64(
				RunTelemetry.LazyHabitatQueryCount, LazyRegionalTelemetry.HabitatQueryCount)
				&& bLazyTelemetryValid;
			bLazyTelemetryValid = AddRunInt64(
				RunTelemetry.LazyQueryWindowCount, LazyRegionalTelemetry.QueryWindowCount)
				&& bLazyTelemetryValid;
			bLazyTelemetryValid = AddRunInt64(
				RunTelemetry.LazyEnvelopeViolationCount,
				LazyRegionalTelemetry.EnvelopeViolationCount)
				&& bLazyTelemetryValid;
			bLazyTelemetryValid = AddRunInt64(
				RunTelemetry.LazyCompletedRoundCount,
				LazyRegionalTelemetry.CompletedRoundCount)
				&& bLazyTelemetryValid;
			RunTelemetry.LazyMaximumRetryOrdinal = FMath::Max(
				RunTelemetry.LazyMaximumRetryOrdinal,
				LazyRegionalTelemetry.MaximumRetryOrdinal);
			RunTelemetry.LazyPeakProjectedScratchBytes = FMath::Max(
				RunTelemetry.LazyPeakProjectedScratchBytes,
				LazyRegionalTelemetry.PeakProjectedScratchBytes);
			RunTelemetry.LazyPeakActualScratchBytes = FMath::Max(
				RunTelemetry.LazyPeakActualScratchBytes,
				LazyRegionalTelemetry.PeakActualScratchBytes);
			const double NewLazySamplerWallMilliseconds =
				RunTelemetry.LazySamplerWallMilliseconds
				+ LazyRegionalTelemetry.WallMilliseconds;
			if (!FMath::IsFinite(NewLazySamplerWallMilliseconds)
				|| !FMath::IsFinite(LazyRegionalProviderWallMilliseconds)
				|| LazyRegionalProviderWallMilliseconds < 0.0)
			{
				if (OutError.IsEmpty())
				{
					OutError = TEXT("ArcForest lazy regional recruitment timing telemetry was invalid.");
				}
				bLazyTelemetryValid = false;
			}
			else
			{
				RunTelemetry.LazySamplerWallMilliseconds =
					NewLazySamplerWallMilliseconds;
			}

			const bool bSamplerContractFailure =
				LazySamplerOutcome == ELazyRegionalSamplerOutcome::EnvelopeViolation
				|| LazySamplerOutcome == ELazyRegionalSamplerOutcome::Exhaustion;
			const bool bAggregateSamplerResult =
				LazySamplerOutcome == ELazyRegionalSamplerOutcome::Success
				|| bSamplerContractFailure;
			for (const FLazyRegionalRecruitmentTelemetry::FSpeciesTelemetry& YearSpecies :
				LazyRegionalTelemetry.Species)
			{
				if (!YearSpecies.bProposalMassEvaluated)
				{
					continue;
				}
				FRegionalTargetRunTelemetry::FLazySpeciesTotals* SpeciesTotals =
					RunTelemetry.LazySpeciesTotals.FindByPredicate(
						[&YearSpecies](const FRegionalTargetRunTelemetry::FLazySpeciesTotals& Existing)
						{
							return Existing.SpeciesIndex == YearSpecies.SpeciesIndex
								&& Existing.SpeciesId == YearSpecies.SpeciesId;
						});
				if (SpeciesTotals == nullptr)
				{
					FRegionalTargetRunTelemetry::FLazySpeciesTotals& Added =
						RunTelemetry.LazySpeciesTotals.AddDefaulted_GetRef();
					Added.SpeciesId = YearSpecies.SpeciesId;
					Added.SpeciesIndex = YearSpecies.SpeciesIndex;
					SpeciesTotals = &Added;
				}
				bLazyTelemetryValid = AddRunInt64(
					SpeciesTotals->ProposalCount, YearSpecies.ProposalCount)
					&& bLazyTelemetryValid;
				SpeciesTotals->MaximumRetryOrdinal = FMath::Max(
					SpeciesTotals->MaximumRetryOrdinal,
					YearSpecies.MaximumRetryOrdinal);
				if (bAggregateSamplerResult)
				{
					bLazyTelemetryValid = AddRunInt64(
						SpeciesTotals->CompletedProposalCount,
						YearSpecies.ProposalCount)
						&& bLazyTelemetryValid;
					bLazyTelemetryValid = AddRunInt64(
						SpeciesTotals->AcceptedAttemptCount,
						YearSpecies.AcceptedAttemptCount)
						&& bLazyTelemetryValid;
					bLazyTelemetryValid = AddRunInt64(
						SpeciesTotals->UnresolvedAttemptCount,
						YearSpecies.UnresolvedAttemptCount)
						&& bLazyTelemetryValid;
					if (YearSpecies.bZeroProposalMass)
					{
						bLazyTelemetryValid = AddRunInt32(
							SpeciesTotals->ZeroMassYearCount, 1)
							&& bLazyTelemetryValid;
					}
				}
			}

			if (bAggregateSamplerResult)
			{
				bLazyTelemetryValid = AddRunInt64(
					RunTelemetry.LazyAllocatedNonZeroMassAttempts,
					LazyRegionalTelemetry.AllocatedNonZeroMassAttempts)
					&& bLazyTelemetryValid;
				bLazyTelemetryValid = AddRunInt64(
					RunTelemetry.LazyAcceptedAttemptCount,
					LazyRegionalTelemetry.AcceptedAttemptCount)
					&& bLazyTelemetryValid;
				bLazyTelemetryValid = AddRunInt64(
					RunTelemetry.LazyUnresolvedAttemptCount,
					LazyRegionalTelemetry.UnresolvedAttemptCount)
					&& bLazyTelemetryValid;
				bLazyTelemetryValid = AddRunInt64(
					RunTelemetry.LazyZeroMassGroupCount,
					LazyRegionalTelemetry.ZeroMassGroupCount)
					&& bLazyTelemetryValid;
			}
			if (LazySamplerOutcome == ELazyRegionalSamplerOutcome::Success)
			{
				bLazyTelemetryValid = AddRunInt32(
					RunTelemetry.LazySamplerCompletedYearCount, 1)
					&& bLazyTelemetryValid;
				bLazyTelemetryValid = AddRunInt32(
					RunTelemetry.LazySamplerSuccessYearCount, 1)
					&& bLazyTelemetryValid;
			}
			else if (LazySamplerOutcome == ELazyRegionalSamplerOutcome::EnvelopeViolation)
			{
				bLazyTelemetryValid = AddRunInt32(
					RunTelemetry.LazySamplerEnvelopeViolationYearCount, 1)
					&& bLazyTelemetryValid;
			}
			else if (LazySamplerOutcome == ELazyRegionalSamplerOutcome::Exhaustion)
			{
				bLazyTelemetryValid = AddRunInt32(
					RunTelemetry.LazySamplerExhaustionYearCount, 1)
					&& bLazyTelemetryValid;
			}

			const TCHAR* LazyOutcomeText = TEXT("failed");
			const TCHAR* LazyOutcomeReason = TEXT("error");
			switch (LazySamplerOutcome)
			{
			case ELazyRegionalSamplerOutcome::Success:
				LazyOutcomeText = TEXT("success");
				LazyOutcomeReason = TEXT("completed");
				break;
			case ELazyRegionalSamplerOutcome::EnvelopeViolation:
				LazyOutcomeReason = TEXT("envelopeViolation");
				break;
			case ELazyRegionalSamplerOutcome::Exhaustion:
				LazyOutcomeReason = TEXT("exhaustion");
				break;
			case ELazyRegionalSamplerOutcome::Cancelled:
				LazyOutcomeText = TEXT("cancelled");
				LazyOutcomeReason = TEXT("cancelled");
				break;
			case ELazyRegionalSamplerOutcome::PostSamplerFailure:
				LazyOutcomeReason = TEXT("postSamplerFailure");
				break;
			case ELazyRegionalSamplerOutcome::Error:
			case ELazyRegionalSamplerOutcome::NotAttempted:
			default:
				break;
			}
			UE_LOGF(
				LogArcEcotope,
				Log,
				"ArcForest regional recruitment sampler | year=%d outcome=%ls reason=%ls completedYear=%d"
				" allocatedNonZeroMass=%lld proposals=%lld queries=%lld windows=%lld"
				" accepted=%lld unresolved=%lld zeroMassGroups=%d envelopeViolations=%d"
				" rounds=%d maxRetry=%d projectedScratchBytes=%lld actualScratchBytes=%lld"
				" samplerWallMs=%.2f providerWallMs=%.2f",
				Year,
				LazyOutcomeText,
				LazyOutcomeReason,
				LazySamplerOutcome == ELazyRegionalSamplerOutcome::Success ? 1 : 0,
				LazyRegionalTelemetry.AllocatedNonZeroMassAttempts,
				LazyRegionalTelemetry.ProposalCount,
				LazyRegionalTelemetry.HabitatQueryCount,
				LazyRegionalTelemetry.QueryWindowCount,
				LazyRegionalTelemetry.AcceptedAttemptCount,
				LazyRegionalTelemetry.UnresolvedAttemptCount,
				LazyRegionalTelemetry.ZeroMassGroupCount,
				LazyRegionalTelemetry.EnvelopeViolationCount,
				LazyRegionalTelemetry.CompletedRoundCount,
				LazyRegionalTelemetry.MaximumRetryOrdinal,
				LazyRegionalTelemetry.PeakProjectedScratchBytes,
				LazyRegionalTelemetry.PeakActualScratchBytes,
				LazyRegionalTelemetry.WallMilliseconds,
				LazyRegionalProviderWallMilliseconds);
			if (!bLazyTelemetryValid)
			{
				return false;
			}
		}
		return bGeneratedCandidates;
	}

	bool BuildUsedRecordIds(const FArcForestWorldSnapshot& Snapshot, TSet<uint64>& OutIds, FString& OutError)
	{
		const auto AddId = [&OutIds, &OutError](const uint64 Id)
		{
			if (Id == 0 || OutIds.Contains(Id))
			{
				OutError = TEXT("ArcForest found a zero or colliding stable record identifier.");
				return false;
			}
			OutIds.Add(Id);
			return true;
		};
		for (const FArcForestIndividualRecord& Record : Snapshot.Individuals)
		{
			if (!AddId(Record.Id))
			{
				return false;
			}
		}
		for (const FArcForestCohortRecord& Record : Snapshot.Cohorts)
		{
			if (!AddId(Record.Id))
			{
				return false;
			}
		}
		for (const FArcForestPatchRecord& Record : Snapshot.Patches)
		{
			if (!AddId(Record.Id))
			{
				return false;
			}
		}
		for (const FArcForestDeadwoodRecord& Record : Snapshot.Deadwood)
		{
			if (!AddId(Record.Id))
			{
				return false;
			}
		}
		return true;
	}

	/** Per-chunk production result for the factored cover provider. */
	struct FCoverHabitatChunkResult
	{
		FString Error;
		int32 FirstErrorRecordCellIndex = MAX_int32;
		int64 RecordCells = 0;
		int64 SpeciesCells = 0;
		bool bCancelled = false;
	};

	bool FFusedCoverBatchContext::Initialize(
		const FArcForestRunInput& InInput,
		const FValidBaseCellTable& InValidBaseCells,
		const FRegionalRecruitmentRecordIndex& InRecordIndex,
		const TArray<FArcForestFrozenSpeciesSetup>& InSpecies,
		const TMap<FGuid, int32>& InSpeciesIndices,
		const FStratifiedOccupancy& InCellOccupancy,
		const FArcEcotopeDecayGrid& InDecayGrid,
		const FArcForestWorldSnapshot& InSnapshot,
		TArray<double>& InOutCoverFloorCapacity,
		TArray<double>& InOutCoverSuitability,
		FString& OutError)
	{
		const double StartSeconds = FPlatformTime::Seconds();
		Input = &InInput;
		ValidBaseCells = &InValidBaseCells;
		RecordIndex = &InRecordIndex;
		BaseValidityMask = InValidBaseCells.BaseValidityMask;
		Species = &InSpecies;
		CellOccupancy = &InCellOccupancy;
		DecayGrid = &InDecayGrid;
		Snapshot = &InSnapshot;
		OutCoverFloorCapacity = &InOutCoverFloorCapacity;
		OutCoverSuitability = &InOutCoverSuitability;
		Site = GetSiteMetadata(InInput);
		RecordCellCount = Site.GetLevelCellCount(Site.RecordGridLevel);
		RecordDimensions = Site.GetLevelDimensions(Site.RecordGridLevel);
		if (InRecordIndex.ValidBaseCellsIdentity != &InValidBaseCells
			|| InRecordIndex.ValidEntryIndexByBaseCell.Num() != Site.GetCellCount())
		{
			OutError = TEXT("ArcForest fused cover habitat received a record index inconsistent with the valid base cell table.");
			return false;
		}
		if (RecordCellCount <= 0 || InSpecies.IsEmpty())
		{
			InOutCoverFloorCapacity.Reset();
			InOutCoverSuitability.Reset();
			TotalWallMilliseconds += (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
			return true;
		}
		int64 CapacityCount64 = 0;
		if (!TryMultiplyCapacityValue(InSpecies.Num(), RecordCellCount, CapacityCount64)
			|| CapacityCount64 > MAX_int32)
		{
			OutError = TEXT("ArcForest cover floor capacity count is not representable.");
			return false;
		}
		if (RecordDimensions.X <= 0)
		{
			OutError = TEXT("ArcForest fused cover habitat received invalid record dimensions.");
			return false;
		}
		InOutCoverFloorCapacity.Init(0.0, static_cast<int32>(CapacityCount64));
		InOutCoverSuitability.Init(0.0, static_cast<int32>(CapacityCount64));

		for (int32 SpeciesIndex = 0; SpeciesIndex < InSpecies.Num(); ++SpeciesIndex)
		{
			if (!InSpecies[SpeciesIndex].bIndividualizes)
			{
				CoverSpeciesIndices.Add(SpeciesIndex);
			}
		}
		if (CoverSpeciesIndices.IsEmpty())
		{
			TotalWallMilliseconds += (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
			return true;
		}

		PatchHeightByCell.Reserve(InSnapshot.Patches.Num());
		for (const FArcForestPatchRecord& Record : InSnapshot.Patches)
		{
			const int32 SpeciesIndex = InSpeciesIndices.FindChecked(Record.SpeciesId);
			PatchHeightByCell.Add(
				static_cast<int64>(SpeciesIndex) * RecordCellCount + Record.CellIndex,
				static_cast<double>(Record.MeanHeightCm));
		}
		TotalWallMilliseconds += (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
		if (!FMath::IsFinite(TotalWallMilliseconds))
		{
			OutError = TEXT("ArcForest fused cover habitat initialization telemetry overflowed.");
			return false;
		}
		return true;
	}

	bool FFusedCoverBatchContext::ConsumeBatch(
		const FAnnualFactoredHabitatProvider& Provider,
		const TConstArrayView<FFactoredHabitatChunkSpan> ChunkSpans,
		const FArcForestCancellationToken& Cancellation,
		FString& OutError)
	{
		if (Input == nullptr || ValidBaseCells == nullptr || RecordIndex == nullptr || Species == nullptr
			|| CellOccupancy == nullptr || DecayGrid == nullptr
			|| Snapshot == nullptr || OutCoverFloorCapacity == nullptr
			|| OutCoverSuitability == nullptr)
		{
			OutError = TEXT("ArcForest fused cover habitat context was not initialized.");
			return false;
		}
		if (ChunkSpans.IsEmpty())
		{
			OutError = TEXT("ArcForest fused cover habitat received an empty factor batch.");
			return false;
		}
		const double StartSeconds = FPlatformTime::Seconds();
		TArray<FCoverHabitatChunkResult> Results;
		Results.SetNum(ChunkSpans.Num());
		if (!CoverSpeciesIndices.IsEmpty())
		{
			ParallelFor(
				ChunkSpans.Num(),
				[this, &Provider, ChunkSpans, &Cancellation, &Results](const int32 BatchChunkIndex)
			{
				FCoverHabitatChunkResult& Result = Results[BatchChunkIndex];
				const FFactoredHabitatChunkSpan& Span = ChunkSpans[BatchChunkIndex];
				const FAnnualFactoredHabitatProvider::FChunkLease& Lease = *Span.Lease;
				const FArcForestHabitatFactorChunk& FactorChunk = Provider.GetChunk(Lease);
				TArray<double> ResourceFactors;
				TArray<double> Scores;
				ResourceFactors.SetNumZeroed(CoverSpeciesIndices.Num());
				Scores.SetNumZeroed(CoverSpeciesIndices.Num());
				int32 WorkCounter = 0;
				for (int32 RecordY = Span.RecordCoreRect.Min.Y;
					RecordY < Span.RecordCoreRect.Max.Y;
					++RecordY)
				{
					for (int32 RecordX = Span.RecordCoreRect.Min.X;
						RecordX < Span.RecordCoreRect.Max.X;
						++RecordX)
					{
						const int32 RecordCellIndex = RecordX + RecordY * RecordDimensions.X;
						if (((++WorkCounter) & 255) == 0 && Cancellation.IsCancellationRequested())
						{
							Result.bCancelled = true;
							return;
						}
						int32 BaseCellIndex = INDEX_NONE;
						if (!DecayGrid->CellAreaHectares.IsValidIndex(RecordCellIndex)
							|| DecayGrid->CellAreaHectares[RecordCellIndex] <= 0.0
							|| !CellOccupancy->IsValidCell(RecordCellIndex)
							|| !TryGetRepresentativeBaseCellForRecordCell(
								Site, BaseValidityMask, RecordCellIndex, BaseCellIndex))
						{
							continue;
						}
						const int32 BaseX = BaseCellIndex % Site.GridDimensions.X;
						const int32 BaseY = BaseCellIndex / Site.GridDimensions.X;
						if (!FactorChunk.CoreRect.Contains(FIntPoint(BaseX, BaseY)))
						{
							Result.FirstErrorRecordCellIndex = RecordCellIndex;
							Result.Error = TEXT("Representative base cell lies outside its owning factor chunk.");
							return;
						}
						const int32 CoreLocalIndex = BaseX - FactorChunk.CoreRect.Min.X
							+ (BaseY - FactorChunk.CoreRect.Min.Y) * FactorChunk.CoreRect.Width();
						if (!RecordIndex->ValidEntryIndexByBaseCell.IsValidIndex(BaseCellIndex))
						{
							Result.FirstErrorRecordCellIndex = RecordCellIndex;
							Result.Error = TEXT("Representative base cell falls outside the regional recruitment record index.");
							return;
						}
						const int32 ValidEntryIndex =
							RecordIndex->ValidEntryIndexByBaseCell[BaseCellIndex];
						if (ValidEntryIndex == INDEX_NONE
							|| !ValidBaseCells->ModificationCutByValidEntry.IsValidIndex(ValidEntryIndex)
							|| !ValidBaseCells->BareRockByValidEntry.IsValidIndex(ValidEntryIndex)
							|| !ValidBaseCells->ChannelExclusionByValidEntry.IsValidIndex(ValidEntryIndex))
						{
							Result.FirstErrorRecordCellIndex = RecordCellIndex;
							Result.Error = TEXT("Representative base cell has no frozen exclusion entry.");
							return;
						}
						const double Cut = static_cast<double>(
							ValidBaseCells->ModificationCutByValidEntry[ValidEntryIndex]);
						const double BareRock = static_cast<double>(
							ValidBaseCells->BareRockByValidEntry[ValidEntryIndex]);
						const double ChannelExclusion = static_cast<double>(
							ValidBaseCells->ChannelExclusionByValidEntry[ValidEntryIndex]);
						double ScoreSum = 0.0;
						for (int32 Slot = 0; Slot < CoverSpeciesIndices.Num(); ++Slot)
						{
							const int32 SpeciesIndex = CoverSpeciesIndices[Slot];
							const FArcForestFrozenSpeciesSetup& Model = (*Species)[SpeciesIndex];
							const uint32 HabitatQuantum = Provider.EvaluateQuantizedSiteOnlyCoreLocal(
								Lease, SpeciesIndex, CoreLocalIndex);
							const double* PatchHeightCm = PatchHeightByCell.Find(
								static_cast<int64>(SpeciesIndex) * RecordCellCount + RecordCellIndex);
							const double HeightCm = PatchHeightCm != nullptr
								? *PatchHeightCm
								: static_cast<double>(MinimumPlantHeightCm);
							const double ResourceFactor = CalculateResourceFactorFromQuantum(
								HabitatQuantum,
								Model,
								CalculateLightAtHeight(
									*CellOccupancy,
									RecordCellIndex,
									HeightCm,
									Model,
									Input->UnderstoryShade))
								* UE::ArcForest::Habitat::MakeExclusionFactor(
									Cut, Model.ExclusionSensitivity)
								* UE::ArcForest::Habitat::MakeExclusionFactor(
									BareRock, Model.ExclusionSensitivity)
								* UE::ArcForest::Habitat::MakeExclusionFactor(
									ChannelExclusion, Model.ExclusionSensitivity);
							ResourceFactors[Slot] = FMath::IsFinite(ResourceFactor) && ResourceFactor > 0.0
								? ResourceFactor
								: 0.0;
							const int64 SuitabilitySlot64 =
								static_cast<int64>(CoverSpeciesIndices[Slot]) * RecordCellCount + RecordCellIndex;
							checkSlow(SuitabilitySlot64 >= 0 && SuitabilitySlot64 < OutCoverSuitability->Num());
							(*OutCoverSuitability)[static_cast<int32>(SuitabilitySlot64)] = ResourceFactors[Slot];
							const uint64 NoiseKey = UE::ArcForest::MakeKey(
								Snapshot->SimulationId,
								Input->Seed,
								UE::ArcForest::EDeterministicDomain::CoverShareNoise,
								Model.SpeciesId,
								RecordCellIndex);
							const double Noise = 1.0
								+ Input->GroundCoverPatchiness
									* (2.0 * UE::ArcForest::KeyToUnitOpen(NoiseKey) - 1.0);
							Scores[Slot] = ResourceFactors[Slot] * Noise;
							ScoreSum += Scores[Slot];
							++Result.SpeciesCells;
						}
						++Result.RecordCells;
						double FloorFill = 0.0;
						double FloorTarget = 0.0;
						for (int32 Slot = 0; Slot < CoverSpeciesIndices.Num(); ++Slot)
						{
							const double Target = FMath::Max(
								static_cast<double>((*Species)[CoverSpeciesIndices[Slot]].TargetStemsPerHectare),
								ArcForestSpeciesModel::MinimumTargetStemsPerHectare);
							if (ScoreSum > 0.0)
							{
								const double Share = Scores[Slot] / ScoreSum;
								FloorFill += Share * ResourceFactors[Slot];
								FloorTarget += Share * Target;
							}
						}
						const double CellFloor = ScoreSum > 0.0 ? FloorTarget
							* FloorFill
							* DecayGrid->CellAreaHectares[RecordCellIndex]
							* FMath::Max(Input->GroundCoverDensity, 0.0) : 0.0;
						for (int32 Slot = 0; Slot < CoverSpeciesIndices.Num(); ++Slot)
						{
							const double Capacity = ScoreSum > 0.0
								? Scores[Slot] / ScoreSum * CellFloor
								: 0.0;
							const double ProviderCapacity =
								FMath::IsFinite(Capacity) && Capacity > 0.0 ? Capacity : 0.0;
							const int64 CapacitySlot64 =
								static_cast<int64>(CoverSpeciesIndices[Slot]) * RecordCellCount + RecordCellIndex;
							checkSlow(CapacitySlot64 >= 0 && CapacitySlot64 < OutCoverFloorCapacity->Num());
							const int32 CapacitySlot = static_cast<int32>(CapacitySlot64);
							(*OutCoverFloorCapacity)[
								CapacitySlot] = ProviderCapacity;
						}
					}
				}
			});
		}

		int32 FirstErrorResult = INDEX_NONE;
			for (int32 ResultIndex = 0; ResultIndex < Results.Num(); ++ResultIndex)
			{
				const FCoverHabitatChunkResult& Result = Results[ResultIndex];
				if (Result.bCancelled)
				{
					OutError = TEXT("ArcForest cover habitat provider was cancelled.");
					return false;
				}
				if (!Result.Error.IsEmpty()
					&& (FirstErrorResult == INDEX_NONE
						|| Result.FirstErrorRecordCellIndex
							< Results[FirstErrorResult].FirstErrorRecordCellIndex))
				{
					FirstErrorResult = ResultIndex;
				}
				if (!TryAddCapacityValue(AnnualRecordCells, Result.RecordCells, AnnualRecordCells)
					|| !TryAddCapacityValue(AnnualSpeciesCells, Result.SpeciesCells, AnnualSpeciesCells))
				{
					OutError = TEXT("ArcForest fused cover habitat telemetry overflowed.");
					return false;
				}
			}
			if (FirstErrorResult != INDEX_NONE)
			{
				OutError = FString::Printf(
					TEXT("ArcForest cover habitat provider failed at record cell %d: %s"),
					Results[FirstErrorResult].FirstErrorRecordCellIndex,
					*Results[FirstErrorResult].Error);
				return false;
			}
		if (!TryAddCapacityValue(AnnualBatches, 1, AnnualBatches)
			|| !TryAddCapacityValue(
				AnnualReusedChunkLeases,
				static_cast<int64>(ChunkSpans.Num()),
				AnnualReusedChunkLeases))
		{
			OutError = TEXT("ArcForest fused cover habitat batch telemetry overflowed.");
			return false;
		}
		AnnualPeakChunksPerBatch = FMath::Max(AnnualPeakChunksPerBatch, ChunkSpans.Num());
		const double BatchWallMilliseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
		ConsumeWallMilliseconds += BatchWallMilliseconds;
		TotalWallMilliseconds += BatchWallMilliseconds;
		if (!FMath::IsFinite(ConsumeWallMilliseconds) || !FMath::IsFinite(TotalWallMilliseconds))
		{
			OutError = TEXT("ArcForest fused cover habitat wall telemetry overflowed.");
			return false;
		}
		return true;
	}

	bool FFusedCoverBatchContext::Finalize(
		FCoverHabitatProviderRunTelemetry& InOutTelemetry,
		FString& OutError)
	{
		const double StartSeconds = FPlatformTime::Seconds();
		TotalWallMilliseconds += (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
		FCoverHabitatProviderRunTelemetry Updated = InOutTelemetry;
		bool bOverflow = false;
		bOverflow |= !TryAddCapacityValue(Updated.RecordCells, AnnualRecordCells, Updated.RecordCells);
		bOverflow |= !TryAddCapacityValue(Updated.SpeciesCells, AnnualSpeciesCells, Updated.SpeciesCells);
		bOverflow |= !TryAddCapacityValue(Updated.EvaluationBatches, AnnualBatches, Updated.EvaluationBatches);
		bOverflow |= !TryAddCapacityValue(
			Updated.ReusedChunkLeases,
			AnnualReusedChunkLeases,
			Updated.ReusedChunkLeases);
		Updated.PeakChunksPerBatch = FMath::Max(Updated.PeakChunksPerBatch, AnnualPeakChunksPerBatch);
		Updated.WallMilliseconds += TotalWallMilliseconds;
		if (bOverflow || !FMath::IsFinite(Updated.WallMilliseconds))
		{
			OutError = TEXT("ArcForest cover habitat provider telemetry overflowed.");
			return false;
		}
		InOutTelemetry = MoveTemp(Updated);
		return true;
	}

	enum class EPatchRaiseOutcome : uint8
	{
		Raised,
		RaisedClampedByLiveBudget,
		AtCapacity,
		LiveBudgetBound,
	};

	/**
	 * Raises a patch toward TargetEquivalent under the same CeilToInt64 charge TryCountBudgets applies.
	 * Breaching MaxLiveStems is a hard run failure a year later, not a clamp, and this path bypasses the
	 * admission clamp that is otherwise the only way an equivalent grows.
	 */
	EPatchRaiseOutcome TryRaisePatchEquivalent(
		FArcForestPatchRecord& Record,
		const double TargetEquivalent,
		int64& InOutRemainingLive)
	{
		const int64 ExistingBudget = FMath::CeilToInt64(Record.StemOrCoverEquivalent);
		const double BudgetCeiling = static_cast<double>(ExistingBudget + InOutRemainingLive);
		const float NewEquivalent = static_cast<float>(FMath::Min(TargetEquivalent, BudgetCeiling));
		if (!FMath::IsFinite(NewEquivalent) || NewEquivalent <= Record.StemOrCoverEquivalent)
		{
			return TargetEquivalent > BudgetCeiling
				? EPatchRaiseOutcome::LiveBudgetBound
				: EPatchRaiseOutcome::AtCapacity;
		}
		const int64 LiveDelta = FMath::CeilToInt64(NewEquivalent) - ExistingBudget;
		if (LiveDelta < 0 || LiveDelta > InOutRemainingLive)
		{
			return EPatchRaiseOutcome::LiveBudgetBound;
		}
		Record.StemOrCoverEquivalent = NewEquivalent;
		InOutRemainingLive -= LiveDelta;
		return TargetEquivalent > BudgetCeiling
			? EPatchRaiseOutcome::RaisedClampedByLiveBudget
			: EPatchRaiseOutcome::Raised;
	}

	/**
	 * Logistic per-cell growth and lateral spread of the forms that emit no propagules. It runs before
	 * ApplyGrowth_Patches, which recomputes BiomassKg from the equivalent this leaves.
	 */
	bool ApplyCoverGrowthAndSpread(
		const FArcForestRunInput& Input,
		const TArray<FArcForestFrozenSpeciesSetup>& Species,
		const TMap<FGuid, int32>& SpeciesIndices,
		const TArray<double>& CoverFloorCapacity,
		const TArray<double>& CoverSuitability,
		const int32 Year,
		FArcForestWorldSnapshot& Snapshot,
		bool& bInOutBudgetWarningEmitted,
		FString& OutError)
	{
		const FArcEcotopeSiteMetadata& SiteMetadata = GetSiteMetadata(Input);
		const int32 RecordCellCount =
			SiteMetadata.GetLevelCellCount(SiteMetadata.RecordGridLevel);
		const FIntPoint RecordDimensions =
			SiteMetadata.GetLevelDimensions(SiteMetadata.RecordGridLevel);
		if (Snapshot.Patches.IsEmpty() || RecordCellCount <= 0 || RecordDimensions.X <= 0)
		{
			return true;
		}

		int64 LiveStems = 0;
		int64 LiveRecordCount = 0;
		int64 CoverRecordCount = 0;
		int64 NonBranchDeadwoodRecordCount = 0;
		int64 BranchRecordCount = 0;
		int64 TotalRecordCount = 0;
		if (!TryCountBudgets(
				Snapshot,
				LiveStems,
				LiveRecordCount,
				CoverRecordCount,
				NonBranchDeadwoodRecordCount,
				BranchRecordCount,
				TotalRecordCount,
				OutError))
		{
			return false;
		}
		int64 RemainingLive = FMath::Max<int64>(0, static_cast<int64>(Input.MaxLiveStems) - LiveStems);
		int64 RemainingCoverRecords =
			FMath::Max<int64>(0, static_cast<int64>(Input.MaxCoverRecords) - CoverRecordCount);
		int64 LiveBudgetBoundRaises = 0;
		int64 RecordSlotSkippedCreations = 0;
		int64 LiveStemSkippedCreations = 0;

		for (int32 PatchIndex = 0; PatchIndex < Snapshot.Patches.Num(); ++PatchIndex)
		{
			FArcForestPatchRecord& Record = Snapshot.Patches[PatchIndex];
			const int32 SpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
			const FArcForestFrozenSpeciesSetup& Model = Species[SpeciesIndex];
			if (Model.bIndividualizes)
			{
				continue;
			}
			const double Equivalent = static_cast<double>(Record.StemOrCoverEquivalent);
			const double Capacity =
				GetCoverFloorCapacity(CoverFloorCapacity, SpeciesIndex, RecordCellCount, Record.CellIndex);
			if (Capacity <= 0.0 || Equivalent >= Capacity)
			{
				continue;
			}
			const double Increment =
				CalculateCoverLogisticRate(Model) * Equivalent * (1.0 - Equivalent / Capacity);
			if (!FMath::IsFinite(Increment) || Increment <= 0.0)
			{
				continue;
			}
			if (RemainingLive <= 0)
			{
				++LiveBudgetBoundRaises;
				continue;
			}
			const EPatchRaiseOutcome GrowthOutcome = TryRaisePatchEquivalent(
				Record, FMath::Min(Equivalent + Increment, Capacity), RemainingLive);
			if (GrowthOutcome == EPatchRaiseOutcome::LiveBudgetBound
				|| GrowthOutcome == EPatchRaiseOutcome::RaisedClampedByLiveBudget)
			{
				++LiveBudgetBoundRaises;
			}
		}

		// Keyed on (species, record cell), the same pair the canonical merge treats as one record. A linear
		// scan per neighbour is quadratic in the record count spread is free to grow.
		TMap<int64, int32> PatchIndicesByCell;
		PatchIndicesByCell.Reserve(Snapshot.Patches.Num());
		for (int32 PatchIndex = 0; PatchIndex < Snapshot.Patches.Num(); ++PatchIndex)
		{
			const FArcForestPatchRecord& Record = Snapshot.Patches[PatchIndex];
			const int32 SpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
			PatchIndicesByCell.Add(
				static_cast<int64>(SpeciesIndex) * RecordCellCount + Record.CellIndex, PatchIndex);
		}

		// Odd years add the diagonals, so the reachable set is an octagon rather than the diamond a
		// cardinal-only kernel gives.
		constexpr int32 NeighbourOffsetX[] = {1, -1, 0, 0, 1, 1, -1, -1};
		constexpr int32 NeighbourOffsetY[] = {0, 0, 1, -1, 1, -1, 1, -1};
		const int32 NeighbourCount = Year % 2 == 0 ? 4 : 8;
		TSet<uint64> UsedIds;
		bool bUsedIdsBuilt = false;
		const int32 SourcePatchCount = Snapshot.Patches.Num();
		for (int32 SourceIndex = 0; SourceIndex < SourcePatchCount; ++SourceIndex)
		{
			const FGuid SourceSpeciesId = Snapshot.Patches[SourceIndex].SpeciesId;
			const int32 SourceCellIndex = Snapshot.Patches[SourceIndex].CellIndex;
			const int32 SourceSpeciesIndex = SpeciesIndices.FindChecked(SourceSpeciesId);
			const FArcForestFrozenSpeciesSetup& Model = Species[SourceSpeciesIndex];
			if (Model.bIndividualizes)
			{
				continue;
			}
			const double SpreadAmount = static_cast<double>(Model.LateralSpreadRatePerYear)
				* static_cast<double>(Snapshot.Patches[SourceIndex].StemOrCoverEquivalent);
			if (!FMath::IsFinite(SpreadAmount) || SpreadAmount <= 0.0)
			{
				continue;
			}
			const int32 SourceCellX = SourceCellIndex % RecordDimensions.X;
			const int32 SourceCellY = SourceCellIndex / RecordDimensions.X;
			for (int32 NeighbourIndex = 0; NeighbourIndex < NeighbourCount; ++NeighbourIndex)
			{
				const int32 TargetCellX = SourceCellX + NeighbourOffsetX[NeighbourIndex];
				const int32 TargetCellY = SourceCellY + NeighbourOffsetY[NeighbourIndex];
				if (TargetCellX < 0 || TargetCellX >= RecordDimensions.X
					|| TargetCellY < 0 || TargetCellY >= RecordDimensions.Y)
				{
					continue;
				}
				const int32 TargetCellIndex = TargetCellX + TargetCellY * RecordDimensions.X;
				const int64 TargetKey =
					static_cast<int64>(SourceSpeciesIndex) * RecordCellCount + TargetCellIndex;
				const int32* ExistingIndex = PatchIndicesByCell.Find(TargetKey);
				const double Capacity = GetCoverFloorCapacity(
					CoverFloorCapacity, SourceSpeciesIndex, RecordCellCount, TargetCellIndex);
				if (Capacity <= 0.0)
				{
					continue;
				}
				// Creating a record is an establishment decision and draws against suitability; a merge into a
				// cell that already holds one is not, an established colony is not re-tested every year.
				if (ExistingIndex != nullptr)
				{
					FArcForestPatchRecord& Existing = Snapshot.Patches[*ExistingIndex];
					const EPatchRaiseOutcome MergeOutcome = TryRaisePatchEquivalent(
						Existing,
						FMath::Min(static_cast<double>(Existing.StemOrCoverEquivalent) + SpreadAmount, Capacity),
						RemainingLive);
					if (MergeOutcome == EPatchRaiseOutcome::LiveBudgetBound
						|| MergeOutcome == EPatchRaiseOutcome::RaisedClampedByLiveBudget)
					{
						++LiveBudgetBoundRaises;
					}
					continue;
				}
				const double Suitability = GetCoverSuitability(
					CoverSuitability, SourceSpeciesIndex, RecordCellCount, TargetCellIndex);
				const int64 QuantizedSuitability = Suitability > 0.0
					? FMath::Max<int64>(QuantizeProbability(Suitability), 1)
					: 0;
				if (QuantizedSuitability <= 0)
				{
					continue;
				}
				const uint64 EstablishmentKey = UE::ArcForest::MakeAnnualKey(
					Snapshot.SimulationId,
					Input.Seed,
					Year,
					UE::ArcForest::EDeterministicDomain::LateralSpreadEstablishment,
					SourceSpeciesId,
					TargetCellIndex,
					0,
					0,
					QuantizedSuitability);
				if (UE::ArcForest::KeyToUnitOpen(EstablishmentKey)
					>= DequantizeProbability(QuantizedSuitability))
				{
					continue;
				}
				if (RemainingCoverRecords <= 0 || RemainingLive <= 0)
				{
					if (RemainingCoverRecords <= 0)
					{
						++RecordSlotSkippedCreations;
					}
					else
					{
						++LiveStemSkippedCreations;
					}
					continue;
				}
				const float Equivalent = static_cast<float>(FMath::Min(SpreadAmount, Capacity));
				if (!FMath::IsFinite(Equivalent) || Equivalent <= 0.0f)
				{
					continue;
				}
				if (FMath::CeilToInt64(Equivalent) > RemainingLive)
				{
					++LiveStemSkippedCreations;
					continue;
				}
				double DbhCm = 0.0;
				double BiomassPerEquivalentKg = 0.0;
				if (!TryCalculateDbh(Model, MinimumPlantHeightCm, DbhCm)
					|| !TryCalculateBiomass(Model, DbhCm, BiomassPerEquivalentKg))
				{
					OutError = TEXT("ArcForest lateral spread could not represent 30 cm allometry.");
					return false;
				}
				const double Biomass = BiomassPerEquivalentKg * static_cast<double>(Equivalent);
				if (!FMath::IsFinite(Biomass) || Biomass > MAX_flt)
				{
					OutError = TEXT("ArcForest lateral spread biomass overflowed.");
					return false;
				}
				if (!bUsedIdsBuilt)
				{
					if (!BuildUsedRecordIds(Snapshot, UsedIds, OutError))
					{
						return false;
					}
					bUsedIdsBuilt = true;
				}
				const uint64 RecordId = UE::ArcForest::MakeAnnualKey(
					Snapshot.SimulationId,
					Input.Seed,
					Year,
					UE::ArcForest::EDeterministicDomain::LateralSpreadRecordIdentity,
					SourceSpeciesId,
					TargetCellIndex,
					0,
					0,
					0);
				if (RecordId == 0 || UsedIds.Contains(RecordId))
				{
					OutError = TEXT("ArcForest lateral spread produced a zero or colliding patch identifier.");
					return false;
				}
				UsedIds.Add(RecordId);
				FArcForestPatchRecord& Created = Snapshot.Patches.AddDefaulted_GetRef();
				Created.Id = RecordId;
				Created.SpeciesId = SourceSpeciesId;
				Created.CellIndex = TargetCellIndex;
				Created.StemOrCoverEquivalent = Equivalent;
				Created.MeanHeightCm = MinimumPlantHeightCm;
				Created.MeanAgeYears = 0.0f;
				Created.MeanVigor = 1.0f;
				Created.BiomassKg = static_cast<float>(Biomass);
				PatchIndicesByCell.Add(TargetKey, Snapshot.Patches.Num() - 1);
				RemainingLive -= FMath::CeilToInt64(Equivalent);
				--RemainingCoverRecords;
			}
		}
		if (LiveBudgetBoundRaises > 0 || RecordSlotSkippedCreations > 0 || LiveStemSkippedCreations > 0)
		{
			const FString BudgetMessage = FString::Printf(
				TEXT("ArcForest cover growth was bound by the run budgets | year=%d boundRaiseAttempts=%lld ")
					TEXT("recordSlotSkippedCreationAttempts=%lld liveStemSkippedCreationAttempts=%lld ")
					TEXT("remainingLive=%lld remainingCoverRecords=%lld maxLiveStems=%d maxCoverRecords=%d"),
				Year,
				LiveBudgetBoundRaises,
				RecordSlotSkippedCreations,
				LiveStemSkippedCreations,
				RemainingLive,
				RemainingCoverRecords,
				Input.MaxLiveStems,
				Input.MaxCoverRecords);
			if (bInOutBudgetWarningEmitted)
			{
				UE_LOGF(LogArcEcotope, Log,"%ls", *BudgetMessage);
			}
			else
			{
				bInOutBudgetWarningEmitted = true;
				UE_LOGF(LogArcEcotope, Warning,"%ls", *BudgetMessage);
			}
		}
		return true;
	}

	bool AdmitRecruitment(
		const FArcForestRunInput& Input,
		const TArray<FArcForestFrozenSpeciesSetup>& Species,
		const TMap<FGuid, int32>& SpeciesIndices,
		const TArray<double>& CoverFloorCapacity,
		const int32 Year,
		const TArray<FRecruitmentCandidate>& Candidates,
		const FArcForestCancellationToken& Cancellation,
		UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics,
		const bool bGrowthDiagnosticsEnabled,
		FYearDiagnostics& InOutYearStats,
		FArcForestWorldSnapshot& Snapshot,
		bool& bOutCancelled,
		FString& OutError)
	{
		bOutCancelled = false;
		if (bGrowthDiagnosticsEnabled)
		{
			for (const FRecruitmentCandidate& DiagnosticCandidate : Candidates)
			{
				InOutYearStats.PromotionRecruitmentFlow.RecruitmentGenerated += DiagnosticCandidate.StemCount;
			}
		}
		const FArcEcotopeSiteMetadata& SiteMetadata = GetSiteMetadata(Input);
		if (Cancellation.IsCancellationRequested())
		{
			bOutCancelled = true;
			return false;
		}
		const int32 RecordCellCount =
			SiteMetadata.GetLevelCellCount(SiteMetadata.RecordGridLevel);
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Admission_MergeBefore);
			if (!MergeCanonicalLiveRecords(Snapshot, OutError))
			{
				return false;
			}
		}
		int64 LiveStems = 0;
		int64 LiveRecordCount = 0;
		int64 CoverRecordCount = 0;
		int64 NonBranchDeadwoodRecordCount = 0;
		int64 BranchRecordCount = 0;
		int64 TotalRecordCount = 0;
		if (!TryCountBudgets(
				Snapshot,
				LiveStems,
				LiveRecordCount,
				CoverRecordCount,
				NonBranchDeadwoodRecordCount,
				BranchRecordCount,
				TotalRecordCount,
				OutError))
		{
			return false;
		}
		if (LiveStems > Input.MaxLiveStems)
		{
			OutError = FString::Printf(
				TEXT("ArcForest recruitment began with LiveStems over MaxLiveStems | liveStems=%lld maxLiveStems=%d"),
				LiveStems,
				Input.MaxLiveStems);
			return false;
		}
		if (LiveRecordCount > Input.MaxLiveRecords)
		{
			OutError = FString::Printf(
				TEXT("ArcForest recruitment began with LiveRecordCount over MaxLiveRecords | liveRecordCount=%lld maxLiveRecords=%d"),
				LiveRecordCount,
				Input.MaxLiveRecords);
			return false;
		}
		if (CoverRecordCount > Input.MaxCoverRecords)
		{
			OutError = FString::Printf(
				TEXT("ArcForest recruitment began with CoverRecordCount over MaxCoverRecords | coverRecordCount=%lld maxCoverRecords=%d"),
				CoverRecordCount,
				Input.MaxCoverRecords);
			return false;
		}
		if (TotalRecordCount > Input.MaxStateRecords)
		{
			OutError = FString::Printf(
				TEXT("ArcForest recruitment began with TotalRecordCount over MaxStateRecords | totalRecordCount=%lld maxStateRecords=%d"),
				TotalRecordCount,
				Input.MaxStateRecords);
			return false;
		}
		int64 RemainingLive = static_cast<int64>(Input.MaxLiveStems) - LiveStems;
		int64 RemainingLiveRecords =
			FMath::Max<int64>(0, static_cast<int64>(Input.MaxLiveRecords) - LiveRecordCount);
		int64 RemainingCoverRecords =
			FMath::Max<int64>(0, static_cast<int64>(Input.MaxCoverRecords) - CoverRecordCount);
		TSet<uint64> UsedIds;
		if (!BuildUsedRecordIds(Snapshot, UsedIds, OutError))
		{
			return false;
		}

		int32 WorkCounter = 0;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Admission_Admit);
			// Existing cohorts cannot have BirthYear == Year: the snapshot rejects future births and
			// Year is CurrentYear + 1. Only records appended earlier in this admission pass can match.
			// Patches persist across years, so seed their lookup from the canonical pre-merge state.
			TMap<int64, int32> CurrentYearCohortIndicesByCell;
			CurrentYearCohortIndicesByCell.Reserve(static_cast<int32>(
				FMath::Min<int64>(static_cast<int64>(Candidates.Num()), RemainingLiveRecords)));
			TMap<int64, int32> PatchIndicesByCell;
			PatchIndicesByCell.Reserve(Snapshot.Patches.Num());
			for (int32 PatchIndex = 0; PatchIndex < Snapshot.Patches.Num(); ++PatchIndex)
			{
				const FArcForestPatchRecord& Patch = Snapshot.Patches[PatchIndex];
				const int32* SpeciesIndex = SpeciesIndices.Find(Patch.SpeciesId);
				if (SpeciesIndex == nullptr)
				{
					OutError = TEXT("ArcForest recruitment found a patch with an unknown species.");
					return false;
				}
				const int64 Key = static_cast<int64>(*SpeciesIndex) * RecordCellCount + Patch.CellIndex;
				if (PatchIndicesByCell.Contains(Key))
				{
					OutError = TEXT("ArcForest recruitment found duplicate canonical patch keys.");
					return false;
				}
				PatchIndicesByCell.Add(Key, PatchIndex);
			}
			for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
			{
				const FRecruitmentCandidate& Candidate = Candidates[CandidateIndex];
				if ((++WorkCounter & 4095) == 0 && Cancellation.IsCancellationRequested())
				{
					bOutCancelled = true;
					return false;
				}
				if (RemainingLive <= 0)
				{
					if (bGrowthDiagnosticsEnabled)
					{
						for (int32 TailIndex = CandidateIndex; TailIndex < Candidates.Num(); ++TailIndex)
						{
							InOutYearStats.PromotionRecruitmentFlow.RecruitmentRefusedLiveStem +=
								Candidates[TailIndex].StemCount;
						}
					}
					break;
				}
				const FArcForestFrozenSpeciesSetup& Model = Species[Candidate.SpeciesIndex];
				int64 Admitted = FMath::Min(Candidate.StemCount, RemainingLive);
				if (bGrowthDiagnosticsEnabled)
				{
					InOutYearStats.PromotionRecruitmentFlow.RecruitmentRefusedLiveStem +=
						Candidate.StemCount - Admitted;
				}
				if (Admitted <= 0)
				{
					continue;
				}
				// Targets are selected on the base grid; this is the single conversion onto the
				// record grid, and it precedes the budget decrements so candidates that coalesce
				// into one record charge the record budget once.
				const int32 RecordCellIndex =
					GetRecordCellIndexForBaseCell(SiteMetadata, Candidate.TargetCellIndex);
				if (RecordCellIndex == INDEX_NONE)
				{
					OutError = TEXT("ArcForest recruitment target could not be mapped onto the record grid.");
					return false;
				}
				double InitialDbhCm = 0.0;
				double BiomassPerStemKg = 0.0;
				if (!TryCalculateDbh(Model, MinimumPlantHeightCm, InitialDbhCm)
					|| !TryCalculateBiomass(Model, InitialDbhCm, BiomassPerStemKg))
				{
					OutError = TEXT("ArcForest recruitment could not represent 30 cm allometry.");
					return false;
				}

				if (Model.bIndividualizes)
				{
					const int64 Key =
						static_cast<int64>(Candidate.SpeciesIndex) * RecordCellCount + RecordCellIndex;
					const int32* ExistingIndexPtr = CurrentYearCohortIndicesByCell.Find(Key);
					const int32 ExistingIndex = ExistingIndexPtr != nullptr ? *ExistingIndexPtr : INDEX_NONE;
					if (ExistingIndex != INDEX_NONE && !Snapshot.Cohorts.IsValidIndex(ExistingIndex))
					{
						OutError = TEXT("ArcForest recruitment cohort lookup became invalid.");
						return false;
					}
					if (ExistingIndex == INDEX_NONE && RemainingLiveRecords <= 0)
					{
						if (bGrowthDiagnosticsEnabled)
						{
							InOutYearStats.PromotionRecruitmentFlow.RecruitmentRefusedRecordSlot += Admitted;
						}
						continue;
					}
					const double AddedBiomass = BiomassPerStemKg * static_cast<double>(Admitted);
					if (!FMath::IsFinite(AddedBiomass) || AddedBiomass > MAX_flt)
					{
						OutError = TEXT("ArcForest recruited cohort biomass overflowed.");
						return false;
					}
					if (ExistingIndex != INDEX_NONE)
					{
						FArcForestCohortRecord& Existing = Snapshot.Cohorts[ExistingIndex];
						if (Existing.StemCount > MAX_int64 - Admitted
							|| static_cast<double>(Existing.BiomassKg) + AddedBiomass > MAX_flt)
						{
							OutError = TEXT("ArcForest recruited cohort merge overflowed.");
							return false;
						}
						const int64 NewCount = Existing.StemCount + Admitted;
						Existing.MeanHeightCm = static_cast<float>(
							(Existing.MeanHeightCm * static_cast<double>(Existing.StemCount)
								+ MinimumPlantHeightCm * static_cast<double>(Admitted))
							/ static_cast<double>(NewCount));
						Existing.MeanAgeYears = static_cast<float>(
							Existing.MeanAgeYears * static_cast<double>(Existing.StemCount)
							/ static_cast<double>(NewCount));
						Existing.MeanVigor = static_cast<float>(
							(Existing.MeanVigor * static_cast<double>(Existing.StemCount)
								+ static_cast<double>(Admitted))
							/ static_cast<double>(NewCount));
						Existing.StemCount = NewCount;
						Existing.BiomassKg =
							static_cast<float>(static_cast<double>(Existing.BiomassKg) + AddedBiomass);
					}
					else
					{
						const uint64 RecordId = UE::ArcForest::MakeAnnualKey(
							Snapshot.SimulationId,
							Input.Seed,
							Year,
							UE::ArcForest::EDeterministicDomain::RecruitmentRecordIdentity,
							Candidate.SpeciesId,
							Candidate.TargetCellIndex,
							0,
							0,
							0);
						if (RecordId == 0 || UsedIds.Contains(RecordId))
						{
							OutError = TEXT("ArcForest recruitment produced a zero or colliding cohort identifier.");
							return false;
						}
						UsedIds.Add(RecordId);
						FArcForestCohortRecord& Record = Snapshot.Cohorts.AddDefaulted_GetRef();
						Record.Id = RecordId;
						Record.SpeciesId = Candidate.SpeciesId;
						Record.CellIndex = RecordCellIndex;
						Record.BirthYear = Year;
						Record.HeightBin = 0;
						Record.StemCount = Admitted;
						Record.MeanHeightCm = MinimumPlantHeightCm;
						Record.MeanAgeYears = 0.0f;
						Record.MeanVigor = 1.0f;
						Record.BiomassKg = static_cast<float>(AddedBiomass);
						CurrentYearCohortIndicesByCell.Add(Key, Snapshot.Cohorts.Num() - 1);
						--RemainingLiveRecords;
					}
					RemainingLive -= Admitted;
					if (bGrowthDiagnosticsEnabled)
					{
						InOutYearStats.PromotionRecruitmentFlow.RecruitmentAdmitted += Admitted;
					}
				}
				else
				{
					// Regional arrival re-seeds a cell every year, so without this gate the floor budget
					// would be advisory: a species holding no share there would keep re-establishing.
					const double Capacity = GetCoverFloorCapacity(
						CoverFloorCapacity, Candidate.SpeciesIndex, RecordCellCount, RecordCellIndex);
					if (Capacity <= 0.0)
					{
						if (bGrowthDiagnosticsEnabled)
						{
							InOutYearStats.PromotionRecruitmentFlow.RecruitmentRefusedHabitatZero += Admitted;
						}
						continue;
					}
					const int64 Key =
						static_cast<int64>(Candidate.SpeciesIndex) * RecordCellCount + RecordCellIndex;
					const int32* ExistingIndexPtr = PatchIndicesByCell.Find(Key);
					const int32 ExistingIndex = ExistingIndexPtr != nullptr ? *ExistingIndexPtr : INDEX_NONE;
					if (ExistingIndex != INDEX_NONE && !Snapshot.Patches.IsValidIndex(ExistingIndex))
					{
						OutError = TEXT("ArcForest recruitment patch lookup became invalid.");
						return false;
					}
					if (ExistingIndex == INDEX_NONE && RemainingCoverRecords <= 0)
					{
						if (bGrowthDiagnosticsEnabled)
						{
							InOutYearStats.PromotionRecruitmentFlow.RecruitmentRefusedRecordSlot += Admitted;
						}
						continue;
					}
					const double Headroom = Capacity - (ExistingIndex != INDEX_NONE
						? static_cast<double>(Snapshot.Patches[ExistingIndex].StemOrCoverEquivalent)
						: 0.0);
					if (Headroom <= 0.0)
					{
						if (bGrowthDiagnosticsEnabled)
						{
							InOutYearStats.PromotionRecruitmentFlow.RecruitmentRefusedCoverFloorHeadroom += Admitted;
						}
						continue;
					}
					const int64 HeadroomAdmitted = FMath::Min(Admitted, FMath::FloorToInt64(Headroom));
					if (bGrowthDiagnosticsEnabled)
					{
						InOutYearStats.PromotionRecruitmentFlow.RecruitmentRefusedCoverFloorHeadroom +=
							Admitted - HeadroomAdmitted;
					}
					Admitted = HeadroomAdmitted;
					if (Admitted <= 0)
					{
						continue;
					}
					if (ExistingIndex != INDEX_NONE)
					{
						FArcForestPatchRecord& Existing = Snapshot.Patches[ExistingIndex];
						const int64 ExistingBudget = FMath::CeilToInt64(Existing.StemOrCoverEquivalent);
						int64 Low = 0;
						int64 High = Admitted;
						while (Low < High)
						{
							const int64 Mid = Low + (High - Low + 1) / 2;
							const float Serialized = static_cast<float>(
								static_cast<double>(Existing.StemOrCoverEquivalent) + static_cast<double>(Mid));
							const int64 Delta = FMath::CeilToInt64(Serialized) - ExistingBudget;
							if (FMath::IsFinite(Serialized) && Delta <= RemainingLive)
							{
								Low = Mid;
							}
							else
							{
								High = Mid - 1;
							}
						}
						if (bGrowthDiagnosticsEnabled)
						{
							InOutYearStats.PromotionRecruitmentFlow.RecruitmentRefusedLiveStem += Admitted - Low;
						}
						Admitted = Low;
						if (Admitted <= 0)
						{
							continue;
						}
						const double OldEquivalent = Existing.StemOrCoverEquivalent;
						const float NewEquivalent =
							static_cast<float>(OldEquivalent + static_cast<double>(Admitted));
						const int64 LiveDelta = FMath::CeilToInt64(NewEquivalent) - ExistingBudget;
						const double AddedBiomass = BiomassPerStemKg * static_cast<double>(Admitted);
						const double NewBiomass = static_cast<double>(Existing.BiomassKg) + AddedBiomass;
						// Saturated cover: the search already maximised Admitted, so no smaller count
						// moves the serialized value either, and these recruits do not establish.
						if (FMath::IsFinite(NewEquivalent)
							&& NewEquivalent <= Existing.StemOrCoverEquivalent)
						{
							UE_LOGF(
								LogArcEcotope,
								Warning,"Year %d: patch cover %.9g on record cell %d cannot represent %lld more stems; those recruits are dropped.",
								Year,
								OldEquivalent,
								Existing.CellIndex,
								Admitted);
							if (bGrowthDiagnosticsEnabled)
							{
								InOutYearStats.PromotionRecruitmentFlow.RecruitmentRefusedCoverSaturated += Admitted;
							}
							continue;
						}
						if (!FMath::IsFinite(NewEquivalent)
							|| LiveDelta < 0
							|| LiveDelta > RemainingLive
							|| !FMath::IsFinite(NewBiomass)
							|| NewBiomass > MAX_flt)
						{
							OutError = TEXT("ArcForest recruited patch merge was not representable.");
							return false;
						}
						const double NewWeight = NewEquivalent;
						Existing.MeanHeightCm = static_cast<float>(
							(Existing.MeanHeightCm * OldEquivalent
								+ MinimumPlantHeightCm * static_cast<double>(Admitted))
							/ NewWeight);
						Existing.MeanAgeYears =
							static_cast<float>(Existing.MeanAgeYears * OldEquivalent / NewWeight);
						Existing.MeanVigor = static_cast<float>(
							(Existing.MeanVigor * OldEquivalent + static_cast<double>(Admitted)) / NewWeight);
						Existing.StemOrCoverEquivalent = NewEquivalent;
						Existing.BiomassKg = static_cast<float>(NewBiomass);
						RemainingLive -= LiveDelta;
						if (bGrowthDiagnosticsEnabled)
						{
							InOutYearStats.PromotionRecruitmentFlow.RecruitmentAdmitted += Admitted;
						}
					}
					else
					{
						const float Equivalent = static_cast<float>(Admitted);
						if (!FMath::IsFinite(Equivalent)
							|| Equivalent <= 0.0f
							|| FMath::CeilToInt64(Equivalent) > RemainingLive)
						{
							OutError = TEXT("ArcForest recruited patch count was not representable.");
							return false;
						}
						const double Biomass = BiomassPerStemKg * static_cast<double>(Admitted);
						if (!FMath::IsFinite(Biomass) || Biomass > MAX_flt)
						{
							OutError = TEXT("ArcForest recruited patch biomass overflowed.");
							return false;
						}
						const uint64 RecordId = UE::ArcForest::MakeAnnualKey(
							Snapshot.SimulationId,
							Input.Seed,
							Year,
							UE::ArcForest::EDeterministicDomain::RecruitmentRecordIdentity,
							Candidate.SpeciesId,
							Candidate.TargetCellIndex,
							0,
							1,
							0);
						if (RecordId == 0 || UsedIds.Contains(RecordId))
						{
							OutError = TEXT("ArcForest recruitment produced a zero or colliding patch identifier.");
							return false;
						}
						UsedIds.Add(RecordId);
						FArcForestPatchRecord& Record = Snapshot.Patches.AddDefaulted_GetRef();
						Record.Id = RecordId;
						Record.SpeciesId = Candidate.SpeciesId;
						Record.CellIndex = RecordCellIndex;
						Record.StemOrCoverEquivalent = Equivalent;
						Record.MeanHeightCm = MinimumPlantHeightCm;
						Record.MeanAgeYears = 0.0f;
						Record.MeanVigor = 1.0f;
						Record.BiomassKg = static_cast<float>(Biomass);
						PatchIndicesByCell.Add(Key, Snapshot.Patches.Num() - 1);
						RemainingLive -= FMath::CeilToInt64(Equivalent);
						--RemainingCoverRecords;
						if (bGrowthDiagnosticsEnabled)
						{
							InOutYearStats.PromotionRecruitmentFlow.RecruitmentAdmitted += Admitted;
						}
					}
				}
			}
		}
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Admission_MergeAfter);
			return MergeCanonicalLiveRecords(Snapshot, OutError);
		}
	}

	bool PromoteCanonicalCohorts(
		const FArcForestRunInput& Input,
		const FValidBaseCellTable& ValidBaseCells,
		const TArray<FArcForestFrozenSpeciesSetup>& Species,
		const TMap<FGuid, int32>& SpeciesIndices,
		const int32 Year,
		const FArcForestCancellationToken& Cancellation,
		UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics,
		FPromotionWorkTotals& WorkTotals,
		FArcForestWorldSnapshot& Snapshot,
		bool& bInOutChannelBufferWarningEmitted,
		bool& bOutCancelled,
		FString& OutError)
	{
		bOutCancelled = false;
		const FArcEcotopeSiteMetadata& SiteMetadata = GetSiteMetadata(Input);
		if (Cancellation.IsCancellationRequested())
		{
			bOutCancelled = true;
			return false;
		}
		// The sole caller runs AdmitRecruitment immediately before promotion; its successful
		// Admission_MergeAfter leaves cohorts and patches canonical, with no intervening mutation.
		int64 LiveStems = 0;
		int64 LiveRecordCount = 0;
		int64 CoverRecordCount = 0;
		int64 NonBranchDeadwoodRecordCount = 0;
		int64 BranchRecordCount = 0;
		int64 TotalRecordCount = 0;
		if (!TryCountBudgets(
				Snapshot,
				LiveStems,
				LiveRecordCount,
				CoverRecordCount,
				NonBranchDeadwoodRecordCount,
				BranchRecordCount,
				TotalRecordCount,
				OutError))
		{
			return false;
		}
		if (LiveRecordCount > Input.MaxLiveRecords)
		{
			OutError = FString::Printf(
				TEXT("ArcForest promotion began with LiveRecordCount over MaxLiveRecords | liveRecordCount=%lld maxLiveRecords=%d"),
				LiveRecordCount,
				Input.MaxLiveRecords);
			return false;
		}
		if (CoverRecordCount > Input.MaxCoverRecords)
		{
			OutError = FString::Printf(
				TEXT("ArcForest promotion began with CoverRecordCount over MaxCoverRecords | coverRecordCount=%lld maxCoverRecords=%d"),
				CoverRecordCount,
				Input.MaxCoverRecords);
			return false;
		}
		if (TotalRecordCount > Input.MaxStateRecords)
		{
			OutError = FString::Printf(
				TEXT("ArcForest promotion began with TotalRecordCount over MaxStateRecords | totalRecordCount=%lld maxStateRecords=%d"),
				TotalRecordCount,
				Input.MaxStateRecords);
			return false;
		}
		const bool bLiveRecordBudgetSaturated = LiveRecordCount == Input.MaxLiveRecords;
		WorkTotals.SaturatedYears += bLiveRecordBudgetSaturated ? 1 : 0;
		TSet<uint64> UsedIds;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Promotion_BuildUsedIds);
			UsedIds.Reserve(static_cast<int32>(TotalRecordCount));
			if (!BuildUsedRecordIds(Snapshot, UsedIds, OutError))
			{
				return false;
			}
		}

		struct FEligibleCohort
		{
			uint64 CohortId = 0;
			uint64 Priority = 0;
			int32 RecordIndex = INDEX_NONE;
			int32 SpeciesIndex = INDEX_NONE;
		};
		TArray<FEligibleCohort> Eligible;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Promotion_Eligible);
			for (int32 RecordIndex = 0; RecordIndex < Snapshot.Cohorts.Num(); ++RecordIndex)
			{
				const FArcForestCohortRecord& Record = Snapshot.Cohorts[RecordIndex];
				const int32 SpeciesIndex = SpeciesIndices.FindChecked(Record.SpeciesId);
				const FArcForestFrozenSpeciesSetup& Model = Species[SpeciesIndex];
				if (Model.bIndividualizes
					&& HasReachedIndividualization(Record, Model))
				{
					++WorkTotals.EligibleBeforeBudgetFilter;
					if (bLiveRecordBudgetSaturated && Record.StemCount != 1)
					{
						++WorkTotals.SaturatedMultiStemSkipped;
						continue;
					}
					const uint64 Priority = UE::ArcForest::MakeAnnualKey(
						Snapshot.SimulationId,
						Input.Seed,
						Year,
						UE::ArcForest::EDeterministicDomain::PromotionSelection,
						Record.SpeciesId,
						Record.CellIndex,
						Record.Id,
						0,
						Record.StemCount);
					Eligible.Add({Record.Id, Priority, RecordIndex, SpeciesIndex});
				}
			}
		}
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Promotion_EligibleSort);
			Eligible.Sort(
				[](const FEligibleCohort& Left, const FEligibleCohort& Right)
				{
					if (Left.Priority != Right.Priority)
					{
						return Left.Priority < Right.Priority;
					}
					return Left.CohortId < Right.CohortId;
				});
		}

		struct FStemSelection
		{
			int64 Ordinal = 0;
			uint64 Priority = 0;
		};
		struct FHighestPriorityFirst
		{
			bool operator()(const FStemSelection& Left, const FStemSelection& Right) const
			{
				return Left.Priority > Right.Priority;
			}
		};
		TArray<float> PromotedBiomassKg;
		TArray<int64> PromotedOrdinals;
		TArray<FStemSelection> Selection;
		TArray<int32> PlantableBaseCells;
		int64 ChannelBufferSkippedCohorts = 0;

		// Removals are masked rather than applied in place so the indices carried out of the
		// eligibility pass stay valid for the whole promotion walk.
		TArray<uint8> RemovedCohorts;
		RemovedCohorts.Init(0, Snapshot.Cohorts.Num());
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Promotion_Promote);
			for (const FEligibleCohort& EligibleCohort : Eligible)
			{
				if (Cancellation.IsCancellationRequested())
				{
					bOutCancelled = true;
					return false;
				}
				const int32 CohortIndex = EligibleCohort.RecordIndex;
				FArcForestCohortRecord& Cohort = Snapshot.Cohorts[CohortIndex];
				const int64 FreeLiveRecords = static_cast<int64>(Input.MaxLiveRecords) - LiveRecordCount;
				int64 PromotionCount = 0;
				if (Cohort.StemCount <= FreeLiveRecords + 1)
				{
					PromotionCount = Cohort.StemCount;
				}
				else
				{
					PromotionCount = FMath::Min(Cohort.StemCount - 1, FreeLiveRecords);
				}
				if (PromotionCount <= 0)
				{
					continue;
				}
				if (!TryGetValidBaseCellsForRecordCell(
						SiteMetadata,
						ValidBaseCells.BaseValidityMask,
						Cohort.CellIndex,
						PlantableBaseCells)
					|| PlantableBaseCells.IsEmpty())
				{
					OutError = TEXT("ArcForest promotion found an invalid cohort cell.");
					return false;
				}
				if (!TryFilterTreePlantableBaseCells(ValidBaseCells, PlantableBaseCells, OutError))
				{
					return false;
				}
				if (PlantableBaseCells.IsEmpty())
				{
					++ChannelBufferSkippedCohorts;
					continue;
				}
				const FArcForestFrozenSpeciesSetup& Model = Species[EligibleCohort.SpeciesIndex];
				const int64 OriginalStemCount = Cohort.StemCount;
				const int64 RemainingStemCount = OriginalStemCount - PromotionCount;
				++WorkTotals.PromotedCohorts;
				WorkTotals.PromotedStems += PromotionCount;
				WorkTotals.SingletonPromotions += OriginalStemCount == 1 && PromotionCount == 1 ? 1 : 0;
				WorkTotals.OrdinalsScanned += OriginalStemCount;
				const double PerStemBiomassTarget =
					static_cast<double>(Cohort.BiomassKg) / static_cast<double>(OriginalStemCount);
				if (!FMath::IsFinite(PerStemBiomassTarget) || PerStemBiomassTarget <= 0.0)
				{
					OutError = TEXT("ArcForest promotion found non-positive cohort biomass per stem.");
					return false;
				}

				PromotedBiomassKg.Init(static_cast<float>(PerStemBiomassTarget), static_cast<int32>(PromotionCount));
				float SerializedRemainingBiomass = RemainingStemCount > 0
					? static_cast<float>(PerStemBiomassTarget * static_cast<double>(RemainingStemCount))
					: 0.0f;
				if (PromotedBiomassKg[0] <= 0.0f
					|| !FMath::IsFinite(PromotedBiomassKg[0])
					|| !FMath::IsFinite(SerializedRemainingBiomass)
					|| SerializedRemainingBiomass < 0.0f)
				{
					OutError = TEXT("ArcForest promotion biomass was not representable as positive float state.");
					return false;
				}

				// Ordinary promoted stems receive the rounded per-stem target. The
				// final selected stem receives the deterministic serialized residual.
				double OrdinaryPromotedTotal = 0.0;
				for (int32 PromotedIndex = 0; PromotedIndex + 1 < PromotedBiomassKg.Num(); ++PromotedIndex)
				{
					OrdinaryPromotedTotal += PromotedBiomassKg[PromotedIndex];
				}
				PromotedBiomassKg.Last() = static_cast<float>(
					static_cast<double>(Cohort.BiomassKg)
					- static_cast<double>(SerializedRemainingBiomass)
					- OrdinaryPromotedTotal);
				if (!FMath::IsFinite(PromotedBiomassKg.Last()) || PromotedBiomassKg.Last() <= 0.0f)
				{
					OutError = TEXT("ArcForest promotion residual biomass was not positive and representable.");
					return false;
				}

				double ConservedBiomass = SerializedRemainingBiomass;
				for (const float BiomassKg : PromotedBiomassKg)
				{
					ConservedBiomass += BiomassKg;
				}
				constexpr double FloatEpsilon = 1.1920928955078125e-7;
				const double ConservationToleranceKg =
					8.0 * FloatEpsilon * FMath::Max(1.0, static_cast<double>(Cohort.BiomassKg));
				const double ResidualCorrection =
					static_cast<double>(Cohort.BiomassKg) - ConservedBiomass;
				if (FMath::Abs(ResidualCorrection) > 0.0)
				{
					PromotedBiomassKg.Last() =
						static_cast<float>(static_cast<double>(PromotedBiomassKg.Last()) + ResidualCorrection);
					ConservedBiomass = SerializedRemainingBiomass;
					for (const float BiomassKg : PromotedBiomassKg)
					{
						ConservedBiomass += BiomassKg;
					}
				}
				if (!FMath::IsFinite(PromotedBiomassKg.Last())
					|| PromotedBiomassKg.Last() <= 0.0f
					|| FMath::Abs(ConservedBiomass - static_cast<double>(Cohort.BiomassKg))
						> ConservationToleranceKg)
				{
					OutError = TEXT("ArcForest promotion could not conserve authoritative serialized biomass.");
					return false;
				}

				PromotedOrdinals.Reset();
				PromotedOrdinals.Reserve(static_cast<int32>(PromotionCount));
				if (PromotionCount >= OriginalStemCount)
				{
					for (int64 StemOrdinal = 0; StemOrdinal < OriginalStemCount; ++StemOrdinal)
					{
						if ((StemOrdinal & 4095) == 0 && Cancellation.IsCancellationRequested())
						{
							bOutCancelled = true;
							return false;
						}
						PromotedOrdinals.Add(StemOrdinal);
					}
				}
				else
				{
					// Bounded max-heap of the PromotionCount lowest hashed priorities: an unbiased
					// K-of-N draw that never materialises one entry per stem.
					Selection.Reset();
					Selection.Reserve(static_cast<int32>(PromotionCount));
					for (int64 StemOrdinal = 0; StemOrdinal < OriginalStemCount; ++StemOrdinal)
					{
						if ((StemOrdinal & 4095) == 0 && Cancellation.IsCancellationRequested())
						{
							bOutCancelled = true;
							return false;
						}
						const uint64 Priority = UE::ArcForest::MakeAnnualKey(
							Snapshot.SimulationId,
							Input.Seed,
							Year,
							UE::ArcForest::EDeterministicDomain::PromotionSelection,
							Cohort.SpeciesId,
							Cohort.CellIndex,
							Cohort.Id,
							static_cast<int32>(StemOrdinal),
							Cohort.StemCount);
						if (Selection.Num() < static_cast<int32>(PromotionCount))
						{
							Selection.HeapPush(FStemSelection{StemOrdinal, Priority}, FHighestPriorityFirst());
						}
						else if (Priority < Selection.HeapTop().Priority)
						{
							Selection.HeapPopDiscard(FHighestPriorityFirst(), EAllowShrinking::No);
							Selection.HeapPush(FStemSelection{StemOrdinal, Priority}, FHighestPriorityFirst());
						}
					}
					for (const FStemSelection& Stem : Selection)
					{
						PromotedOrdinals.Add(Stem.Ordinal);
					}
					PromotedOrdinals.Sort();
				}

				WorkTotals.PlantableChildren += PlantableBaseCells.Num();
				const bool bPretranslatedPlantableEntries = PromotionCount > PlantableBaseCells.Num();
				if (bPretranslatedPlantableEntries)
				{
					for (int32& PlantableBaseCell : PlantableBaseCells)
					{
						const int32 ValidEntryIndex =
							FindValidBaseCellEntryIndex(ValidBaseCells, PlantableBaseCell);
						++WorkTotals.SurfaceEntryLookups;
						if (ValidEntryIndex == INDEX_NONE
							|| !ValidBaseCells.Entries.IsValidIndex(ValidEntryIndex)
							|| !ValidBaseCells.LocalSurfaceZCmByValidEntry.IsValidIndex(ValidEntryIndex))
						{
							OutError = TEXT("ArcForest promotion selected a valid base cell missing from the valid-cell table.");
							return false;
						}
						PlantableBaseCell = ValidEntryIndex;
					}
				}
				const int64 AgeYears64 = FMath::Max<int64>(
					0,
					static_cast<int64>(Year) - static_cast<int64>(Cohort.BirthYear));
				if (AgeYears64 > MAX_int32)
				{
					OutError = TEXT("ArcForest promoted individual age was not representable.");
					return false;
				}

				for (int32 PromotedIndex = 0; PromotedIndex < PromotedOrdinals.Num(); ++PromotedIndex)
				{
					if (PromotedIndex != 0
						&& (PromotedIndex & 4095) == 0
						&& Cancellation.IsCancellationRequested())
					{
						bOutCancelled = true;
						return false;
					}
					const int64 StemOrdinal = PromotedOrdinals[PromotedIndex];
					const float IndividualBiomassKg = PromotedBiomassKg[PromotedIndex];
					float IndividualDbhCm = 0.0f;
					if (!TryDeriveDbhFromAuthoritativeBiomass(Model, IndividualBiomassKg, IndividualDbhCm))
					{
						OutError = TEXT("ArcForest promotion could not derive valid DBH from authoritative biomass.");
						return false;
					}
					const uint64 IndividualId = UE::ArcForest::MakeAnnualKey(
						Snapshot.SimulationId,
						Input.Seed,
						Year,
						UE::ArcForest::EDeterministicDomain::PromotionRecordIdentity,
						Cohort.SpeciesId,
						Cohort.CellIndex,
						Cohort.Id,
						static_cast<int32>(StemOrdinal),
						Cohort.StemCount);
					if (IndividualId == 0 || UsedIds.Contains(IndividualId))
					{
						OutError = TEXT("ArcForest promotion produced a zero or colliding individual identifier.");
						return false;
					}
					UsedIds.Add(IndividualId);
					const uint64 CellKey = UE::ArcForest::MakeAnnualKey(
						Snapshot.SimulationId,
						Input.Seed,
						Year,
						UE::ArcForest::EDeterministicDomain::PromotionPositionCell,
						Cohort.SpeciesId,
						Cohort.CellIndex,
						Cohort.Id,
						static_cast<int32>(StemOrdinal),
						Cohort.StemCount);
					const int32 PlantableOrdinal = FMath::Min(
						PlantableBaseCells.Num() - 1,
						static_cast<int32>(
							UE::ArcForest::KeyToUnitOpen(CellKey) * static_cast<double>(PlantableBaseCells.Num())));
					const int32 PlantedBaseCellEntryIndex = bPretranslatedPlantableEntries
						? PlantableBaseCells[PlantableOrdinal]
						: FindValidBaseCellEntryIndex(ValidBaseCells, PlantableBaseCells[PlantableOrdinal]);
					WorkTotals.SurfaceEntryLookups += bPretranslatedPlantableEntries ? 0 : 1;
					if (PlantedBaseCellEntryIndex == INDEX_NONE
						|| !ValidBaseCells.Entries.IsValidIndex(PlantedBaseCellEntryIndex)
						|| !ValidBaseCells.LocalSurfaceZCmByValidEntry.IsValidIndex(PlantedBaseCellEntryIndex))
					{
						OutError = TEXT("ArcForest promotion selected a valid base cell missing from the valid-cell table.");
						return false;
					}
					const int32 PlantedBaseCellIndex =
						ValidBaseCells.Entries[PlantedBaseCellEntryIndex].BaseCellIndex;
					FVector2D CellMinLocal = FVector2D::ZeroVector;
					FVector2D CellMaxLocal = FVector2D::ZeroVector;
					if (!SiteMetadata.TryGetLevelCellRect(PlantedBaseCellIndex, 0, CellMinLocal, CellMaxLocal))
					{
						OutError = TEXT("ArcForest promotion could not resolve a planted base cell rectangle.");
						return false;
					}
					const uint64 XKey = UE::ArcForest::MakeAnnualKey(
						Snapshot.SimulationId,
						Input.Seed,
						Year,
						UE::ArcForest::EDeterministicDomain::PromotionPositionX,
						Cohort.SpeciesId,
						Cohort.CellIndex,
						Cohort.Id,
						static_cast<int32>(StemOrdinal),
						Cohort.StemCount);
					const uint64 YKey = UE::ArcForest::MakeAnnualKey(
						Snapshot.SimulationId,
						Input.Seed,
						Year,
						UE::ArcForest::EDeterministicDomain::PromotionPositionY,
						Cohort.SpeciesId,
						Cohort.CellIndex,
						Cohort.Id,
						static_cast<int32>(StemOrdinal),
						Cohort.StemCount);
					FArcForestIndividualRecord& Individual = Snapshot.Individuals.AddDefaulted_GetRef();
					Individual.Id = IndividualId;
					Individual.SpeciesId = Cohort.SpeciesId;
					Individual.LocalPosition = FVector(
						FMath::Lerp(CellMinLocal.X, CellMaxLocal.X, UE::ArcForest::KeyToUnitOpen(XKey)),
						FMath::Lerp(CellMinLocal.Y, CellMaxLocal.Y, UE::ArcForest::KeyToUnitOpen(YKey)),
						ValidBaseCells.LocalSurfaceZCmByValidEntry[PlantedBaseCellEntryIndex]);
					Individual.AgeYears = static_cast<int32>(AgeYears64);
					Individual.HeightCm = Cohort.MeanHeightCm;
					Individual.DbhCm = IndividualDbhCm;
					Individual.CrownRadiusCm = FMath::Max(0.0f, Cohort.MeanHeightCm * Model.CrownRadiusHeightRatio);
					Individual.BiomassKg = IndividualBiomassKg;
					Individual.Vigor = Cohort.MeanVigor;
					Individual.LifeStatus = EArcForestLifeStatus::Alive;
					const uint64 ShedKey = UE::ArcForest::MakeAnnualKey(
						Snapshot.SimulationId,
						Input.Seed,
						Year,
						UE::ArcForest::EDeterministicDomain::BranchShedOccurrence,
						Cohort.SpeciesId,
						PlantedBaseCellIndex,
						IndividualId,
						0,
						/*Salt*/ 0);
					Individual.NextShedYear = CalculateGeometricEventYear(
						Year,
						static_cast<double>(Input.DeadwoodSettings.BranchShedProbabilityPerYear),
						UE::ArcForest::KeyToUnitOpen(ShedKey));
				}

				if (RemainingStemCount == 0)
				{
					RemovedCohorts[CohortIndex] = 1;
					LiveRecordCount += PromotionCount - 1;
					TotalRecordCount += PromotionCount - 1;
				}
				else
				{
					Cohort.StemCount = RemainingStemCount;
					Cohort.BiomassKg = SerializedRemainingBiomass;
					LiveRecordCount += PromotionCount;
					TotalRecordCount += PromotionCount;
				}
			}
		}
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, Promotion_Compact);
			CompactRemovedRecords(Snapshot.Cohorts, RemovedCohorts);
		}
		if (LiveRecordCount > Input.MaxLiveRecords)
		{
			OutError = FString::Printf(
				TEXT("ArcForest promotion exceeded MaxLiveRecords | liveRecordCount=%lld maxLiveRecords=%d"),
				LiveRecordCount,
				Input.MaxLiveRecords);
			return false;
		}
		if (TotalRecordCount > Input.MaxStateRecords)
		{
			OutError = FString::Printf(
				TEXT("ArcForest promotion exceeded MaxStateRecords | totalRecordCount=%lld maxStateRecords=%d"),
				TotalRecordCount,
				Input.MaxStateRecords);
			return false;
		}
		WorkTotals.ChannelBufferSkippedCohorts += ChannelBufferSkippedCohorts;
		if (ChannelBufferSkippedCohorts > 0)
		{
			const FString ChannelBufferMessage = FString::Printf(
				TEXT("ArcForest promotion skipped cohorts whose record cell lies wholly inside the channel ")
					TEXT("buffer hard zone; they keep their stems and their biomass but never promote | ")
					TEXT("year=%d skippedCohortRecords=%lld eligibleCohorts=%d"),
				Year,
				ChannelBufferSkippedCohorts,
				Eligible.Num());
			if (bInOutChannelBufferWarningEmitted)
			{
				UE_LOGF(LogArcEcotope, Log,"%ls", *ChannelBufferMessage);
			}
			else
			{
				bInOutChannelBufferWarningEmitted = true;
				UE_LOGF(LogArcEcotope, Warning,"%ls", *ChannelBufferMessage);
			}
		}
		return true;
	}
}

void FArcForestCancellationToken::RequestCancellation()
{
	bCancellationRequested.Store(true);
}

bool FArcForestCancellationToken::IsCancellationRequested() const
{
	return bCancellationRequested.Load();
}

void FArcForestProgressState::Reset(const int32 InTotalYears)
{
	Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Idle));
	CompletedYears.Store(0);
	TotalYears.Store(InTotalYears);
}

EArcForestGrowthPhase FArcForestProgressState::GetPhase() const
{
	return static_cast<EArcForestGrowthPhase>(Phase.Load());
}

bool FArcForestGrowthEngine::EstimateCapacity(
	const FArcForestRunInput& Input,
	FArcForestCapacityEstimate& OutEstimate,
	FString& OutError)
{
	using namespace ArcForestGrowthEnginePrivate;

	OutEstimate = FArcForestCapacityEstimate();
	OutError.Reset();

	if (!Input.SiteSource.IsValid())
	{
		OutError = TEXT("ArcForest capacity estimate requires an immutable site source.");
		return false;
	}
	const FArcEcotopeSiteMetadata& SiteMetadata = GetSiteMetadata(Input);
	const FIntPoint BaseDimensions = SiteMetadata.GridDimensions;
	if (BaseDimensions.X <= 0 || BaseDimensions.Y <= 0)
	{
		OutError = TEXT("ArcForest capacity estimate requires positive base-grid dimensions.");
		return false;
	}
	if (SiteMetadata.RecordGridLevel > FArcEcotopeSiteMetadata::MaxGridLevel)
	{
		OutError = TEXT("ArcForest capacity estimate found an unsupported record-grid level.");
		return false;
	}
	if (Input.ChunkBaseCellSpan <= 0)
	{
		OutError = TEXT("ArcForest capacity estimate requires a positive chunk base-cell span.");
		return false;
	}

	const int64 BaseWidth = static_cast<int64>(BaseDimensions.X);
	const int64 BaseHeight = static_cast<int64>(BaseDimensions.Y);
	const int64 RecordDivisor = 1ll << SiteMetadata.RecordGridLevel;
	int64 RecordWidth = 0;
	int64 RecordHeight = 0;
	if (!TryAddCapacityValue(BaseWidth, RecordDivisor - 1, RecordWidth)
		|| !TryAddCapacityValue(BaseHeight, RecordDivisor - 1, RecordHeight))
	{
		OutError = TEXT("ArcForest capacity estimate overflowed while calculating record-grid dimensions.");
		return false;
	}
	RecordWidth /= RecordDivisor;
	RecordHeight /= RecordDivisor;
	if (RecordWidth > MAX_int32 || RecordHeight > MAX_int32)
	{
		OutError = TEXT("ArcForest capacity estimate record-grid dimensions exceeded int32.");
		return false;
	}

	int64 BaseCellCount = 0;
	int64 RecordCellCount = 0;
	if (!TryMultiplyCapacityValue(BaseWidth, BaseHeight, BaseCellCount)
		|| !TryMultiplyCapacityValue(RecordWidth, RecordHeight, RecordCellCount))
	{
		OutError = TEXT("ArcForest capacity estimate overflowed while calculating grid cell counts.");
		return false;
	}
	if (BaseCellCount > MAX_int32 || RecordCellCount > MAX_int32)
	{
		OutError = TEXT("ArcForest capacity estimate grid cell counts exceeded int32 indexing.");
		return false;
	}

	const int64 ChunkSpan = static_cast<int64>(Input.ChunkBaseCellSpan);
	int64 ChunkWidthNumerator = 0;
	int64 ChunkHeightNumerator = 0;
	if (!TryAddCapacityValue(BaseWidth, ChunkSpan - 1, ChunkWidthNumerator)
		|| !TryAddCapacityValue(BaseHeight, ChunkSpan - 1, ChunkHeightNumerator))
	{
		OutError = TEXT("ArcForest capacity estimate overflowed while calculating chunk-grid dimensions.");
		return false;
	}
	const int64 ChunkWidth = ChunkWidthNumerator / ChunkSpan;
	const int64 ChunkHeight = ChunkHeightNumerator / ChunkSpan;
	int64 ChunkCount = 0;
	if (ChunkWidth > MAX_int32 || ChunkHeight > MAX_int32
		|| !TryMultiplyCapacityValue(ChunkWidth, ChunkHeight, ChunkCount)
		|| ChunkCount > MAX_int32)
	{
		OutError = TEXT("ArcForest capacity estimate chunk-grid dimensions exceeded their representable range.");
		return false;
	}

	const int64 MaxChunkCoreWidth = FMath::Min(BaseWidth, ChunkSpan);
	const int64 MaxChunkCoreHeight = FMath::Min(BaseHeight, ChunkSpan);
	int64 MaxChunkCoreBaseCells = 0;
	int64 HaloBaseCells = 0;
	int64 TwiceHaloBaseCells = 0;
	int64 ReadWidthUnclamped = 0;
	int64 ReadHeightUnclamped = 0;
	int64 MaxChunkReadBaseCells = 0;
	if (!TryMultiplyCapacityValue(MaxChunkCoreWidth, MaxChunkCoreHeight, MaxChunkCoreBaseCells)
		|| !TryMultiplyCapacityValue(
			static_cast<int64>(CapacityEstimateRecordHaloCells), RecordDivisor, HaloBaseCells)
		|| !TryMultiplyCapacityValue(HaloBaseCells, 2, TwiceHaloBaseCells)
		|| !TryAddCapacityValue(MaxChunkCoreWidth, TwiceHaloBaseCells, ReadWidthUnclamped)
		|| !TryAddCapacityValue(MaxChunkCoreHeight, TwiceHaloBaseCells, ReadHeightUnclamped)
		|| !TryMultiplyCapacityValue(
			FMath::Min(BaseWidth, ReadWidthUnclamped),
			FMath::Min(BaseHeight, ReadHeightUnclamped),
			MaxChunkReadBaseCells))
	{
		OutError = TEXT("ArcForest capacity estimate overflowed while calculating chunk core or halo sizes.");
		return false;
	}

	int64 RegionalSpeciesCount = 0;
	for (const FArcForestFrozenSpeciesSetup& SpeciesSetup : Input.SpeciesSetups)
	{
		if (SpeciesSetup.RegionalArrivalPerHectarePerYear > 0.0f
			&& !TryAddCapacityValue(RegionalSpeciesCount, 1, RegionalSpeciesCount))
		{
			OutError = TEXT("ArcForest capacity estimate overflowed while counting regional species.");
			return false;
		}
	}

	int64 SourceRecordCount = 0;
	int64 EstimatedCandidatePayloadBytes = 0;
	const auto AccumulateRecordArray = [&SourceRecordCount, &EstimatedCandidatePayloadBytes](
		const int32 RecordCount,
		const SIZE_T RecordSize)
	{
		return TryAddCapacityValue(SourceRecordCount, static_cast<int64>(RecordCount), SourceRecordCount)
			&& TryAccumulateCapacityBytes(
				static_cast<int64>(RecordCount), RecordSize, EstimatedCandidatePayloadBytes);
	};
	if (!AccumulateRecordArray(Input.SourceSnapshot.Individuals.Num(), sizeof(FArcForestIndividualRecord))
		|| !AccumulateRecordArray(Input.SourceSnapshot.Cohorts.Num(), sizeof(FArcForestCohortRecord))
		|| !AccumulateRecordArray(Input.SourceSnapshot.Patches.Num(), sizeof(FArcForestPatchRecord))
		|| !AccumulateRecordArray(Input.SourceSnapshot.Deadwood.Num(), sizeof(FArcForestDeadwoodRecord)))
	{
		OutError = TEXT("ArcForest capacity estimate overflowed while projecting candidate storage.");
		return false;
	}

	// Habitat is a core-only output in the planned pipeline. MaxChunkReadBaseCells separately exposes
	// the larger site/occupancy read footprint and must not be mistaken for Habitat output storage.
	int64 MaxChunkCoreHabitatSamples = 0;
	int64 MaxChunkCoreHabitatBytes = 0;
	if (!TryMultiplyCapacityValue(
			static_cast<int64>(Input.SpeciesSetups.Num()),
			MaxChunkCoreBaseCells,
			MaxChunkCoreHabitatSamples)
		|| !TryMultiplyCapacityValue(
			MaxChunkCoreHabitatSamples,
			static_cast<int64>(sizeof(uint32)),
			MaxChunkCoreHabitatBytes))
	{
		OutError = TEXT("ArcForest capacity estimate overflowed while projecting chunk habitat storage.");
		return false;
	}

	const int32 AvailableWorkerCount = FMath::Max(FPlatformMisc::NumberOfWorkerThreadsToSpawn(), 1);
	const int32 PlannedWorkerSlots = static_cast<int32>(FMath::Min<int64>(
		ChunkCount,
		static_cast<int64>(AvailableWorkerCount)));
	int64 PlannedActiveHabitatCoreBytes = 0;
	if (!TryMultiplyCapacityValue(
		static_cast<int64>(PlannedWorkerSlots),
		MaxChunkCoreHabitatBytes,
		PlannedActiveHabitatCoreBytes))
	{
		OutError = TEXT("ArcForest capacity estimate overflowed while projecting active chunk scratch.");
		return false;
	}

	OutEstimate.BaseGridDimensions = BaseDimensions;
	OutEstimate.RecordGridDimensions = FIntPoint(static_cast<int32>(RecordWidth), static_cast<int32>(RecordHeight));
	OutEstimate.ChunkGridDimensions = FIntPoint(static_cast<int32>(ChunkWidth), static_cast<int32>(ChunkHeight));
	OutEstimate.ChunkBaseCellSpan = Input.ChunkBaseCellSpan;
	OutEstimate.RecordHaloCells = CapacityEstimateRecordHaloCells;
	OutEstimate.PlannedWorkerSlots = PlannedWorkerSlots;
	OutEstimate.BaseCellCount = BaseCellCount;
	OutEstimate.RecordCellCount = RecordCellCount;
	OutEstimate.SpeciesCount = Input.SpeciesSetups.Num();
	OutEstimate.RegionalSpeciesCount = RegionalSpeciesCount;
	OutEstimate.ChunkCount = ChunkCount;
	OutEstimate.SourceRecordCount = SourceRecordCount;
	OutEstimate.EstimatedCandidatePayloadBytes = EstimatedCandidatePayloadBytes;
	OutEstimate.MaxChunkCoreBaseCells = MaxChunkCoreBaseCells;
	OutEstimate.MaxChunkReadBaseCells = MaxChunkReadBaseCells;
	OutEstimate.MaxChunkCoreHabitatSamples = MaxChunkCoreHabitatSamples;
	OutEstimate.MaxChunkCoreHabitatBytes = MaxChunkCoreHabitatBytes;
	OutEstimate.PlannedActiveHabitatCoreBytes = PlannedActiveHabitatCoreBytes;
	return true;
}

bool FArcForestGrowthEngine::Simulate(
	const FArcForestRunInput& Input,
	const int32 NumYears,
	const FArcForestCancellationToken& Cancellation,
	FArcForestProgressState& Progress,
	FArcForestRunResult& OutResult)
{
	using namespace ArcForestGrowthEnginePrivate;
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcForest_Simulate);
	UE::ArcForest::FDiagnosticScopeReport Report(TEXT("ArcForest Simulate"));
	UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics = Report.GetPhases();
	const UE::ArcForest::FSharedHelperCounters HelperBaseline = UE::ArcForest::CaptureSharedHelperCounters();

	OutResult = FArcForestRunResult();
	Progress.Reset(NumYears);
	if (IsCancellationRequested(Cancellation, Progress, OutResult))
	{
		Report.SetOutcome(TEXT("cancelled before validation"));
		return false;
	}
	if (!Input.RegionalRecruitmentConfig.IsValid())
	{
		OutResult.Status = EArcForestRunStatus::InvalidInput;
		OutResult.Error = TEXT("ArcForest annual regional recruitment configuration is invalid.");
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		Report.SetOutcome(TEXT("invalid regional recruitment configuration"));
		return false;
	}
	if (!Input.SiteSource.IsValid())
	{
		OutResult.Status = EArcForestRunStatus::InvalidInput;
		OutResult.Error = TEXT("ArcForest annual simulation requires an immutable site source.");
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		Report.SetOutcome(TEXT("missing site source"));
		return false;
	}
	const FArcEcotopeSiteMetadata& SiteMetadata = GetSiteMetadata(Input);
	const int64 GrowthReaderBudgetBytes = static_cast<int64>(FMath::Max(
		CVarArcForestGrowthSiteReaderBudgetMiB.GetValueOnAnyThread(), 1)) * 1024ll * 1024ll;
	FArcEcotopeSiteChunkReader AnnualSiteReader;
	const FArcEcotopeChunkLayout* AnnualReaderLayout = &Input.SiteSource->Layout;
	FString SiteBoundaryError;
#if WITH_EDITORONLY_DATA
	FArcEcotopeSiteBulkValidationResult ValidationResult;
	FArcEcotopeSiteBulkReaderStats ValidationStats;
	if (!FArcEcotopeSiteChunkReader::ValidateEditorBulkSource(
			Input.SiteSource,
			GrowthReaderBudgetBytes,
			AnnualSiteReader,
			ValidationResult,
			ValidationStats,
			SiteBoundaryError,
			&Cancellation))
	{
		OutResult.Status = EArcForestRunStatus::InvalidInput;
		OutResult.Error = FString::Printf(TEXT("ArcForest annual site source boundary validation failed: %s"), *SiteBoundaryError);
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		Report.SetOutcome(TEXT("site source boundary invalid"));
		return false;
	}
#else
	if (!AnnualSiteReader.OpenFrozen(Input.SiteSource, GrowthReaderBudgetBytes, SiteBoundaryError))
	{
		OutResult.Status = EArcForestRunStatus::InvalidInput;
		OutResult.Error = FString::Printf(TEXT("ArcForest annual site source failed to open: %s"), *SiteBoundaryError);
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		Report.SetOutcome(TEXT("site source open failed"));
		return false;
	}
#endif

	FArcForestCapacityEstimate CapacityEstimate;
	FString CapacityError;
	if (EstimateCapacity(Input, CapacityEstimate, CapacityError))
	{
		UE_LOGF(
			LogArcEcotope,
			Log,"ArcForest capacity | base=%dx%d (%lld) record=%dx%d (%lld) species=%lld regional=%lld "
				"sourceRecords=%lld candidatePayload=%.2fMiB | chunks=%dx%d (%lld) span=%d haloRecord=%d "
				"core=%lld read=%lld plannedWorkerSlots=%d chunkCoreHabitat=%lld/%.2fMiB "
				"plannedActiveHabitatCore=%.2fMiB",
			CapacityEstimate.BaseGridDimensions.X,
			CapacityEstimate.BaseGridDimensions.Y,
			CapacityEstimate.BaseCellCount,
			CapacityEstimate.RecordGridDimensions.X,
			CapacityEstimate.RecordGridDimensions.Y,
			CapacityEstimate.RecordCellCount,
			CapacityEstimate.SpeciesCount,
			CapacityEstimate.RegionalSpeciesCount,
			CapacityEstimate.SourceRecordCount,
			ToMebibytes(CapacityEstimate.EstimatedCandidatePayloadBytes),
			CapacityEstimate.ChunkGridDimensions.X,
			CapacityEstimate.ChunkGridDimensions.Y,
			CapacityEstimate.ChunkCount,
			CapacityEstimate.ChunkBaseCellSpan,
			CapacityEstimate.RecordHaloCells,
			CapacityEstimate.MaxChunkCoreBaseCells,
			CapacityEstimate.MaxChunkReadBaseCells,
			CapacityEstimate.PlannedWorkerSlots,
			CapacityEstimate.MaxChunkCoreHabitatSamples,
			ToMebibytes(CapacityEstimate.MaxChunkCoreHabitatBytes),
			ToMebibytes(CapacityEstimate.PlannedActiveHabitatCoreBytes));
	}
	else
	{
		UE_LOGF(LogArcEcotope, Warning,"ArcForest capacity estimate unavailable: %ls", *CapacityError);
	}

	const FParallelGrowthSettings ParallelGrowthSettings{
		FMath::Max(0, CVarArcForestSimulationParallelGrowthTaskSlots.GetValueOnAnyThread())};
	UE_LOGF(
		LogArcEcotope,
		Log,"ArcForest growth task planning | requestedTaskSlots=%d (%ls; task slots, not worker threads)",
		ParallelGrowthSettings.RequestedTaskSlots,
		ParallelGrowthSettings.RequestedTaskSlots == 0 ? TEXT("auto") : TEXT("CVar"));

	Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Validation));
	TArray<FArcForestFrozenSpeciesSetup> Species;
	TArray<FArcForestSpeciesEcologyHash> EcologyHashes;
	FString Error;
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Validation);
		if (!ValidateInput(Input, NumYears, Species, EcologyHashes, Error))
		{
			OutResult.Status = EArcForestRunStatus::InvalidInput;
			OutResult.Error = MoveTemp(Error);
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			Report.SetOutcome(TEXT("invalid input"));
			return false;
		}
	}
	FText SourceCompatibilityError;
	FArcForestWorldSiteValidationStats SourceCompatibilityStats;
	if (!Input.SourceSnapshot.ValidateSiteCompatibility(
			SiteMetadata,
			*AnnualReaderLayout,
			AnnualSiteReader,
			SiteCompatibilityScheduleBudgetBytes,
			SourceCompatibilityStats,
			SourceCompatibilityError))
	{
		OutResult.Status = EArcForestRunStatus::InvalidInput;
		OutResult.Error = FString::Printf(
			TEXT("ArcForest annual source snapshot is site-incompatible: %s"),
			*SourceCompatibilityError.ToString());
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		Report.SetOutcome(TEXT("source site compatibility failed"));
		return false;
	}

	const int32 OccupancyRecordHaloCells = CalculateOccupancyRecordHaloCells(SiteMetadata, Species);
	FArcEcotopeChunkLayout ChunkLayout;
	FString ChunkLayoutError;
	if (!FArcEcotopeChunkLayout::Build(
			SiteMetadata,
			Input.ChunkBaseCellSpan,
			OccupancyRecordHaloCells,
			ChunkLayout,
			ChunkLayoutError))
	{
		OutResult.Status = EArcForestRunStatus::InvalidInput;
		OutResult.Error = FString::Printf(TEXT("ArcForest chunk layout is invalid: %s"), *ChunkLayoutError);
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		Report.SetOutcome(TEXT("chunk layout failed"));
		return false;
	}
	TArray<TArray<int32>> TileGatherSourceOwnerChunksByDestination;
	if (!BuildTileGatherAdjacency(
			ChunkLayout,
			TileGatherSourceOwnerChunksByDestination,
			ChunkLayoutError))
	{
		OutResult.Status = EArcForestRunStatus::InvalidInput;
		OutResult.Error = FString::Printf(TEXT("ArcForest TileGather adjacency is invalid: %s"), *ChunkLayoutError);
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		Report.SetOutcome(TEXT("TileGather adjacency failed"));
		return false;
	}
	const FIntPoint ChunkGridDimensions = ChunkLayout.GetChunkGridDimensions();
	UE_LOGF(
		LogArcEcotope,
		Log,
		"ArcForest execution layout | siteLayout=chunked annualScheduler=global-phases"
		" chunkGrid=%dx%d chunkCount=%d span=%d haloRecord=%d",
		ChunkGridDimensions.X,
		ChunkGridDimensions.Y,
		ChunkLayout.GetChunkCount(),
		ChunkLayout.GetCoreBaseCellSpan(),
		ChunkLayout.GetRecordHaloCells());
	UE_LOGF(
		LogArcEcotope,
		Log,"ArcForest occupancy | mode=TileGather haloRecord=%d footprintSpanCap=%d sourceOrder=stable-kway",
		OccupancyRecordHaloCells,
		MaximumFootprintSpanCells);

	TMap<FGuid, int32> SpeciesIndices;
	for (int32 SpeciesIndex = 0; SpeciesIndex < Species.Num(); ++SpeciesIndex)
	{
		SpeciesIndices.Add(Species[SpeciesIndex].SpeciesId, SpeciesIndex);
	}

	if (IsCancellationRequested(Cancellation, Progress, OutResult))
	{
		Report.SetOutcome(TEXT("cancelled before annual site reader open"));
		return false;
	}
	if (AnnualReaderLayout->GetBaseDimensions() != SiteMetadata.GridDimensions
		|| AnnualReaderLayout->GetRecordDimensions()
			!= SiteMetadata.GetLevelDimensions(SiteMetadata.RecordGridLevel))
	{
		OutResult.Status = EArcForestRunStatus::InvalidInput;
		OutResult.Error = TEXT("ArcForest annual site reader layout does not match the frozen site geometry.");
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		Report.SetOutcome(TEXT("annual site reader layout mismatch"));
		return false;
	}

	bool bPreprocessingCancelled = false;
	FArcEcotopeDecayGrid DecayGrid;
	if (!BuildDecayGrid(
			SiteMetadata,
			*AnnualReaderLayout,
			AnnualSiteReader,
			Cancellation,
			DecayGrid,
			bPreprocessingCancelled,
			Error))
	{
		if (bPreprocessingCancelled && IsCancellationRequested(Cancellation, Progress, OutResult))
		{
			Report.SetOutcome(TEXT("cancelled while building decay grid"));
			return false;
		}
		OutResult.Status = EArcForestRunStatus::InvalidInput;
		OutResult.Error = MoveTemp(Error);
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		Report.SetOutcome(TEXT("decay grid failed"));
		return false;
	}

	FValidBaseCellTable ValidBaseCells;
	if (!BuildValidBaseCellTable(
			SiteMetadata,
			*AnnualReaderLayout,
			AnnualSiteReader,
			Cancellation,
			ValidBaseCells,
			bPreprocessingCancelled,
			Error))
	{
		if (bPreprocessingCancelled && IsCancellationRequested(Cancellation, Progress, OutResult))
		{
			Report.SetOutcome(TEXT("cancelled while building valid base cell table"));
			return false;
		}
		OutResult.Status = EArcForestRunStatus::InvalidInput;
		OutResult.Error = MoveTemp(Error);
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		Report.SetOutcome(TEXT("valid base cell table failed"));
		return false;
	}

	FHabitatScoringState HabitatTables;
	if (!BuildHabitatScoringState(
			SiteMetadata,
			Species,
			HabitatTables,
			Error))
	{
		OutResult.Status = EArcForestRunStatus::InvalidInput;
		OutResult.Error = MoveTemp(Error);
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		Report.SetOutcome(TEXT("habitat site table failed"));
		return false;
	}
	FArcForestHabitatFactorStore FactoredHabitatFactorStore;
	FFactoredHabitatRunTelemetry FactoredHabitatRunTelemetry;
	if (!FactoredHabitatFactorStore.Build(
			SiteMetadata,
			Species,
			*AnnualReaderLayout,
			AnnualSiteReader,
			static_cast<int64>(FMath::Max(
				CVarArcForestHabitatFactorStoreBudgetMiB.GetValueOnAnyThread(), 1)) * 1024ll * 1024ll,
			FMath::Clamp(
				CVarArcForestHabitatFactorStoreMaxActiveLeases.GetValueOnAnyThread(),
				1,
				MaximumHabitatFactorTaskSlots),
			Cancellation,
			Error))
	{
		if (IsCancellationRequested(Cancellation, Progress, OutResult))
		{
			Report.SetOutcome(TEXT("cancelled while building factored habitat store"));
			return false;
		}
		OutResult.Status = EArcForestRunStatus::Failed;
		OutResult.Error = MoveTemp(Error);
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		Report.SetOutcome(TEXT("factored habitat store build failed"));
		return false;
	}
	{
		const FArcForestHabitatFactorStoreStats FactorStats = FactoredHabitatFactorStore.GetStats();
		FactoredHabitatRunTelemetry.BuiltSamples = FactorStats.BuiltSamples;
		FactoredHabitatRunTelemetry.HardZeroSamples = FactorStats.HardZeroSamples;
		FactoredHabitatRunTelemetry.HardZeroSlopeSamples = FactorStats.HardZeroSlopeSamples;
		FactoredHabitatRunTelemetry.HardZeroSunLoadSamples = FactorStats.HardZeroSunLoadSamples;
		FactoredHabitatRunTelemetry.HardZeroWindSamples = FactorStats.HardZeroWindSamples;
		FactoredHabitatRunTelemetry.HardZeroFertilitySamples = FactorStats.HardZeroFertilitySamples;
		FactoredHabitatRunTelemetry.HardZeroTemperatureSamples = FactorStats.HardZeroTemperatureSamples;
		FactoredHabitatRunTelemetry.HardZeroMoistureSamples = FactorStats.HardZeroMoistureSamples;
		FactoredHabitatRunTelemetry.ProductBytes = FactorStats.ProductBytes;
		FactoredHabitatRunTelemetry.HardZeroBytes = FactorStats.HardZeroBytes;
		FactoredHabitatRunTelemetry.BuildWallMilliseconds = FactorStats.BuildMilliseconds;
	}
#if WITH_EDITORONLY_DATA
	if (Input.SiteSource.IsValid())
	{
		AnnualSiteReader.LogBulkStats(TEXT("annual-soil-validity-habitat"));
	}
#endif
	if (IsCancellationRequested(Cancellation, Progress, OutResult))
	{
		Report.SetOutcome(TEXT("cancelled after growth preprocessing"));
		return false;
	}
	TArray<double> RecordCellAreas;
	if (!BuildRecordCellAreas(
			SiteMetadata, ValidBaseCells.BaseValidityMask, RecordCellAreas, Error))
	{
		OutResult.Status = EArcForestRunStatus::InvalidInput;
		OutResult.Error = MoveTemp(Error);
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
		Report.SetOutcome(TEXT("record cell areas failed"));
		return false;
	}

	TArray<double> DiagnosticBaseCellAreas;
	TArray<double> DiagnosticBaseCellCover;
	TArray<uint8> DiagnosticPreviousZeroStemMask;
	bool bCoverBudgetWarningEmitted = false;
	bool bPromotionChannelBufferWarningEmitted = false;

	FArcForestWorldSnapshot Candidate = Input.SourceSnapshot;
	FPromotionWorkTotals PromotionWorkTotals;
	FRegionalHabitatRouteSeedTable RegionalHabitatRouteSeeds;
	FRegionalRecruitmentRecordIndex RegionalRecruitmentRecordIndex;
	FRegionalRecruitmentEnvelopeTable RegionalRecruitmentEnvelopeTable;
	ON_SCOPE_EXIT
	{
		RegionalHabitatRouteSeeds.Empty();
		RegionalRecruitmentEnvelopeTable.Empty();
		RegionalRecruitmentRecordIndex.Empty();
	};
	FRegionalTargetRunTelemetry RegionalTargetRunTelemetry;
	FRegionalRecruitmentRecordIndexBuildResult RegionalRecruitmentRecordIndexBuild;
	FRegionalRecruitmentEnvelopeBuildResult RegionalRecruitmentEnvelopeBuild;
	FPlatformMemoryStats RegionalRecruitmentMemoryBeforeBuild = {};
	FPlatformMemoryStats RegionalRecruitmentMemoryAfterBuild = {};
	bool bRegionalRecruitmentMemoryBeforeBuildSampled = false;
	bool bRegionalRecruitmentMemoryAfterBuildSampled = false;
	FCoverHabitatProviderRunTelemetry CoverHabitatProviderRunTelemetry;
	FFactoredHabitatConsumerTelemetry FactoredHabitatConsumerTelemetry;
	const int64 RegionalRouteSeedAllowanceBytes = static_cast<int64>(FMath::Max(
		CVarArcForestRegionalRouteSeedBudgetMiB.GetValueOnAnyThread(), 1)) * 1024ll * 1024ll;
	const int64 HabitatQueryScratchAllowanceBytes = static_cast<int64>(FMath::Max(
		CVarArcForestHabitatQueryScratchBudgetMiB.GetValueOnAnyThread(), 1)) * 1024ll * 1024ll;
	const int64 HabitatFactorStoreAllowanceBytes = static_cast<int64>(FMath::Max(
		CVarArcForestHabitatFactorStoreBudgetMiB.GetValueOnAnyThread(), 1)) * 1024ll * 1024ll;
	const int64 RegionalRecruitmentRetainedAllowanceBytes = static_cast<int64>(FMath::Max(
		CVarArcForestRegionalRecruitmentRetainedBudgetMiB.GetValueOnAnyThread(), 1)) * 1024ll * 1024ll;
	const int64 RegionalRecruitmentSamplerScratchAllowanceBytes = static_cast<int64>(FMath::Max(
		CVarArcForestRegionalRecruitmentSamplerScratchBudgetMiB.GetValueOnAnyThread(), 1)) * 1024ll * 1024ll;
	const int64 ActiveRegionalRecruitmentSamplerScratchAllowanceBytes =
		RegionalRecruitmentSamplerScratchAllowanceBytes;
	const int64 ConfiguredSubsystemAllowanceBytes = RegionalRouteSeedAllowanceBytes
		+ HabitatQueryScratchAllowanceBytes
		+ HabitatFactorStoreAllowanceBytes
		+ RegionalRecruitmentRetainedAllowanceBytes
		+ ActiveRegionalRecruitmentSamplerScratchAllowanceBytes;
	const auto GetRunStatusText = [](const EArcForestRunStatus Status)
	{
		switch (Status)
		{
		case EArcForestRunStatus::Succeeded:
			return TEXT("succeeded");
		case EArcForestRunStatus::Cancelled:
			return TEXT("cancelled");
		case EArcForestRunStatus::InvalidInput:
			return TEXT("invalidInput");
		case EArcForestRunStatus::Failed:
		default:
			return TEXT("failed");
		}
	};
	const auto LogQueryConsumer = [](
		const TCHAR* RunStatus,
		const TCHAR* Consumer,
		const FFactoredHabitatQueryTelemetry& Telemetry)
	{
		UE_LOGF(
			LogArcEcotope,
			Log,
			"ArcForest grouped habitat queries | status=%ls consumer=%ls calls=%lld requests=%lld chunkAcquires=%lld"
			" seededRequests=%lld seedHits=%lld unseededFallbackRequests=%lld seedValidationFailures=%lld"
			" completedSeededWindows=%lld completedFallbackWindows=%lld completedMixedWindows=%lld"
			" batches=%lld plannedShards=%lld parallelLaunches=%lld"
			" peakQueriesPerWindow=%d peakChunksPerBatch=%d peakShardsPerBatch=%d"
			" minRoutesPerShard=%d peakRoutesPerShard=%d"
			" peakScratch=%.2f MiB routingMs=%.2f seededRoutingMs=%.2f fallbackRoutingMs=%.2f"
			" mixedRoutingMs=%.2f acquireMs=%.2f evaluationMs=%.2f wallMs=%.2f",
			RunStatus,
			Consumer,
			Telemetry.Calls,
			Telemetry.Requests,
			Telemetry.ChunkAcquires,
			Telemetry.SeededRequests,
			Telemetry.SeedHits,
			Telemetry.UnseededFallbackRequests,
			Telemetry.RouteSeedValidationFailures,
			Telemetry.CompletedSeededWindows,
			Telemetry.CompletedFallbackWindows,
			Telemetry.CompletedMixedWindows,
			Telemetry.Batches,
			Telemetry.TotalPlannedShards,
			Telemetry.ParallelLaunches,
			Telemetry.PeakQueriesPerWindow,
			Telemetry.PeakChunksPerBatch,
			Telemetry.PeakShardsPerBatch,
			Telemetry.TotalPlannedShards > 0 ? Telemetry.MinimumRoutesPerShard : 0,
			Telemetry.PeakRoutesPerShard,
			ToMebibytes(Telemetry.PeakScratchBytes),
			Telemetry.RoutingWallMilliseconds,
			Telemetry.SeededRoutingWallMilliseconds,
			Telemetry.FallbackRoutingWallMilliseconds,
			Telemetry.MixedRoutingWallMilliseconds,
			Telemetry.AcquireWallMilliseconds,
			Telemetry.EvaluationWallMilliseconds,
			Telemetry.WallMilliseconds);
	};
	ON_SCOPE_EXIT
	{
		const TCHAR* RunStatus = GetRunStatusText(OutResult.Status);
		const FPlatformMemoryStats RegionalRecruitmentRunExitMemory = FPlatformMemory::GetStats();
		UE_LOGF(
			LogArcEcotope,
			Log,
			"ArcForest regional recruitment lineage | status=%ls"
			" authoritativeSnapshotMaxProposalRounds=%d"
			" capturedCVarMaxProposalRounds=%d",
			RunStatus,
			Input.SourceSnapshot.RegionalRecruitmentConfig.MaxProposalRounds,
			Input.RegionalRecruitmentConfig.MaxProposalRounds);
		UE_LOGF(
			LogArcEcotope,
			Log,
			"ArcForest regional recruitment retained build | status=%ls"
			" recordAttempted=%d recordAdmitted=%d recordActualBytes=%lld recordBuildMs=%.2f"
			" envelopeAttempted=%d envelopeAdmitted=%d envelopeActualBytes=%lld envelopeBuildMs=%.2f"
			" combinedRetainedBytes=%lld retainedBudgetBytes=%lld",
			RunStatus,
			RegionalRecruitmentRecordIndexBuild.bAdmissionAttempted ? 1 : 0,
			RegionalRecruitmentRecordIndexBuild.bAdmitted ? 1 : 0,
			RegionalRecruitmentRecordIndexBuild.ActualBytes,
			RegionalRecruitmentRecordIndexBuild.BuildMilliseconds,
			RegionalRecruitmentEnvelopeBuild.bAdmissionAttempted ? 1 : 0,
			RegionalRecruitmentEnvelopeBuild.bAdmitted ? 1 : 0,
			RegionalRecruitmentEnvelopeBuild.ActualBytes,
			RegionalRecruitmentEnvelopeBuild.BuildMilliseconds,
			RegionalRecruitmentEnvelopeBuild.CombinedActualBytes,
			RegionalRecruitmentRetainedAllowanceBytes);
		UE_LOGF(
			LogArcEcotope,
			Log,
			"ArcForest regional recruitment process memory | status=%ls"
			" beforeBuildSampled=%d beforeBuildPhysicalBytes=%llu"
			" afterBuildSampled=%d afterBuildPhysicalBytes=%llu"
			" runExitPhysicalBytes=%llu runExitPeakPhysicalBytes=%llu",
			RunStatus,
			bRegionalRecruitmentMemoryBeforeBuildSampled ? 1 : 0,
			bRegionalRecruitmentMemoryBeforeBuildSampled
				? static_cast<uint64>(RegionalRecruitmentMemoryBeforeBuild.UsedPhysical)
				: 0ull,
			bRegionalRecruitmentMemoryAfterBuildSampled ? 1 : 0,
			bRegionalRecruitmentMemoryAfterBuildSampled
				? static_cast<uint64>(RegionalRecruitmentMemoryAfterBuild.UsedPhysical)
				: 0ull,
			static_cast<uint64>(RegionalRecruitmentRunExitMemory.UsedPhysical),
			static_cast<uint64>(RegionalRecruitmentRunExitMemory.PeakUsedPhysical));
		UE_LOGF(
				LogArcEcotope,
				Log,
				"ArcForest regional recruitment sampler run | status=%ls"
				" attemptedYears=%d completedYears=%d successYears=%d exhaustionYears=%d"
				" envelopeViolationYears=%d"
				" allocatedNonZeroMass=%lld proposals=%lld habitatQueries=%lld queryWindows=%lld"
				" accepted=%lld unresolved=%lld zeroMassGroups=%lld envelopeViolations=%lld"
				" rounds=%lld maxRetry=%d peakProjectedScratchBytes=%lld peakActualScratchBytes=%lld"
				" samplerWallMs=%.2f providerWallMs=%.2f",
				RunStatus,
				RegionalTargetRunTelemetry.LazySamplerAttemptedYearCount,
				RegionalTargetRunTelemetry.LazySamplerCompletedYearCount,
				RegionalTargetRunTelemetry.LazySamplerSuccessYearCount,
				RegionalTargetRunTelemetry.LazySamplerExhaustionYearCount,
				RegionalTargetRunTelemetry.LazySamplerEnvelopeViolationYearCount,
				RegionalTargetRunTelemetry.LazyAllocatedNonZeroMassAttempts,
				RegionalTargetRunTelemetry.LazyProposalCount,
				RegionalTargetRunTelemetry.LazyHabitatQueryCount,
				RegionalTargetRunTelemetry.LazyQueryWindowCount,
				RegionalTargetRunTelemetry.LazyAcceptedAttemptCount,
				RegionalTargetRunTelemetry.LazyUnresolvedAttemptCount,
				RegionalTargetRunTelemetry.LazyZeroMassGroupCount,
				RegionalTargetRunTelemetry.LazyEnvelopeViolationCount,
				RegionalTargetRunTelemetry.LazyCompletedRoundCount,
				RegionalTargetRunTelemetry.LazyMaximumRetryOrdinal,
				RegionalTargetRunTelemetry.LazyPeakProjectedScratchBytes,
				RegionalTargetRunTelemetry.LazyPeakActualScratchBytes,
				RegionalTargetRunTelemetry.LazySamplerWallMilliseconds,
				FactoredHabitatConsumerTelemetry.RegionalSampler.WallMilliseconds);
			for (const FRegionalTargetRunTelemetry::FLazySpeciesTotals& SpeciesTelemetry :
				RegionalTargetRunTelemetry.LazySpeciesTotals)
			{
				const double Acceptance = SpeciesTelemetry.CompletedProposalCount > 0
					? static_cast<double>(SpeciesTelemetry.AcceptedAttemptCount)
						/ static_cast<double>(SpeciesTelemetry.CompletedProposalCount)
					: 0.0;
				UE_LOGF(
					LogArcEcotope,
					Log,
					"ArcForest regional recruitment sampler species | status=%ls species=%ls speciesIndex=%d"
					" acceptance=%.6f proposals=%lld completedProposals=%lld accepted=%lld unresolved=%lld"
					" zeroMassYears=%d maxRetry=%d",
					RunStatus,
					*SpeciesTelemetry.SpeciesId.ToString(EGuidFormats::DigitsWithHyphensLower),
					SpeciesTelemetry.SpeciesIndex,
					Acceptance,
					SpeciesTelemetry.ProposalCount,
					SpeciesTelemetry.CompletedProposalCount,
					SpeciesTelemetry.AcceptedAttemptCount,
					SpeciesTelemetry.UnresolvedAttemptCount,
					SpeciesTelemetry.ZeroMassYearCount,
					SpeciesTelemetry.MaximumRetryOrdinal);
			}
		const FRegionalHabitatRouteSeedBuildResult& RegionalRouteSeedBuild =
			RegionalTargetRunTelemetry.RegionalRouteSeedBuild;
		const int64 RetainedRegionalRouteSeedBytes = RegionalRouteSeedBuild.bAdmitted
			? RegionalRouteSeedBuild.ActualBytes
			: 0;
		UE_LOGF(
			LogArcEcotope,
			Log,
			"ArcForest regional habitat route seeds | status=%ls admissionAttempted=%d admitted=%d cancelled=%d"
			" runAdmissions=%d runFallbacks=%d"
			" reason=%ls projectedBytes=%lld (%.2f MiB) actualBytes=%lld (%.2f MiB)"
			" retainedBytes=%lld (%.2f MiB) completedSeeds=%d buildMs=%.2f",
			RunStatus,
			RegionalRouteSeedBuild.bAdmissionAttempted ? 1 : 0,
			RegionalRouteSeedBuild.bAdmitted ? 1 : 0,
			RegionalRouteSeedBuild.bCancelled ? 1 : 0,
			RegionalTargetRunTelemetry.RegionalRouteSeedAdmissions,
			RegionalTargetRunTelemetry.RegionalRouteSeedFallbacks,
			*RegionalRouteSeedBuild.AdmissionReason,
			RegionalRouteSeedBuild.ProjectedBytes,
			ToMebibytes(RegionalRouteSeedBuild.ProjectedBytes),
			RegionalRouteSeedBuild.ActualBytes,
			ToMebibytes(RegionalRouteSeedBuild.ActualBytes),
			RetainedRegionalRouteSeedBytes,
			ToMebibytes(RetainedRegionalRouteSeedBytes),
			RegionalRouteSeedBuild.CompletedSeedCount,
			RegionalRouteSeedBuild.BuildMilliseconds);
		const int64 PeakHabitatQueryScratchBytes = FMath::Max(
			FMath::Max(
				FactoredHabitatConsumerTelemetry.Growth.PeakScratchBytes,
				FactoredHabitatConsumerTelemetry.LocalRecruitment.PeakScratchBytes),
			FactoredHabitatConsumerTelemetry.RegionalSampler.PeakScratchBytes);
		UE_LOGF(
			LogArcEcotope,
			Log,
			"ArcForest regional habitat subsystem allowance | status=%ls"
			" routeSeedAllowanceBytes=%lld (%.2f MiB) queryScratchAllowanceBytes=%lld (%.2f MiB)"
			" factorStoreAllowanceBytes=%lld (%.2f MiB)"
			" regionalRecruitmentRetainedAllowanceBytes=%lld (%.2f MiB)"
			" regionalRecruitmentSamplerScratchAllowanceBytes=%lld (%.2f MiB)"
			" configuredMaximumAllowanceBytes=%lld (%.2f MiB)"
			" retainedRouteSeedBytes=%lld (%.2f MiB) peakQueryScratchBytes=%lld (%.2f MiB)"
			" wholeProcessRssIncluded=0",
			RunStatus,
			RegionalRouteSeedAllowanceBytes,
			ToMebibytes(RegionalRouteSeedAllowanceBytes),
			HabitatQueryScratchAllowanceBytes,
			ToMebibytes(HabitatQueryScratchAllowanceBytes),
			HabitatFactorStoreAllowanceBytes,
			ToMebibytes(HabitatFactorStoreAllowanceBytes),
			RegionalRecruitmentRetainedAllowanceBytes,
			ToMebibytes(RegionalRecruitmentRetainedAllowanceBytes),
			ActiveRegionalRecruitmentSamplerScratchAllowanceBytes,
			ToMebibytes(ActiveRegionalRecruitmentSamplerScratchAllowanceBytes),
			ConfiguredSubsystemAllowanceBytes,
			ToMebibytes(ConfiguredSubsystemAllowanceBytes),
			RetainedRegionalRouteSeedBytes,
			ToMebibytes(RetainedRegionalRouteSeedBytes),
			PeakHabitatQueryScratchBytes,
			ToMebibytes(PeakHabitatQueryScratchBytes));
		LogQueryConsumer(RunStatus, TEXT("growth"), FactoredHabitatConsumerTelemetry.Growth);
		LogQueryConsumer(
			RunStatus,
			TEXT("localRecruitment"),
			FactoredHabitatConsumerTelemetry.LocalRecruitment);
		LogQueryConsumer(
			RunStatus,
			TEXT("regionalSampler"),
			FactoredHabitatConsumerTelemetry.RegionalSampler);
	};
	{
		const bool bRegionalRouteSeedBuildSucceeded = BuildRegionalHabitatRouteSeeds(
			SiteMetadata,
			*AnnualReaderLayout,
			ValidBaseCells,
			Cancellation,
			RegionalHabitatRouteSeeds,
			RegionalTargetRunTelemetry.RegionalRouteSeedBuild,
			Error);
		RegionalTargetRunTelemetry.RegionalRouteSeedAdmissions =
			bRegionalRouteSeedBuildSucceeded
			&& RegionalTargetRunTelemetry.RegionalRouteSeedBuild.bAdmitted
				? 1
				: 0;
		RegionalTargetRunTelemetry.RegionalRouteSeedFallbacks =
			bRegionalRouteSeedBuildSucceeded
			&& RegionalTargetRunTelemetry.RegionalRouteSeedBuild.bAdmissionAttempted
			&& !RegionalTargetRunTelemetry.RegionalRouteSeedBuild.bAdmitted
				? 1
				: 0;
		if (!bRegionalRouteSeedBuildSucceeded)
		{
			if (RegionalTargetRunTelemetry.RegionalRouteSeedBuild.bCancelled
				&& IsCancellationRequested(Cancellation, Progress, OutResult))
			{
				Report.SetOutcome(TEXT("cancelled while building regional habitat route seeds"));
				return false;
			}
			OutResult.Status = EArcForestRunStatus::Failed;
			OutResult.Error = MoveTemp(Error);
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			Report.SetOutcome(TEXT("regional habitat route seed build failed"));
			return false;
		}
	}
	{
		RegionalRecruitmentMemoryBeforeBuild = FPlatformMemory::GetStats();
		bRegionalRecruitmentMemoryBeforeBuildSampled = true;
		ON_SCOPE_EXIT
		{
			RegionalRecruitmentMemoryAfterBuild = FPlatformMemory::GetStats();
			bRegionalRecruitmentMemoryAfterBuildSampled = true;
			UE_LOGF(
				LogArcEcotope,
				Log,
				"ArcForest regional recruitment build process memory | beforePhysicalBytes=%llu (%.2f MiB)"
				" afterPhysicalBytes=%llu (%.2f MiB) peakPhysicalBytes=%llu (%.2f MiB) retainedBudgetAccounting=separate",
				static_cast<uint64>(RegionalRecruitmentMemoryBeforeBuild.UsedPhysical),
				static_cast<double>(RegionalRecruitmentMemoryBeforeBuild.UsedPhysical) / (1024.0 * 1024.0),
				static_cast<uint64>(RegionalRecruitmentMemoryAfterBuild.UsedPhysical),
				static_cast<double>(RegionalRecruitmentMemoryAfterBuild.UsedPhysical) / (1024.0 * 1024.0),
				static_cast<uint64>(RegionalRecruitmentMemoryAfterBuild.PeakUsedPhysical),
				static_cast<double>(RegionalRecruitmentMemoryAfterBuild.PeakUsedPhysical) / (1024.0 * 1024.0));
		};
		const bool bRecordIndexBuildSucceeded = BuildRegionalRecruitmentRecordIndex(
			SiteMetadata,
			ValidBaseCells,
			RegionalRecruitmentRetainedAllowanceBytes,
			Cancellation,
			RegionalRecruitmentRecordIndex,
			RegionalRecruitmentRecordIndexBuild,
			Error);
		UE_LOGF(
			LogArcEcotope,
			Log,
			"ArcForest regional recruitment record index | attempted=%d admitted=%d cancelled=%d reason=%ls"
			" records=%d entries=%d projectedBytes=%lld (%.2f MiB) actualBytes=%lld (%.2f MiB)"
			" peakScratchBytes=%lld (%.2f MiB) budgetBytes=%lld (%.2f MiB) buildMs=%.2f",
			RegionalRecruitmentRecordIndexBuild.bAdmissionAttempted ? 1 : 0,
			RegionalRecruitmentRecordIndexBuild.bAdmitted ? 1 : 0,
			RegionalRecruitmentRecordIndexBuild.bCancelled ? 1 : 0,
			*RegionalRecruitmentRecordIndexBuild.AdmissionReason,
			RegionalRecruitmentRecordIndexBuild.RecordCellCount,
			RegionalRecruitmentRecordIndexBuild.IndexedEntryCount,
			RegionalRecruitmentRecordIndexBuild.ProjectedBytes,
			ToMebibytes(RegionalRecruitmentRecordIndexBuild.ProjectedBytes),
			RegionalRecruitmentRecordIndexBuild.ActualBytes,
			ToMebibytes(RegionalRecruitmentRecordIndexBuild.ActualBytes),
			RegionalRecruitmentRecordIndexBuild.PeakScratchBytes,
			ToMebibytes(RegionalRecruitmentRecordIndexBuild.PeakScratchBytes),
			RegionalRecruitmentRecordIndexBuild.BudgetBytes,
			ToMebibytes(RegionalRecruitmentRecordIndexBuild.BudgetBytes),
			RegionalRecruitmentRecordIndexBuild.BuildMilliseconds);
		if (!bRecordIndexBuildSucceeded)
		{
			if (RegionalRecruitmentRecordIndexBuild.bCancelled
				&& IsCancellationRequested(Cancellation, Progress, OutResult))
			{
				Report.SetOutcome(TEXT("cancelled while building regional recruitment record index"));
				return false;
			}
			OutResult.Status = EArcForestRunStatus::Failed;
			OutResult.Error = MoveTemp(Error);
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			Report.SetOutcome(TEXT("regional recruitment record index build failed"));
			return false;
		}
		if (!RegionalRecruitmentRecordIndexBuild.bAdmitted)
		{
			OutResult.Status = EArcForestRunStatus::Failed;
			OutResult.Error = FString::Printf(
				TEXT("ArcForest RecordEnvelopeRejection record index was rejected (%s, projected %.2f MiB, actual %.2f MiB, budget %.2f MiB). Increase arcecotope.Simulation.RegionalRecruitmentRetainedBudgetMiB or reduce the site, then Clear State and bootstrap again."),
				*RegionalRecruitmentRecordIndexBuild.AdmissionReason,
				ToMebibytes(RegionalRecruitmentRecordIndexBuild.ProjectedBytes),
				ToMebibytes(RegionalRecruitmentRecordIndexBuild.ActualBytes),
				ToMebibytes(RegionalRecruitmentRecordIndexBuild.BudgetBytes));
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			Report.SetOutcome(TEXT("regional recruitment record index admission rejected"));
			return false;
		}

		const bool bEnvelopeBuildSucceeded = BuildRegionalRecruitmentEnvelopeTable(
			SiteMetadata,
			*AnnualReaderLayout,
			ValidBaseCells,
			RegionalRecruitmentRecordIndex,
			Species,
			HabitatTables.Scoring,
			FactoredHabitatFactorStore,
			RegionalRecruitmentRecordIndexBuild.ActualBytes,
			RegionalRecruitmentRetainedAllowanceBytes,
			Cancellation,
			RegionalRecruitmentEnvelopeTable,
			RegionalRecruitmentEnvelopeBuild,
			Error);
		UE_LOGF(
			LogArcEcotope,
			Log,
			"ArcForest regional recruitment envelope | attempted=%d admitted=%d cancelled=%d reason=%ls"
			" records=%d species=%d scannedSamples=%lld scannedChunks=%d"
			" projectedBytes=%lld (%.2f MiB) actualBytes=%lld (%.2f MiB)"
			" combinedProjectedBytes=%lld (%.2f MiB) combinedActualBytes=%lld (%.2f MiB)"
			" budgetBytes=%lld (%.2f MiB) buildMs=%.2f",
			RegionalRecruitmentEnvelopeBuild.bAdmissionAttempted ? 1 : 0,
			RegionalRecruitmentEnvelopeBuild.bAdmitted ? 1 : 0,
			RegionalRecruitmentEnvelopeBuild.bCancelled ? 1 : 0,
			*RegionalRecruitmentEnvelopeBuild.AdmissionReason,
			RegionalRecruitmentEnvelopeBuild.RecordCellCount,
			RegionalRecruitmentEnvelopeBuild.IndexedSpeciesCount,
			RegionalRecruitmentEnvelopeBuild.ScannedFactorSamples,
			RegionalRecruitmentEnvelopeBuild.ScannedChunkCount,
			RegionalRecruitmentEnvelopeBuild.ProjectedBytes,
			ToMebibytes(RegionalRecruitmentEnvelopeBuild.ProjectedBytes),
			RegionalRecruitmentEnvelopeBuild.ActualBytes,
			ToMebibytes(RegionalRecruitmentEnvelopeBuild.ActualBytes),
			RegionalRecruitmentEnvelopeBuild.CombinedProjectedBytes,
			ToMebibytes(RegionalRecruitmentEnvelopeBuild.CombinedProjectedBytes),
			RegionalRecruitmentEnvelopeBuild.CombinedActualBytes,
			ToMebibytes(RegionalRecruitmentEnvelopeBuild.CombinedActualBytes),
			RegionalRecruitmentEnvelopeBuild.BudgetBytes,
			ToMebibytes(RegionalRecruitmentEnvelopeBuild.BudgetBytes),
			RegionalRecruitmentEnvelopeBuild.BuildMilliseconds);
		if (!bEnvelopeBuildSucceeded)
		{
			if (RegionalRecruitmentEnvelopeBuild.bCancelled
				&& IsCancellationRequested(Cancellation, Progress, OutResult))
			{
				Report.SetOutcome(TEXT("cancelled while building regional recruitment envelope"));
				return false;
			}
			OutResult.Status = EArcForestRunStatus::Failed;
			OutResult.Error = MoveTemp(Error);
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			Report.SetOutcome(TEXT("regional recruitment envelope build failed"));
			return false;
		}
		if (!RegionalRecruitmentEnvelopeBuild.bAdmitted)
		{
			OutResult.Status = EArcForestRunStatus::Failed;
			OutResult.Error = FString::Printf(
				TEXT("ArcForest RecordEnvelopeRejection envelope was rejected (%s, combined projected %.2f MiB, combined actual %.2f MiB, budget %.2f MiB). Increase arcecotope.Simulation.RegionalRecruitmentRetainedBudgetMiB or reduce the site, then Clear State and bootstrap again."),
				*RegionalRecruitmentEnvelopeBuild.AdmissionReason,
				ToMebibytes(RegionalRecruitmentEnvelopeBuild.CombinedProjectedBytes),
				ToMebibytes(RegionalRecruitmentEnvelopeBuild.CombinedActualBytes),
				ToMebibytes(RegionalRecruitmentEnvelopeBuild.BudgetBytes));
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			Report.SetOutcome(TEXT("regional recruitment envelope admission rejected"));
			return false;
		}
	}
	uint64 HabitatGenerationToken = 0;
	const bool bEmitDiagnostics = Input.bEmitGrowthDiagnostics;
	const int32 DiagnosticsInterval = FMath::Max(Input.GrowthDiagnosticsIntervalYears, 1);
	/*
	 * Annual global-phase dependency and barrier contract:
	 *
	 * Stage                         | Reads                              | Writes                              | Spatial support            | RNG / reduction                         | Barrier after stage
	 * BuildCanopyAndHabitat         | Candidate, site, soil              | occupancy, habitat, cover capacity  | crown core + record halo   | none / crown contributions -> owner      | immutable start-of-year fields
	 * Fused cover batch             | Candidate, soil, pinned factors    | cover-floor capacity                | base -> owning record cell | none / none                              | capacity is frozen before lease release
	 * Year/Crown diagnostics        | Candidate, occupancy, habitat      | diagnostic snapshots/counters       | diagnostic base footprint  | none / diagnostic aggregates              | finish all readers before mutation
	 * ApplyCoverGrowthAndSpread     | Candidate patches, cover capacity  | Candidate patches                   | owning record + neighbours | keyed spread ids / live and record budget | patches committed before growth
	 * ApplyGrowth                   | Candidate, habitat, occupancy      | Candidate live records               | record-local habitat/light | none / none                              | all growth visible to mortality
	 * ApplyMortality                | Candidate, cover capacity           | Candidate, deadwood                 | owning record cell         | keyed mortality / none                    | deaths complete
	 * ApplyDisturbance              | Candidate, site metadata           | Candidate, deadwood                 | event footprint over record cells | keyed occurrence/centre/radius/kill, consumes the global deadwood-record budget ahead of thinning | footprint effects complete before thinning
	 * ApplySelfThinning             | post-mortality Candidate, decay grid | Candidate, deadwood               | owning record cell         | none / own-rooted sums, ordered removals   | thinning state complete before decay
	 * ApplyBranchShed               | surviving Candidate individuals    | deadwood                            | owning record cell         | keyed shed occurrence/placement / branch and deadwood budgets | branch records complete before decay
	 * ApplyDeadwoodDecay             | Candidate deadwood                 | Candidate deadwood                  | owning record cell         | none / none                                | deadwood state frozen for reproduction
	 * Recruitment attempt groups    | Candidate, valid base cells         | attempt groups                       | source cell / regional set | keyed demand / propagule totals            | all demand known before allocation
	 * Recruitment allocation        | attempt groups, global budgets      | allocated attempts                   | global                     | deterministic allocation / attempt budget  | allocation is frozen for draws
	 * Recruitment generation        | Candidate, habitat, occupancy       | recruitment candidates               | local destination / region | keyed draws / record-envelope proposals and destination buckets | all candidates merged before admission
	 * Recruitment admission         | candidates, cover capacity          | Candidate                             | owning target record cell  | deterministic priority / live+record caps  | admitted records visible to promotion
	 * PromoteCohorts                | Candidate                            | Candidate individuals/cohorts        | owning record cell         | deterministic ids / record budget          | representation is settled
	 * Canonicalization              | Candidate                            | revision, order, checksum            | global canonical order     | none / canonical checksum                  | publishable annual state
	 *
	 * The scheduler completes each global phase before advancing to its successor. Chunk-core work inside
	 * a phase joins at that boundary; read halos remain immutable and cross-core writes use owner reductions.
	 */
	for (int32 YearIndex = 0; YearIndex < NumYears; ++YearIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ArcForest_SimulateYear);
		const double YearStartSeconds = FPlatformTime::Seconds();
		if (IsCancellationRequested(Cancellation, Progress, OutResult))
		{
			Report.SetOutcome(FString::Printf(TEXT("cancelled after %d of %d years"), YearIndex, NumYears));
			return false;
		}
		if (Candidate.CurrentYear == MAX_int32 || Candidate.StateRevision == MAX_uint64)
		{
			OutResult.Status = EArcForestRunStatus::Failed;
			OutResult.Error = TEXT("ArcForest annual metadata cannot be incremented without overflow.");
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			Report.SetOutcome(TEXT("annual metadata overflow"));
			return false;
		}
		const int32 Year = Candidate.CurrentYear + 1;
		++HabitatGenerationToken;
		const bool bGrowthDiagnosticsEnabled =
			bEmitDiagnostics && (Year % DiagnosticsInterval == 0 || YearIndex == NumYears - 1);

		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::CanopyAndHabitat));
		FStratifiedOccupancy CellOccupancy;
		TArray<double> CanopyOwnRootedOccupancy;
		TArray<double> CoverFloorCapacity;
		TArray<double> CoverSuitability;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, CanopyAndHabitat);
			if (!BuildCanopyAndHabitat(
					Input,
					ValidBaseCells,
					RegionalRecruitmentRecordIndex,
					Species,
					SpeciesIndices,
					ChunkLayout,
					TileGatherSourceOwnerChunksByDestination,
					DecayGrid,
					RecordCellAreas,
					HabitatTables,
					*AnnualReaderLayout,
					Candidate,
					Year,
					HabitatGenerationToken,
					Cancellation,
					Diagnostics,
					FactoredHabitatFactorStore,
					FactoredHabitatRunTelemetry,
					CellOccupancy,
					bGrowthDiagnosticsEnabled ? &CanopyOwnRootedOccupancy : nullptr,
					CoverHabitatProviderRunTelemetry,
					CoverFloorCapacity,
					CoverSuitability,
					Error))
			{
				if (IsCancellationRequested(Cancellation, Progress, OutResult))
				{
					Report.SetOutcome(FString::Printf(TEXT("cancelled after %d of %d years"), YearIndex, NumYears));
					return false;
				}
				OutResult.Status = EArcForestRunStatus::Failed;
				OutResult.Error = MoveTemp(Error);
				Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
				Report.SetOutcome(TEXT("canopy and habitat failed"));
				return false;
			}
		}
		FFactoredHabitatQueryContext AnnualFactoredQueryContext;
		AnnualFactoredQueryContext.Input = &Input;
		AnnualFactoredQueryContext.Site = &SiteMetadata;
		AnnualFactoredQueryContext.Scoring = &HabitatTables.Scoring;
		AnnualFactoredQueryContext.Layout = AnnualReaderLayout;
		AnnualFactoredQueryContext.ValidBaseCells = &ValidBaseCells;
		AnnualFactoredQueryContext.RegionalRouteSeeds =
			RegionalTargetRunTelemetry.RegionalRouteSeedBuild.bAdmitted
				? &RegionalHabitatRouteSeeds
				: nullptr;
		AnnualFactoredQueryContext.Store = &FactoredHabitatFactorStore;
		AnnualFactoredQueryContext.Cancellation = &Cancellation;
		if (!AnnualFactoredQueryContext.IsValid())
		{
			OutResult.Status = EArcForestRunStatus::Failed;
			OutResult.Error = TEXT("ArcForest annual factored habitat query context was inconsistent.");
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			Report.SetOutcome(TEXT("annual factored habitat query context failed"));
			return false;
		}
		if (IsCancellationRequested(Cancellation, Progress, OutResult))
		{
			Report.SetOutcome(FString::Printf(TEXT("cancelled after %d of %d years"), YearIndex, NumYears));
			return false;
		}

		FYearDiagnostics YearStats;
		FYearCellStats YearCellStats;
		if (bGrowthDiagnosticsEnabled)
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, YearDiagnostics);
			GatherYearDiagnostics(
				Input,
				ValidBaseCells.BaseValidityMask,
				DecayGrid,
				Candidate,
				CellOccupancy,
				CanopyOwnRootedOccupancy,
				Year,
				YearStats);
			GatherYearCellStats(
				Input,
				ValidBaseCells.BaseValidityMask,
				DecayGrid,
				Candidate,
				YearStats.HabitatSpeciesIndex,
				Year,
				DiagnosticPreviousZeroStemMask,
				YearCellStats);
		}
		if (bGrowthDiagnosticsEnabled)
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, YearCrownDiagnostics);
			GatherCrownDiagnostics(
				Input,
				ValidBaseCells.BaseValidityMask,
				Species,
				SpeciesIndices,
				Candidate,
				CellOccupancy,
				DiagnosticBaseCellAreas,
				DiagnosticBaseCellCover,
				YearStats);
		}

		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::GrowthAndCompetition));
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, ApplyCoverGrowthAndSpread);
			if (!ApplyCoverGrowthAndSpread(
					Input,
					Species,
					SpeciesIndices,
					CoverFloorCapacity,
					CoverSuitability,
					Year,
					Candidate,
					bCoverBudgetWarningEmitted,
					Error))
			{
				OutResult.Status = EArcForestRunStatus::Failed;
				OutResult.Error = MoveTemp(Error);
				Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
				Report.SetOutcome(TEXT("cover growth and spread failed"));
				return false;
			}
		}
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, ApplyGrowth);
			if (!ApplyGrowth(
					Input,
					ValidBaseCells.BaseValidityMask,
					Species,
					SpeciesIndices,
					AnnualFactoredQueryContext,
					CellOccupancy,
					ParallelGrowthSettings,
					Diagnostics,
					FactoredHabitatConsumerTelemetry.Growth,
					bGrowthDiagnosticsEnabled,
					YearStats,
					Candidate,
					Error))
			{
				OutResult.Status = EArcForestRunStatus::Failed;
				OutResult.Error = MoveTemp(Error);
				Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
				Report.SetOutcome(TEXT("growth failed"));
				return false;
			}
		}
		if (bGrowthDiagnosticsEnabled)
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, YearDiagnostics);
			FinalizeYearDiagnostics(Candidate, YearStats);
			LogYearDiagnostics(YearStats);
		}
		if (IsCancellationRequested(Cancellation, Progress, OutResult))
		{
			Report.SetOutcome(FString::Printf(TEXT("cancelled after %d of %d years"), YearIndex, NumYears));
			return false;
		}

		FYearDemographics YearDemographics;
		YearDemographics.Year = Year;

		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Mortality));
		FLiveRecordCounts CountsBeforeMortality;
		CountLiveRecords(Candidate, CountsBeforeMortality);
		FDeadwoodBudgetCounters DeadwoodBudgetCounters;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, ApplyMortality);
			if (!ApplyMortality(
					Input,
					ValidBaseCells.BaseValidityMask,
					ValidBaseCells.SubstrateMobilityByBaseCell,
					Species,
					SpeciesIndices,
					CoverFloorCapacity,
					DecayGrid,
					Year,
					Diagnostics,
					bGrowthDiagnosticsEnabled,
					YearStats,
					Candidate,
					DeadwoodBudgetCounters,
					Error))
			{
				OutResult.Status = EArcForestRunStatus::Failed;
				OutResult.Error = MoveTemp(Error);
				Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
				Report.SetOutcome(TEXT("mortality failed"));
				return false;
			}
		}
		int32 DeadwoodCountBeforeDecay = 0;
		{
			FLiveRecordCounts CountsAfterMortality;
			CountLiveRecords(Candidate, CountsAfterMortality);
			DeadwoodCountBeforeDecay = CountsAfterMortality.DeadwoodCount;
			YearDemographics.IndividualDeaths =
				CountsBeforeMortality.IndividualCount - CountsAfterMortality.IndividualCount;
			YearDemographics.CohortStemDeaths =
				CountsBeforeMortality.CohortStemCount - CountsAfterMortality.CohortStemCount;
			YearDemographics.CohortRecordDeaths =
				CountsBeforeMortality.CohortCount - CountsAfterMortality.CohortCount;
			YearDemographics.PatchRecordDeaths =
				CountsBeforeMortality.PatchCount - CountsAfterMortality.PatchCount;
			YearDemographics.DeadwoodCreated =
				CountsAfterMortality.DeadwoodCount - CountsBeforeMortality.DeadwoodCount;
		}
		FArcForestDisturbanceCounts DisturbanceCounts;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, ApplyDisturbance);
			if (!ApplyDisturbance(
					Input,
					ValidBaseCells.BaseValidityMask,
					DecayGrid,
					Species,
					SpeciesIndices,
					Year,
					Diagnostics,
					bGrowthDiagnosticsEnabled,
					YearStats,
					Candidate,
					DeadwoodBudgetCounters,
					DisturbanceCounts,
					Error))
			{
				OutResult.Status = EArcForestRunStatus::Failed;
				OutResult.Error = MoveTemp(Error);
				Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
				Report.SetOutcome(TEXT("disturbance failed"));
				return false;
			}
			YearDemographics.DisturbanceEvents = DisturbanceCounts.EventsFired;
			YearDemographics.DisturbedIndividuals = DisturbanceCounts.IndividualsKilled;
			YearDemographics.DisturbedCohortStems = DisturbanceCounts.CohortStemsKilled;
			YearDemographics.DisturbedRecords = DisturbanceCounts.RecordsRemoved;
			// DeadwoodCreated is assigned from the mortality delta above and only ever added to afterwards.
			YearDemographics.DeadwoodCreated += DisturbanceCounts.DeadwoodCreated;
		}
		FSelfThinningCounts ThinningCounts;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, ApplySelfThinning);
			if (!ApplySelfThinning(
					Input,
					ValidBaseCells.BaseValidityMask,
					Species,
					SpeciesIndices,
					ChunkLayout,
					RecordCellAreas,
					DecayGrid,
					Year,
					bGrowthDiagnosticsEnabled,
					Diagnostics,
					YearStats,
					Candidate,
					DeadwoodBudgetCounters,
					ThinningCounts,
					Error))
			{
				OutResult.Status = EArcForestRunStatus::Failed;
				OutResult.Error = MoveTemp(Error);
				Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
				Report.SetOutcome(TEXT("self-thinning failed"));
				return false;
			}
			YearDemographics.ThinnedIndividuals = ThinningCounts.IndividualsRemoved;
			YearDemographics.ThinnedCohortStems = ThinningCounts.CohortStemsRemoved;
			YearDemographics.ThinnedRecords = ThinningCounts.RecordsRemoved;
			YearDemographics.DeadwoodCreated += ThinningCounts.DeadwoodCreated;
		}
		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::BranchShed));
		{
			int32 BranchesCreated = 0;
			if (!ApplyBranchShed(
					Input,
					ValidBaseCells.BaseValidityMask,
					DecayGrid,
					Year,
					Diagnostics,
					Candidate,
					DeadwoodBudgetCounters,
					BranchesCreated,
					Error))
			{
				OutResult.Status = EArcForestRunStatus::Failed;
				OutResult.Error = MoveTemp(Error);
				Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
				Report.SetOutcome(TEXT("branch shed failed"));
				return false;
			}
			YearDemographics.DeadwoodCreated += BranchesCreated;
			DeadwoodCountBeforeDecay = Candidate.Deadwood.Num();
		}
		if (DeadwoodBudgetCounters.RecordsRefusedByDeadwoodCap > 0
			|| DeadwoodBudgetCounters.RecordsRefusedByBranchCap > 0
			|| DeadwoodBudgetCounters.BranchPositionFallbacks > 0
			|| DeadwoodBudgetCounters.FallSatelliteGroupsSkipped > 0)
		{
			UE_LOGF(
				LogArcForestGrowth,
				Warning,
				"ArcForest year %d deadwood budget | %d records refused against MaxDeadwoodRecords %d,"
				" %d branch records refused against MaxBranchRecords %d, %d offsite branch positions fell"
				" back onto the parent stem, %d fall satellite groups skipped for stems that produced no"
				" fallen record",
				Year,
				DeadwoodBudgetCounters.RecordsRefusedByDeadwoodCap,
				Input.MaxDeadwoodRecords,
				DeadwoodBudgetCounters.RecordsRefusedByBranchCap,
				Input.MaxBranchRecords,
				DeadwoodBudgetCounters.BranchPositionFallbacks,
				DeadwoodBudgetCounters.FallSatelliteGroupsSkipped);
		}
		if (bGrowthDiagnosticsEnabled)
		{
			UE_LOGF(
				LogArcForestGrowth,
				VeryVerbose,
				"ArcForest year %d thinocc | ownocc[min/mean/max] %.4f %.4f %.4f postmax %.4f cap %.4f cells %d"
				" (own-rooted share of individualizing records only, before and after removal;"
				" patches shade but are not counted, so a covered tree-free cell reads 0)",
				Year,
				ThinningCounts.OwnRootedMin,
				ThinningCounts.OwnRootedMean,
				ThinningCounts.OwnRootedMax,
				ThinningCounts.OwnRootedMaxAfterRemoval,
				static_cast<double>(Input.MaximumCrownOccupancy),
				ThinningCounts.OwnRootedCellCount);
		}
		if (IsCancellationRequested(Cancellation, Progress, OutResult))
		{
			Report.SetOutcome(FString::Printf(TEXT("cancelled after %d of %d years"), YearIndex, NumYears));
			return false;
		}

		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::DeadwoodAndDecay));
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, ApplyDeadwoodDecay);
			if (!ApplyDeadwoodDecay(
					Input,
					ValidBaseCells.BaseValidityMask,
					Year,
					Diagnostics,
					Candidate,
					YearDemographics.DeadwoodRetired,
					Error))
			{
				OutResult.Status = EArcForestRunStatus::Failed;
				OutResult.Error = MoveTemp(Error);
				Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
				Report.SetOutcome(TEXT("decay failed"));
				return false;
			}
		}
		YearDemographics.DeadwoodDecayed = DeadwoodCountBeforeDecay - Candidate.Deadwood.Num();
		if (IsCancellationRequested(Cancellation, Progress, OutResult))
		{
			Report.SetOutcome(FString::Printf(TEXT("cancelled after %d of %d years"), YearIndex, NumYears));
			return false;
		}

		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Reproduction));
		TArray<FRecruitmentAttemptGroup> RecruitmentGroups;
		bool bPhaseCancelled = false;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, BuildRecruitmentAttemptGroups);
			if (!BuildRecruitmentAttemptGroups(
					Input,
					ValidBaseCells.BaseValidityMask,
					Species,
					SpeciesIndices,
					ValidBaseCells,
					Year,
					Candidate,
					Cancellation,
					Diagnostics,
					RecruitmentGroups,
					bPhaseCancelled,
					Error))
			{
				if (bPhaseCancelled && IsCancellationRequested(Cancellation, Progress, OutResult))
				{
					Report.SetOutcome(FString::Printf(TEXT("cancelled after %d of %d years"), YearIndex, NumYears));
					return false;
				}
				OutResult.Status = EArcForestRunStatus::Failed;
				OutResult.Error = MoveTemp(Error);
				Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
				Report.SetOutcome(TEXT("reproduction failed"));
				return false;
			}
		}
		int64 LiveStemsBeforeRecruitment = 0;
		int64 LiveRecordCountBeforeRecruitment = 0;
		int64 CoverRecordCountBeforeRecruitment = 0;
		int64 NonBranchDeadwoodRecordCountBeforeRecruitment = 0;
		int64 BranchRecordCountBeforeRecruitment = 0;
		int64 TotalRecordCountBeforeRecruitment = 0;
		if (!TryCountBudgets(
				Candidate,
				LiveStemsBeforeRecruitment,
				LiveRecordCountBeforeRecruitment,
				CoverRecordCountBeforeRecruitment,
				NonBranchDeadwoodRecordCountBeforeRecruitment,
				BranchRecordCountBeforeRecruitment,
				TotalRecordCountBeforeRecruitment,
				Error))
		{
			OutResult.Status = EArcForestRunStatus::Failed;
			OutResult.Error = MoveTemp(Error);
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			Report.SetOutcome(TEXT("reproduction budget check failed"));
			return false;
		}
		if (LiveStemsBeforeRecruitment > Input.MaxLiveStems)
		{
			OutResult.Status = EArcForestRunStatus::Failed;
			OutResult.Error = FString::Printf(
				TEXT("ArcForest reproduction began with LiveStems over MaxLiveStems | liveStems=%lld maxLiveStems=%d"),
				LiveStemsBeforeRecruitment,
				Input.MaxLiveStems);
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			Report.SetOutcome(TEXT("reproduction budget check failed"));
			return false;
		}
		if (LiveRecordCountBeforeRecruitment > Input.MaxLiveRecords)
		{
			OutResult.Status = EArcForestRunStatus::Failed;
			OutResult.Error = FString::Printf(
				TEXT("ArcForest reproduction began with LiveRecordCount over MaxLiveRecords | liveRecordCount=%lld maxLiveRecords=%d"),
				LiveRecordCountBeforeRecruitment,
				Input.MaxLiveRecords);
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			Report.SetOutcome(TEXT("reproduction budget check failed"));
			return false;
		}
		if (CoverRecordCountBeforeRecruitment > Input.MaxCoverRecords)
		{
			OutResult.Status = EArcForestRunStatus::Failed;
			OutResult.Error = FString::Printf(
				TEXT("ArcForest reproduction began with CoverRecordCount over MaxCoverRecords | coverRecordCount=%lld maxCoverRecords=%d"),
				CoverRecordCountBeforeRecruitment,
				Input.MaxCoverRecords);
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			Report.SetOutcome(TEXT("reproduction budget check failed"));
			return false;
		}
		if (NonBranchDeadwoodRecordCountBeforeRecruitment > Input.MaxDeadwoodRecords)
		{
			OutResult.Status = EArcForestRunStatus::Failed;
			OutResult.Error = FString::Printf(
				TEXT("ArcForest reproduction began with NonBranchDeadwoodRecordCount over MaxDeadwoodRecords | nonBranchDeadwoodRecordCount=%lld maxDeadwoodRecords=%d"),
				NonBranchDeadwoodRecordCountBeforeRecruitment,
				Input.MaxDeadwoodRecords);
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			Report.SetOutcome(TEXT("reproduction budget check failed"));
			return false;
		}
		if (BranchRecordCountBeforeRecruitment > Input.MaxBranchRecords)
		{
			OutResult.Status = EArcForestRunStatus::Failed;
			OutResult.Error = FString::Printf(
				TEXT("ArcForest reproduction began with BranchRecordCount over MaxBranchRecords | branchRecordCount=%lld maxBranchRecords=%d"),
				BranchRecordCountBeforeRecruitment,
				Input.MaxBranchRecords);
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			Report.SetOutcome(TEXT("reproduction budget check failed"));
			return false;
		}
		if (TotalRecordCountBeforeRecruitment > Input.MaxStateRecords)
		{
			OutResult.Status = EArcForestRunStatus::Failed;
			OutResult.Error = FString::Printf(
				TEXT("ArcForest reproduction began with TotalRecordCount over MaxStateRecords | totalRecordCount=%lld maxStateRecords=%d"),
				TotalRecordCountBeforeRecruitment,
				Input.MaxStateRecords);
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			Report.SetOutcome(TEXT("reproduction budget check failed"));
			return false;
		}
		const int64 RemainingLiveStemsBeforeRecruitment = FMath::Max<int64>(
			0,
			static_cast<int64>(Input.MaxLiveStems) - LiveStemsBeforeRecruitment);
		const int64 RecruitmentAttemptBudget = FMath::Min(
			RemainingLiveStemsBeforeRecruitment * static_cast<int64>(Input.RecruitmentOversamplingFactor),
			MaximumRecruitmentAttemptsPerYear);
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, AllocateRecruitmentAttempts);
			if (!AllocateRecruitmentAttempts(
					Input,
					Year,
					Candidate,
					RecruitmentAttemptBudget,
					RecruitmentGroups,
					Error))
			{
				OutResult.Status = EArcForestRunStatus::Failed;
				OutResult.Error = MoveTemp(Error);
				Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
				Report.SetOutcome(TEXT("recruitment allocation failed"));
				return false;
			}
		}
		for (const FRecruitmentAttemptGroup& Group : RecruitmentGroups)
		{
			if (Group.Source == ERecruitmentSource::Local)
			{
				YearDemographics.LocalDesiredAttempts += Group.DesiredAttempts;
				YearDemographics.LocalAllocatedAttempts += Group.AllocatedAttempts;
			}
			else
			{
				YearDemographics.RegionalDesiredAttempts += Group.DesiredAttempts;
				YearDemographics.RegionalAllocatedAttempts += Group.AllocatedAttempts;
			}
		}
		if (IsCancellationRequested(Cancellation, Progress, OutResult))
		{
			Report.SetOutcome(FString::Printf(TEXT("cancelled after %d of %d years"), YearIndex, NumYears));
			return false;
		}

		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Recruitment));
		TArray<FRecruitmentCandidate> RecruitmentCandidates;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, GenerateRecruitmentCandidates);
			if (!GenerateRecruitmentCandidates(
					Input,
					Species,
					ValidBaseCells,
					AnnualFactoredQueryContext,
					RegionalRecruitmentRecordIndex,
					RegionalRecruitmentEnvelopeTable,
					Input.RegionalRecruitmentConfig,
					CellOccupancy,
					Year,
					Candidate,
					Cancellation,
					RecruitmentGroups,
					Diagnostics,
					RegionalTargetRunTelemetry,
					FactoredHabitatConsumerTelemetry,
					RecruitmentCandidates,
					bPhaseCancelled,
					Error))
			{
				if (bPhaseCancelled && IsCancellationRequested(Cancellation, Progress, OutResult))
				{
					Report.SetOutcome(FString::Printf(TEXT("cancelled after %d of %d years"), YearIndex, NumYears));
					return false;
				}
				OutResult.Status = EArcForestRunStatus::Failed;
				OutResult.Error = MoveTemp(Error);
				Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
				Report.SetOutcome(TEXT("recruitment generation failed"));
				return false;
			}
		}
		int64 GeneratedRecruitmentStems = 0;
		for (const FRecruitmentCandidate& RecruitmentCandidate : RecruitmentCandidates)
		{
			GeneratedRecruitmentStems += RecruitmentCandidate.StemCount;
		}
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, AdmitRecruitment);
			if (!AdmitRecruitment(
					Input,
					Species,
					SpeciesIndices,
					CoverFloorCapacity,
					Year,
					RecruitmentCandidates,
					Cancellation,
					Diagnostics,
					bGrowthDiagnosticsEnabled,
					YearStats,
					Candidate,
					bPhaseCancelled,
					Error))
			{
				if (bPhaseCancelled && IsCancellationRequested(Cancellation, Progress, OutResult))
				{
					Report.SetOutcome(FString::Printf(TEXT("cancelled after %d of %d years"), YearIndex, NumYears));
					return false;
				}
				OutResult.Status = EArcForestRunStatus::Failed;
				OutResult.Error = MoveTemp(Error);
				Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
				Report.SetOutcome(TEXT("recruitment admission failed"));
				return false;
			}
		}
		int64 LiveStemsAfterRecruitment = 0;
		int64 LiveRecordCountAfterRecruitment = 0;
		int64 CoverRecordCountAfterRecruitment = 0;
		int64 NonBranchDeadwoodRecordCountAfterRecruitment = 0;
		int64 BranchRecordCountAfterRecruitment = 0;
		int64 TotalRecordCountAfterRecruitment = 0;
		FString CountError;
		const bool bLiveStemCountAvailable =
			TryCountBudgets(
				Candidate,
				LiveStemsAfterRecruitment,
				LiveRecordCountAfterRecruitment,
				CoverRecordCountAfterRecruitment,
				NonBranchDeadwoodRecordCountAfterRecruitment,
				BranchRecordCountAfterRecruitment,
				TotalRecordCountAfterRecruitment,
				CountError);
		if (IsCancellationRequested(Cancellation, Progress, OutResult))
		{
			Report.SetOutcome(FString::Printf(TEXT("cancelled after %d of %d years"), YearIndex, NumYears));
			return false;
		}

		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::CohortPromotion));
		const int32 IndividualCountBeforePromotion = Candidate.Individuals.Num();
		const FPromotionWorkTotals PromotionWorkTotalsBeforeYear = PromotionWorkTotals;
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, PromoteCohorts);
			// AdmitRecruitment's unconditional MergeAfter is the canonical-state barrier required here.
			if (!PromoteCanonicalCohorts(
					Input,
					ValidBaseCells,
					Species,
					SpeciesIndices,
					Year,
					Cancellation,
					Diagnostics,
					PromotionWorkTotals,
					Candidate,
					bPromotionChannelBufferWarningEmitted,
					bPhaseCancelled,
					Error))
			{
				if (bPhaseCancelled && IsCancellationRequested(Cancellation, Progress, OutResult))
				{
					Report.SetOutcome(FString::Printf(TEXT("cancelled after %d of %d years"), YearIndex, NumYears));
					return false;
				}
				OutResult.Status = EArcForestRunStatus::Failed;
				OutResult.Error = MoveTemp(Error);
				Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
				Report.SetOutcome(TEXT("promotion failed"));
				return false;
			}
		}
		if (bGrowthDiagnosticsEnabled)
		{
			FRecruitmentPromotionFlow& Flow = YearStats.PromotionRecruitmentFlow;
			Flow.CohortsCrossingIndividualization = PromotionWorkTotals.EligibleBeforeBudgetFilter
				- PromotionWorkTotalsBeforeYear.EligibleBeforeBudgetFilter;
			Flow.PromotionsGranted = PromotionWorkTotals.PromotedCohorts
				- PromotionWorkTotalsBeforeYear.PromotedCohorts;
			Flow.SaturatedMultiStemSkipped = PromotionWorkTotals.SaturatedMultiStemSkipped
				- PromotionWorkTotalsBeforeYear.SaturatedMultiStemSkipped;
			Flow.PromotionsRefusedChannelBuffer = PromotionWorkTotals.ChannelBufferSkippedCohorts
				- PromotionWorkTotalsBeforeYear.ChannelBufferSkippedCohorts;
		}
		YearDemographics.Promotions = Candidate.Individuals.Num() - IndividualCountBeforePromotion;
		if (IsCancellationRequested(Cancellation, Progress, OutResult))
		{
			Report.SetOutcome(FString::Printf(TEXT("cancelled after %d of %d years"), YearIndex, NumYears));
			return false;
		}

		Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Canonicalization));
		Candidate.CurrentYear = Year;
		++Candidate.StateRevision;
		const FString LiveStemsText = bLiveStemCountAvailable
			? FString::Printf(TEXT("%lld"), LiveStemsAfterRecruitment)
			: FString::Printf(TEXT("? (%s)"), *CountError);
		const FString AdmittedText = bLiveStemCountAvailable
			? FString::Printf(TEXT("%lld"), LiveStemsAfterRecruitment - LiveStemsBeforeRecruitment)
			: FString(TEXT("?"));
		const FString DeadwoodText = bLiveStemCountAvailable
			? FString::Printf(TEXT("%lld"), NonBranchDeadwoodRecordCountAfterRecruitment)
			: FString::Printf(TEXT("? (%s)"), *CountError);
		const FString BranchesText = bLiveStemCountAvailable
			? FString::Printf(TEXT("%lld"), BranchRecordCountAfterRecruitment)
			: FString::Printf(TEXT("? (%s)"), *CountError);
		UE_LOGF(
			LogArcEcotope,
			Log,"Simulate year %d: %.2f ms | individuals=%d cohorts=%d patches=%d deadwood=%ls branches=%ls retired=%d liveStems=%ls | recruitment generated=%lld admitted=%ls",
			Year,
			UE::ArcForest::GetElapsedMilliseconds(YearStartSeconds),
			Candidate.Individuals.Num(),
			Candidate.Cohorts.Num(),
			Candidate.Patches.Num(),
			*DeadwoodText,
			*BranchesText,
			YearDemographics.DeadwoodRetired,
			*LiveStemsText,
			GeneratedRecruitmentStems,
			*AdmittedText);
		YearDemographics.GeneratedStems = GeneratedRecruitmentStems;
		YearDemographics.bAdmittedStemsMeasured = bLiveStemCountAvailable;
		YearDemographics.AdmittedStems = bLiveStemCountAvailable
			? LiveStemsAfterRecruitment - LiveStemsBeforeRecruitment
			: 0;
		LogYearDemographics(YearDemographics);
		if (bGrowthDiagnosticsEnabled)
		{
			ARCFOREST_SCOPED_PHASE(Diagnostics, YearDiagnostics);
			FinalizeYearCellStats(Input, RecruitmentGroups, RecruitmentCandidates, YearCellStats);
			LogYearCellStats(YearCellStats);
			LogYearSizeAgeGrowthMortality(Input, Year, YearStats);
			LogYearPromotionRecruitmentFlow(Year, YearStats.PromotionRecruitmentFlow);
			if (bLiveStemCountAvailable)
			{
				LogYearBudgetHeadroom(
					Input,
					Year,
					FMath::Max<int64>(0, static_cast<int64>(Input.MaxLiveStems) - LiveStemsAfterRecruitment),
					FMath::Max<int64>(0, static_cast<int64>(Input.MaxLiveRecords) - LiveRecordCountAfterRecruitment),
					FMath::Max<int64>(0, static_cast<int64>(Input.MaxCoverRecords) - CoverRecordCountAfterRecruitment),
					FMath::Max<int64>(
						0, static_cast<int64>(Input.MaxDeadwoodRecords) - NonBranchDeadwoodRecordCountAfterRecruitment),
					FMath::Max<int64>(
						0, static_cast<int64>(Input.MaxBranchRecords) - BranchRecordCountAfterRecruitment));
			}
		}
		if (IsCancellationRequested(Cancellation, Progress, OutResult))
		{
			Report.SetOutcome(FString::Printf(TEXT("cancelled after %d of %d years"), YearIndex, NumYears));
			return false;
		}
		Progress.CompletedYears.Store(YearIndex + 1);
	}
	UE_LOGF(
		LogArcEcotope,
		Log,
		"ArcForest promotion work | saturatedYears=%d eligibleBeforeFilter=%lld saturatedMultiStemSkipped=%lld"
		" promotedCohorts=%lld promotedStems=%lld singletonPromotions=%lld ordinalsScanned=%lld"
		" plantableChildren=%lld surfaceEntryLookups=%lld channelBufferSkippedCohorts=%lld",
		PromotionWorkTotals.SaturatedYears,
		PromotionWorkTotals.EligibleBeforeBudgetFilter,
		PromotionWorkTotals.SaturatedMultiStemSkipped,
		PromotionWorkTotals.PromotedCohorts,
		PromotionWorkTotals.PromotedStems,
		PromotionWorkTotals.SingletonPromotions,
		PromotionWorkTotals.OrdinalsScanned,
		PromotionWorkTotals.PlantableChildren,
		PromotionWorkTotals.SurfaceEntryLookups,
		PromotionWorkTotals.ChannelBufferSkippedCohorts);

	// Validate consumes the Id ordering and checksum that only CanonicalizeAndRefreshChecksum establishes.
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Canonicalization);
		Candidate.CanonicalizeAndRefreshChecksum();
	}

	FText SnapshotError;
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, CandidateValidation);
		if (Candidate.Validate(
				SnapshotError,
				Input.SettingsHash,
				&EcologyHashes) != EArcForestSimulationStateStatus::Valid)
		{
			OutResult.Status = EArcForestRunStatus::Failed;
			OutResult.Error = FString::Printf(
				TEXT("ArcForest annual candidate validation failed: %s"),
				*SnapshotError.ToString());
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			Report.SetOutcome(TEXT("candidate validation failed"));
			return false;
		}
		FArcForestWorldSiteValidationStats CandidateSiteStats;
		if (!Candidate.ValidateSiteCompatibility(
				SiteMetadata,
				*AnnualReaderLayout,
				AnnualSiteReader,
				SiteCompatibilityScheduleBudgetBytes,
				CandidateSiteStats,
				SnapshotError))
		{
			OutResult.Status = EArcForestRunStatus::Failed;
			OutResult.Error = FString::Printf(
				TEXT("ArcForest annual candidate site compatibility failed: %s"),
				*SnapshotError.ToString());
			Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Failed));
			Report.SetOutcome(TEXT("candidate site compatibility failed"));
			return false;
		}
	}

	OutResult.Status = EArcForestRunStatus::Succeeded;
	OutResult.Error.Reset();
	OutResult.YearsSimulated = NumYears;
	OutResult.Snapshot = MoveTemp(Candidate);
	Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Complete));
	UE_LOGF(
		LogArcEcotope,
		Log,
		"ArcForest cover habitat provider | recordCells=%lld speciesCells=%lld"
		" fusedBatches=%lld reusedPinnedChunks=%lld additionalChunkAcquires=0"
		" peakChunksPerBatch=%d wallMs=%.2f",
		CoverHabitatProviderRunTelemetry.RecordCells,
		CoverHabitatProviderRunTelemetry.SpeciesCells,
		CoverHabitatProviderRunTelemetry.EvaluationBatches,
		CoverHabitatProviderRunTelemetry.ReusedChunkLeases,
		CoverHabitatProviderRunTelemetry.PeakChunksPerBatch,
		CoverHabitatProviderRunTelemetry.WallMilliseconds);
	UE_LOGF(
		LogArcEcotope,
		Log,
		"ArcForest habitat production | status=SUCCESS authoritative=FactoredChunkStore"
		" evaluationBatches=%lld evaluationWallMs=%.2f factorPrimaryYears=%lld"
		" productionOnlyYears=%lld"
		" forwardYears=%lld reverseYears=%lld peakChunksPerBatch=%d"
		" factorBuildWallMs=%.2f"
		" nextBlockers=ValidBaseCellTable+RecordCellState",
		FactoredHabitatRunTelemetry.EvaluationBatchCount,
		FactoredHabitatRunTelemetry.EvaluationWallMilliseconds,
		FactoredHabitatRunTelemetry.FactorPrimaryYears,
		FactoredHabitatRunTelemetry.ProductionOnlyYears,
		FactoredHabitatRunTelemetry.ForwardTraversalYears,
		FactoredHabitatRunTelemetry.ReverseTraversalYears,
		FactoredHabitatRunTelemetry.PeakChunksPerBatch,
		FactoredHabitatRunTelemetry.BuildWallMilliseconds);
	{
		const FArcForestHabitatFactorStoreStats FactorStoreStats = FactoredHabitatFactorStore.GetStats();
		UE_LOGF(
			LogArcEcotope,
			Log,
			"ArcForest habitat factor store | builtSamples=%lld hardZeroSamples=%lld"
			" hardZeroBy[slope/sunLoad/wind/fertility/temperature/moisture]=%lld %lld %lld %lld %lld %lld"
			" productBytes=%lld (%.2f MiB) hardZeroBytes=%lld (%.2f MiB)"
			" backingBytes=%lld (%.2f MiB)"
			" cacheBudgetBytes=%lld (%.2f MiB) residentBytes=%lld pinnedBytes=%lld scratchBytes=%lld"
			" peakResidentBytes=%lld peakPinnedBytes=%lld peakScratchBytes=%lld peakAccountedPayloadBytes=%lld"
			" acquire=%llu loads=%llu hits=%llu misses=%llu evictions=%llu failed=%llu"
			" forwardCacheHits=%lld reverseCacheHits=%lld forwardPayloadLoads=%lld reversePayloadLoads=%lld"
			" readerOpenAttempts=%llu readerOpenSuccess=%llu readerOpenFailures=%llu"
			" readerInvalidations=%llu readerOpenMs=%.2f"
			" activeLeases=%d peakActiveLeases=%d maxActiveLeases=%d buildMs=%.2f loadMs=%.2f",
			FactoredHabitatRunTelemetry.BuiltSamples,
			FactoredHabitatRunTelemetry.HardZeroSamples,
			FactoredHabitatRunTelemetry.HardZeroSlopeSamples,
			FactoredHabitatRunTelemetry.HardZeroSunLoadSamples,
			FactoredHabitatRunTelemetry.HardZeroWindSamples,
			FactoredHabitatRunTelemetry.HardZeroFertilitySamples,
			FactoredHabitatRunTelemetry.HardZeroTemperatureSamples,
			FactoredHabitatRunTelemetry.HardZeroMoistureSamples,
			FactoredHabitatRunTelemetry.ProductBytes,
			ToMebibytes(FactoredHabitatRunTelemetry.ProductBytes),
			FactoredHabitatRunTelemetry.HardZeroBytes,
			ToMebibytes(FactoredHabitatRunTelemetry.HardZeroBytes),
			FactorStoreStats.BackingBytes,
			ToMebibytes(FactorStoreStats.BackingBytes),
			FactorStoreStats.CacheBudgetBytes,
			ToMebibytes(FactorStoreStats.CacheBudgetBytes),
			FactorStoreStats.ResidentBytes,
			FactorStoreStats.PinnedBytes,
			FactorStoreStats.ScratchBytes,
			FactorStoreStats.PeakResidentBytes,
			FactorStoreStats.PeakPinnedBytes,
			FactorStoreStats.PeakScratchBytes,
			FactorStoreStats.PeakAccountedPayloadBytes,
			FactorStoreStats.AcquireCount,
			FactorStoreStats.PayloadLoadCount,
			FactorStoreStats.CacheHitCount,
			FactorStoreStats.CacheMissCount,
			FactorStoreStats.EvictionCount,
			FactorStoreStats.FailedAcquireCount,
			FactoredHabitatRunTelemetry.ForwardCacheHitCount,
			FactoredHabitatRunTelemetry.ReverseCacheHitCount,
			FactoredHabitatRunTelemetry.ForwardPayloadLoadCount,
			FactoredHabitatRunTelemetry.ReversePayloadLoadCount,
			FactorStoreStats.BackingReaderOpenAttemptCount,
			FactorStoreStats.BackingReaderOpenSuccessCount,
			FactorStoreStats.BackingReaderOpenFailureCount,
			FactorStoreStats.BackingReaderInvalidationCount,
			FactorStoreStats.BackingReaderOpenMilliseconds,
			FactorStoreStats.ActiveLeases,
			FactorStoreStats.PeakActiveLeases,
			FactorStoreStats.MaxActiveLeases,
			FactorStoreStats.BuildMilliseconds,
			FactorStoreStats.LoadMilliseconds);
	}
	const FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
	UE_LOGF(
		LogArcEcotope,
		Log,"ArcForest Simulate memory | usedPhysical=%.2fMiB peakUsedPhysical=%.2fMiB",
		static_cast<double>(MemoryStats.UsedPhysical) / (1024.0 * 1024.0),
		static_cast<double>(MemoryStats.PeakUsedPhysical) / (1024.0 * 1024.0));
	Report.SetOutcome(FString::Printf(TEXT("succeeded, %d years"), NumYears));
	UE::ArcForest::LogSharedHelperCounterDelta(TEXT("Simulate"), HelperBaseline);
	return true;
}
