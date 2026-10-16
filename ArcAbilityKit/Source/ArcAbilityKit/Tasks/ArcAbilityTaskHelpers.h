// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"

struct FMassEntityManager;
class ULocalPlayer;

namespace ArcAbilityKit
{
    /**
     * Resolve the locally-controlling ULocalPlayer for a Mass entity, if any.
     *
     * Returns nullptr when the entity has no FMassActorFragment, the actor is not
     * a Pawn, the pawn has no PlayerController, or the controller has no
     * ULocalPlayer (dedicated server / simulated proxy / autonomous proxy on a
     * remote machine).
     *
     * Cosmetic owning-client work should gate on a non-null return value.
     */
    ARCABILITYKIT_API ULocalPlayer* GetLocalPlayerForEntity(FMassEntityManager& EntityManager,
                                                            FMassEntityHandle Entity);
}
