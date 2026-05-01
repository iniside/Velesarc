// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_ListenInputReleased.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Signals/ArcAbilitySignals.h"
#include "MassEntityView.h"
#include "MassSignalSubsystem.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FArcAbilityTask_ListenInputReleased::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcAbilityTask_ListenInputReleased::Tick(
    FStateTreeExecutionContext& Context,
    const float DeltaTime) const
{
    FArcAbilityTask_ListenInputReleasedInstanceData& InstanceData = Context.GetInstanceData<FArcAbilityTask_ListenInputReleasedInstanceData>(*this);

    if (!InstanceData.InputTag.IsValid())
    {
        return EStateTreeRunStatus::Running;
    }

    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    FMassEntityHandle Entity = AbilityContext.GetEntity();

    const FMassEntityView EntityView(EntityManager, Entity);
    const FArcAbilityInputFragment& Input = EntityView.GetFragmentData<FArcAbilityInputFragment>();

    if (Input.ReleasedThisFrame.Contains(InstanceData.InputTag))
    {
        Context.BroadcastDelegate(InstanceData.OnInputReleased);

        UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Context.GetWorld());
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
        }
    }

    return EStateTreeRunStatus::Running;
}
