// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "ArcAbilityTaskBase.generated.h"

USTRUCT(meta = (Hidden, DisplayName = "Arc Ability Task Base"))
struct ARCMASSABILITIES_API FArcAbilityTaskBase : public FStateTreeTaskBase
{
    GENERATED_BODY()
};
