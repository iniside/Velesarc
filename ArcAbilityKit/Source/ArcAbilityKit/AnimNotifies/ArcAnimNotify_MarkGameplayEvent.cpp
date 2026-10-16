// Copyright Lukasz Baran. All Rights Reserved.

#include "AnimNotifies/ArcAnimNotify_MarkGameplayEvent.h"

UArcAnimNotify_MarkGameplayEvent::UArcAnimNotify_MarkGameplayEvent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

FString UArcAnimNotify_MarkGameplayEvent::GetNotifyName_Implementation() const
{
    if (EventTag.IsValid())
    {
        return FString::Printf(TEXT("MarkGameplayEvent: %s"), *EventTag.ToString());
    }
    return TEXT("MarkGameplayEvent");
}
