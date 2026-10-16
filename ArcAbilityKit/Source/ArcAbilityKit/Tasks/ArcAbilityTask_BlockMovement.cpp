// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_BlockMovement.h"

#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "GameFramework/Actor.h"
#include "Mass/EntityManager.h"
#include "MassActorSubsystem.h"
#include "Mover/ArcCharacterMoverComponent.h"
#include "Mover/ArcChaosCharacterMoverComponent.h"
#include "Mover/ArcLayeredMove_Root.h"
#include "Mover/ArcMovementModifiers.h"
#include "StateTreeExecutionContext.h"

namespace ArcAbilityTaskBlockMovement
{
	AActor* ResolveOwnerActor(FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
	{
		FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
		return ActorFragment && ActorFragment->IsValid() ? ActorFragment->GetMutable() : nullptr;
	}

	void ReleaseModifier(FArcAbilityTask_BlockMovementInstanceData& InstanceData)
	{
		if (InstanceData.ModifierHandle.IsValid())
		{
			if (UMoverComponent* MoverComponent = InstanceData.MoverComponent.Get())
			{
				MoverComponent->CancelModifier(InstanceData.ModifierHandle);
			}
			InstanceData.ModifierHandle.Invalidate();
		}

		InstanceData.MoverComponent.Reset();
	}

	FMovementModifierHandle QueueRoot(AActor& OwnerActor, UMoverComponent*& OutMoverComponent)
	{
		const FGuid SourceId = FGuid::NewGuid();
		if (UArcCharacterMoverComponent* NPMover = OwnerActor.FindComponentByClass<UArcCharacterMoverComponent>())
		{
			OutMoverComponent = NPMover;
			return NPMover->QueueRootMovementModifier(SourceId);
		}

		if (UArcChaosCharacterMoverComponent* ChaosMover = OwnerActor.FindComponentByClass<UArcChaosCharacterMoverComponent>())
		{
			OutMoverComponent = ChaosMover;
			TSharedPtr<FArcMovementModifier_Root> Marker = MakeShared<FArcMovementModifier_Root>();
			Marker->SourceId = SourceId;
			return ArcMover::QueueRoot(*ChaosMover, Marker);
		}

		OutMoverComponent = nullptr;
		return FMovementModifierHandle();
	}
}

EStateTreeRunStatus FArcAbilityTask_BlockMovement::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	ArcAbilityTaskBlockMovement::ReleaseModifier(InstanceData);

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	AActor* OwnerActor = ArcAbilityTaskBlockMovement::ResolveOwnerActor(
		AbilityContext.GetEntityManager(), AbilityContext.GetEntity());
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return EStateTreeRunStatus::Failed;
	}

	UMoverComponent* MoverComponent = nullptr;
	const FMovementModifierHandle ModifierHandle = ArcAbilityTaskBlockMovement::QueueRoot(*OwnerActor, MoverComponent);
	if (!MoverComponent || !ModifierHandle.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.MoverComponent = MoverComponent;
	InstanceData.ModifierHandle = ModifierHandle;
	return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_BlockMovement::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	ArcAbilityTaskBlockMovement::ReleaseModifier(InstanceData);
}
