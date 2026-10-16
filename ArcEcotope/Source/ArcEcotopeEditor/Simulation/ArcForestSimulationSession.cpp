// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcForestSimulationSession.h"

#include "ArcEcotopeModule.h"
#include "Determinism/ArcForestDeterministic.h"
#include "Diagnostics/ArcForestDiagnostics.h"
#include "Foliage/ArcEcotopeFoliageFields.h"
#include "HAL/IConsoleManager.h"
#include "ScopedTransaction.h"
#include "Serialization/ArcEcotopeCustomVersion.h"
#include "Simulation/ArcEcotopeInstanceBuilder.h"
#include "Simulation/ArcForestSimulationState.h"
#include "Simulation/ArcEcotopeVolume.h"
#include "Simulation/ArcEcotopeSiteChunkReader.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "ArcForestSimulationSession"

namespace ArcForestSimulationSessionPrivate
{
	bool EcologyHashesEqual(
		const TArray<FArcForestSpeciesEcologyHash>& Left,
		const TArray<FArcForestSpeciesEcologyHash>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index].SpeciesId != Right[Index].SpeciesId
				|| Left[Index].EcologyHash != Right[Index].EcologyHash)
			{
				return false;
			}
		}
		return true;
	}

	FArcForestRunInput MakeRunInput(
		FArcForestBootstrapInput&& BootstrapInput,
		FArcForestWorldSnapshot&& InitialSnapshot,
		const int32 ChunkBaseCellSpan)
	{
		FArcForestRunInput RunInput;
		RunInput.VolumeId = BootstrapInput.VolumeId;
		RunInput.Seed = BootstrapInput.Seed;
		RunInput.ChunkBaseCellSpan = ChunkBaseCellSpan;
		RunInput.MaxLiveStems = BootstrapInput.MaxLiveStems;
		RunInput.MaxLiveRecords = BootstrapInput.MaxLiveRecords;
		RunInput.RecruitmentOversamplingFactor = BootstrapInput.RecruitmentOversamplingFactor;
		RunInput.MaxCoverRecords = BootstrapInput.MaxCoverRecords;
		RunInput.MaxStateRecords = BootstrapInput.MaxStateRecords;
		RunInput.MaxDeadwoodRecords = BootstrapInput.MaxDeadwoodRecords;
		RunInput.MaxBranchRecords = BootstrapInput.MaxBranchRecords;
		RunInput.bEmitGrowthDiagnostics = BootstrapInput.bEmitGrowthDiagnostics;
		RunInput.GrowthDiagnosticsIntervalYears = BootstrapInput.GrowthDiagnosticsIntervalYears;
		RunInput.SettingsHash = BootstrapInput.SettingsHash;
		RunInput.RegionalRecruitmentConfig = BootstrapInput.RegionalRecruitmentConfig;
		RunInput.MaximumCrownOccupancy = BootstrapInput.MaximumCrownOccupancy;
		RunInput.UnderstoryShade = BootstrapInput.UnderstoryShade;
		RunInput.GroundCoverDensity = BootstrapInput.GroundCoverDensity;
		RunInput.GroundCoverPatchiness = BootstrapInput.GroundCoverPatchiness;
		RunInput.DeadwoodSettings = BootstrapInput.DeadwoodSettings;
		RunInput.DisturbanceSettings = BootstrapInput.DisturbanceSettings;
		RunInput.SiteSource = MoveTemp(BootstrapInput.SiteSource);
		RunInput.SpeciesSetups = MoveTemp(BootstrapInput.SpeciesSetups);
		RunInput.SourceSnapshot = MoveTemp(InitialSnapshot);
		return RunInput;
	}

}

struct FArcForestSimulationSession::FSharedWorkerState
{
	FArcForestCancellationToken Cancellation;
	FArcForestProgressState Progress;
	FCriticalSection ResultMutex;
	TOptional<FArcForestRunResult> PublishedResult;
};

