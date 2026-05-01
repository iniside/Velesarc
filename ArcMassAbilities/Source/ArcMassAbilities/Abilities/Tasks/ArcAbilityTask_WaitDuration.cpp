// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_WaitDuration.h"

#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Fragments/ArcAbilityWaitFragment.h"
#include "MassCommands.h"
#include "MassEntityManager.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FArcAbilityTask_WaitDuration::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityTask_WaitDurationInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_WaitDurationInstanceData>(*this);

	if (InstanceData.Duration <= 0.f)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();
	float WaitDuration = InstanceData.Duration;

	EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Add>>(
		[Entity, WaitDuration](FMassEntityManager& Mgr)
		{
			FArcAbilityWaitFragment& WaitFragment = Mgr.AddSparseElementToEntity<FArcAbilityWaitFragment>(Entity);
			WaitFragment.Duration = WaitDuration;
			WaitFragment.ElapsedTime = 0.f;
			Mgr.AddSparseElementToEntity<FArcAbilityWaitTag>(Entity);
		});

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcAbilityTask_WaitDuration::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FArcAbilityTask_WaitDurationInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_WaitDurationInstanceData>(*this);

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FStructView WaitView = EntityManager.GetMutableSparseElementDataForEntity(
		FArcAbilityWaitFragment::StaticStruct(), AbilityContext.GetEntity());

	FArcAbilityWaitFragment* WaitFragment = WaitView.GetPtr<FArcAbilityWaitFragment>();
	if (WaitFragment && WaitFragment->ElapsedTime >= InstanceData.Duration)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_WaitDuration::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();

	EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Remove>>(
		[Entity](FMassEntityManager& Mgr)
		{
			Mgr.RemoveSparseElementFromEntity<FArcAbilityWaitFragment>(Entity);
			Mgr.RemoveSparseElementFromEntity<FArcAbilityWaitTag>(Entity);
		});
}
