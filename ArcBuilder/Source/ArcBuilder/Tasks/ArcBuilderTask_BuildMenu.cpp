// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcBuilderTask_BuildMenu.h"
#include "ArcBuilderSubsystem.h"
#include "ArcBuilderToolFragment.h"
#include "StateTreeExecutionContext.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Tasks/ArcAbilityTaskHelpers.h"
#include "Mass/EntityManager.h"
#include "Engine/LocalPlayer.h"

EStateTreeRunStatus FArcBuilderTask_BuildMenu::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcBuilderTask_BuildMenuInstanceData& InstanceData = Context.GetInstanceData(*this);

    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);

    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle Entity = AbilityContext.GetEntity();

    // (1) Owning-client gate — server, simulated, or actor-less entity → skip.
    ULocalPlayer* LocalPlayer = ArcAbilityKit::GetLocalPlayerForEntity(EntityManager, Entity);
    if (LocalPlayer == nullptr)
    {
        return EStateTreeRunStatus::Failed;
    }

    // (2) Register the authored build list on the local player's builder subsystem.
    UArcBuilderSubsystem* BuilderSubsystem = LocalPlayer->GetSubsystem<UArcBuilderSubsystem>();
    if (!BuilderSubsystem)
    {
        return EStateTreeRunStatus::Failed;
    }

    FArcBuilderToolContext ToolContext;
    ArcBuilder::ResolveAvailableBuildings(InstanceData.Buildings, InstanceData.BuildingFilter, ToolContext.AvailableBuildings);
    ToolContext.BuildingFilter = InstanceData.BuildingFilter;

    BuilderSubsystem->RegisterTool(MoveTemp(ToolContext));

    return EStateTreeRunStatus::Running;
}

void FArcBuilderTask_BuildMenu::ExitState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);

    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle Entity = AbilityContext.GetEntity();

    // World may be tearing down — every lookup is null-guarded.
    ULocalPlayer* LocalPlayer = ArcAbilityKit::GetLocalPlayerForEntity(EntityManager, Entity);
    if (!LocalPlayer)
    {
        return;
    }

    if (UArcBuilderSubsystem* BuilderSubsystem = LocalPlayer->GetSubsystem<UArcBuilderSubsystem>())
    {
        BuilderSubsystem->UnregisterTool();
    }
}
