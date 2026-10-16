// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/ArcForestBootstrap.h"

enum class EArcForestGrowthPhase : int32
{
	Idle,
	Validation,
	CanopyAndHabitat,
	GrowthAndCompetition,
	Mortality,
	BranchShed,
	DeadwoodAndDecay,
	Reproduction,
	Recruitment,
	CohortPromotion,
	Canonicalization,
	Complete,
	Cancelled,
	Failed,
};

/** Worker-safe cancellation state. It owns no callbacks or UObject references. */
class ARCECOTOPE_API FArcForestCancellationToken
{
public:
	void RequestCancellation();
	bool IsCancellationRequested() const;

private:
	TAtomic<bool> bCancellationRequested = false;
};

/** Atomics polled by the game thread while a worker owns the simulation call. */
struct ARCECOTOPE_API FArcForestProgressState
{
	TAtomic<int32> Phase = static_cast<int32>(EArcForestGrowthPhase::Idle);
	TAtomic<int32> CompletedYears = 0;
	TAtomic<int32> TotalYears = 0;

	void Reset(int32 InTotalYears);
	EArcForestGrowthPhase GetPhase() const;
};

/** Complete immutable annual-simulation input. Contains no UObject references. */
struct ARCECOTOPE_API FArcForestRunInput
{
	FGuid VolumeId;
	int32 Seed = 0;
	/** Execution-only chunk core width in base cells. This is deliberately outside SettingsHash. */
	int32 ChunkBaseCellSpan = 256;
	int32 MaxLiveStems = 0;
	int32 MaxLiveRecords = 0;
	int32 RecruitmentOversamplingFactor = 0;
	int32 MaxCoverRecords = 0;
	int32 MaxStateRecords = 0;
	int32 MaxDeadwoodRecords = 0;
	int32 MaxBranchRecords = 0;
	bool bEmitGrowthDiagnostics = true;
	int32 GrowthDiagnosticsIntervalYears = 20;
	uint64 SettingsHash = 0;
	FArcForestRegionalRecruitmentConfig RegionalRecruitmentConfig;
	double MaximumCrownOccupancy = 1.3;
	/** Extinction coefficient in light = exp(-k * occupancy); shades only above a receiver's crown base. */
	double UnderstoryShade = 1.0;
	double GroundCoverDensity = 1.0;
	double GroundCoverPatchiness = 0.6;
	FArcEcotopeDeadwoodSettings DeadwoodSettings;
	FArcForestDisturbanceSettings DisturbanceSettings;
	/** Authoritative immutable site storage used by the worker. */
	FArcEcotopeFrozenSiteSourcePtr SiteSource;
	TArray<FArcForestFrozenSpeciesSetup> SpeciesSetups;
	FArcForestWorldSnapshot SourceSnapshot;
};

/** Checked projection of active simulation payloads. It allocates no working buffers. */
struct ARCECOTOPE_API FArcForestCapacityEstimate
{
	FIntPoint BaseGridDimensions = FIntPoint::ZeroValue;
	FIntPoint RecordGridDimensions = FIntPoint::ZeroValue;
	FIntPoint ChunkGridDimensions = FIntPoint::ZeroValue;
	int32 ChunkBaseCellSpan = 0;
	int32 RecordHaloCells = 9;
	int32 PlannedWorkerSlots = 0;
	int64 BaseCellCount = 0;
	int64 RecordCellCount = 0;
	int64 SpeciesCount = 0;
	int64 RegionalSpeciesCount = 0;
	int64 ChunkCount = 0;
	int64 SourceRecordCount = 0;
	int64 EstimatedCandidatePayloadBytes = 0;
	int64 MaxChunkCoreBaseCells = 0;
	int64 MaxChunkReadBaseCells = 0;
	int64 MaxChunkCoreHabitatSamples = 0;
	int64 MaxChunkCoreHabitatBytes = 0;
	int64 PlannedActiveHabitatCoreBytes = 0;
};

enum class EArcForestRunStatus : uint8
{
	Succeeded,
	Cancelled,
	InvalidInput,
	Failed,
};

/**
 * Published only as a whole. Failure and cancellation results contain no
 * candidate snapshot, so they cannot accidentally become authoritative.
 */
struct ARCECOTOPE_API FArcForestRunResult
{
	EArcForestRunStatus Status = EArcForestRunStatus::Failed;
	FString Error;
	int32 YearsSimulated = 0;
	FArcForestWorldSnapshot Snapshot;
};

class ARCECOTOPE_API FArcForestGrowthEngine
{
public:
	/** Estimates representable capacity without allocating simulation working buffers. */
	static bool EstimateCapacity(
		const FArcForestRunInput& Input,
		FArcForestCapacityEstimate& OutEstimate,
		FString& OutError);

	/**
	 * Runs complete annual steps over a private snapshot copy. The source input
	 * is never modified, and OutResult never contains a partial candidate.
	 */
	static bool Simulate(
		const FArcForestRunInput& Input,
		int32 NumYears,
		const FArcForestCancellationToken& Cancellation,
		FArcForestProgressState& Progress,
		FArcForestRunResult& OutResult);
};
