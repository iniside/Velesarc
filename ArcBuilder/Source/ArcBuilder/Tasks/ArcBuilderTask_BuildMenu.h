// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcNamedPrimaryAssetId.h"
#include "ArcBuilderTask_BuildMenu.generated.h"

USTRUCT()
struct FArcBuilderTask_BuildMenuInstanceData
{
    GENERATED_BODY()

    /** Explicit list of building definitions always offered by this menu. */
    UPROPERTY(EditAnywhere, Category = "Builder", meta = (AllowedClasses = "/Script/ArcBuilder.ArcBuildingDefinition", DisplayThumbnail = false))
    TArray<FArcNamedPrimaryAssetId> Buildings;

    /** Additional buildings are offered when their BuildingTags match this query. Empty query = no tag-derived entries. */
    UPROPERTY(EditAnywhere, Category = "Builder")
    FGameplayTagQuery BuildingFilter;
};

/**
 * Build-menu ability task — item-free variant of FArcBuilderTask_BuildMode.
 *
 * Owning-client-only entry point for the building placement session. On EnterState it
 * resolves the local player for the ability's entity (server / simulated proxies skip),
 * then reads the authored Buildings / BuildingFilter directly off its instance data (there
 * is no builder tool item), resolves the available buildings, and opens a session on the
 * local player's UArcBuilderSubsystem. ExitState closes it.
 *
 * Unlike FArcBuilderTask_BuildMode there is no FArcAbilitySource_Item / builder-tool
 * fragment gate: the build list is authored on the task itself.
 */
USTRUCT(DisplayName = "Build Menu", Category = "Arc|Builder")
struct ARCBUILDER_API FArcBuilderTask_BuildMenu : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcBuilderTask_BuildMenuInstanceData;

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FInstanceDataType::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
