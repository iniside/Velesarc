// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"

struct FBodyInstance;
struct FMassEntityManager;

namespace ArcMassPhysicsForce
{
	ARCMASS_API void ApplyImpulse(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FBodyInstance& Body, FVector Impulse);
	ARCMASS_API void AddForce(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FBodyInstance& Body, FVector Force);
}
