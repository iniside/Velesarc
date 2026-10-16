// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/ArcTownSimulationInput.h"
#include "Simulation/ArcTownTraffic.h"
#include "Simulation/ArcTownWorldSnapshot.h"
#include "Tasks/Task.h"
#include "Templates/Atomic.h"

class AArcTownVolume;
class UArcTownSimulationState;
class UWorld;

DECLARE_LOG_CATEGORY_EXTERN(LogArcTownSimulation, Log, All);

enum class EArcTownSimulationSessionStatus : uint8
{
	Running,
	Cancelling,
	Succeeded,
	Cancelled,
	Failed,
	Stale,
};

enum class EArcTownSimulationPhase : int32
{
	Idle,
	Simulating,
	Complete,
	Cancelled,
	Failed,
};

enum class EArcTownRunStatus : uint8
{
	Succeeded,
	Cancelled,
	Failed,
};

class FArcTownCancellationToken
{
public:
	void RequestCancellation();
	bool IsCancellationRequested() const;

private:
	TAtomic<bool> bCancellationRequested = false;
};

struct FArcTownProgressState
{
	TAtomic<int32> Phase = static_cast<int32>(EArcTownSimulationPhase::Idle);
	TAtomic<int32> CompletedYears = 0;
	TAtomic<int32> TotalYears = 0;

	void Reset(int32 InTotalYears);
	EArcTownSimulationPhase GetPhase() const;
};

struct FArcTownRunResult
{
	EArcTownRunStatus Status = EArcTownRunStatus::Failed;
	FString Error;
	FArcTownWorldSnapshot Snapshot;
	FArcTownTrafficYearReport FinalYearTraffic;
};

struct FArcTownSimulationSessionSnapshot
{
	EArcTownSimulationSessionStatus Status = EArcTownSimulationSessionStatus::Failed;
	EArcTownSimulationPhase Phase = EArcTownSimulationPhase::Idle;
	int32 CompletedYears = 0;
	int32 TotalYears = 0;
	FString Error;
	FArcTownTrafficYearReport FinalYearTraffic;
	uint64 OperationGeneration = 0;
};

class FArcTownSimulationSession
{
public:
	FArcTownSimulationSession(
		AArcTownVolume& InOwner,
		uint64 InOperationGeneration,
		FArcTownSimulationInput&& InInput,
		FName InWorldPackageName);
	~FArcTownSimulationSession();

	void RequestCancellation();
	void CancelAndWait();

	bool IsWorkerComplete() const;
	bool IsOwnedBy(const AArcTownVolume* Candidate) const;
	bool IsOwnerUsable() const;
	FArcTownSimulationSessionSnapshot GetSnapshot() const;

	void FinalizeOnGameThread(uint64 CurrentOperationGeneration);

private:
	struct FSharedWorkerState;

	bool IsCommitStillEligible(
		const AArcTownVolume& CurrentOwner,
		uint64 CurrentOperationGeneration,
		FString& OutReason) const;
	void SetTerminalStatus(EArcTownSimulationSessionStatus InStatus, FString&& InError);

	TWeakObjectPtr<AArcTownVolume> Owner;
	TWeakObjectPtr<UArcTownSimulationState> CapturedState;
	TWeakObjectPtr<UWorld> CapturedWorld;
	uint64 OperationGeneration = 0;
	uint64 BaselineStateChecksum = 0;
	FGuid CapturedSimulationId;
	int32 CapturedSeed = 0;
	uint64 CapturedSiteSnapshotHash = 0;
	uint64 CapturedSettingsHash = 0;
	FName CapturedWorldPackageName;

	TSharedRef<FSharedWorkerState, ESPMode::ThreadSafe> SharedState;
	UE::Tasks::TTask<void> WorkerTask;
	bool bWorkerLaunched = false;
	EArcTownSimulationSessionStatus Status = EArcTownSimulationSessionStatus::Running;
	FString Error;
	FArcTownTrafficYearReport FinalYearTraffic;
};
