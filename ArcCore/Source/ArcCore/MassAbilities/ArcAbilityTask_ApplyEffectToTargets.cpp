// Copyright Lukasz Baran. All Rights Reserved.

#include "MassAbilities/ArcAbilityTask_ApplyEffectToTargets.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "MassEntityManager.h"
#include "MassAgentComponent.h"

EStateTreeRunStatus FArcAbilityTask_ApplyEffectToTargets::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FArcAbilityTask_ApplyEffectToTargetsInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_ApplyEffectToTargetsInstanceData>(*this);

    if (!InstanceData.EffectDefinition)
    {
        return EStateTreeRunStatus::Failed;
    }

    if (!InstanceData.TargetDataHandle.HasHitResults())
    {
        return EStateTreeRunStatus::Failed;
    }

    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle SourceEntity = AbilityContext.GetEntity();

    FInstancedStruct SourceData;
    const FArcActiveAbility* ActiveAbility = ArcAbilities::FindAbility(EntityManager, SourceEntity, AbilityContext.GetAbilityHandle());
    if (ActiveAbility)
    {
        SourceData = ActiveAbility->SourceData;
    }

    const TArray<FHitResult>& HitResults = InstanceData.TargetDataHandle.GetHitResults();
    bool bAnyApplied = false;

    for (const FHitResult& Hit : HitResults)
    {
        AActor* HitActor = Hit.GetActor();
        if (!HitActor)
        {
            continue;
        }

        UMassAgentComponent* AgentComp = HitActor->FindComponentByClass<UMassAgentComponent>();
        if (!AgentComp)
        {
            continue;
        }

        FMassEntityHandle TargetEntity = AgentComp->GetEntityHandle();
        if (!EntityManager.IsEntityValid(TargetEntity))
        {
            continue;
        }

        FArcActiveEffectHandle ApplyHandle = ArcEffects::TryApplyEffect(
            EntityManager, TargetEntity,
            const_cast<UArcEffectDefinition*>(InstanceData.EffectDefinition.Get()),
            SourceEntity, SourceData);

        bAnyApplied = bAnyApplied || ApplyHandle.IsValid();
    }

    return bAnyApplied ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
