// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_ApplyEffect.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "StructUtils/InstancedStruct.h"

EStateTreeRunStatus FArcAbilityTask_ApplyEffect::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);

    if (!EffectDefinition)
    {
        return EStateTreeRunStatus::Failed;
    }

    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle Entity = AbilityContext.GetEntity();

    const FMassEntityHandle Target = Entity;
    const FMassEntityHandle Source = Entity;

    FInstancedStruct SourceData;
    const FArcActiveAbility* ActiveAbility = ArcAbilities::FindAbility(EntityManager, Entity, AbilityContext.GetAbilityHandle());
    if (ActiveAbility)
    {
        SourceData = ActiveAbility->SourceData;
    }

    FArcActiveEffectHandle ResultHandle = ArcEffects::TryApplyEffect(EntityManager, Target,
        const_cast<UArcEffectDefinition*>(EffectDefinition.Get()), Source, SourceData);

    return ResultHandle.IsValid() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
