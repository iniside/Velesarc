// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_WaitDuration.h"

#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FArcAbilityTask_WaitDuration::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityTask_WaitDurationInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_WaitDurationInstanceData>(*this);

	InstanceData.Elapsed = 0.f;

	if (InstanceData.Duration <= 0.f)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.ScheduledTickHandle = Context.AddScheduledTickRequest(
		FStateTreeScheduledTick::MakeCustomTickRate(InstanceData.Duration));
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcAbilityTask_WaitDuration::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FArcAbilityTask_WaitDurationInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_WaitDurationInstanceData>(*this);

	InstanceData.Elapsed += DeltaTime;

	if (InstanceData.Elapsed >= InstanceData.Duration)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// Re-request remaining wait. Guard against zero/negative remainder:
	// MakeCustomTickRate(0) hits an engine ensure and falls back to MakeEveryFrames,
	// which would spam the tick processor every frame for one frame.
	const float Remaining = FMath::Max(KINDA_SMALL_NUMBER, InstanceData.Duration - InstanceData.Elapsed);
	Context.UpdateScheduledTickRequest(
		InstanceData.ScheduledTickHandle,
		FStateTreeScheduledTick::MakeCustomTickRate(Remaining));
	return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_WaitDuration::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityTask_WaitDurationInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_WaitDurationInstanceData>(*this);

	if (InstanceData.ScheduledTickHandle.IsValid())
	{
		Context.RemoveScheduledTickRequest(InstanceData.ScheduledTickHandle);
		InstanceData.ScheduledTickHandle = {};
	}
}
