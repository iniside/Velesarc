// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreePropertyFunctionBase.h"
#include "MassStateTreeDependency.h"
#include "ArcEffectPropertyFunctionBase.generated.h"

USTRUCT(meta = (Hidden, DisplayName = "Arc Effect Property Function Base"))
struct ARCMASSABILITIES_API FArcEffectPropertyFunctionBase : public FStateTreePropertyFunctionCommonBase
{
    GENERATED_BODY()

    virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const {}
};
