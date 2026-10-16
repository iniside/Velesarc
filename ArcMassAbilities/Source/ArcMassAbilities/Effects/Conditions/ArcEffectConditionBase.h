// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "MassStateTreeDependency.h"
#include "ArcEffectConditionBase.generated.h"

USTRUCT(meta = (Hidden, DisplayName = "Arc Effect Condition Base"))
struct ARCMASSABILITIES_API FArcEffectConditionBase : public FStateTreeConditionBase
{
    GENERATED_BODY()

    virtual void GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const {}
};
