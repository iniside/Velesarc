// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcFactionTypes.generated.h"

/** Diplomatic stance between two factions. */
UENUM(BlueprintType)
enum class EArcDiplomaticStance : uint8
{
    Hostile,    // Will attack on sight
    Unfriendly, // Won't trade, won't attack unprovoked
    Neutral,    // Default
    Friendly,   // Will trade, non-aggression
    Allied      // Mutual defense, resource sharing
};

/** Faction archetype — drives reward weights in ML training and fallback heuristic behavior. */
UENUM(BlueprintType)
enum class EArcFactionArchetype : uint8
{
    Economic,
    Militaristic,
    Bandit,
    Nomadic
};
