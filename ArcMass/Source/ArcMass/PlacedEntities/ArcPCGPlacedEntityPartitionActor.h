// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ArcPCGPlacedEntityPartitionActor.generated.h"

/**
 * PCG-owned placed entity partition actor.
 * Inherits full SpawnEntities() and editor preview ISMCs from parent.
 * Disables instance editing — PCG manages these instances.
 * Lives in the runtime ArcMass module so PCG-baked actors survive cook.
 */
UCLASS()
class ARCMASS_API AArcPCGPlacedEntityPartitionActor : public AArcPlacedEntityPartitionActor
{
	GENERATED_BODY()

public:
	AArcPCGPlacedEntityPartitionActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:
	//~ IArcMassEntityManager overrides — disable editing on componentless MassEntity path
	virtual bool CanEditMassEntityInstance(const FArcMassEntityElementId& InId) const override;
	virtual bool CanMoveMassEntityInstance(const FArcMassEntityElementId& InId, ETypedElementWorldType WorldType) const override;
	virtual bool DeleteMassEntityInstances(TArrayView<const FArcMassEntityElementId> InIds) override;
	virtual bool DuplicateMassEntityInstances(TArrayView<const FArcMassEntityElementId> InIds, TArray<FArcMassEntityElementId>& OutNewIds) override;
};
