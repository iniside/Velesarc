// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_SpawnMassProjectile.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "ArcMassAbilitiesLogs.h"

EStateTreeRunStatus FArcAbilityTask_SpawnMassProjectile::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    // GAS retired (Bucket D 7). The previous origin / target lookup went through
    // UArcCoreAbilitySystemComponent::GetGlobalHitResult, which no longer exists.
    // Task remains stubbed until a Mass-side targeting source is wired up.
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    const FMassEntityHandle Entity = AbilityContext.GetEntity();
    UWorld* World = AbilityContext.GetMassContext().GetWorld();

    ARCMA_VLOG(World, Warning,
        TEXT("[Ability] SpawnMassProjectile STUBBED entity=[%d:%d]"),
        Entity.Index, Entity.SerialNumber);
    return EStateTreeRunStatus::Failed;
}