FArcForestSimulationSession::FArcForestSimulationSession(
	AArcEcotopeVolume& InOwner,
	const uint64 InOperationGeneration,
	const FArcForestStartRequest& Request,
	FArcForestBootstrapInput&& InBootstrapInput,
	const FName InWorldPackageName)
	: Owner(&InOwner)
	, CapturedState(InOwner.SimulationState)
	, CapturedWorld(InOwner.GetWorld())
	, OperationGeneration(InOperationGeneration)
	, CapturedSiteSnapshotHash(InBootstrapInput.SiteSource.IsValid()
		? InBootstrapInput.SiteSource->Metadata.SiteSnapshotHash : 0)
	, CapturedSettingsHash(InBootstrapInput.SettingsHash)
	, CapturedVolumeId(InBootstrapInput.VolumeId)
	, CapturedSeed(InBootstrapInput.Seed)
	, CapturedWorldPackageName(InWorldPackageName)
	, SharedState(MakeShared<FSharedWorkerState, ESPMode::ThreadSafe>())
{
	check(IsInGameThread());
	check(Request.StartMode == InBootstrapInput.StartMode);

	CapturedEcologyHashes = InOwner.BuildEnabledSpeciesEcologyHashes();

	if (const UArcForestSimulationState* State = CapturedState.Get())
	{
		BaselineStateRevision = State->Snapshot.StateRevision;
		BaselineStateChecksum = State->Snapshot.StateChecksum;
		BaselineSourceCustomVersion = State->SourceCustomVersion;
		BaselineAssociatedWorldPackageName = State->AssociatedWorldPackageName;
		BaselineAssociatedSiteSnapshotHash = State->AssociatedSiteSnapshotHash;
	}

	SharedState->Progress.Reset(Request.AnnualStepsAfterInitialization);

	const int32 NumYears = Request.AnnualStepsAfterInitialization;
	const int32 RunChunkBaseCellSpan = InOwner.ChunkBaseCellSpan;
	WorkerTask = UE::Tasks::Launch(
		UE_SOURCE_LOCATION,
		[WorkerState = SharedState, BootstrapInput = MoveTemp(InBootstrapInput), NumYears, RunChunkBaseCellSpan]() mutable
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ArcForest_SessionWorker);
			const double WorkerStartSeconds = FPlatformTime::Seconds();
			FArcForestRunResult Result;
			if (WorkerState->Cancellation.IsCancellationRequested())
			{
				Result.Status = EArcForestRunStatus::Cancelled;
				WorkerState->Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Cancelled));
			}
			else
			{
				WorkerState->Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Validation));
				FArcForestWorldSnapshot InitialSnapshot;
				FText BootstrapError;
				if (!FArcForestBootstrap::CreateInitialSnapshot(
						BootstrapInput,
						InitialSnapshot,
						BootstrapError,
						&WorkerState->Cancellation))
				{
					if (WorkerState->Cancellation.IsCancellationRequested())
					{
						Result.Status = EArcForestRunStatus::Cancelled;
						WorkerState->Progress.Phase.Store(
							static_cast<int32>(EArcForestGrowthPhase::Cancelled));
					}
					else
					{
						Result.Status = EArcForestRunStatus::InvalidInput;
						Result.Error = BootstrapError.ToString();
						WorkerState->Progress.Phase.Store(
							static_cast<int32>(EArcForestGrowthPhase::Failed));
					}
				}
				else if (WorkerState->Cancellation.IsCancellationRequested())
				{
					Result.Status = EArcForestRunStatus::Cancelled;
					WorkerState->Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Cancelled));
				}
				else if (NumYears == 0)
				{
					Result.Status = EArcForestRunStatus::Succeeded;
					Result.Snapshot = MoveTemp(InitialSnapshot);
					WorkerState->Progress.Phase.Store(static_cast<int32>(EArcForestGrowthPhase::Complete));
				}
				else
				{
					FArcForestRunInput RunInput =
						ArcForestSimulationSessionPrivate::MakeRunInput(
							MoveTemp(BootstrapInput),
							MoveTemp(InitialSnapshot),
							RunChunkBaseCellSpan);
					FArcForestGrowthEngine::Simulate(
						RunInput,
						NumYears,
						WorkerState->Cancellation,
						WorkerState->Progress,
						Result);
				}
			}

			UE_LOGF(
				LogArcEcotope,
				Log,"Session worker finished in %.2f ms (status=%d, years=%d)",
				UE::ArcForest::GetElapsedMilliseconds(WorkerStartSeconds),
				static_cast<int32>(Result.Status),
				Result.YearsSimulated);
			UE_CLOGF(
				Result.Status != EArcForestRunStatus::Succeeded
					&& Result.Status != EArcForestRunStatus::Cancelled,
				LogArcEcotope,
				Error,"Session worker terminated with an error: %ls",
				Result.Error.IsEmpty()
					? TEXT("ArcForest worker failed without an error message.")
					: *Result.Error);

			FScopeLock ResultLock(&WorkerState->ResultMutex);
			WorkerState->PublishedResult.Emplace(MoveTemp(Result));
		});
	bWorkerLaunched = true;
}

