// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Abilities/ArcAbilityHandle.h"
#include "ArcAbilityActivationRequirement.generated.h"

struct FMassEntityManager;
struct FMassEntityView;
class UArcAbilityDefinition;

struct FArcAbilityActivationContext
{
    FMassEntityManager& EntityManager;
    FMassEntityHandle Entity;
    FArcAbilityHandle AbilityHandle;
    const UArcAbilityDefinition* Definition = nullptr;
    FMassEntityView& EntityView;   // requirements fetch the sparse fragments they need
};

/**
 * Pluggable, additive activation gate authored on an ability definition. External plugins subclass
 * this to add custom activation requirements (e.g. line-of-sight, equipped-weapon) evaluated at
 * activation time. Return false to block activation. These run AFTER the built-in tag/cost/cooldown
 * gates — they cannot gate before cost (a deliberate v1 constraint).
 */
USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityActivationRequirement
{
    GENERATED_BODY()
    virtual ~FArcAbilityActivationRequirement() = default;
    virtual bool Check(const FArcAbilityActivationContext& Context) const PURE_VIRTUAL(FArcAbilityActivationRequirement::Check, return true;);
};
