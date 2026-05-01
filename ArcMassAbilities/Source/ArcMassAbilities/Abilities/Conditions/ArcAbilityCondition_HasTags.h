// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Conditions/ArcAbilityConditionBase.h"
#include "GameplayTagContainer.h"
#include "ArcAbilityCondition_HasTags.generated.h"

USTRUCT(DisplayName = "Has Tags", Category = "Arc|Ability")
struct ARCMASSABILITIES_API FArcAbilityCondition_HasTags : public FArcAbilityConditionBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter")
    FGameplayTagContainer RequiredTags;

    virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
