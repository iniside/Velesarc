// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_ApplyEffectToTargets.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Mass/EntityManager.h"
#include "ArcMass/Physics/ArcMassPhysicsEntityLink.h"
#include "ArcMassAbilitiesLogs.h"

EStateTreeRunStatus FArcAbilityTask_ApplyEffectToTargets::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FArcAbilityTask_ApplyEffectToTargetsInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_ApplyEffectToTargetsInstanceData>(*this);
    UWorld* World = AbilityContext.GetMassContext().GetWorld();
    const FMassEntityHandle SourceEntity = AbilityContext.GetEntity();

    if (!InstanceData.EffectDefinition)
    {
        ARCMA_VLOG(World, Warning,
            TEXT("[Effect] ApplyEffectToTargets FAILED: no EffectDefinition entity=[%d:%d]"),
            SourceEntity.Index, SourceEntity.SerialNumber);
        return EStateTreeRunStatus::Failed;
    }

    if (!InstanceData.TargetDataHandle.HasHitResults())
    {
        ARCMA_VLOG(World, Warning,
            TEXT("[Effect] ApplyEffectToTargets FAILED: no hit results entity=[%d:%d]"),
            SourceEntity.Index, SourceEntity.SerialNumber);
        return EStateTreeRunStatus::Failed;
    }

    const TArray<FHitResult>& HitResults = InstanceData.TargetDataHandle.GetHitResults();

    ARCMA_VLOG(World, Log,
        TEXT("[Effect] ApplyEffectToTargets enter def=%s targets=%d entity=[%d:%d]"),
        *InstanceData.EffectDefinition->GetName(), HitResults.Num(),
        SourceEntity.Index, SourceEntity.SerialNumber);

    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();

    const FArcEffectContext EffectContext = ArcAbilities::MakeAbilityEffectContext(EntityManager, SourceEntity, AbilityContext.GetAbilityHandle());

    bool bAnyApplied = false;
    int32 AppliedCount = 0;

    for (const FHitResult& Hit : HitResults)
    {
        const FMassEntityHandle TargetEntity = ArcMassPhysicsEntityLink::ResolveHit(Hit);
        if (!EntityManager.IsEntityValid(TargetEntity))
        {
            ARCMA_VLOG(World, Verbose,
                TEXT("[Effect] ApplyEffectToTargets: hit resolved no valid entity (skipped) impact_point=%s"),
                *Hit.ImpactPoint.ToString());
            continue;
        }

        ARCMA_VLOG_LOCATION(World, Hit.ImpactPoint, 15.f, FColor::Orange,
            TEXT("[Effect] target hit def=%s instigator=[%d:%d]"),
            *InstanceData.EffectDefinition->GetName(),
            SourceEntity.Index, SourceEntity.SerialNumber);

        const FArcEffectApplyResult ApplyResult = ArcEffects::TryApplyEffect(
            EntityManager, TargetEntity,
            const_cast<UArcEffectDefinition*>(InstanceData.EffectDefinition.Get()),
			EffectContext);

        if (ApplyResult.bApplied)
        {
            bAnyApplied = true;
            ++AppliedCount;
            ARCMA_VLOG(World, Log,
                TEXT("[Effect] ApplyEffectToTargets: effect applied to entity [%d:%d]"),
                TargetEntity.Index, TargetEntity.SerialNumber);
        }
    }

    ARCMA_VLOG(World, Log,
        TEXT("[Effect] ApplyEffectToTargets %s applied=%d/%d entity=[%d:%d]"),
        bAnyApplied ? TEXT("Succeeded") : TEXT("Failed"),
        AppliedCount, HitResults.Num(),
        SourceEntity.Index, SourceEntity.SerialNumber);

    return bAnyApplied ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
