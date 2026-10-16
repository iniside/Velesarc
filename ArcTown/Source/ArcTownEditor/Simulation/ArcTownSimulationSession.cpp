// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcTownSimulationSession.h"

#include "Engine/World.h"
#include "Misc/ScopeLock.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ScopedTransaction.h"
#include "Simulation/ArcTownDeterministic.h"
#include "Simulation/ArcTownGrowthEngine.h"
#include "Simulation/ArcTownSimulationState.h"
#include "Town/ArcTownVolume.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "ArcTownSimulationSession"

DEFINE_LOG_CATEGORY(LogArcTownSimulation);

void FArcTownCancellationToken::RequestCancellation()
{
	bCancellationRequested.Store(true);
}

bool FArcTownCancellationToken::IsCancellationRequested() const
{
	return bCancellationRequested.Load();
}

void FArcTownProgressState::Reset(const int32 InTotalYears)
{
	Phase.Store(static_cast<int32>(EArcTownSimulationPhase::Idle));
	CompletedYears.Store(0);
	TotalYears.Store(InTotalYears);
}

EArcTownSimulationPhase FArcTownProgressState::GetPhase() const
{
	return static_cast<EArcTownSimulationPhase>(Phase.Load());
}

struct FArcTownSimulationSession::FSharedWorkerState
{
	FArcTownCancellationToken Cancellation;
	FArcTownProgressState Progress;
	FCriticalSection ResultMutex;
	TOptional<FArcTownRunResult> PublishedResult;
};

FArcTownSimulationSession::FArcTownSimulationSession(
	AArcTownVolume& InOwner,
	const uint64 InOperationGeneration,
	FArcTownSimulationInput&& InInput,
	const FName InWorldPackageName)
	: Owner(&InOwner)
	, CapturedState(InOwner.SimulationState)
	, CapturedWorld(InOwner.GetWorld())
	, OperationGeneration(InOperationGeneration)
	, CapturedSimulationId(InInput.SimulationId)
	, CapturedSeed(InInput.Seed)
	, CapturedSiteSnapshotHash(InInput.SiteSnapshotHash)
	, CapturedSettingsHash(InInput.SettingsHash)
	, CapturedWorldPackageName(InWorldPackageName)
	, SharedState(MakeShared<FSharedWorkerState, ESPMode::ThreadSafe>())
{
	check(IsInGameThread());

	if (const UArcTownSimulationState* const State = CapturedState.Get())
	{
		BaselineStateChecksum = State->Snapshot.StateChecksum;
	}

	SharedState->Progress.Reset(InInput.YearsToSimulate);

	WorkerTask = UE::Tasks::Launch(
		UE_SOURCE_LOCATION,
		[WorkerState = SharedState, Input = MoveTemp(InInput)]() mutable
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ArcTown_SessionWorker);

			FArcTownRunResult Result;
			if (WorkerState->Cancellation.IsCancellationRequested())
			{
				Result.Status = EArcTownRunStatus::Cancelled;
				WorkerState->Progress.Phase.Store(static_cast<int32>(EArcTownSimulationPhase::Cancelled));
			}
			else
			{
				WorkerState->Progress.Phase.Store(static_cast<int32>(EArcTownSimulationPhase::Simulating));

				FArcTownWorldSnapshot Snapshot;
				FArcTownTrafficYearReport FinalYearTraffic;
				FText Error;
				const bool bSucceeded = ArcTownGrowthEngine::Run(
					Input,
					[&WorkerState](const int32 CompletedYears) -> bool
					{
						WorkerState->Progress.CompletedYears.Store(CompletedYears);
						return !WorkerState->Cancellation.IsCancellationRequested();
					},
					Snapshot,
					FinalYearTraffic,
					Error);

				if (WorkerState->Cancellation.IsCancellationRequested())
				{
					Result.Status = EArcTownRunStatus::Cancelled;
					WorkerState->Progress.Phase.Store(static_cast<int32>(EArcTownSimulationPhase::Cancelled));
				}
				else if (!bSucceeded)
				{
					Result.Status = EArcTownRunStatus::Failed;
					Result.Error = Error.ToString();
					WorkerState->Progress.Phase.Store(static_cast<int32>(EArcTownSimulationPhase::Failed));
				}
				else
				{
					Result.Status = EArcTownRunStatus::Succeeded;
					Result.Snapshot = MoveTemp(Snapshot);
					Result.FinalYearTraffic = FinalYearTraffic;
					WorkerState->Progress.CompletedYears.Store(WorkerState->Progress.TotalYears.Load());
					WorkerState->Progress.Phase.Store(static_cast<int32>(EArcTownSimulationPhase::Complete));
				}
			}

			FScopeLock ResultLock(&WorkerState->ResultMutex);
			WorkerState->PublishedResult.Emplace(MoveTemp(Result));
		});
	bWorkerLaunched = true;
}

