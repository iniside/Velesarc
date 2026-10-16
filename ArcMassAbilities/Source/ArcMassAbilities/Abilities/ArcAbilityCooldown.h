// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Abilities/ArcAbilityHandle.h"
#include "ArcScalableFloat.h"
#include "Effects/ArcEffectTypes.h"
#include "ArcAbilityCooldown.generated.h"

struct FMassEntityManager;

struct ARCMASSABILITIES_API FArcCooldownCheckResult
{
    bool  bOnCooldown = false;
    float RemainingSeconds = 0.f;
    float MaxCooldownSeconds = 0.f;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityCooldown
{
    GENERATED_BODY()
    virtual ~FArcAbilityCooldown() = default;
    virtual bool CheckCooldown(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle, FArcCooldownCheckResult* OutResult = nullptr) const PURE_VIRTUAL(FArcAbilityCooldown::CheckCooldown, return true;);
    virtual void ApplyCooldown(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle, float* OutAppliedDuration = nullptr) const PURE_VIRTUAL(FArcAbilityCooldown::ApplyCooldown,);

    /** Whether an active entry for this cooldown blocks re-activation. Override to false for non-blocking idle timers that decay and fire OnExpired without gating activation. */
    virtual bool IsBlocking() const { return true; }

    /** Called by the cooldown processor when an entry expires, before removal. Override to run on-expiry side effects (e.g. from external plugins). */
    virtual void OnExpired(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle) const {}
};

USTRUCT(BlueprintType, meta=(DisplayName="Duration Cooldown"))
struct ARCMASSABILITIES_API FArcAbilityCooldown_Duration : public FArcAbilityCooldown
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FArcScalableFloat CooldownDuration;

    UPROPERTY(EditAnywhere)
    EArcRangeResolution RangeResolution = EArcRangeResolution::Roll;

    virtual bool CheckCooldown(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle, FArcCooldownCheckResult* OutResult = nullptr) const override;
    virtual void ApplyCooldown(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle, float* OutAppliedDuration = nullptr) const override;
};
