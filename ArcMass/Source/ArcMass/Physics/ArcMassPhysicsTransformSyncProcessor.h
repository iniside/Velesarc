// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ArcMassPhysicsTransformSyncProcessor.generated.h"

/**
 * PostPhysics processor that syncs Chaos body transforms back to FTransformFragment
 * for entities with FArcMassPhysicsSimulatingTag. Detects body sleep and restores
 * kinematic mode, removing the sparse tag.
 */
UCLASS()
class ARCMASS_API UArcMassPhysicsTransformSyncProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcMassPhysicsTransformSyncProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
