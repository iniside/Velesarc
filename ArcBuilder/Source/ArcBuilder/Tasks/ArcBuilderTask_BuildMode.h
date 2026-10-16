// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcBuilderTask_BuildMode.generated.h"

USTRUCT()
struct FArcBuilderTask_BuildModeInstanceData
{
    GENERATED_BODY()

    // No editable inputs: the builder tool item is resolved from the ability's
    // FArcAbilitySource_Item custom data, so this task needs no bound parameters.
};

/**
 * Build-mode ability task.
 *
 * Owning-client-only entry point for the building placement session. On EnterState it
 * resolves the local player for the ability's entity (server / simulated proxies skip),
 * reads the builder tool item definition from the ability's FArcAbilitySource_Item, and
 * opens a session on the local player's UArcBuilderSubsystem. ExitState closes it.
 *
 * Mirrors the FArcAbilityTask_TargetingPreview skeleton (F-prefixed USTRUCT task on
 * FArcAbilityTaskBase, instance-data USTRUCT, EnterState/ExitState overrides).
 */
USTRUCT(DisplayName = "Build Mode", Category = "Arc|Builder")
struct ARCBUILDER_API FArcBuilderTask_BuildMode : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FArcBuilderTask_BuildModeInstanceData::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
