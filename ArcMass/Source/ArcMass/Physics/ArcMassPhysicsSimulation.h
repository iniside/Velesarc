// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityElementTypes.h"

#include "ArcMassPhysicsSimulation.generated.h"

namespace UE::ArcMass::Signals
{
	inline const FName PhysicsTransformUpdated = FName(TEXT("ArcMassPhysicsTransformUpdated"));
	inline const FName PhysicsBodySlept = FName(TEXT("ArcMassPhysicsBodySlept"));
	inline const FName PhysicsBodyRequested = FName(TEXT("ArcMassPhysicsBodyRequested"));
	inline const FName PhysicsBodyReleased = FName(TEXT("ArcMassPhysicsBodyReleased"));
}

/** Sparse tag — entity's transform is owned by Chaos physics simulation.
 *  Added when body switches to dynamic, removed when body goes to sleep. */
USTRUCT()
struct ARCMASS_API FArcMassPhysicsSimulatingTag : public FMassSparseTag
{
	GENERATED_BODY()
};
