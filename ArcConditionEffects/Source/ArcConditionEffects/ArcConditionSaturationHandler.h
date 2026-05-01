// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcConditionTypes.h"
#include "Attributes/ArcAttributeHandlerConfig.h"

#include "ArcConditionSaturationHandler.generated.h"

struct FMassEntityManager;

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcConditionSaturationHandler : public FArcAttributeHandler
{
    GENERATED_BODY()

    UPROPERTY()
    EArcConditionType ConditionType = EArcConditionType::Burning;

    virtual void PostChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) override;
};
