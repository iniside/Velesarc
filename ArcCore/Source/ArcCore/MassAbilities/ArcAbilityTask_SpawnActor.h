// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Abilities/Tasks/ArcMassSpawnOrigin.h"
#include "Abilities/Targeting/ArcMassTargetData.h"
#include "ArcAbilityTask_SpawnActor.generated.h"

class AActor;

USTRUCT()
struct FArcAbilityTask_SpawnActorInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter")
    TSubclassOf<AActor> ActorClass;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    EArcMassSpawnOrigin SpawnOrigin = EArcMassSpawnOrigin::HitPoint;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (EditCondition = "SpawnOrigin==EArcMassSpawnOrigin::Custom", EditConditionHides))
    FVector CustomSpawnLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category = "Input")
    FArcMassTargetDataHandle TargetDataHandle;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    TWeakObjectPtr<AActor> SpawnedActor;
};

USTRUCT(DisplayName = "Spawn Actor", Category = "Arc|Ability|Spawning")
struct ARCCORE_API FArcAbilityTask_SpawnActor : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FArcAbilityTask_SpawnActorInstanceData::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
