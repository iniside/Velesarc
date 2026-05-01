// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeDelegate.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcAbilityTask_ListenInputHeld.generated.h"

USTRUCT()
struct FArcAbilityTask_ListenInputHeldInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "InputTag"))
    FGameplayTag InputTag;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    float MinHeldDuration = 0.f;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    FStateTreeDelegateDispatcher OnHeldStarted;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    FStateTreeDelegateDispatcher OnHeldStopped;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    float HeldDuration = 0.f;
};

USTRUCT(DisplayName = "Listen Input Held", Category = "Arc|Ability|Input")
struct ARCMASSABILITIES_API FArcAbilityTask_ListenInputHeld : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcAbilityTask_ListenInputHeldInstanceData;

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FInstanceDataType::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
