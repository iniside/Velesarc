// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcAbilityTask_SpawnActorProjectile.generated.h"

USTRUCT()
struct FArcAbilityTask_SpawnActorProjectileInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter")
    TSubclassOf<AActor> ActorClass;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "GlobalTargeting"))
    FGameplayTag OriginTargetingTag;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "GlobalTargeting"))
    FGameplayTag TargetTargetingTag;

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
    float InitialSpeed = 5000.f;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    bool bIgnoreInstigator = true;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    TWeakObjectPtr<AActor> SpawnedActor;
};

USTRUCT(DisplayName = "Spawn Actor Projectile", Category = "Arc|Ability|Spawning")
struct ARCCORE_API FArcAbilityTask_SpawnActorProjectile : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FArcAbilityTask_SpawnActorProjectileInstanceData::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
