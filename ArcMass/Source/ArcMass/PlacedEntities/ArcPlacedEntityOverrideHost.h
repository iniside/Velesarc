// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ArcPlacedEntityTypes.h"
#include "ArcPlacedEntityOverrideHost.generated.h"

/**
 * Interface implemented by actors that own a TArray<FArcPlacedEntityEntry> and support
 * per-instance fragment override editing through UArcPlacedEntityProxyEditingObject.
 * Both AArcPlacedEntityPartitionActor and AArcAlwaysLoadedEntityActor implement this so the
 * MassEntity details path does not need to cast to a specific concrete actor type.
 */
UINTERFACE(MinimalAPI)
class UArcPlacedEntityOverrideHost : public UInterface
{
	GENERATED_BODY()
};

class IArcPlacedEntityOverrideHost
{
	GENERATED_BODY()

public:
	/**
	 * Returns a mutable reference to the actor's placed entity entries array.
	 * Used by UArcPlacedEntityProxyEditingObject to read fragment definitions and write overrides.
	 */
	virtual TArray<FArcPlacedEntityEntry>& GetPlacedEntityEntries() = 0;
};
