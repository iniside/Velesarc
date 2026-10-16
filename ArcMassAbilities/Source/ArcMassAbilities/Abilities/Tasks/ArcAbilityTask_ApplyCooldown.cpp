// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_ApplyCooldown.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityCooldown.h"
#include "ArcMassAbilitiesLogs.h"

EStateTreeRunStatus FArcAbilityTask_ApplyCooldown::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle Entity = AbilityContext.GetEntity();
    const FArcAbilityHandle Handle = AbilityContext.GetAbilityHandle();
    UWorld* World = AbilityContext.GetMassContext().GetWorld();

    const FArcActiveAbility* ActiveAbility = ArcAbilities::FindAbility(EntityManager, Entity, Handle);
    if (!ActiveAbility || !ActiveAbility->Definition)
    {
        ARCMA_VLOG(World, Warning,
            TEXT("[Cooldown] ApplyCooldown FAILED: ability not found or no definition entity=[%d:%d]"),
            Entity.Index, Entity.SerialNumber);
        return EStateTreeRunStatus::Failed;
    }

    const UArcAbilityDefinition* Def = ActiveAbility->Definition;

    if (Def->Cooldown.IsValid())
    {
        const FArcAbilityCooldown* Cd = Def->Cooldown.GetPtr<FArcAbilityCooldown>();
        if (Cd)
        {
            float AppliedDuration = 0.f;
            Cd->ApplyCooldown(EntityManager, Entity, Handle, &AppliedDuration);
            InstanceData.bApplied = true;
            InstanceData.AppliedDuration = AppliedDuration;
            ARCMA_VLOG(World, Log,
                TEXT("[Cooldown] ApplyCooldown: cooldown applied def=%s entity=[%d:%d]"),
                *Def->GetName(), Entity.Index, Entity.SerialNumber);
        }
    }

    return EStateTreeRunStatus::Succeeded;
}
