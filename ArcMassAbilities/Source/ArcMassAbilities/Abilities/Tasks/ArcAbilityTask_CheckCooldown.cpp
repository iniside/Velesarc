// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_CheckCooldown.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityCooldown.h"
#include "Events/ArcGameEventSubsystem.h"
#include "ArcMassAbilitiesLogs.h"

EStateTreeRunStatus FArcAbilityTask_CheckCooldown::EnterState(
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
            TEXT("[Cooldown] CheckCooldown FAILED: ability not found or no definition entity=[%d:%d]"),
            Entity.Index, Entity.SerialNumber);
        return EStateTreeRunStatus::Failed;
    }

    const UArcAbilityDefinition* Def = ActiveAbility->Definition;

    if (Def->Cooldown.IsValid())
    {
        const FArcAbilityCooldown* Cd = Def->Cooldown.GetPtr<FArcAbilityCooldown>();
        if (Cd)
        {
            FArcCooldownCheckResult CdResult;
            const bool bPassed = Cd->CheckCooldown(EntityManager, Entity, Handle, &CdResult);

            InstanceData.bOnCooldown = CdResult.bOnCooldown;
            InstanceData.RemainingSeconds = CdResult.RemainingSeconds;
            InstanceData.MaxCooldownSeconds = CdResult.MaxCooldownSeconds;

            if (!bPassed)
            {
                ARCMA_VLOG(World, Warning,
                    TEXT("[Cooldown] CheckCooldown FAILED: cooldown active def=%s entity=[%d:%d]"),
                    *Def->GetName(), Entity.Index, Entity.SerialNumber);
                if (UArcGameEventSubsystem* EventBus = World ? World->GetSubsystem<UArcGameEventSubsystem>() : nullptr)
                {
                    ArcAbilities::Private::QueueCooldownFailed(*EventBus, EntityManager, Entity, Handle, CdResult);
                }
                if (InstanceData.Handling == EArcGateHandling::FailState)
                {
                    return EStateTreeRunStatus::Failed;
                }
            }
        }
    }

    return EStateTreeRunStatus::Succeeded;
}
