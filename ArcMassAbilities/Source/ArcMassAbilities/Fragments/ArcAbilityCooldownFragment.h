// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Abilities/ArcAbilityHandle.h"
#include "ArcAbilityCooldownFragment.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcCooldownEntry
{
    GENERATED_BODY()

    FArcAbilityHandle AbilityHandle;
    float RemainingTime = 0.f;
    float MaxCooldownTime = 0.f;
    // false = non-blocking timer (e.g. cascade idle reset) that decays + fires OnExpired but never blocks activation.
    bool bBlocking = true;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityCooldownFragment : public FMassFragment
{
    GENERATED_BODY()

    TArray<FArcCooldownEntry> ActiveCooldowns;
};

template<>
struct TMassFragmentTraits<FArcAbilityCooldownFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
