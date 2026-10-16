// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ArcAnimNotify_MarkGameplayEvent.generated.h"

UCLASS(const, hidecategories = Object, collapsecategories, meta = (DisplayName = "Mark Gameplay Event"))
class ARCABILITYKIT_API UArcAnimNotify_MarkGameplayEvent : public UAnimNotify
{
    GENERATED_UCLASS_BODY()

private:
    UPROPERTY(EditAnywhere, Category = "Config")
    FGameplayTag EventTag;

public:
    const FGameplayTag& GetEventTag() const { return EventTag; }

    virtual FString GetNotifyName_Implementation() const override;
};