FArcForestSimulationSession::~FArcForestSimulationSession()
{
	CancelAndWait();
}

void FArcForestSimulationSession::Launch()
{
	check(IsInGameThread());
	check(bWorkerLaunched);
}

void FArcForestSimulationSession::RequestCancellation()
{
	SharedState->Cancellation.RequestCancellation();
	if (Status == EArcForestSimulationSessionStatus::Running)
	{
		Status = EArcForestSimulationSessionStatus::Cancelling;
	}
}

void FArcForestSimulationSession::CancelAndWait()
{
	RequestCancellation();
	if (bWorkerLaunched)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ArcForest_SessionCancelAndWait);
		const double WaitStartSeconds = FPlatformTime::Seconds();
		WorkerTask.Wait();
		bWorkerLaunched = false;
		UE_LOGF(
			LogArcEcotope,
			Log,"CancelAndWait: %ls of %.2f ms waiting for the ArcForest worker",
			IsInGameThread() ? TEXT("game-thread stall") : TEXT("non-game-thread wait"),
			UE::ArcForest::GetElapsedMilliseconds(WaitStartSeconds));
	}
	if (Status == EArcForestSimulationSessionStatus::Running
		|| Status == EArcForestSimulationSessionStatus::Cancelling)
	{
		Status = EArcForestSimulationSessionStatus::Cancelled;
	}
}

bool FArcForestSimulationSession::IsWorkerComplete() const
{
	return bWorkerLaunched && WorkerTask.IsCompleted();
}

bool FArcForestSimulationSession::IsOwnedBy(const AArcEcotopeVolume* Candidate) const
{
	return Owner.Get() == Candidate;
}

bool FArcForestSimulationSession::IsOwnerUsable() const
{
	const AArcEcotopeVolume* CurrentOwner = Owner.Get();
	return IsValid(CurrentOwner) && !CurrentOwner->IsActorBeingDestroyed();
}

uint64 FArcForestSimulationSession::GetOperationGeneration() const
{
	return OperationGeneration;
}

FArcForestSimulationSessionSnapshot FArcForestSimulationSession::GetSnapshot() const
{
	FArcForestSimulationSessionSnapshot Snapshot;
	Snapshot.Status = Status;
	Snapshot.Phase = SharedState->Progress.GetPhase();
	Snapshot.CompletedYears = SharedState->Progress.CompletedYears.Load();
	Snapshot.TotalYears = SharedState->Progress.TotalYears.Load();
	Snapshot.Error = Error;
	Snapshot.OperationGeneration = OperationGeneration;
	return Snapshot;
}

