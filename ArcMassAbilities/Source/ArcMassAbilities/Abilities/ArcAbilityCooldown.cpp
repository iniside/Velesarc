// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/ArcAbilityCooldown.h"
#include "Effects/ArcEffectFunctions.h"
#include "Fragments/ArcAbilityCooldownFragment.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"

bool FArcAbilityCooldown_Duration::CheckCooldown(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle, FArcCooldownCheckResult* OutResult) const
{
    if (!EntityManager.IsEntityValid(Entity))
    {
        return false;
    }

    FMassEntityView View(EntityManager, Entity);
    const FArcAbilityCooldownFragment* Frag = View.GetFragmentDataPtr<FArcAbilityCooldownFragment>();
    if (!Frag)
    {
        return true;
    }

    for (const FArcCooldownEntry& Entry : Frag->ActiveCooldowns)
    {
        if (Entry.AbilityHandle == Handle && Entry.bBlocking)
        {
            if (OutResult)
            {
                OutResult->bOnCooldown = true;
                OutResult->RemainingSeconds = Entry.RemainingTime;
                OutResult->MaxCooldownSeconds = Entry.MaxCooldownTime;
            }
            return false;
        }
    }

    return true;
}

void FArcAbilityCooldown_Duration::ApplyCooldown(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle, float* OutAppliedDuration) const
{
    if (!EntityManager.IsEntityValid(Entity))
    {
        return;
    }

    FMassEntityView View(EntityManager, Entity);
    FArcAbilityCooldownFragment* Frag = View.GetFragmentDataPtr<FArcAbilityCooldownFragment>();
    if (!Frag)
    {
        return;
    }

    const float Duration = ArcEffects::ResolveScalableMagnitude(CooldownDuration, RangeResolution);
    if (OutAppliedDuration)
    {
        *OutAppliedDuration = Duration;
    }
    FArcCooldownEntry& Entry = Frag->ActiveCooldowns.AddDefaulted_GetRef();
    Entry.AbilityHandle = Handle;
    Entry.RemainingTime = Duration;
    Entry.MaxCooldownTime = Duration;
    Entry.bBlocking = IsBlocking();
}
