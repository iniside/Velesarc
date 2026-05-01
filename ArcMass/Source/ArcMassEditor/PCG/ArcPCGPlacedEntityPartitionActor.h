// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ArcPCGPlacedEntityPartitionActor.generated.h"

/**
 * PCG-owned placed entity partition actor.
 * Inherits full SpawnEntities() and editor preview ISMCs from parent.
 * Disables instance editing — PCG manages these instances.
 */
UCLASS()
class AArcPCGPlacedEntityPartitionActor : public AArcPlacedEntityPartitionActor
{
	GENERATED_BODY()

public:
	AArcPCGPlacedEntityPartitionActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:
	//~ ISMInstanceManager overrides — disable editing
	virtual bool CanEditSMInstance(const FSMInstanceId& InstanceId) const override;
	virtual bool CanMoveSMInstance(const FSMInstanceId& InstanceId, const ETypedElementWorldType WorldType) const override;
	virtual bool DeleteSMInstances(TArrayView<const FSMInstanceId> InstanceIds) override;
	virtual bool DuplicateSMInstances(TArrayView<const FSMInstanceId> InstanceIds, TArray<FSMInstanceId>& OutNewInstanceIds) override;
};
