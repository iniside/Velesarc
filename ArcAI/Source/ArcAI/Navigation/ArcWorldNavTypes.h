// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "ZoneGraphTypes.h"
#include "ArcWorldNavTypes.generated.h"

UENUM()
enum class EArcWorldNavMode : uint8
{
    LaneFollow,
    NavMeshCarrot,
};

USTRUCT()
struct FArcWorldRouteFragment : public FMassFragment
{
    GENERATED_BODY()

    void Reset()
    {
        RouteLanes.Reset();
        CurrentLaneIndex = 0;
        Destination = FVector::ZeroVector;
        bRouteValid = false;
    }

    bool IsRouteComplete() const
    {
        return bRouteValid && CurrentLaneIndex >= RouteLanes.Num();
    }

    bool AdvanceToNextLane()
    {
        if (CurrentLaneIndex < RouteLanes.Num() - 1)
        {
            ++CurrentLaneIndex;
            return true;
        }
        return false;
    }

    FZoneGraphLaneHandle GetCurrentLaneHandle() const
    {
        if (RouteLanes.IsValidIndex(CurrentLaneIndex))
        {
            return RouteLanes[CurrentLaneIndex];
        }
        return FZoneGraphLaneHandle();
    }

    TArray<FZoneGraphLaneHandle> RouteLanes;
    int32 CurrentLaneIndex = 0;
    FVector Destination = FVector::ZeroVector;
    bool bRouteValid = false;
};

template <>
struct TMassFragmentTraits<FArcWorldRouteFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
