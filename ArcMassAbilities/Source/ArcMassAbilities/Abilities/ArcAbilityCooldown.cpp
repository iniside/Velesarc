// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/ArcAbilityCooldown.h"
#include "Fragments/ArcAbilityCooldownFragment.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"

bool FArcAbilityCooldown_Duration::CheckCooldown(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle) const
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
        if (Entry.AbilityHandle == Handle)
        {
            return false;
        }
    }

    return true;
}

void FArcAbilityCooldown_Duration::ApplyCooldown(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle) const
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

    const float Duration = CooldownDuration.GetValue();
    FArcCooldownEntry& Entry = Frag->ActiveCooldowns.AddDefaulted_GetRef();
    Entry.AbilityHandle = Handle;
    Entry.RemainingTime = Duration;
    Entry.MaxCooldownTime = Duration;
}