void FArcForestSimulationSession::FinalizeOnGameThread(const uint64 CurrentOperationGeneration)
{
	check(IsInGameThread());
	if (!IsWorkerComplete())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcForest_FinalizeOnGameThread);
	UE::ArcForest::FDiagnosticScopeReport Report(TEXT("ArcForest FinalizeOnGameThread"));
	UE::ArcForest::FDiagnosticPhaseTotals& Diagnostics = Report.GetPhases();
	const UE::ArcForest::FSharedHelperCounters HelperBaseline = UE::ArcForest::CaptureSharedHelperCounters();

	WorkerTask.Wait();
	bWorkerLaunched = false;

	FArcForestRunResult Result;
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Finalize_TakeResult);
		FScopeLock ResultLock(&SharedState->ResultMutex);
		if (!SharedState->PublishedResult.IsSet())
		{
			SetTerminalStatus(
				EArcForestSimulationSessionStatus::Failed,
				TEXT("ArcForest worker completed without publishing a result."));
			Report.SetOutcome(TEXT("worker published no result"));
			return;
		}
		Result = MoveTemp(SharedState->PublishedResult.GetValue());
		SharedState->PublishedResult.Reset();
	}

	if (Result.Status == EArcForestRunStatus::Cancelled
		|| SharedState->Cancellation.IsCancellationRequested())
	{
		SetTerminalStatus(EArcForestSimulationSessionStatus::Cancelled, FString());
		Report.SetOutcome(TEXT("cancelled"));
		return;
	}
	if (Result.Status != EArcForestRunStatus::Succeeded)
	{
		SetTerminalStatus(
			EArcForestSimulationSessionStatus::Failed,
			Result.Error.IsEmpty() ? TEXT("ArcForest worker failed.") : MoveTemp(Result.Error));
		Report.SetOutcome(TEXT("worker failed"));
		return;
	}

	AArcEcotopeVolume* CurrentOwner = Owner.Get();
	FString StaleReason;
	if (!IsValid(CurrentOwner)
		|| !IsCommitStillEligible(*CurrentOwner, CurrentOperationGeneration, StaleReason))
	{
		SetTerminalStatus(
			EArcForestSimulationSessionStatus::Stale,
			StaleReason.IsEmpty() ? TEXT("ArcForest result owner no longer exists.") : MoveTemp(StaleReason));
		Report.SetOutcome(TEXT("stale"));
		return;
	}

	FText CandidateError;
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Finalize_ValidateSnapshot);
		if (Result.Snapshot.Seed != CapturedSeed
			|| Result.Snapshot.SimulationId != UE::ArcForest::MakeSimulationId(CapturedVolumeId, CapturedSeed)
			|| Result.Snapshot.Validate(
				CandidateError,
				CapturedSettingsHash,
				&CapturedEcologyHashes) != EArcForestSimulationStateStatus::Valid)
		{
			SetTerminalStatus(
				EArcForestSimulationSessionStatus::Failed,
				FString::Printf(
					TEXT("ArcForest worker published an invalid candidate: %s"),
					CandidateError.IsEmpty() ? TEXT("identity mismatch") : *CandidateError.ToString()));
			Report.SetOutcome(TEXT("invalid candidate"));
			return;
		}
	}

	FArcEcotopePreparedInstances PreparedInstances;
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Finalize_InstanceBuilderBuild);
		if (!FArcEcotopeInstanceBuilder::Build(*CurrentOwner, Result.Snapshot, PreparedInstances, CandidateError))
		{
			FArcEcotopeInstanceBuilder::CommitFailedBakeRecords(*CurrentOwner, PreparedInstances);
			SetTerminalStatus(
				EArcForestSimulationSessionStatus::Failed,
				FString::Printf(TEXT("ArcForest could not prepare native instances: %s"), *CandidateError.ToString()));
			Report.SetOutcome(TEXT("instance build failed"));
			return;
		}
	}

	UArcForestSimulationState* CurrentState = CurrentOwner->SimulationState;
	check(CurrentState != nullptr);
	const double TransactionStartSeconds = FPlatformTime::Seconds();
	const FScopedTransaction Transaction(LOCTEXT("CommitSimulation", "Commit ArcForest Simulation"));
	Diagnostics.AddSample(
		TEXT("Finalize_OpenTransaction"),
		UE::ArcForest::GetElapsedMilliseconds(TransactionStartSeconds));
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Finalize_ModifyForUndo);
		CurrentOwner->Modify();
		CurrentState->Modify();
	}
	FArcEcotopeInstanceBuilder::CommitEmittedActorChanges(*CurrentOwner, PreparedInstances);
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Finalize_AdoptPreparedInstanceComponents);
		if (!FArcEcotopeInstanceBuilder::AdoptPreparedInstances(
				*CurrentOwner, Result.Snapshot, PreparedInstances, CandidateError))
		{
			SetTerminalStatus(
				EArcForestSimulationSessionStatus::Failed,
				FString::Printf(TEXT("ArcForest could not adopt native instances: %s"), *CandidateError.ToString()));
			Report.SetOutcome(TEXT("adoption failed"));
			return;
		}
	}
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Finalize_ApplyCommittedSnapshot);
		CurrentState->ApplyCommittedSnapshot(MoveTemp(Result.Snapshot), CapturedWorldPackageName);
	}
	{
		ARCFOREST_SCOPED_PHASE(Diagnostics, Finalize_DeriveFoliageField);
		FText FieldError;
		if (!ArcEcotopeFoliageFields::DeriveFieldPayload(
				*CurrentOwner, CurrentOwner->FoliageFieldPayload, FieldError))
		{
			CurrentOwner->FoliageFieldPayload.Reset();
			UE_LOGF(LogArcEcotope, Warning,"[FoliageFields] volume='%ls' has no forest field after this commit, so its ground foliage will not draw: %ls",
				*CurrentOwner->GetName(), *FieldError.ToString());
		}
	}
	SetTerminalStatus(EArcForestSimulationSessionStatus::Succeeded, FString());
	Report.SetOutcome(TEXT("committed"));
	UE::ArcForest::LogSharedHelperCounterDelta(TEXT("FinalizeOnGameThread"), HelperBaseline);
}

