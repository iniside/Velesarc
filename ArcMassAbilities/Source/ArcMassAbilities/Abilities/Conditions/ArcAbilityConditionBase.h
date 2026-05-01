// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "ArcAbilityConditionBase.generated.h"

USTRUCT(meta = (Hidden, DisplayName = "Arc Ability Condition Base"))
struct ARCMASSABILITIES_API FArcAbilityConditionBase : public FStateTreeConditionBase
{
    GENERATED_BODY()
};
