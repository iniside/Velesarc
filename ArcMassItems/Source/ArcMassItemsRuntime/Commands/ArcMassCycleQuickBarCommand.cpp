// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcMassCycleQuickBarCommand.h"

#include "Engine/World.h"
#include "Commands/ArcMassItemReplicatedCommand.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "QuickBar/ArcMassQuickBarTypes.h"

bool FArcMassCycleQuickBarCommand::CanSendCommand() const
{
	if (Entity.IsSet() == false)
	{
		return false;
	}

	if (QuickBarGroup.IsValid() == false)
	{
		return false;
	}

	if (Direction == 0)
	{
		return false;
	}

	return true;
}

bool FArcMassCycleQuickBarCommand::Execute(UObject* WorldContext)
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

	if (Entity.IsSet() == false || Manager.IsEntityActive(Entity) == false || Direction == 0)
	{
		return false;
	}

	const FArcMassQuickBarLocation Location = ArcMassItems::QuickBar::FindGroup(Manager, Entity, QuickBarGroup);
	if (Location.Group == nullptr)
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "CycleQuickBar: no group config %ls", *QuickBarGroup.ToString());
		return false;
	}

	if (Location.Group->bCyclable == false)
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "CycleQuickBar: group %ls is not cyclable", *QuickBarGroup.ToString());
		return false;
	}

	if (ResolvedQuickBar.IsValid()
		&& Location.Group->Bars.ContainsByPredicate([this](const FArcMassQuickBarEntry& Bar) { return Bar.BarId == ResolvedQuickBar; }) == false)
	{
		UE_LOGF(LogArcReplicatedCommand, Warning, "CycleQuickBar: resolved Bar=%ls is not in group %ls", *ResolvedQuickBar.ToString(), *QuickBarGroup.ToString());
		return false;
	}

	PredictionCapture = ArcMassItems::QuickBar::CaptureGroupSelection(Manager, Entity, QuickBarGroup);

	FGameplayTag NewBar;
	if (ResolvedQuickBar.IsValid())
	{
		if (ArcMassItems::QuickBar::ActivateBar(Manager, Entity, ResolvedQuickBar))
		{
			NewBar = ResolvedQuickBar;
		}
	}
	else
	{
		NewBar = ArcMassItems::QuickBar::CycleActiveBar(Manager, Entity, QuickBarGroup, Direction);
		ResolvedQuickBar = NewBar;
	}

	if (NewBar.IsValid() == false)
	{
		PredictionCapture.Reset();
		return false;
	}
	return true;
}

void FArcMassCycleQuickBarCommand::UndoPrediction(UObject* WorldContext)
{
	UWorld* ResolvedWorld = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	UMassEntitySubsystem* Subsystem = ResolvedWorld != nullptr ? ResolvedWorld->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (Subsystem == nullptr)
	{
		return;
	}

	ArcMassItems::QuickBar::RestoreSelection(Subsystem->GetMutableEntityManager(), Entity, PredictionCapture);
}
