// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "ArcEffectTaskBase.generated.h"

USTRUCT(meta = (Hidden, DisplayName = "Arc Effect Task Base"))
struct ARCMASSABILITIES_API FArcEffectTaskBase : public FStateTreeTaskBase
{
    GENERATED_BODY()
};