FArcTownSimulationSession::~FArcTownSimulationSession()
{
	CancelAndWait();
}

void FArcTownSimulationSession::RequestCancellation()
{
	SharedState->Cancellation.RequestCancellation();
	if (Status == EArcTownSimulationSessionStatus::Running)
	{
		Status = EArcTownSimulationSessionStatus::Cancelling;
	}
}

void FArcTownSimulationSession::CancelAndWait()
{
	RequestCancellation();
	if (bWorkerLaunched)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ArcTown_SessionCancelAndWait);
		WorkerTask.Wait();
		bWorkerLaunched = false;
	}
	if (Status == EArcTownSimulationSessionStatus::Running
		|| Status == EArcTownSimulationSessionStatus::Cancelling)
	{
		Status = EArcTownSimulationSessionStatus::Cancelled;
	}
}

bool FArcTownSimulationSession::IsWorkerComplete() const
{
	return bWorkerLaunched && WorkerTask.IsCompleted();
}

bool FArcTownSimulationSession::IsOwnedBy(const AArcTownVolume* const Candidate) const
{
	return Owner.Get() == Candidate;
}

bool FArcTownSimulationSession::IsOwnerUsable() const
{
	const AArcTownVolume* const CurrentOwner = Owner.Get();
	return IsValid(CurrentOwner) && !CurrentOwner->IsActorBeingDestroyed();
}

FArcTownSimulationSessionSnapshot FArcTownSimulationSession::GetSnapshot() const
{
	FArcTownSimulationSessionSnapshot Snapshot;
	Snapshot.Status = Status;
	Snapshot.Phase = SharedState->Progress.GetPhase();
	Snapshot.CompletedYears = SharedState->Progress.CompletedYears.Load();
	Snapshot.TotalYears = SharedState->Progress.TotalYears.Load();
	Snapshot.Error = Error;
	Snapshot.FinalYearTraffic = FinalYearTraffic;
	Snapshot.OperationGeneration = OperationGeneration;
	return Snapshot;
}

void FArcTownSimulationSession::FinalizeOnGameThread(const uint64 CurrentOperationGeneration)
{
	check(IsInGameThread());
	if (!IsWorkerComplete())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcTown_FinalizeOnGameThread);
	WorkerTask.Wait();
	bWorkerLaunched = false;

	FArcTownRunResult Result;
	{
		FScopeLock ResultLock(&SharedState->ResultMutex);
		if (!SharedState->PublishedResult.IsSet())
		{
			SetTerminalStatus(
				EArcTownSimulationSessionStatus::Failed,
				TEXT("The town simulation worker finished without publishing a result."));
			return;
		}
		Result = MoveTemp(SharedState->PublishedResult.GetValue());
		SharedState->PublishedResult.Reset();
	}

	// A cancelled run commits nothing: a half-finished year would validate and then be simulated on.
	if (Result.Status == EArcTownRunStatus::Cancelled
		|| SharedState->Cancellation.IsCancellationRequested())
	{
		SetTerminalStatus(EArcTownSimulationSessionStatus::Cancelled, FString());
		return;
	}
	if (Result.Status != EArcTownRunStatus::Succeeded)
	{
		SetTerminalStatus(
			EArcTownSimulationSessionStatus::Failed,
			Result.Error.IsEmpty() ? TEXT("The town simulation worker failed.") : MoveTemp(Result.Error));
		return;
	}

	AArcTownVolume* const CurrentOwner = Owner.Get();
	FString StaleReason;
	if (!IsValid(CurrentOwner)
		|| !IsCommitStillEligible(*CurrentOwner, CurrentOperationGeneration, StaleReason))
	{
		SetTerminalStatus(
			EArcTownSimulationSessionStatus::Stale,
			StaleReason.IsEmpty() ? TEXT("The town this run belongs to no longer exists.") : MoveTemp(StaleReason));
		return;
	}

	FText ValidationError;
	if (!Result.Snapshot.Validate(CurrentOwner->SiteSnapshot, ValidationError))
	{
		SetTerminalStatus(
			EArcTownSimulationSessionStatus::Failed,
			FString::Printf(
				TEXT("The town simulation worker produced an unusable state: %s"),
				*ValidationError.ToString()));
		return;
	}

	UArcTownSimulationState* const CurrentState = CurrentOwner->SimulationState;
	check(CurrentState != nullptr);

	const FScopedTransaction Transaction(LOCTEXT("CommitSimulation", "Commit ArcTown Simulation"));
	CurrentOwner->Modify();
	CurrentState->Modify();
	CurrentState->Snapshot = MoveTemp(Result.Snapshot);

	FinalYearTraffic = Result.FinalYearTraffic;
	SetTerminalStatus(EArcTownSimulationSessionStatus::Succeeded, FString());

	UE_LOGF(LogArcTownSimulation, Log,"ArcTown committed '%ls' at year %d: %d node(s), %d road(s), %d building(s).",
		*CurrentOwner->GetName(),
		CurrentState->Snapshot.Year,
		CurrentState->Snapshot.Nodes.Num(),
		CurrentState->Snapshot.Edges.Num(),
		CurrentState->Snapshot.Buildings.Num());
}

