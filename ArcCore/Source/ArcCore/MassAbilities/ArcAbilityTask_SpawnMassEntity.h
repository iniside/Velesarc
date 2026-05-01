// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Abilities/Tasks/ArcMassSpawnOrigin.h"
#include "Abilities/Targeting/ArcMassTargetData.h"
#include "ArcAbilityTask_SpawnMassEntity.generated.h"

class UMassEntityConfigAsset;

USTRUCT()
struct FArcAbilityTask_SpawnMassEntityInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter")
    TObjectPtr<UMassEntityConfigAsset> EntityConfig = nullptr;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    EArcMassSpawnOrigin SpawnOrigin = EArcMassSpawnOrigin::HitPoint;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (EditCondition = "SpawnOrigin==EArcMassSpawnOrigin::Custom", EditConditionHides))
    FVector CustomSpawnLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category = "Input")
    FArcMassTargetDataHandle TargetDataHandle;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    FMassEntityHandle SpawnedEntity;
};

USTRUCT(DisplayName = "Spawn Mass Entity", Category = "Arc|Ability|Spawning")
struct ARCCORE_API FArcAbilityTask_SpawnMassEntity : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FArcAbilityTask_SpawnMassEntityInstanceData::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
