// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "MassEntityTypes.h"
#include "GameplayTagContainer.h"
#include "Faction/ArcFactionTypes.h"
#include "Strategy/ArcStrategyTypes.h"
#include "ArcFactionFragments.generated.h"

class UArcStrategyActionSet;

/**
 * Core faction identity. One per faction Mass entity.
 */
USTRUCT()
struct ARCECONOMY_API FArcFactionFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Faction")
    FName FactionName;

    UPROPERTY(EditAnywhere, Category = "Faction")
    FGameplayTag FactionArchetypeTag;

    UPROPERTY(EditAnywhere, Category = "Faction")
    EArcFactionArchetype Archetype = EArcFactionArchetype::Economic;

    /** How this faction picks strategic actions (ML or Utility). */
    UPROPERTY(EditAnywhere, Category = "Strategy")
    EArcStrategyDecisionMode StrategyMode = EArcStrategyDecisionMode::Utility;

    /** Action set for utility scoring. Only used when StrategyMode == Utility. */
    UPROPERTY(EditAnywhere, Category = "Strategy")
    TObjectPtr<UArcStrategyActionSet> FactionActionSet;
};

/**
 * Diplomatic stances toward other factions. Keyed by faction entity handle.
 */
USTRUCT()
struct ARCECONOMY_API FArcFactionDiplomacyFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY()
    TMap<FMassEntityHandle, EArcDiplomaticStance> Stances;

    EArcDiplomaticStance GetStanceToward(FMassEntityHandle OtherFaction) const
    {
        const EArcDiplomaticStance* Found = Stances.Find(OtherFaction);
        return Found ? *Found : EArcDiplomaticStance::Neutral;
    }
};

/**
 * List of settlement entity handles owned by this faction.
 */
USTRUCT()
struct ARCECONOMY_API FArcFactionSettlementsFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FMassEntityHandle> OwnedSettlements;
};

/**
 * Links a settlement to its owning faction. Added to settlement entities.
 * Invalid handle = player-owned or unaffiliated.
 */
USTRUCT()
struct ARCECONOMY_API FArcSettlementFactionFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY()
    FMassEntityHandle OwningFaction;
};

// ============================================================================
// Fragment Traits (non-trivially-copyable)
// ============================================================================

template <>
struct TMassFragmentTraits<FArcFactionFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

template <>
struct TMassFragmentTraits<FArcFactionDiplomacyFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

template <>
struct TMassFragmentTraits<FArcFactionSettlementsFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
