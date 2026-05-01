// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcGovernorActivateRecipesTask.generated.h"

USTRUCT()
struct FArcGovernorActivateRecipesInstanceData
{
    GENERATED_BODY()
};

/**
 * Governor task: assigns recipes to empty building slots.
 * Prefers recipes with higher BasePrice output. Always assigns — no profitability gate.
 */
USTRUCT(DisplayName = "Arc Governor Activate Recipes", meta = (Category = "Economy|Governor"))
struct ARCECONOMY_API FArcGovernorActivateRecipesTask : public FMassStateTreeTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcGovernorActivateRecipesInstanceData;

    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual bool Link(FStateTreeLinker& Linker) override;
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
