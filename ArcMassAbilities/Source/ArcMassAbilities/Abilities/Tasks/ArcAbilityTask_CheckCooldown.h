// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcAbilityTask_CheckCooldown.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityTask_CheckCooldown_InstanceData
{
    GENERATED_BODY()

    // How to handle a cooldown-active result: FailState returns Failed (hard gate);
    // HandleInTree returns Succeeded with bOnCooldown=true so the tree can branch.
    UPROPERTY(EditAnywhere, Category = "Parameter")
    EArcGateHandling Handling = EArcGateHandling::HandleInTree;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    bool bOnCooldown = false;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    float RemainingSeconds = 0.f;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    float MaxCooldownSeconds = 0.f;
};

// Conditional gate task: checks whether the ability's cooldown is currently active.
// Use in bGateInStateTree trees to replace the input processor's built-in cooldown GATE.
USTRUCT(meta = (DisplayName = "Check Cooldown"))
struct ARCMASSABILITIES_API FArcAbilityTask_CheckCooldown : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcAbilityTask_CheckCooldown_InstanceData;
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
