// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcBuilderTask_BuildMode.h"
#include "ArcBuilderSubsystem.h"
#include "ArcBuilderToolFragment.h"
#include "Items/ArcItemDefinitionData.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Sources/ArcSourceItemResolver.h"
#include "Items/ArcItemData.h"
#include "Tasks/ArcAbilityTaskHelpers.h"
#include "Mass/EntityManager.h"
#include "Engine/LocalPlayer.h"

EStateTreeRunStatus FArcBuilderTask_BuildMode::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);

    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle Entity = AbilityContext.GetEntity();

    // (1) Owning-client gate — server, simulated, or actor-less entity → skip.
    ULocalPlayer* LocalPlayer = ArcAbilityKit::GetLocalPlayerForEntity(EntityManager, Entity);
    if (LocalPlayer == nullptr)
    {
        return EStateTreeRunStatus::Failed;
    }

    // (2) Resolve the builder tool item definition from the ability's item source.
    const FArcItemData* ItemData = ArcAbilityKit::FindSourceItem(EntityManager, Entity, AbilityContext.GetAbilityHandle());
    if (!ItemData)
    {
        return EStateTreeRunStatus::Failed;
    }

    const FArcItemDefinitionData* ItemDefinition = ItemData->GetItemDefinition();
    if (ItemDefinition == nullptr)
    {
        return EStateTreeRunStatus::Failed;
    }

    // (3) The item must actually be a builder tool.
    const FArcItemFragment_BuilderTool* ToolFragment =
        ItemDefinition->FindFragment<FArcItemFragment_BuilderTool>();
    if (ToolFragment == nullptr)
    {
        return EStateTreeRunStatus::Failed;
    }

    // (4) Register the tool's resolved context on the local player's builder subsystem.
    UArcBuilderSubsystem* BuilderSubsystem = LocalPlayer->GetSubsystem<UArcBuilderSubsystem>();
    if (!BuilderSubsystem)
    {
        return EStateTreeRunStatus::Failed;
    }

    FArcBuilderToolContext ToolContext;
    ArcBuilder::ResolveAvailableBuildings(ToolFragment->Buildings, ToolFragment->BuildingFilter, ToolContext.AvailableBuildings);
    ToolContext.BuildingFilter = ToolFragment->BuildingFilter;

    BuilderSubsystem->RegisterTool(MoveTemp(ToolContext));

    return EStateTreeRunStatus::Running;
}

void FArcBuilderTask_BuildMode::ExitState(
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
