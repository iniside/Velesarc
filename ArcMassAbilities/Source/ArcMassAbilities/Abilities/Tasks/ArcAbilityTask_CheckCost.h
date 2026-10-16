// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Abilities/ArcMassAbilityCost.h"
#include "ArcAbilityTask_CheckCost.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityTask_CheckCost_InstanceData
{
    GENERATED_BODY()

    // How to handle a cannot-afford result: FailState returns Failed (hard gate);
    // HandleInTree returns Succeeded with bCanAfford=false so the tree can branch.
    UPROPERTY(EditAnywhere, Category = "Parameter")
    EArcGateHandling Handling = EArcGateHandling::HandleInTree;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    bool bCanAfford = true;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    float RequiredAmount = 0.f;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    float CurrentAmount = 0.f;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    float MissingAmount = 0.f;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    EArcCostFailReason Reason = EArcCostFailReason::Insufficient;
};

// Conditional gate task: checks whether the ability's cost can be afforded.
// CHECK only — does NOT apply the cost (Commit still applies it).
// Use in bGateInStateTree trees to replace the input processor's built-in cost GATE.
USTRUCT(meta = (DisplayName = "Check Cost"))
struct ARCMASSABILITIES_API FArcAbilityTask_CheckCost : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcAbilityTask_CheckCost_InstanceData;
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
