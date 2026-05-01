// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Abilities/ArcAbilityHandle.h"
#include "ScalableFloat.h"
#include "ArcAbilityCooldown.generated.h"

struct FMassEntityManager;

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityCooldown
{
    GENERATED_BODY()
    virtual ~FArcAbilityCooldown() = default;
    virtual bool CheckCooldown(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle) const PURE_VIRTUAL(FArcAbilityCooldown::CheckCooldown, return true;);
    virtual void ApplyCooldown(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle) const PURE_VIRTUAL(FArcAbilityCooldown::ApplyCooldown,);
};

USTRUCT(BlueprintType, meta=(DisplayName="Duration Cooldown"))
struct ARCMASSABILITIES_API FArcAbilityCooldown_Duration : public FArcAbilityCooldown
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FScalableFloat CooldownDuration;

    virtual bool CheckCooldown(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle) const override;
    virtual void ApplyCooldown(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle) const override;
};
