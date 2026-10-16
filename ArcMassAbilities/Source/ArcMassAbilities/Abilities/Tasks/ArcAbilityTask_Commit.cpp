// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_Commit.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcMassAbilityCost.h"
#include "Abilities/ArcAbilityCooldown.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Events/ArcGameEventSubsystem.h"
#include "ArcMassAbilitiesLogs.h"

EStateTreeRunStatus FArcAbilityTask_Commit::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle Entity = AbilityContext.GetEntity();
    const FArcAbilityHandle Handle = AbilityContext.GetAbilityHandle();
    UWorld* World = AbilityContext.GetMassContext().GetWorld();

    ARCMA_VLOG(World, Log,
        TEXT("[Ability] Commit enter entity=[%d:%d]"),
        Entity.Index, Entity.SerialNumber);

    const FArcActiveAbility* ActiveAbility = ArcAbilities::FindAbility(EntityManager, Entity, Handle);
    if (!ActiveAbility || !ActiveAbility->Definition)
    {
        ARCMA_VLOG(World, Warning,
            TEXT("[Ability] Commit FAILED: ability not found or no definition entity=[%d:%d]"),
            Entity.Index, Entity.SerialNumber);
        return EStateTreeRunStatus::Failed;
    }

    const UArcAbilityDefinition* Def = ActiveAbility->Definition;

    if (Def->Cost.IsValid())
    {
        const FArcMassAbilityCost* CostPtr = Def->Cost.GetPtr<FArcMassAbilityCost>();
        if (CostPtr)
        {
            FArcMassAbilityCostContext CostCtx{EntityManager, Entity, Handle, Def};
            FArcCostCheckResult CostResult;
            const bool bCanAfford = CostPtr->CheckCost(CostCtx, &CostResult);

            InstanceData.bCanAffordCost = CostResult.bCanAfford;
            InstanceData.MissingAmount = CostResult.MissingAmount;
            InstanceData.Reason = CostResult.Reason;

            if (!bCanAfford)
            {
                ARCMA_VLOG(World, Warning,
                    TEXT("[Cost] Commit FAILED: cost check failed def=%s entity=[%d:%d]"),
                    *Def->GetName(), Entity.Index, Entity.SerialNumber);
                if (UArcGameEventSubsystem* EventBus = World ? World->GetSubsystem<UArcGameEventSubsystem>() : nullptr)
                {
                    ArcAbilities::Private::QueueCostFailed(*EventBus, EntityManager, Entity, Handle, CostResult);
                }
                if (InstanceData.Handling == EArcGateHandling::FailState)
                {
                    return EStateTreeRunStatus::Failed;
                }
                // HandleInTree: surface outputs but do not apply anything
                return EStateTreeRunStatus::Succeeded;
            }

            CostPtr->ApplyCost(CostCtx);
            InstanceData.bCostApplied = true;
            ARCMA_VLOG(World, Log,
                TEXT("[Cost] Commit: cost applied def=%s entity=[%d:%d]"),
                *Def->GetName(), Entity.Index, Entity.SerialNumber);
        }
    }

    if (!Def->bGateInStateTree)
    {
        if (Def->Cooldown.IsValid())
        {
            const FArcAbilityCooldown* CooldownPtr = Def->Cooldown.GetPtr<FArcAbilityCooldown>();
            if (CooldownPtr)
            {
                float AppliedCooldownDuration = 0.f;
                CooldownPtr->ApplyCooldown(EntityManager, Entity, Handle, &AppliedCooldownDuration);
                InstanceData.bCooldownApplied = true;
                InstanceData.AppliedCooldownDuration = AppliedCooldownDuration;
                ARCMA_VLOG(World, Log,
                    TEXT("[Cooldown] Commit: cooldown applied def=%s entity=[%d:%d]"),
                    *Def->GetName(), Entity.Index, Entity.SerialNumber);
            }
        }
    }

    InstanceData.bCommitted = true;
    ARCMA_VLOG(World, Log,
        TEXT("[Ability] Commit SUCCEEDED def=%s entity=[%d:%d]"),
        *Def->GetName(), Entity.Index, Entity.SerialNumber);
    return EStateTreeRunStatus::Succeeded;
}