// Refuses a commit onto a town the author changed while the worker ran: newer run, replaced state, re-sampled site, edited settings, reloaded world.
bool FArcTownSimulationSession::IsCommitStillEligible(
	const AArcTownVolume& CurrentOwner,
	const uint64 CurrentOperationGeneration,
	FString& OutReason) const
{
	if (CurrentOperationGeneration != OperationGeneration)
	{
		OutReason = TEXT("This town simulation result was replaced by a newer run.");
		return false;
	}

	const UArcTownSimulationState* const CurrentState = CurrentOwner.SimulationState;
	if (CurrentState == nullptr || CurrentState != CapturedState.Get())
	{
		OutReason = TEXT("This town's simulation state object changed while the worker was running.");
		return false;
	}
	if (CurrentState->Snapshot.StateChecksum != BaselineStateChecksum)
	{
		OutReason = TEXT("This town's simulation state changed while the worker was running.");
		return false;
	}

	const UWorld* const CurrentWorld = CurrentOwner.GetWorld();
	if (CurrentWorld == nullptr
		|| CurrentWorld != CapturedWorld.Get()
		|| CurrentWorld->GetPackage() == nullptr
		|| CurrentWorld->GetPackage()->GetFName() != CapturedWorldPackageName)
	{
		OutReason = TEXT("This town's world changed while the worker was running.");
		return false;
	}

	FArcTownSimulationInput CurrentInput;
	FText FreezeError;
	if (!CurrentOwner.FreezeSimulationInput(1, CurrentInput, FreezeError))
	{
		OutReason = FString::Printf(
			TEXT("This town can no longer be simulated, so the finished run was dropped: %s"),
			*FreezeError.ToString());
		return false;
	}
	if (CurrentInput.SimulationId != CapturedSimulationId || CurrentInput.Seed != CapturedSeed)
	{
		OutReason = TEXT("This town's identity or seed changed while the worker was running.");
		return false;
	}
	if (CurrentInput.SiteSnapshotHash != CapturedSiteSnapshotHash)
	{
		OutReason = TEXT("This town's sampled terrain changed while the worker was running.");
		return false;
	}
	if (CurrentInput.SettingsHash != CapturedSettingsHash)
	{
		OutReason = TEXT("This town's growth settings changed while the worker was running.");
		return false;
	}

	OutReason.Reset();
	return true;
}

void FArcTownSimulationSession::SetTerminalStatus(
	const EArcTownSimulationSessionStatus InStatus,
	FString&& InError)
{
	Status = InStatus;
	Error = MoveTemp(InError);

	const bool bIsStale = InStatus == EArcTownSimulationSessionStatus::Stale;
	UE_CLOGF(!Error.IsEmpty() && bIsStale, LogArcTownSimulation, Warning,"%ls", *Error);
	UE_CLOGF(!Error.IsEmpty() && !bIsStale, LogArcTownSimulation, Error,"%ls", *Error);
}

#undef LOCTEXT_NAMESPACE
