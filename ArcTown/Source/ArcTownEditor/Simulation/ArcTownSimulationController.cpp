// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcTownSimulationController.h"

#include "Engine/World.h"
#include "Simulation/ArcTownSimulationState.h"
#include "Town/ArcTownVolume.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "ArcTownSimulationController"

void FArcTownSimulationController::Register()
{
	check(IsInGameThread());
	check(!TickerHandle.IsValid());
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FArcTownSimulationController::Tick));
}

void FArcTownSimulationController::Unregister()
{
	check(IsInGameThread());
	CancelAndReleaseActiveSession();
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
}

bool FArcTownSimulationController::Start(AArcTownVolume& Owner, const int32 YearsToSimulate, FText& OutError)
{
	check(IsInGameThread());
	CancelAndReleaseActiveSession();

	OutError = FText::GetEmpty();

	if (YearsToSimulate <= 0)
	{
		OutError = LOCTEXT("StartYears", "A simulation run needs at least one year to simulate.");
		return false;
	}

	if (Owner.SimulationState == nullptr)
	{
		OutError = LOCTEXT("StartNoState", "This town has no simulation state object. Reset Simulation, then simulate again.");
		return false;
	}

	const UWorld* const World = Owner.GetWorld();
	if (World == nullptr || World->GetPackage() == nullptr)
	{
		OutError = LOCTEXT("StartNoWorld", "A town simulation needs an editor world to commit into.");
		return false;
	}

	FArcTownSimulationInput Input;
	if (!Owner.FreezeSimulationInput(YearsToSimulate, Input, OutError))
	{
		return false;
	}

	if (OperationGeneration == MAX_uint64)
	{
		OutError = LOCTEXT("StartGenerationExhausted", "This editor session has run out of town simulation generations. Restart the editor.");
		return false;
	}
	++OperationGeneration;

	ActiveSession = MakeUnique<FArcTownSimulationSession>(
		Owner, OperationGeneration, MoveTemp(Input), World->GetPackage()->GetFName());

	UE_LOGF(LogArcTownSimulation, Log,"ArcTown simulation started for '%ls' (generation=%llu, years=%d).",
		*Owner.GetName(),
		OperationGeneration,
		YearsToSimulate);
	return true;
}

void FArcTownSimulationController::Cancel(const AArcTownVolume& Owner)
{
	check(IsInGameThread());
	if (ActiveSession && ActiveSession->IsOwnedBy(&Owner))
	{
		ActiveSession->RequestCancellation();
	}
}

bool FArcTownSimulationController::HasActiveSession() const
{
	if (!ActiveSession)
	{
		return false;
	}

	const EArcTownSimulationSessionStatus Status = ActiveSession->GetSnapshot().Status;
	return Status == EArcTownSimulationSessionStatus::Running
		|| Status == EArcTownSimulationSessionStatus::Cancelling;
}

TOptional<FArcTownSimulationSessionSnapshot> FArcTownSimulationController::GetSessionSnapshot(
	const AArcTownVolume& Owner) const
{
	if (!ActiveSession || !ActiveSession->IsOwnedBy(&Owner))
	{
		return {};
	}
	return ActiveSession->GetSnapshot();
}

bool FArcTownSimulationController::Tick(const float DeltaTime)
{
	(void)DeltaTime;
	check(IsInGameThread());

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
		UE_LOGF(LogArcTownSimulation, Log,"ArcTown simulation finalized (generation=%llu, status=%d).",
			OperationGeneration,
			static_cast<int32>(ActiveSession->GetSnapshot().Status));
	}
	return true;
}

void FArcTownSimulationController::CancelAndReleaseActiveSession()
{
	if (ActiveSession)
	{
		ActiveSession->CancelAndWait();
		ActiveSession.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
