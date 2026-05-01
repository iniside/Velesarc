// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcAbilityTask_SpawnMassProjectile.generated.h"

class UMassEntityConfigAsset;

USTRUCT()
struct FArcAbilityTask_SpawnMassProjectileInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter")
    TObjectPtr<UMassEntityConfigAsset> EntityConfig = nullptr;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "GlobalTargeting"))
    FGameplayTag OriginTargetingTag;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "GlobalTargeting"))
    FGameplayTag TargetTargetingTag;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
    float InitialSpeed = 5000.f;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    bool bIgnoreInstigator = true;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    FMassEntityHandle SpawnedEntity;
};

USTRUCT(DisplayName = "Spawn Mass Projectile", Category = "Arc|Ability|Spawning")
struct ARCCORE_API FArcAbilityTask_SpawnMassProjectile : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FArcAbilityTask_SpawnMassProjectileInstanceData::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
