// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_ListenInputPressed.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Signals/ArcAbilitySignals.h"
#include "MassEntityView.h"
#include "MassSignalSubsystem.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FArcAbilityTask_ListenInputPressed::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcAbilityTask_ListenInputPressed::Tick(
    FStateTreeExecutionContext& Context,
    const float DeltaTime) const
{
    FArcAbilityTask_ListenInputPressedInstanceData& InstanceData = Context.GetInstanceData<FArcAbilityTask_ListenInputPressedInstanceData>(*this);

    if (!InstanceData.InputTag.IsValid())
    {
        return EStateTreeRunStatus::Running;
    }

    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    FMassEntityHandle Entity = AbilityContext.GetEntity();

    const FMassEntityView EntityView(EntityManager, Entity);
    const FArcAbilityInputFragment& Input = EntityView.GetFragmentData<FArcAbilityInputFragment>();

    if (Input.PressedThisFrame.Contains(InstanceData.InputTag))
    {
        Context.BroadcastDelegate(InstanceData.OnInputPressed);

        UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Context.GetWorld());
        if (SignalSubsystem)
        {
            SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
        }
    }

    return EStateTreeRunStatus::Running;
}
