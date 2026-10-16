// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/ArcForestBootstrap.h"
#include "Simulation/ArcForestGrowthEngine.h"
#include "Tasks/Task.h"

class AArcEcotopeVolume;
class UArcForestSimulationState;
class UWorld;

enum class EArcForestSimulationSessionStatus : uint8
{
	Running,
	Cancelling,
	Succeeded,
	Cancelled,
	Failed,
	Stale,
};

struct ARCECOTOPEEDITOR_API FArcForestSimulationSessionSnapshot
{
	EArcForestSimulationSessionStatus Status = EArcForestSimulationSessionStatus::Failed;
	EArcForestGrowthPhase Phase = EArcForestGrowthPhase::Idle;
	int32 CompletedYears = 0;
	int32 TotalYears = 0;
	FString Error;
	uint64 OperationGeneration = 0;
};

/**
 * One editor-owned asynchronous operation. UObject references and commit
 * eligibility stay on the game thread; the worker receives only frozen values.
 */
class ARCECOTOPEEDITOR_API FArcForestSimulationSession
{
public:
	FArcForestSimulationSession(
		AArcEcotopeVolume& InOwner,
		uint64 InOperationGeneration,
		const FArcForestStartRequest& Request,
		FArcForestBootstrapInput&& InBootstrapInput,
		FName InWorldPackageName);
	~FArcForestSimulationSession();

	void Launch();
	void RequestCancellation();
	void CancelAndWait();

	bool IsWorkerComplete() const;
	bool IsOwnedBy(const AArcEcotopeVolume* Candidate) const;
	bool IsOwnerUsable() const;
	uint64 GetOperationGeneration() const;
	FArcForestSimulationSessionSnapshot GetSnapshot() const;

	/** Called only by the game-thread controller after task completion. */
	void FinalizeOnGameThread(uint64 CurrentOperationGeneration);

private:
	struct FSharedWorkerState;

	bool IsCommitStillEligible(
		const AArcEcotopeVolume& CurrentOwner,
		uint64 CurrentOperationGeneration,
		FString& OutReason) const;
	void SetTerminalStatus(EArcForestSimulationSessionStatus InStatus, FString&& InError);

	TWeakObjectPtr<AArcEcotopeVolume> Owner;
	TWeakObjectPtr<UArcForestSimulationState> CapturedState;
	TWeakObjectPtr<UWorld> CapturedWorld;
	uint64 OperationGeneration = 0;
	uint64 BaselineStateRevision = 0;
	uint64 BaselineStateChecksum = 0;
	int32 BaselineSourceCustomVersion = 0;
	FName BaselineAssociatedWorldPackageName;
	uint64 BaselineAssociatedSiteSnapshotHash = 0;
	uint64 CapturedSiteSnapshotHash = 0;
	uint64 CapturedSettingsHash = 0;
	TArray<FArcForestSpeciesEcologyHash> CapturedEcologyHashes;
	FGuid CapturedVolumeId;
	int32 CapturedSeed = 0;
	FName CapturedWorldPackageName;

	TSharedRef<FSharedWorkerState, ESPMode::ThreadSafe> SharedState;
	UE::Tasks::TTask<void> WorkerTask;
	bool bWorkerLaunched = false;
	EArcForestSimulationSessionStatus Status = EArcForestSimulationSessionStatus::Running;
	FString Error;
};
