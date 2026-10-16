// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Abilities/ArcAbilityHandle.h"
#include "Abilities/ArcAbilityCooldown.h"
#include "ArcRPGCooldown_CascadeReset.generated.h"

struct FMassEntityManager;

/**
 * Cascade soft-cooldown re-implemented as an external FArcAbilityCooldown subclass.
 *
 * Re-uses the shared FArcAbilityCooldownFragment + UArcCooldownProcessor instead of a bespoke
 * cascade fragment/processor. The entry is NON-BLOCKING: it decays and fires OnExpired (which resets
 * the source item's cascade stacks) but never gates re-activation. Non-blocking semantics are enforced
 * BOTH here (CheckCooldown returns true) AND via the entry's bBlocking=false flag, so any other scan that
 * inspects ActiveCooldowns also treats it as non-gating.
 */
USTRUCT(BlueprintType, meta=(DisplayName="Cascade Reset Cooldown"))
struct ARCRPGMECHANICS_API FArcRPGCooldown_CascadeReset : public FArcAbilityCooldown
{
    GENERATED_BODY()

    /** Never blocks recast — both here and via the bBlocking=false entry. */
    virtual bool CheckCooldown(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle, FArcCooldownCheckResult* OutResult = nullptr) const override { return true; }

    /** Non-blocking idle timer — never gates activation. */
    virtual bool IsBlocking() const override { return false; }

    virtual void ApplyCooldown(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle, float* OutAppliedDuration = nullptr) const override;

    virtual void OnExpired(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle) const override;
};
