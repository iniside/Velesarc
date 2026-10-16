// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcMassDeactivateQuickSlotCommand.h"

#include "Engine/World.h"
#include "Commands/ArcMassItemReplicatedCommand.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "QuickBar/ArcMassQuickBarTypes.h"

bool FArcMassDeactivateQuickSlotCommand::CanSendCommand() const
{
	if (Entity.IsSet() == false)
	{
		return false;
	}

	if (QuickBar.IsValid() == false || QuickSlot.IsValid() == false)
	{
		return false;
	}

	return true;
}

bool FArcMassDeactivateQuickSlotCommand::Execute(UObject* WorldContext)
{
	PredictionCapture.Reset();

	UWorld* ResolvedWorld = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	if (ResolvedWorld == nullptr)
	{
		return false;
	}

	UMassEntitySubsystem* Subsystem = ResolvedWorld->GetSubsystem<UMassEntitySubsystem>();
	if (Subsystem == nullptr)
	{
		return false;
	}

	FMassEntityManager& Manager = Subsystem->GetMutableEntityManager();

	if (Entity.IsSet() == false || Manager.IsEntityActive(Entity) == false)
	{
		return false;
	}

	const FArcMassQuickBarLocation Location = ArcMassItems::QuickBar::FindBar(Manager, Entity, QuickBar);
	if (Location.Bar != nullptr && Location.Bar->ParentBarId.IsValid())
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "DeactivateQuickSlot: Bar=%ls is a child bar, driven only by its parent bar", *QuickBar.ToString());
		return false;
	}

	PredictionCapture = ArcMassItems::QuickBar::CaptureBarSelection(Manager, Entity, QuickBar);
	const bool bDeactivated = ArcMassItems::QuickBar::DeactivateSlot(Manager, Entity, QuickBar, QuickSlot);
	if (!bDeactivated)
	{
		PredictionCapture.Reset();
	}
	return bDeactivated;
}

void FArcMassDeactivateQuickSlotCommand::UndoPrediction(UObject* WorldContext)
{
	UWorld* ResolvedWorld = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	UMassEntitySubsystem* Subsystem = ResolvedWorld != nullptr ? ResolvedWorld->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (Subsystem == nullptr)
	{
		return;
	}

	ArcMassItems::QuickBar::RestoreSelection(Subsystem->GetMutableEntityManager(), Entity, PredictionCapture);
}
