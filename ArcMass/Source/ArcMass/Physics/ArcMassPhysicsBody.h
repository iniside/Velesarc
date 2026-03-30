// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "PhysicsEngine/BodyInstance.h"
#include "ArcMassPhysicsBody.generated.h"

/** Controls how the Chaos physics body is created for Mass entities. */
UENUM(BlueprintType)
enum class EArcMassPhysicsBodyType : uint8
{
	/** Immovable body — trace-only. Cannot respond to forces or impulses. */
	Static,
	/** Non-static body. Starts kinematic; call SetInstanceSimulatePhysics(true) to enable simulation. */
	Dynamic
};

/** Per-entity standalone physics body instance — no owning UPrimitiveComponent.
 *  Entity handle is embedded in Chaos user data via ArcMassPhysicsEntityLink::Attach(). */
USTRUCT()
struct ARCMASS_API FArcMassPhysicsBodyFragment : public FMassFragment
{
    GENERATED_BODY()

    /** Single standalone physics body instance. Null when no physics body is active. */
    FBodyInstance* Body = nullptr;

    /** Terminate and delete the physics body. Safe to call multiple times. */
    void TerminateBody();
};

template<>
struct TMassFragmentTraits<FArcMassPhysicsBodyFragment> final
{
    enum
    {
        AuthorAcceptsItsNotTriviallyCopyable = true
    };
};

namespace UE { namespace ArcMass { namespace Physics
{
	/** Dispatch a batch of async term payloads on a background thread. */
	ARCMASS_API void AsyncTermBodies(TArray<FBodyInstance::FAsyncTermBodyPayload>&& Payloads);
} } } // namespace UE::ArcMass::Physics
