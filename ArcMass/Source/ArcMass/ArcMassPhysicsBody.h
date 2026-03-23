// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "ArcMassPhysicsBody.generated.h"

struct FBodyInstance;

/** Per-entity standalone physics body instances — no owning UPrimitiveComponent.
 *  Entity handle is embedded in Chaos user data via ArcMassPhysicsEntityLink::Attach(). */
USTRUCT()
struct ARCMASS_API FArcMassPhysicsBodyFragment : public FMassFragment
{
    GENERATED_BODY()

    /** One body per collision source. Null entries for sources without collision. */
    TArray<FBodyInstance*> Bodies;

    /** Terminate and delete all physics bodies. Safe to call multiple times. */
    ARCMASS_API void TerminateBodies();
};

template<>
struct TMassFragmentTraits<FArcMassPhysicsBodyFragment> final
{
    enum
    {
        AuthorAcceptsItsNotTriviallyCopyable = true
    };
};
