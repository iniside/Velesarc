// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreePropertyFunctionBase.h"
#include "MassStateTreeDependency.h"
#include "ArcAbilityPropertyFunctionBase.generated.h"

USTRUCT(meta = (Hidden, DisplayName = "Arc Ability Property Function Base"))
struct ARCMASSABILITIES_API FArcAbilityPropertyFunctionBase : public FStateTreePropertyFunctionCommonBase
{
    GENERATED_BODY()

    virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const {}
};
