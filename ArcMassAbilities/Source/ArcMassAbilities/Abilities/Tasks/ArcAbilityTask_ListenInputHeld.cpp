// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_ListenInputHeld.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Fragments/ArcAbilityHeldFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "MassCommands.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FArcAbilityTask_ListenInputHeld::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityTask_ListenInputHeldInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_ListenInputHeldInstanceData>(*this);
    InstanceData.HeldDuration = 0.f;

    if (!InstanceData.InputTag.IsValid())
    {
        return EStateTreeRunStatus::Running;
    }

    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    FMassEntityHandle Entity = AbilityContext.GetEntity();

    const FMassEntityView EntityView(EntityManager, Entity);
    const FArcAbilityInputFragment& Input = EntityView.GetFragmentData<FArcAbilityInputFragment>();
    bool bCurrentlyHeld = Input.HeldInputs.HasTag(InstanceData.InputTag);

    FGameplayTag CapturedInputTag = InstanceData.InputTag;
    float CapturedMinDuration = InstanceData.MinHeldDuration;

    EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Add>>(
        [Entity, CapturedInputTag, CapturedMinDuration, bCurrentlyHeld](FMassEntityManager& Mgr)
        {
            FArcAbilityHeldFragment& HeldFrag = Mgr.AddSparseElementToEntity<FArcAbilityHeldFragment>(Entity);
            HeldFrag.InputTag = CapturedInputTag;
            HeldFrag.MinHeldDuration = CapturedMinDuration;
            HeldFrag.HeldDuration = 0.f;
            HeldFrag.bIsHeld = bCurrentlyHeld;
            HeldFrag.bWasHeld = bCurrentlyHeld;
            HeldFrag.bThresholdReached = false;
            Mgr.AddSparseElementToEntity<FArcAbilityHeldTag>(Entity);
        });

    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcAbilityTask_ListenInputHeld::Tick(
    FStateTreeExecutionContext& Context,
    const float DeltaTime) const
{
    FArcAbilityTask_ListenInputHeldInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_ListenInputHeldInstanceData>(*this);

    if (!InstanceData.InputTag.IsValid())
    {
        return EStateTreeRunStatus::Running;
    }

    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    FMassEntityHandle Entity = AbilityContext.GetEntity();

    FStructView HeldView = EntityManager.GetMutableSparseElementDataForEntity(
        FArcAbilityHeldFragment::StaticStruct(), Entity);
    FArcAbilityHeldFragment* HeldFrag = HeldView.GetPtr<FArcAbilityHeldFragment>();

    if (!HeldFrag)
    {
        return EStateTreeRunStatus::Running;
    }

    InstanceData.HeldDuration = HeldFrag->HeldDuration;

    if (HeldFrag->bIsHeld && !HeldFrag->bWasHeld)
    {
        HeldFrag->bWasHeld = true;
        Context.BroadcastDelegate(InstanceData.OnHeldStarted);
    }
    else if (!HeldFrag->bIsHeld && HeldFrag->bWasHeld)
    {
        HeldFrag->bWasHeld = false;
        Context.BroadcastDelegate(InstanceData.OnHeldStopped);
    }

    if (InstanceData.MinHeldDuration > 0.f && HeldFrag->bThresholdReached)
    {
        return EStateTreeRunStatus::Succeeded;
    }

    return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_ListenInputHeld::ExitState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    FMassEntityHandle Entity = AbilityContext.GetEntity();

    EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Remove>>(
        [Entity](FMassEntityManager& Mgr)
        {
            Mgr.RemoveSparseElementFromEntity<FArcAbilityHeldFragment>(Entity);
            Mgr.RemoveSparseElementFromEntity<FArcAbilityHeldTag>(Entity);
        });
}
