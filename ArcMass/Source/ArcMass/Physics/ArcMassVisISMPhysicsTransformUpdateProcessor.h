// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "ArcMassVisISMPhysicsTransformUpdateProcessor.generated.h"

/**
 * Signal processor that updates ISM instance transforms in response to
 * PhysicsTransformUpdated. Reads FTransformFragment and writes to the
 * ISM holder's PerInstanceSMData.
 */
UCLASS()
class ARCMASS_API UArcMassVisISMPhysicsTransformUpdateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcMassVisISMPhysicsTransformUpdateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context,
		FMassSignalNameLookup& EntitySignals) override;
};
