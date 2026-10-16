// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcTargetingPipelineTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcAbilityTask_TargetingPreview.generated.h"

class UArcTargetingPreset;

USTRUCT()
struct FArcAbilityTask_TargetingPreviewInstanceData
{
    GENERATED_BODY()

    // --- Inputs ---

    UPROPERTY(EditAnywhere, Category = "Parameter")
    TObjectPtr<UArcTargetingPreset> TargetingPreset = nullptr;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (MustImplement = "/Script/ArcAbilityKit.ArcTargetingPreview"))
    TSubclassOf<AActor> PreviewActorClass;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (BaseStruct = "/Script/ArcAbilityKit.ArcPreviewShapeBase"))
    FInstancedStruct ShapeParams;

    // --- Internal state ---

    UPROPERTY(VisibleAnywhere, Category = "Output", Transient)
    TWeakObjectPtr<AActor> SpawnedActor;

    FArcTargetingHandle RequestHandle;
};

USTRUCT(DisplayName = "Targeting Preview", Category = "Arc|Ability|Targeting")
struct ARCABILITYKIT_API FArcAbilityTask_TargetingPreview : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FArcAbilityTask_TargetingPreviewInstanceData::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
