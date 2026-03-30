// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "ArcMassVisISMPhysicsBoundsRecalcProcessor.generated.h"

/**
 * Signal processor that recalculates tight ISM holder bounds when physics bodies
 * go to sleep. While simulating, bounds expand to encompass moving instances but
 * never shrink. This processor fires on PhysicsBodySlept to compute exact bounds
 * from the final resting transforms.
 */
UCLASS()
class ARCMASS_API UArcMassVisISMPhysicsBoundsRecalcProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcMassVisISMPhysicsBoundsRecalcProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context,
		FMassSignalNameLookup& EntitySignals) override;
};
