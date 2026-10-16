// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_ApplyEffect.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcMassAbilitiesLogs.h"

EStateTreeRunStatus FArcAbilityTask_ApplyEffect::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    UWorld* World = AbilityContext.GetMassContext().GetWorld();

    if (!EffectDefinition)
    {
        const FMassEntityHandle Entity = AbilityContext.GetEntity();
        ARCMA_VLOG(World, Warning,
            TEXT("[Effect] ApplyEffect FAILED: no EffectDefinition entity=[%d:%d]"),
            Entity.Index, Entity.SerialNumber);
        return EStateTreeRunStatus::Failed;
    }

    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle Entity = AbilityContext.GetEntity();

    ARCMA_VLOG(World, Log,
        TEXT("[Effect] ApplyEffect enter def=%s bApplyToSelf=%d entity=[%d:%d]"),
        *EffectDefinition->GetName(), 1, Entity.Index, Entity.SerialNumber);

    const FMassEntityHandle Target = Entity;
    const FMassEntityHandle Source = Entity;

    const FArcEffectContext EffectContext = ArcAbilities::MakeAbilityEffectContext(EntityManager, Source, AbilityContext.GetAbilityHandle());

    const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(EntityManager, Target,
        const_cast<UArcEffectDefinition*>(EffectDefinition.Get()), EffectContext);

    ARCMA_VLOG(World, Log,
        TEXT("[Effect] ApplyEffect %s def=%s entity=[%d:%d] handle=%u"),
        Result.bApplied ? TEXT("Succeeded") : TEXT("Failed"),
        *EffectDefinition->GetName(), Entity.Index, Entity.SerialNumber,
        GetTypeHash(Result.Handle));

    return Result.bApplied ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
