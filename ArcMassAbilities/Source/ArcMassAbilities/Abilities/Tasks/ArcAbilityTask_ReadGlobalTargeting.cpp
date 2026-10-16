// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_ReadGlobalTargeting.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "ArcTargetingPipelineTypes.h"
#include "ArcTargetingSubsystem.h"

EStateTreeRunStatus FArcAbilityTask_ReadGlobalTargeting::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    if (Transition.ChangeType == EStateTreeStateChangeType::Sustained)
    {
        return EStateTreeRunStatus::Running;
    }

    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FArcAbilityTask_ReadGlobalTargetingInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_ReadGlobalTargetingInstanceData>(*this);
    InstanceData.TargetDataHandle = FArcMassTargetDataHandle();

    const FMassEntityHandle Entity = AbilityContext.GetEntity();
    UWorld* World = AbilityContext.GetMassContext().GetWorld();
    UArcTargetingSubsystem* TargetingSubsystem = UArcTargetingSubsystem::Get(World);
    if (TargetingSubsystem == nullptr)
    {
        return EStateTreeRunStatus::Failed;
    }

    const TArray<FArcTargetCandidate>* Candidates =
        TargetingSubsystem->GetLatestPersistentResult(Entity, InstanceData.TargetingTag);
    if (Candidates == nullptr)
    {
        return EStateTreeRunStatus::Failed;
    }

    TArray<FHitResult> HitResults;
    HitResults.Reserve(Candidates->Num());
    for (const FArcTargetCandidate& Candidate : *Candidates)
    {
        HitResults.Add(Candidate.HitResult);
    }

    InstanceData.TargetDataHandle.SetHitResults(MoveTemp(HitResults));
    return EStateTreeRunStatus::Succeeded;
}
