// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcMassCycleQuickSlotCommand.h"

#include "Engine/World.h"
#include "Commands/ArcMassItemReplicatedCommand.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "QuickBar/ArcMassQuickBarTypes.h"

bool FArcMassCycleQuickSlotCommand::CanSendCommand() const
{
	if (Entity.IsSet() == false)
	{
		return false;
	}

	if (QuickBar.IsValid() == false || QuickSlot.IsValid() == false)
	{
		return false;
	}

	if (Direction == 0)
	{
		return false;
	}

	return true;
}

bool FArcMassCycleQuickSlotCommand::Execute(UObject* WorldContext)
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
		UE_LOGF(LogArcReplicatedCommand, Warning, "CycleQuickSlot: Bar=%ls is a child bar, driven only by its parent bar", *QuickBar.ToString());
		return false;
	}

	if (ResolvedQuickSlot.IsValid()
		&& (Location.Bar == nullptr
			|| Location.Bar->QuickSlotsMode != EArcMassQuickSlotsMode::Cyclable
			|| ArcMassItems::QuickBar::IsBarActive(Manager, Entity, QuickBar) == false))
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "CycleQuickSlot: resolved Slot=%ls needs an active cyclable Bar=%ls", *ResolvedQuickSlot.ToString(), *QuickBar.ToString());
		return false;
	}

	PredictionCapture = ArcMassItems::QuickBar::CaptureBarSelection(Manager, Entity, QuickBar);

	FGameplayTag NewSlot;
	if (ResolvedQuickSlot.IsValid())
	{
		if (ArcMassItems::QuickBar::ActivateSlot(Manager, Entity, QuickBar, ResolvedQuickSlot))
		{
			NewSlot = ResolvedQuickSlot;
		}
	}
	else
	{
		NewSlot = ArcMassItems::QuickBar::CycleSlot(Manager, Entity, QuickBar, QuickSlot, Direction, nullptr);
		ResolvedQuickSlot = NewSlot;
	}

	if (NewSlot.IsValid() == false)
	{
		PredictionCapture.Reset();
		return false;
	}
	return true;
}

void FArcMassCycleQuickSlotCommand::UndoPrediction(UObject* WorldContext)
{
	UWorld* ResolvedWorld = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	UMassEntitySubsystem* Subsystem = ResolvedWorld != nullptr ? ResolvedWorld->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (Subsystem == nullptr)
	{
		return;
	}

	ArcMassItems::QuickBar::RestoreSelection(Subsystem->GetMutableEntityManager(), Entity, PredictionCapture);
}
