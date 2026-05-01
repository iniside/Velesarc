// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_Commit.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcMassAbilityCost.h"
#include "Abilities/ArcAbilityCooldown.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Abilities/ArcAbilityFunctions.h"

EStateTreeRunStatus FArcAbilityTask_Commit::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle Entity = AbilityContext.GetEntity();
    const FArcAbilityHandle Handle = AbilityContext.GetAbilityHandle();

    const FArcActiveAbility* ActiveAbility = ArcAbilities::FindAbility(EntityManager, Entity, Handle);
    if (!ActiveAbility || !ActiveAbility->Definition)
    {
        return EStateTreeRunStatus::Failed;
    }

    const UArcAbilityDefinition* Def = ActiveAbility->Definition;

    if (Def->Cost.IsValid())
    {
        const FArcMassAbilityCost* CostPtr = Def->Cost.GetPtr<FArcMassAbilityCost>();
        if (CostPtr)
        {
            FArcMassAbilityCostContext CostCtx{EntityManager, Entity, Handle, Def};
            if (!CostPtr->CheckCost(CostCtx))
            {
                return EStateTreeRunStatus::Failed;
            }
            CostPtr->ApplyCost(CostCtx);
        }
    }

    if (Def->Cooldown.IsValid())
    {
        const FArcAbilityCooldown* CooldownPtr = Def->Cooldown.GetPtr<FArcAbilityCooldown>();
        if (CooldownPtr)
        {
            CooldownPtr->ApplyCooldown(EntityManager, Entity, Handle);
        }
    }

    return EStateTreeRunStatus::Succeeded;
}
