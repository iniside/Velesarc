// Copyright Lukasz Baran. All Rights Reserved.

#include "MassAbilities/ArcAbilityTask_ExecuteGlobalTargeting.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "MassActorSubsystem.h"
#include "MassEntityManager.h"

EStateTreeRunStatus FArcAbilityTask_ExecuteGlobalTargeting::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FArcAbilityTask_ExecuteGlobalTargetingInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_ExecuteGlobalTargetingInstanceData>(*this);

    if (!InstanceData.TargetingTag.IsValid())
    {
        return EStateTreeRunStatus::Failed;
    }

    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    FMassEntityHandle Entity = AbilityContext.GetEntity();

    FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
    if (!ActorFragment || !ActorFragment->IsValid())
    {
        return EStateTreeRunStatus::Failed;
    }

    AActor* Actor = ActorFragment->GetMutable();
    UArcCoreAbilitySystemComponent* ASC = Actor ? Actor->FindComponentByClass<UArcCoreAbilitySystemComponent>() : nullptr;
    if (!ASC)
    {
        return EStateTreeRunStatus::Failed;
    }

    bool bSuccess = false;
    FHitResult HitResult = UArcCoreAbilitySystemComponent::GetGlobalHitResult(ASC, InstanceData.TargetingTag, bSuccess);
    if (!bSuccess)
    {
        return EStateTreeRunStatus::Failed;
    }

    FArcMassTargetData_HitResults HitResultData;
    HitResultData.HitResults.Add(MoveTemp(HitResult));
    InstanceData.TargetDataHandle.Data.InitializeAs<FArcMassTargetData_HitResults>(MoveTemp(HitResultData));

    return EStateTreeRunStatus::Succeeded;
}
