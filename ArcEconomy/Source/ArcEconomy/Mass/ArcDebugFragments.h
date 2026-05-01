// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityElementTypes.h"
#include "Mass/ArcGovernorLogic.h"
#include "ArcDebugFragments.generated.h"

// ============================================================================
// Debug Command Queue (added lazily by debugger, drained by governor)
// ============================================================================

/**
 * Sparse fragment added to settlement entities by the debugger.
 * The governor processor drains it each tick.
 * Members are plain C++ (not UPROPERTY) because ArcGovernor pending-change
 * structs are not USTRUCT types.
 */
USTRUCT()
struct ARCECONOMY_API FArcDebugCommandQueueFragment : public FMassFragment
{
    GENERATED_BODY()

    TArray<ArcGovernor::FArcGovernorPendingNPCChange> PendingNPCChanges;
    TArray<ArcGovernor::FArcGovernorPendingSlotChange> PendingSlotChanges;
};

template <>
struct TMassFragmentTraits<FArcDebugCommandQueueFragment> final
{
    enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