bool FArcForestSimulationSession::IsCommitStillEligible(
	const AArcEcotopeVolume& CurrentOwner,
	const uint64 CurrentOperationGeneration,
	FString& OutReason) const
{
	using namespace ArcForestSimulationSessionPrivate;

	const UArcForestSimulationState* CurrentState = CurrentOwner.SimulationState;
	if (CurrentOperationGeneration != OperationGeneration)
	{
		OutReason = TEXT("ArcForest result was replaced by a newer operation.");
		return false;
	}
	if (CurrentState == nullptr || CurrentState != CapturedState.Get())
	{
		OutReason = TEXT("ArcForest simulation state owner changed while the worker was running.");
		return false;
	}
	if (CurrentState->Snapshot.StateRevision != BaselineStateRevision
		|| CurrentState->Snapshot.StateChecksum != BaselineStateChecksum
		|| CurrentState->SourceCustomVersion != BaselineSourceCustomVersion
		|| CurrentState->AssociatedWorldPackageName != BaselineAssociatedWorldPackageName
		|| CurrentState->AssociatedSiteSnapshotHash != BaselineAssociatedSiteSnapshotHash)
	{
		OutReason = TEXT("ArcForest authoritative state changed while the worker was running.");
		return false;
	}
	if (CurrentOwner.VolumeId != CapturedVolumeId || CurrentOwner.Seed != CapturedSeed)
	{
		OutReason = TEXT("ArcForest VolumeId or seed changed while the worker was running.");
		return false;
	}
	if (CurrentOwner.GetActiveSiteMetadata().SiteSnapshotHash != CapturedSiteSnapshotHash)
	{
		OutReason = TEXT("ArcForest baked site changed while the worker was running.");
		return false;
	}
	if (CurrentOwner.ComputeSettingsHash() != CapturedSettingsHash)
	{
		OutReason = TEXT("ArcForest simulation settings changed while the worker was running.");
		return false;
	}
	if (!EcologyHashesEqual(CurrentOwner.BuildEnabledSpeciesEcologyHashes(), CapturedEcologyHashes))
	{
		OutReason = TEXT("ArcForest species ecology changed while the worker was running.");
		return false;
	}

	const UWorld* CurrentWorld = CurrentOwner.GetWorld();
	if (CurrentWorld == nullptr
		|| CurrentWorld != CapturedWorld.Get()
		|| CurrentWorld->GetPackage()->GetFName() != CapturedWorldPackageName)
	{
		OutReason = TEXT("ArcForest world association changed while the worker was running.");
		return false;
	}

	OutReason.Reset();
	return true;
}

void FArcForestSimulationSession::SetTerminalStatus(
	const EArcForestSimulationSessionStatus InStatus,
	FString&& InError)
{
	Status = InStatus;
	Error = MoveTemp(InError);
}

#undef LOCTEXT_NAMESPACE
