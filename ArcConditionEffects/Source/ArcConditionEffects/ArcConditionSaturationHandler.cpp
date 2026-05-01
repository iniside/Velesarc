// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcConditionSaturationHandler.h"
#include "ArcConditionEffectsSubsystem.h"
#include "MassEntitySubsystem.h"

void FArcConditionSaturationHandler::PostChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue)
{
    float Delta = NewValue - OldValue;
    if (FMath::IsNearlyZero(Delta))
    {
        return;
    }

    UArcConditionEffectsSubsystem* Subsystem = Context.World ? Context.World->GetSubsystem<UArcConditionEffectsSubsystem>() : nullptr;
    if (Subsystem)
    {
        Subsystem->ApplyCondition(Context.Entity, ConditionType, Delta);
    }
}
