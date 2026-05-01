// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Abilities/Conditions/ArcAbilityConditionBase.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "ArcAbilityCondition_InputState.generated.h"

USTRUCT(DisplayName = "Input State", Category = "Arc|Ability|Input")
struct ARCMASSABILITIES_API FArcAbilityCondition_InputState : public FArcAbilityConditionBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "InputTag"))
    FGameplayTag InputTag;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    EArcInputState RequiredState = EArcInputState::None;

    virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
