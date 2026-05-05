// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "ArcMassEntityVesselClusterRoot.generated.h"

/**
 * Concrete UObject subclass used as the GC cluster root for
 * UArcMassEntityVessel pool members. Its only purpose is to be a
 * non-abstract anchor for FUObjectClusters; it carries no state and
 * has no behavior. Lives for the lifetime of the owning subsystem.
 */
UCLASS()
class ARCMASSREPLICATIONRUNTIME_API UArcMassEntityVesselClusterRoot : public UObject
{
	GENERATED_BODY()
};
