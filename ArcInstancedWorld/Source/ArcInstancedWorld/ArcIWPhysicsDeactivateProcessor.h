// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassSignalProcessorBase.h"
#include "ArcIWPhysicsDeactivateProcessor.generated.h"

// ---------------------------------------------------------------------------
// Physics Deactivate Processor
// Subscribes to PhysicsCellDeactivated signal -- destroys physics bodies.
// ---------------------------------------------------------------------------

UCLASS()
class ARCINSTANCEDWORLD_API UArcIWPhysicsDeactivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcIWPhysicsDeactivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
