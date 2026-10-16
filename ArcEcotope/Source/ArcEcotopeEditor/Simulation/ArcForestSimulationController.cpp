// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcForestSimulationController.h"

#include "ArcEcotopeModule.h"
#include "Diagnostics/ArcForestDiagnostics.h"
#include "Simulation/ArcEcotopeVolume.h"
#include "UObject/Package.h"

void FArcForestSimulationController::Register()
{
	check(IsInGameThread());
	check(!TickerHandle.IsValid());
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FArcForestSimulationController::Tick));
}

void FArcForestSimulationController::Unregister()
{
	check(IsInGameThread());
	CancelAndReleaseActiveSession();
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
}

bool FArcForestSimulationController::Start(
	AArcEcotopeVolume& Owner,
	const FArcForestStartRequest& Request,
	FText& OutError)
{
	check(IsInGameThread());
	CancelAndReleaseActiveSession();

	UE_LOGF(
		LogArcEcotope,
		Log,"ArcForest bootstrap: Controller Start invoked for '%ls' (StartMode=%d, AnnualStepsAfterInitialization=%d).",
		*Owner.GetName(),
		static_cast<int32>(Request.StartMode),
		Request.AnnualStepsAfterInitialization);

	if (static_cast<uint8>(Request.StartMode) > static_cast<uint8>(EArcForestStartMode::ContinueSnapshot))
	{
		OutError = FText::FromString(TEXT("ArcForest simulation start mode is invalid."));
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcForest bootstrap: Controller Start rejected StartMode=%d for '%ls'.",
			static_cast<int32>(Request.StartMode),
			*Owner.GetName());
		return false;
	}
	if (Request.AnnualStepsAfterInitialization < 0
		|| (Request.StartMode == EArcForestStartMode::ContinueSnapshot
			&& Request.AnnualStepsAfterInitialization == 0))
	{
		OutError = FText::FromString(
			TEXT("ArcForest simulation year count is invalid for the selected start mode."));
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcForest bootstrap: Controller Start rejected AnnualStepsAfterInitialization=%d for StartMode=%d on '%ls'.",
			Request.AnnualStepsAfterInitialization,
			static_cast<int32>(Request.StartMode),
			*Owner.GetName());
		return false;
	}

	if (Owner.SimulationState == nullptr)
	{
		OutError = FText::FromString(TEXT("ArcForest simulation state object is missing."));
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcForest bootstrap: Controller Start refused for '%ls': SimulationState is null.",
			*Owner.GetName());
		return false;
	}

	UWorld* World = Owner.GetWorld();
	if (World == nullptr || World->GetPackage() == nullptr)
	{
		OutError = FText::FromString(TEXT("ArcForest simulation requires an associated editor world."));
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcForest bootstrap: Controller Start refused for '%ls': World=%ls, Package=%ls.",
			*Owner.GetName(),
			World == nullptr ? TEXT("null") : TEXT("valid"),
			(World != nullptr && World->GetPackage() != nullptr) ? TEXT("valid") : TEXT("null"));
		return false;
	}

	FArcForestBootstrapInput BootstrapInput;
	if (!Owner.FreezeBootstrapInput(Request.StartMode, BootstrapInput, OutError))
	{
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcForest bootstrap: Controller Start refused for '%ls': FreezeBootstrapInput failed: %ls.",
			*Owner.GetName(),
			*OutError.ToString());
		return false;
	}

	if (OperationGeneration == MAX_uint64)
	{
		OutError = FText::FromString(TEXT("ArcForest operation generation is exhausted."));
		UE_LOGF(
			LogArcEcotope,
			Error,"ArcForest bootstrap: Controller Start refused for '%ls': OperationGeneration exhausted (%llu).",
			*Owner.GetName(),
			OperationGeneration);
		return false;
	}
	++OperationGeneration;

	ActiveSession = MakeUnique<FArcForestSimulationSession>(
		Owner,
		OperationGeneration,
		Request,
		MoveTemp(BootstrapInput),
		World->GetPackage()->GetFName());
	ActiveSession->Launch();
	UE_LOGF(
		LogArcEcotope,
		Log,"Session started for '%ls' (generation=%llu, mode=%d, years=%d)",
		*Owner.GetName(),
		OperationGeneration,
		static_cast<int32>(Request.StartMode),
		Request.AnnualStepsAfterInitialization);
	OutError = FText::GetEmpty();
	return true;
}

void FArcForestSimulationController::Cancel(const AArcEcotopeVolume& Owner)
{
	check(IsInGameThread());
	if (ActiveSession && ActiveSession->IsOwnedBy(&Owner))
	{
		ActiveSession->RequestCancellation();
	}
}

void FArcForestSimulationController::UnregisterOwner(const AArcEcotopeVolume& Owner)
{
	check(IsInGameThread());
	if (ActiveSession && ActiveSession->IsOwnedBy(&Owner))
	{
		CancelAndReleaseActiveSession();
	}
}

bool FArcForestSimulationController::HasActiveSession() const
{
	if (!ActiveSession)
	{
		return false;
	}

	const EArcForestSimulationSessionStatus Status = ActiveSession->GetSnapshot().Status;
	return Status == EArcForestSimulationSessionStatus::Running
		|| Status == EArcForestSimulationSessionStatus::Cancelling;
}

bool FArcForestSimulationController::HasSessionFor(const AArcEcotopeVolume& Owner) const
{
	return HasActiveSession() && ActiveSession->IsOwnedBy(&Owner);
}

TOptional<FArcForestSimulationSessionSnapshot> FArcForestSimulationController::GetSessionSnapshot(
	const AArcEcotopeVolume& Owner) const
{
	if (!ActiveSession || !ActiveSession->IsOwnedBy(&Owner))
	{
		return {};
	}
	return ActiveSession->GetSnapshot();
}

bool FArcForestSimulationController::Tick(const float DeltaTime)
{
	(void)DeltaTime;
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcForest_ControllerTick);

	if (!ActiveSession)
	{
		return true;
	}
	if (!ActiveSession->IsOwnerUsable())
	{
		CancelAndReleaseActiveSession();
		return true;
	}
	if (ActiveSession->IsWorkerComplete())
	{
		ActiveSession->FinalizeOnGameThread(OperationGeneration);
		UE_LOGF(
			LogArcEcotope,
			Log,"Session finalized (generation=%llu, status=%d)",
			OperationGeneration,
			static_cast<int32>(ActiveSession->GetSnapshot().Status));
	}
	return true;
}

void FArcForestSimulationController::CancelAndReleaseActiveSession()
{
	if (ActiveSession)
	{
		ActiveSession->CancelAndWait();
		ActiveSession.Reset();
	}
}
