// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcPlacedEntityRespawnTrackerTypes.generated.h"

class AArcPlacedEntityPartitionActor;

/** Tag exclusively on cell-tracker entities. Used by unrelated queries to opt out of matching them. */
USTRUCT()
struct ARCMASS_API FArcPlacedEntityRespawnTrackerTag : public FMassTag
{
    GENERATED_BODY()
};

/**
 * One-per-partition-actor fragment carrying a back-pointer to the owning AActor.
 * Lifetime: created by the partition actor in BeginPlay, destroyed in EndPlay.
 * The respawn-queue state itself stays on AArcPlacedEntityPartitionActor::PendingRespawns —
 * this fragment is purely the Mass-graph hook that lets a processor invoke per-cell work.
 */
USTRUCT()
struct ARCMASS_API FArcPlacedEntityRespawnTrackerFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY()
    TWeakObjectPtr<AArcPlacedEntityPartitionActor> OwnerPartitionActor;
};

/** TWeakObjectPtr is NOT trivially copyable. Specialization required per CLAUDE.md. */
template <>
struct TMassFragmentTraits<FArcPlacedEntityRespawnTrackerFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
