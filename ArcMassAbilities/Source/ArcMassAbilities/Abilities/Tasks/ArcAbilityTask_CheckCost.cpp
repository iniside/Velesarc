// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_CheckCost.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcMassAbilityCost.h"
#include "Events/ArcGameEventSubsystem.h"
#include "ArcMassAbilitiesLogs.h"

EStateTreeRunStatus FArcAbilityTask_CheckCost::EnterState(
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
            TEXT("[Cost] CheckCost FAILED: ability not found or no definition entity=[%d:%d]"),
            Entity.Index, Entity.SerialNumber);
        return EStateTreeRunStatus::Failed;
    }

    const UArcAbilityDefinition* Def = ActiveAbility->Definition;

    if (Def->Cost.IsValid())
    {
        const FArcMassAbilityCost* Cost = Def->Cost.GetPtr<FArcMassAbilityCost>();
        if (Cost)
        {
            FArcMassAbilityCostContext CostCtx{EntityManager, Entity, Handle, Def};
            FArcCostCheckResult CostResult;
            const bool bPassed = Cost->CheckCost(CostCtx, &CostResult);

            InstanceData.bCanAfford = CostResult.bCanAfford;
            InstanceData.RequiredAmount = CostResult.RequiredAmount;
            InstanceData.CurrentAmount = CostResult.CurrentAmount;
            InstanceData.MissingAmount = CostResult.MissingAmount;
            InstanceData.Reason = CostResult.Reason;

            if (!bPassed)
            {
                ARCMA_VLOG(World, Warning,
                    TEXT("[Cost] CheckCost FAILED: cost insufficient def=%s entity=[%d:%d]"),
                    *Def->GetName(), Entity.Index, Entity.SerialNumber);
                if (UArcGameEventSubsystem* EventBus = World ? World->GetSubsystem<UArcGameEventSubsystem>() : nullptr)
                {
                    ArcAbilities::Private::QueueCostFailed(*EventBus, EntityManager, Entity, Handle, CostResult);
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
