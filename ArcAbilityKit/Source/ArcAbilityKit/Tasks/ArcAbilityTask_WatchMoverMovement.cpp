// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_WatchMoverMovement.h"

#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Mass/EntityManager.h"
#include "MassActorSubsystem.h"
#include "MoverComponent.h"
#include "StateTreeExecutionContext.h"

namespace ArcAbilityTaskWatchMoverMovement
{
	UMoverComponent* ResolveMoverComponent(FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
	{
		FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
		if (!ActorFragment || !ActorFragment->IsValid())
		{
			return nullptr;
		}

		AActor* Actor = ActorFragment->GetMutable();
		return Actor ? Actor->FindComponentByClass<UMoverComponent>() : nullptr;
	}

	bool IsMoving(const UMoverComponent& MoverComponent, const float SpeedThreshold)
	{
		const float ClampedSpeedThreshold = FMath::Max(SpeedThreshold, 0.f);
		return MoverComponent.GetVelocity().SizeSquared() > FMath::Square(ClampedSpeedThreshold);
	}
}

EStateTreeRunStatus FArcAbilityTask_WatchMoverMovement::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	InstanceData.MoverComponent.Reset();

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	UMoverComponent* MoverComponent = ArcAbilityTaskWatchMoverMovement::ResolveMoverComponent(
		AbilityContext.GetEntityManager(), AbilityContext.GetEntity());
	if (!MoverComponent)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.MoverComponent = MoverComponent;
	InstanceData.bIsMoving = ArcAbilityTaskWatchMoverMovement::IsMoving(*MoverComponent, InstanceData.SpeedThreshold);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcAbilityTask_WatchMoverMovement::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	UMoverComponent* MoverComponent = InstanceData.MoverComponent.Get();
	if (!MoverComponent)
	{
		return EStateTreeRunStatus::Failed;
	}

	const bool bIsMoving = ArcAbilityTaskWatchMoverMovement::IsMoving(*MoverComponent, InstanceData.SpeedThreshold);
	if (bIsMoving == InstanceData.bIsMoving)
	{
		return EStateTreeRunStatus::Running;
	}

	InstanceData.bIsMoving = bIsMoving;
	Context.BroadcastDelegate(bIsMoving ? InstanceData.OnMovementStarted : InstanceData.OnMovementStopped);
	return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_WatchMoverMovement::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	InstanceData.MoverComponent.Reset();
}
